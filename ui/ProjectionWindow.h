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
#include "../core/StageOverlay.h"
#include <QElapsedTimer>

class ProjectionWindow : public QOpenGLWidget {
  Q_OBJECT
public:
  void setStageOverlay(const Projection::StageOverlay &value) {
    if (!overlayClock.isValid() || value.text != stageOverlay.text || value.scrolling != stageOverlay.scrolling) overlayClock.restart();
    stageOverlay = value; update();
  }
  void setBlackout(bool hidden) { blackout = hidden; update(); }
  void setTextVisible(bool visible) { textVisible = visible; update(); }
  explicit ProjectionWindow(QWidget *parent = nullptr);

  // New multi-layer API
  bool layerVideoMuted(int layer) const;
  float layerVideoVolume(int layer) const;
  void setLayerVideoAudio(int layer, bool muted, float volume);
  void controlLayerVideo(int layer, int action); // 0 play, 1 pause, 2 stop
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
  Projection::StageOverlay stageOverlay;
  QElapsedTimer overlayClock;
  bool blackout = false;
  bool textVisible = true;
  struct LayerState {
    QMediaPlayer *contentPlayer = nullptr;
    QAudioOutput *contentAudio = nullptr;
    QVideoSink *contentSink = nullptr;
    QVideoFrame contentFrame;
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
