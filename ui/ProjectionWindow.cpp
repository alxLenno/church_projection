#include "ProjectionWindow.h"
#include <QPainter>
#include <QPainterPath>
#include <QTextOption>

using namespace Projection;

ProjectionWindow::ProjectionWindow(QWidget *parent) : QOpenGLWidget(parent) {
  setWindowFlag(Qt::FramelessWindowHint);
  resize(1920, 1080);

  currentLayout = LayoutType::Single;
  setupLayer(0);
  setupLayer(1);
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

void ProjectionWindow::setLayerBackground(int layerIdx, BackgroundType type,
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

void ProjectionWindow::setLayoutType(LayoutType type) {
  currentLayout = type;
  update();
}

void ProjectionWindow::clearLayer(int layerIdx) {
  if (layerIdx < 0 || layerIdx >= (int)layers.size())
    return;
  LayerState *ls = layers[layerIdx];
  ls->mediaPlayer->stop();
  ls->isVideoActive = false;
  ls->content = Content();
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
}

void ProjectionWindow::drawContent(QPainter &painter, int idx,
                                   const QRect &rect) {
  if (idx < 0 || idx >= (int)layers.size())
    return;
  LayerState *ls = layers[idx];
  const Content &c = ls->content;

  // Always fill background with black first to handle letterboxing
  painter.fillRect(rect, Qt::black);

  // 1. Draw Background
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

  // 2. Draw Text (on top)
  if (!c.text.isEmpty()) {
    drawText(painter, c.text, rect);
  }
}

void ProjectionWindow::drawText(QPainter &painter, const QString &text,
                                const QRect &rect) {
  // Start with a reasonable font size
  int fontSize = rect.height() / 10;
  if (fontSize < 12)
    fontSize = 12;

  QRect textRect = rect.adjusted(40, 40, -40, -40);
  QTextOption option;
  option.setAlignment(Qt::AlignCenter);
  option.setWrapMode(QTextOption::WordWrap);

  // Dynamic font scaling to ensure it fits
  while (fontSize > 8) {
    QFont font("Arial", fontSize, QFont::Bold);
    painter.setFont(font);
    QRectF boundingRect = painter.boundingRect(textRect, text, option);
    if (boundingRect.height() <= textRect.height() &&
        boundingRect.width() <= textRect.width()) {
      break;
    }
    fontSize -= 2;
  }

  QFont font("Arial", fontSize, QFont::Bold);
  painter.setFont(font);
  QRectF boundingRect = painter.boundingRect(textRect, text, option);

  qreal padding = 20;
  QRectF bgRect = boundingRect.adjusted(-padding, -padding, padding, padding);

  painter.setPen(Qt::NoPen);
  painter.setBrush(QColor(0, 0, 0, 150));
  painter.drawRoundedRect(bgRect, 15, 15);

  painter.setPen(Qt::white);
  painter.drawText(textRect, text, option);
}

void ProjectionWindow::resizeEvent(QResizeEvent *event) {
  QOpenGLWidget::resizeEvent(event);
}

void ProjectionWindow::handleMediaPlayerError(int layerIdx) {
  if (layerIdx < 0 || layerIdx >= (int)layers.size())
    return;
  emit mediaError(QString("Layer %1 Error: %2")
                      .arg(layerIdx)
                      .arg(layers[layerIdx]->mediaPlayer->errorString()));
}
