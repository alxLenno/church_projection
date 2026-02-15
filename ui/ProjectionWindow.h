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
#include <QTimer>
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
  void setLayerFormatting(int layerIdx, const Projection::TextFormatting &fmt);
  Projection::TextFormatting
  getLayerFormatting(int layerIdx) const; // New Accessor
  void setLayerBackground(int layerIdx, Projection::BackgroundType type,
                          const QString &path = "",
                          const QColor &color = Qt::black);
  void setLayoutType(Projection::LayoutType type);
  void clearLayer(int layerIdx);
  void setLayerMedia(int layerIdx, Projection::Content::MediaType type,
                     const QString &path, int page = 0,
                     const QImage &rendered = QImage());

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

    // Scrolling State
    float scrollOffset = 0.0f;
  };

  std::vector<LayerState *> layers;
  Projection::LayoutType currentLayout;
  QTimer *renderTimer; // To drive animation at 60fps

  void drawContent(QPainter &painter, int layerIdx, const QRect &rect,
                   bool drawBg = true);
  void drawBackground(QPainter &painter, int layerIdx, const QRect &rect);
  void drawText(QPainter &painter, const Projection::Content &content,
                const QRect &rect, float scrollOffset = 0.0f);
  void setupLayer(int idx);
};
