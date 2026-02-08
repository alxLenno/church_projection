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
  ls->mediaPlayer->stop();
  ls->isVideoActive = false;
  ls->content = Content();
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
  QOpenGLWidget::resizeEvent(event);
}

void ProjectionPreview::paintEvent(QPaintEvent *event) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setRenderHint(QPainter::TextAntialiasing);
  painter.setRenderHint(QPainter::SmoothPixmapTransform);

  if (currentLayout == LayoutType::Single) {
    drawContent(painter, 0, rect());
  } else if (currentLayout == LayoutType::SplitVertical) {
    int mid = width() / 2;
    drawContent(painter, 0, QRect(0, 0, mid, height()));
    drawContent(painter, 1, QRect(mid, 0, width() - mid, height()));
  } else if (currentLayout == LayoutType::SplitHorizontal) {
    int mid = height() / 2;
    drawContent(painter, 0, QRect(0, 0, width(), mid));
    drawContent(painter, 1, QRect(0, mid, width(), height() - mid));
  }

  // Border
  QPen pen(QColor("#334155"));
  pen.setWidth(2);
  painter.setPen(pen);
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(rect().adjusted(1, 1, -1, -1));
}

void ProjectionPreview::drawContent(QPainter &painter, int idx,
                                    const QRect &rect) {
  if (idx < 0 || idx >= (int)layers.size())
    return;
  LayerState *ls = layers[idx];
  const Content &c = ls->content;

  // Background Fill
  painter.fillRect(rect, Qt::black);

  if (ls->isVideoActive && c.videoFrame.isValid()) {
    QImage img = c.videoFrame.toImage();
    QSize scaledSize = img.size().scaled(rect.size(), Qt::KeepAspectRatio);
    QRect targetRect(rect.center().x() - scaledSize.width() / 2,
                     rect.center().y() - scaledSize.height() / 2,
                     scaledSize.width(), scaledSize.height());
    painter.drawImage(targetRect, img);
  } else if (!c.pixmap.isNull()) {
    QSize scaledSize = c.pixmap.size().scaled(rect.size(), Qt::KeepAspectRatio);
    QRect targetRect(rect.center().x() - scaledSize.width() / 2,
                     rect.center().y() - scaledSize.height() / 2,
                     scaledSize.width(), scaledSize.height());
    painter.drawPixmap(targetRect,
                       c.pixmap.scaled(scaledSize, Qt::IgnoreAspectRatio,
                                       Qt::SmoothTransformation));
  } else if (c.bgType == BackgroundType::Color) {
    painter.fillRect(rect, c.bgColor);
  }

  if (!c.text.isEmpty()) {
    drawText(painter, c.text, rect);
  }
}

void ProjectionPreview::drawText(QPainter &painter, const QString &text,
                                 const QRect &rect) {
  int fontSize = rect.height() / 8;
  if (fontSize < 8)
    fontSize = 8;

  QRect textRect = rect.adjusted(10, 10, -10, -10);
  QTextOption option;
  option.setAlignment(Qt::AlignCenter);
  option.setWrapMode(QTextOption::WordWrap);

  // Dynamic scaling for preview
  while (fontSize > 6) {
    QFont font("Arial", fontSize, QFont::Bold);
    painter.setFont(font);
    QRectF boundingRect = painter.boundingRect(textRect, text, option);
    if (boundingRect.height() <= textRect.height() &&
        boundingRect.width() <= textRect.width()) {
      break;
    }
    fontSize -= 1;
  }

  QFont font("Arial", fontSize, QFont::Bold);
  painter.setFont(font);
  QRectF boundingRect = painter.boundingRect(textRect, text, option);

  qreal padding = 5;
  QRectF bgRect = boundingRect.adjusted(-padding, -padding, padding, padding);

  painter.setPen(Qt::NoPen);
  painter.setBrush(QColor(0, 0, 0, 150));
  painter.drawRoundedRect(bgRect, 5, 5);

  painter.setPen(Qt::white);
  painter.drawText(textRect, text, option);
}
