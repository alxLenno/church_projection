#include "ProjectionWindow.h"
#include <QPainter>
#include <QPainterPath>
#include <QTextOption>

using namespace Projection;

ProjectionWindow::ProjectionWindow(QWidget *parent) : QOpenGLWidget(parent) {
  setWindowFlag(Qt::FramelessWindowHint);
  resize(1920, 1080);

  // Start with black screen to avoid white flash
  setAttribute(Qt::WA_OpaquePaintEvent);

  // Set up layers (2 layers for now)
  setupLayer(0);
  setupLayer(1);

  currentLayout = LayoutType::Single;

  // Start Animation Loop
  renderTimer = new QTimer(this);
  connect(renderTimer, &QTimer::timeout, this, [this]() {
    // Only update if scrolling is active
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
  renderTimer->start(16); // ~60 FPS
}

void ProjectionWindow::setupLayer(int idx) {
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
            onVideoFrameChanged(idx, frame);
          });

  connect(ls->mediaPlayer, &QMediaPlayer::errorOccurred, this,
          [this, idx]() { handleMediaPlayerError(idx); });

  layers.push_back(ls);
}

void ProjectionWindow::setLayerText(int layerIdx, const QString &text) {
  if (layerIdx < 0 || layerIdx >= (int)layers.size())
    return;
  layers[layerIdx]->content.text = text;
  update();
}

void ProjectionWindow::setLayerFormatting(
    int layerIdx, const Projection::TextFormatting &fmt) {
  if (layerIdx < 0 || layerIdx >= (int)layers.size())
    return;
  layers[layerIdx]->content.formatting = fmt;
  update();
}

Projection::TextFormatting
ProjectionWindow::getLayerFormatting(int layerIdx) const {
  if (layerIdx < 0 || layerIdx >= (int)layers.size())
    return Projection::TextFormatting();
  return layers[layerIdx]->content.formatting;
}

void ProjectionWindow::setLayerBackground(int layerIdx, BackgroundType type,
                                          const QString &path,
                                          const QColor &color) {
  if (layerIdx < 0 || layerIdx >= (int)layers.size())
    return;

  LayerState *ls = layers[layerIdx];
  ls->content.bgType = type;
  ls->content.bgPath = path;
  ls->content.bgColor = color;

  // Reset Media when background changes? No, they might be independent layers.
  // But for now, let's assume Media overrides background or vice versa?
  // Actually, Media is "Content", Text is "Content".
  // If we set Media, we probably want to clear Text?
  // The user might want text OVER image.
  // So Media is like a foreground image/slide.

  // Invalidate pixmap cache when background changes
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
      if (ls->content.pixmap.isNull()) {
        qWarning() << "Failed to load background image:" << path;
      }
    }
  }
  update();
}

void ProjectionWindow::setLayerMedia(int layerIdx,
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

  // If setting media, maybe clear text?
  // Usually yes for slides.
  ls->content.text = "";

  update();
}

void ProjectionWindow::setLayoutType(LayoutType type) {
  currentLayout = type;
  update();
}

void ProjectionWindow::clearLayer(int layerIdx) {
  if (layerIdx < 0 || layerIdx >= (int)layers.size())
    return;
  LayerState *ls = layers[layerIdx];

  // Save formatting BEFORE reset
  auto savedFmt = ls->content.formatting;

  ls->mediaPlayer->stop();
  ls->isVideoActive = false;
  ls->content = Content();
  // Ensure media is cleared
  ls->content.mediaType = Content::MediaType::None;
  ls->content.renderedMedia = QImage();

  ls->content.formatting = savedFmt;

  update();
}

// Legacy API Mappings
void ProjectionWindow::setText(const QString &text) { setLayerText(0, text); }

void ProjectionWindow::setBackgroundImage(const QString &path) {
  setLayerBackground(0, BackgroundType::Image, path);
}

void ProjectionWindow::setBackgroundVideo(const QString &path) {
  setLayerBackground(0, BackgroundType::Video, path);
}

void ProjectionWindow::setBackgroundColor(const QColor &color) {
  setLayerBackground(0, BackgroundType::Color, "", color);
}

void ProjectionWindow::clearBackground() {
  setLayerBackground(0, BackgroundType::None);
}

void ProjectionWindow::onVideoFrameChanged(int layerIdx,
                                           const QVideoFrame &frame) {
  if (layerIdx < 0 || layerIdx >= (int)layers.size())
    return;
  if (frame.isValid()) {
    layers[layerIdx]->content.videoFrame = frame;
    update();
  }
}

void ProjectionWindow::paintEvent(QPaintEvent *event) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setRenderHint(QPainter::TextAntialiasing);
  painter.setRenderHint(QPainter::SmoothPixmapTransform);

  // Always fill with black first
  painter.fillRect(rect(), Qt::black);

  if (currentLayout == LayoutType::Single) {
    // Standard Single Layer
    drawContent(painter, 0, rect(), true); // true = draw background

  } else if (currentLayout == LayoutType::SplitVertical) {
    // Vertical Split (Left/Right)
    int mid = width() / 2;
    QRect leftRect(0, 0, mid, height());
    QRect rightRect(mid, 0, width() - mid, height());

    drawContent(painter, 0, leftRect, true);
    drawContent(painter, 1, rightRect, true);

    // Draw Separator Line
    painter.setPen(QPen(QColor(255, 255, 255, 100), 2));
    painter.drawLine(mid, 0, mid, height());

  } else if (currentLayout == LayoutType::SplitHorizontal) {
    // Horizontal Split (Top/Bottom)
    int mid = height() / 2;
    QRect topRect(0, 0, width(), mid);
    QRect bottomRect(0, mid, width(), height() - mid);

    // Draw Content WITH background (distinct per layer)
    drawContent(painter, 0, topRect, true);
    drawContent(painter, 1, bottomRect, true);

    // Horizontal Separator
    painter.setPen(QPen(QColor(255, 255, 255, 100), 2));
    painter.drawLine(0, mid, width(), mid);
  }
}

void ProjectionWindow::drawBackground(QPainter &painter, int idx,
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
    // Use cached scaled pixmap for performance
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

void ProjectionWindow::drawContent(QPainter &painter, int idx,
                                   const QRect &rect, bool drawBg) {
  if (idx < 0 || idx >= (int)layers.size())
    return;
  LayerState *ls = layers[idx];
  Content &c = ls->content;

  // 1. Draw Background (Optional)
  if (drawBg) {
    drawBackground(painter, idx, rect);
  }

  // 1.5 Draw Media (Image/PDF)
  if (c.mediaType == Content::MediaType::Image ||
      c.mediaType == Content::MediaType::Pdf) {
    if (!c.renderedMedia.isNull()) {
      // Fit to screen (contain)
      QSize imgSize = c.renderedMedia.size();
      QSize scaledSize = imgSize.scaled(rect.size(), Qt::KeepAspectRatio);
      QRect targetRect(rect.center().x() - scaledSize.width() / 2,
                       rect.center().y() - scaledSize.height() / 2,
                       scaledSize.width(), scaledSize.height());

      painter.drawImage(targetRect, c.renderedMedia);
    }
  }

  // 2. Draw Text (on top)
  if (!c.text.isEmpty()) {
    drawText(painter, c, rect, ls->scrollOffset);
  }
}

// Helper: Draw text with shadow and outline for readability
static void drawStyledText(QPainter &painter, const QRectF &rect,
                           const QString &text, const QTextOption &option,
                           const TextFormatting &fmt) {
  // Draw text shadow
  if (fmt.textShadow) {
    painter.setPen(QColor(0, 0, 0, 180));
    QRectF shadowRect = rect.translated(2, 2);
    painter.drawText(shadowRect, text, option);
  }

  // Draw text outline
  if (fmt.outlineWidth > 0) {
    painter.setPen(QColor(0, 0, 0, 200));
    int ow = fmt.outlineWidth;
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

void ProjectionWindow::drawText(QPainter &painter, const Content &content,
                                const QRect &rect, float scrollOffset) {
  const auto &fmt = content.formatting;
  const QString &text = content.text;

  // Apply Margin
  int m = fmt.margin;
  QRect textRect = rect.adjusted(m, m, -m, -m);
  if (textRect.width() <= 0 || textRect.height() <= 0)
    return;

  QTextOption option;
  option.setAlignment((Qt::Alignment)fmt.alignment | Qt::AlignVCenter);
  option.setWrapMode(QTextOption::WordWrap);

  int fontSize = fmt.fontSize;

  // Auto-fit logic if fontSize is 0
  if (fontSize <= 0) {
    fontSize = 150; // Cap at 150px absolute max

    if (fmt.isScrolling) {
      int targetLines = 8;
      int targetSize = rect.height() / targetLines;
      if (targetSize < 40)
        targetSize = 40;
      if (targetSize > 120)
        targetSize = 120;
      fontSize = targetSize;
    } else {
      // Standard Auto-Fit Logic (Area Heuristic)
      double rectArea = (double)rect.width() * rect.height();
      if (text.length() > 0) {
        int estSize = (int)sqrt(rectArea / (text.length() * 0.6));
        if (estSize < fontSize)
          fontSize = estSize;
        if (fontSize > 150)
          fontSize = 150;
      }
    }

    if (fontSize < 20)
      fontSize = 20;

    // Iterative shrink
    while (fontSize > 10) {
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

      fontSize -= 2;
    }
  }

  QFont font(fmt.fontFamily, fontSize, QFont::Bold);
  painter.setFont(font);

  // Handle scrolling
  if (fmt.isScrolling) {
    // TELEPROMPTER MODE (Vertical Scroll)
    QRectF bRect = painter.boundingRect(
        QRectF(textRect.left(), textRect.top(), textRect.width(), 0), text,
        option);
    qreal textHeight = bRect.height();
    qreal visibleHeight = textRect.height();
    qreal gap = visibleHeight * 0.3;
    qreal totalLoopHeight = textHeight + gap;

    qreal shift = fmod((double)scrollOffset, (double)totalLoopHeight);
    qreal startY = textRect.bottom() - shift;

    painter.save();
    painter.setClipRect(textRect);

    // Instance 1
    QRectF drawRect1(textRect.left(), startY, textRect.width(), textHeight);
    drawStyledText(painter, drawRect1, text, option, fmt);

    // Instance 2 (Below)
    qreal nextY = startY + textHeight + gap;
    if (nextY < textRect.bottom()) {
      QRectF drawRect2(textRect.left(), nextY, textRect.width(), textHeight);
      drawStyledText(painter, drawRect2, text, option, fmt);
    }

    // Instance 3 (Above/Previous)
    if (startY > textRect.top()) {
      qreal prevY = startY - totalLoopHeight;
      if (prevY + textHeight > textRect.top()) {
        QRectF drawRectPrev(textRect.left(), prevY, textRect.width(),
                            textHeight);
        drawStyledText(painter, drawRectPrev, text, option, fmt);
      }
    }

    painter.restore();

  } else {
    // Standard Draw (Centered/Wrapped)
    QRectF boundingRect = painter.boundingRect(textRect, text, option);

    // Semi-transparent background box for readability
    qreal padding = 20;
    QRectF bgRect = boundingRect.adjusted(-padding, -padding, padding, padding);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 150));
    painter.drawRoundedRect(bgRect, 15, 15);

    drawStyledText(painter, QRectF(textRect), text, option, fmt);
  }
}

void ProjectionWindow::resizeEvent(QResizeEvent *event) {
  // Invalidate all cached pixmaps on resize
  for (auto *ls : layers) {
    ls->content.cachedPixmap = QPixmap();
    ls->content.cachedPixmapSize = QSize();
  }
  QOpenGLWidget::resizeEvent(event);
}

void ProjectionWindow::handleMediaPlayerError(int layerIdx) {
  if (layerIdx < 0 || layerIdx >= (int)layers.size())
    return;
  QString errorMsg = layers[layerIdx]->mediaPlayer->errorString();
  qWarning() << "Media player error on layer" << layerIdx << ":" << errorMsg;
  emit mediaError(QString("Layer %1 Error: %2").arg(layerIdx).arg(errorMsg));
}
