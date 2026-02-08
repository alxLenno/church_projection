#pragma once
#include <QAudioOutput>
#include <QColor>
#include <QImage>
#include <QMediaPlayer>
#include <QOpenGLWidget>
#include <QPainter>
#include <QPixmap>
#include <QResizeEvent>
#include <QString>
#include <QVideoFrame>
#include <QVideoSink>

#include "../core/ProjectionContent.h"
#include <vector>

class ProjectionWindow : public QOpenGLWidget {
  Q_OBJECT
public:
  explicit ProjectionWindow(QWidget *parent = nullptr);

  // New multi-layer API
  void setLayerText(int layerIdx, const QString &text);
  void setLayerBackground(int layerIdx, Projection::BackgroundType type,
                          const QString &path = "",
                          const QColor &color = Qt::black);
  void setLayoutType(Projection::LayoutType type);
  void clearLayer(int layerIdx);

  // Legacy API (mapped to Layer 0)
  void setText(const QString &text);
  void setBackgroundImage(const QString &path);
  void setBackgroundVideo(const QString &path);
  void setBackgroundColor(const QColor &color);
  void clearBackground();

signals:
  void mediaError(const QString &message);

protected:
  void paintEvent(QPaintEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

private slots:
  void handleMediaPlayerError(int layerIdx);
  void onVideoFrameChanged(int layerIdx, const QVideoFrame &frame);

private:
  struct LayerState {
    QMediaPlayer *mediaPlayer = nullptr;
    QAudioOutput *audioOutput = nullptr;
    QVideoSink *videoSink = nullptr;
    Projection::Content content;
    bool isVideoActive = false;
  };

  std::vector<LayerState *> layers;
  Projection::LayoutType currentLayout;

  void drawContent(QPainter &painter, int layerIdx, const QRect &rect);
  void drawText(QPainter &painter, const QString &text, const QRect &rect);
  void setupLayer(int idx);
};
