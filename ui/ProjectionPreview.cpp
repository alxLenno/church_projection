#include "ProjectionPreview.h"
#include <QPainter>
#include <QPainterPath>
#include <QTextOption>

using namespace Projection;

ProjectionPreview::ProjectionPreview(QWidget *parent) : QOpenGLWidget(parent) {
  setMinimumSize(320, 180); // Reduced for dashboard

  currentLayout = LayoutType::Single;
  setupLayer(0);
  setupLayer(1);

  // Timer for preview animation — only updates when scrolling
  renderTimer = new QTimer(this);
  connect(renderTimer, &QTimer::timeout, this, [this]() {
    bool needsUpdate = false;
    for (auto len : layers) {
      if (len->content.formatting.isScrolling && !len->content.text.isEmpty()) {
        len->scrollOffset += (float)len->content.formatting.scrollSpeed;
        needsUpdate = true;
      } else {
        len->scrollOffset = 0;
      }
    }
    if (needsUpdate)
      update();
  });
  renderTimer->start(16);
}

void ProjectionPreview::setupLayer(int idx) {
  LayerState *ls = new LayerState();
  ls->mediaPlayer = new QMediaPlayer(this);
  ls->audioOutput = new QAudioOutput(this);
  ls->mediaPlayer->setAudioOutput(ls->audioOutput);
  ls->audioOutput->setMuted(true);
  ls->mediaPlayer->setLoops(QMediaPlayer::Infinite);

  ls->videoSink = new QVideoSink(this);
  ls->mediaPlayer->setVideoSink(ls->videoSink);

  connect(ls->videoSink, &QVideoSink::videoFrameChanged, this,
          [this, idx](const QVideoFrame &frame) {
            if (frame.isValid()) {
              layers[idx]->content.videoFrame = frame;
              update();
            }
          });

  layers.push_back(ls);
}

void ProjectionPreview::setLayerText(int layerIdx, const QString &text) {
  if (layerIdx < 0 || layerIdx >= (int)layers.size())
    return;
  layers[layerIdx]->content.text = text;
  update();
}

void ProjectionPreview::setLayerBackground(int layerIdx, BackgroundType type,
                                           const QString &path,
                                           const QColor &color) {
  if (layerIdx < 0 || layerIdx >= (int)layers.size())
    return;

  LayerState *ls = layers[layerIdx];
  ls->content.bgType = type;
  ls->content.bgPath = path;
  ls->content.bgColor = color;

  // Invalidate pixmap cache
  ls->content.cachedPixmap = QPixmap();
  ls->content.cachedPixmapSize = QSize();

  if (type == BackgroundType::Video) {
    ls->isVideoActive = true;
    ls->mediaPlayer->setSource(QUrl::fromLocalFile(path));
    ls->mediaPlayer->play();
  } else {
    ls->isVideoActive = false;
    ls->mediaPlayer->stop();
    if (type == BackgroundType::Image && !path.isEmpty()) {
      ls->content.pixmap = QPixmap(path);
    }
  }
  update();
}

void ProjectionPreview::setLayoutType(LayoutType type) {
  currentLayout = type;
  update();
}

void ProjectionPreview::clearLayer(int layerIdx) {
  if (layerIdx < 0 || layerIdx >= (int)layers.size())
    return;
  LayerState *ls = layers[layerIdx];

  // Save formatting BEFORE reset (matches ProjectionWindow fix)
  auto savedFmt = ls->content.formatting;

  ls->mediaPlayer->stop();
  ls->isVideoActive = false;
  ls->isVideoActive = false;
  ls->content = Content();

  // Ensure media is cleared
  ls->content.mediaType = Content::MediaType::None;
  ls->content.renderedMedia = QImage();

  ls->content.formatting = savedFmt;

  update();
}

void ProjectionPreview::setLayerMedia(int layerIdx,
                                      Projection::Content::MediaType type,
                                      const QString &path, int page,
                                      const QImage &rendered) {
  if (layerIdx < 0 || layerIdx >= (int)layers.size())
    return;

  LayerState *ls = layers[layerIdx];
  ls->content.mediaType = type;
  ls->content.mediaPath = path;
  ls->content.pageNumber = page;
  ls->content.renderedMedia = rendered;

  // Clear text if media is set (optional, but typical)
  ls->content.text = "";

  update();
}

void ProjectionPreview::setLayerFormatting(
    int layerIdx, const Projection::TextFormatting &fmt) {
  if (layerIdx < 0 || layerIdx >= (int)layers.size())
    return;
  layers[layerIdx]->content.formatting = fmt;
  update();
}

// Legacy API
void ProjectionPreview::updateText(const QString &text) {
  setLayerText(0, text);
}

void ProjectionPreview::setBackgroundImage(const QString &path) {
  setLayerBackground(0, BackgroundType::Image, path);
}

void ProjectionPreview::setBackgroundVideo(const QString &path) {
  setLayerBackground(0, BackgroundType::Video, path);
}

void ProjectionPreview::setBackgroundColor(const QColor &color) {
  setLayerBackground(0, BackgroundType::Color, "", color);
}

void ProjectionPreview::clear() {
  clearLayer(0);
  clearLayer(1);
}

void ProjectionPreview::resizeEvent(QResizeEvent *event) {
  // Invalidate all cached pixmaps on resize
  for (auto *ls : layers) {
    ls->content.cachedPixmap = QPixmap();
    ls->content.cachedPixmapSize = QSize();
  }
  QOpenGLWidget::resizeEvent(event);
}

void ProjectionPreview::paintEvent(QPaintEvent *event) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setRenderHint(QPainter::TextAntialiasing);
  painter.setRenderHint(QPainter::SmoothPixmapTransform);

  // Background Fill
  painter.fillRect(rect(), Qt::black);

  if (currentLayout == LayoutType::Single) {
    drawContent(painter, 0, rect(), true);
  } else if (currentLayout == LayoutType::SplitVertical) {
    int mid = width() / 2;
    QRect leftRect(0, 0, mid, height());
    QRect rightRect(mid, 0, width() - mid, height());

    drawContent(painter, 0, leftRect, true);
    drawContent(painter, 1, rightRect, true);

    // Separator
    painter.setPen(QPen(QColor(255, 255, 255, 100), 2));
    painter.drawLine(mid, 0, mid, height());

  } else if (currentLayout == LayoutType::SplitHorizontal) {
    // Horizontal Split with Independent Backgrounds
    int mid = height() / 2;
    QRect topRect(0, 0, width(), mid);
    QRect bottomRect(0, mid, width(), height() - mid);

    drawContent(painter, 0, topRect, true);
    drawContent(painter, 1, bottomRect, true);

    // Horizontal Separator
    painter.setPen(QPen(QColor(255, 255, 255, 100), 2));
    painter.drawLine(0, mid, width(), mid);
  }

  // Border
  QPen pen(QColor("#334155"));
  pen.setWidth(2);
  painter.setPen(pen);
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(rect().adjusted(1, 1, -1, -1));
}

void ProjectionPreview::drawBackground(QPainter &painter, int idx,
                                       const QRect &rect) {
  if (idx < 0 || idx >= (int)layers.size())
    return;
  LayerState *ls = layers[idx];
  Content &c = ls->content;

  painter.save();
  painter.setClipRect(rect);

  if (ls->isVideoActive && c.videoFrame.isValid()) {
    QImage img = c.videoFrame.toImage();
    QSize scaledSize =
        img.size().scaled(rect.size(), Qt::KeepAspectRatioByExpanding);
    QRect targetRect(rect.center().x() - scaledSize.width() / 2,
                     rect.center().y() - scaledSize.height() / 2,
                     scaledSize.width(), scaledSize.height());
    painter.drawImage(targetRect, img);
  } else if (!c.pixmap.isNull()) {
    // Use cached scaled pixmap
    if (c.cachedPixmapSize != rect.size()) {
      QSize scaledSize =
          c.pixmap.size().scaled(rect.size(), Qt::KeepAspectRatioByExpanding);
      c.cachedPixmap = c.pixmap.scaled(scaledSize, Qt::IgnoreAspectRatio,
                                       Qt::SmoothTransformation);
      c.cachedPixmapSize = rect.size();
    }
    QRect targetRect(rect.center().x() - c.cachedPixmap.width() / 2,
                     rect.center().y() - c.cachedPixmap.height() / 2,
                     c.cachedPixmap.width(), c.cachedPixmap.height());
    painter.drawPixmap(targetRect, c.cachedPixmap);
  } else if (c.bgType == BackgroundType::Color) {
    painter.fillRect(rect, c.bgColor);
  }

  painter.restore();
}

void ProjectionPreview::drawContent(QPainter &painter, int idx,
                                    const QRect &rect, bool drawBg) {
  if (idx < 0 || idx >= (int)layers.size())
    return;
  LayerState *ls = layers[idx];
  Content &c = ls->content;

  if (drawBg) {
    drawBackground(painter, idx, rect);
  }

  // Draw Media
  if (c.mediaType == Content::MediaType::Image ||
      c.mediaType == Content::MediaType::Pdf) {
    if (!c.renderedMedia.isNull()) {
      // Fit, scaled (thumbnails might be small, so we might need to re-render
      // or scale up? Actually, c.renderedMedia might be the FULL RES one from
      // ControlWindow when "Project" is clicked. BUT for PREVIEW, we might want
      // to use the same rendered image. Is c.renderedMedia shared? Yes, passed
      // by const ref.

      QSize imgSize = c.renderedMedia.size();
      QSize scaledSize = imgSize.scaled(rect.size(), Qt::KeepAspectRatio);
      QRect targetRect(rect.center().x() - scaledSize.width() / 2,
                       rect.center().y() - scaledSize.height() / 2,
                       scaledSize.width(), scaledSize.height());

      painter.drawImage(targetRect, c.renderedMedia);
    }
  }

  if (!c.text.isEmpty()) {
    drawText(painter, c, rect, ls->scrollOffset);
  }
}

// Helper: Draw text with shadow and outline for readability (scaled for
// preview)
static void drawStyledTextPreview(QPainter &painter, const QRectF &rect,
                                  const QString &text,
                                  const QTextOption &option,
                                  const TextFormatting &fmt,
                                  float scaleFactor) {
  // Draw text shadow (scaled)
  if (fmt.textShadow) {
    painter.setPen(QColor(0, 0, 0, 180));
    int offset = qMax(1, (int)(2 * scaleFactor));
    QRectF shadowRect = rect.translated(offset, offset);
    painter.drawText(shadowRect, text, option);
  }

  // Draw text outline (scaled)
  if (fmt.outlineWidth > 0) {
    painter.setPen(QColor(0, 0, 0, 200));
    int ow = qMax(1, (int)(fmt.outlineWidth * scaleFactor));
    for (int dx = -ow; dx <= ow; dx += ow) {
      for (int dy = -ow; dy <= ow; dy += ow) {
        if (dx == 0 && dy == 0)
          continue;
        painter.drawText(rect.translated(dx, dy), text, option);
      }
    }
  }

  // Draw main text
  painter.setPen(Qt::white);
  painter.drawText(rect, text, option);
}

void ProjectionPreview::drawText(QPainter &painter,
                                 const Projection::Content &content,
                                 const QRect &rect, float scrollOffset) {
  const auto &fmt = content.formatting;
  const QString &text = content.text;

  // Scale factor
  float scaleFactor = (float)rect.height() / 1080.0f;

  // Apply Margin
  int m = (int)(fmt.margin * scaleFactor);
  if (m < 2)
    m = 2; // Min margin
  QRect textRect = rect.adjusted(m, m, -m, -m);

  QTextOption option;
  option.setAlignment((Qt::Alignment)fmt.alignment | Qt::AlignVCenter);
  option.setWrapMode(QTextOption::WordWrap);

  int fontSize = fmt.fontSize;
  if (fontSize <= 0) {
    if (fmt.isScrolling) {
      int targetLines = 8;
      int targetSize = rect.height() / targetLines;
      if (targetSize < (int)(15 * scaleFactor))
        targetSize = (int)(15 * scaleFactor);
      if (targetSize > (int)(60 * scaleFactor))
        targetSize = (int)(60 * scaleFactor);
      fontSize = targetSize;
    } else {
      int maxScaled = (int)(150 * scaleFactor);
      fontSize = maxScaled;
    }

    // Auto-fit loop
    while (fontSize > 4) {
      QFont font(fmt.fontFamily, fontSize, QFont::Bold);
      painter.setFont(font);
      QRectF boundingRect = painter.boundingRect(textRect, text, option);

      bool fits = false;
      if (fmt.isScrolling) {
        if (boundingRect.width() <= textRect.width())
          fits = true;
      } else {
        if (boundingRect.height() <= textRect.height() &&
            boundingRect.width() <= textRect.width())
          fits = true;
      }
      if (fits)
        break;
      fontSize -= 1;
    }
  } else {
    fontSize = (int)(fontSize * scaleFactor);
    if (fontSize < 4)
      fontSize = 4;
  }

  QFont font(fmt.fontFamily, fontSize, QFont::Bold);
  painter.setFont(font);

  // Scrolled Drawing Logic (Scaled)
  if (fmt.isScrolling) {
    QRectF bRect = painter.boundingRect(
        QRectF(textRect.left(), textRect.top(), textRect.width(), 0), text,
        option);
    qreal textHeight = bRect.height();
    float scaledOffset = scrollOffset * scaleFactor;
    qreal visibleHeight = textRect.height();
    qreal gap = visibleHeight * 0.3;
    qreal totalLoopHeight = textHeight + gap;

    qreal shift = fmod((double)scaledOffset, (double)totalLoopHeight);
    qreal startY = textRect.bottom() - shift;

    painter.save();
    painter.setClipRect(textRect);

    QRectF drawRect1(textRect.left(), startY, textRect.width(), textHeight);
    drawStyledTextPreview(painter, drawRect1, text, option, fmt, scaleFactor);

    qreal nextY = startY + textHeight + gap;
    if (nextY < textRect.bottom()) {
      QRectF drawRect2(textRect.left(), nextY, textRect.width(), textHeight);
      drawStyledTextPreview(painter, drawRect2, text, option, fmt, scaleFactor);
    }

    if (startY > textRect.top()) {
      qreal prevY = startY - totalLoopHeight;
      if (prevY + textHeight > textRect.top()) {
        QRectF drawRectPrev(textRect.left(), prevY, textRect.width(),
                            textHeight);
        drawStyledTextPreview(painter, drawRectPrev, text, option, fmt,
                              scaleFactor);
      }
    }

    painter.restore();

  } else {
    // Static Text
    QRectF boundingRect = painter.boundingRect(textRect, text, option);
    qreal padding = 20 * scaleFactor;
    QRectF bgRect = boundingRect.adjusted(-padding, -padding, padding, padding);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 150));
    painter.drawRoundedRect(bgRect, 5, 5);

    drawStyledTextPreview(painter, QRectF(textRect), text, option, fmt,
                          scaleFactor);
  }
}
