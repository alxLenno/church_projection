#pragma once
#include <QAudioOutput>
#include <QImage>
#include <QMediaPlayer>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QResizeEvent>
#include <QString>
#include <QTextOption>
#include <QTimer>
#include <QVideoFrame>
#include <QVideoSink>
#include <QWidget>

#include "../core/ProjectionContent.h"
#include <vector>
#include "../core/StageOverlay.h"
#include <QElapsedTimer>

// Plain QWidget, not QOpenGLWidget: all drawing here is software QPainter
// (see paintEvent) with no paintGL/initializeGL use. A QOpenGLWidget's
// native backing surface composites separately from regular Qt widgets on
// macOS, which — nested inside this panel's QScrollArea — showed stale
// desktop/window content instead of the actual painted preview.
class ProjectionPreview : public QWidget {
  Q_OBJECT
public:
  void setStageOverlay(const Projection::StageOverlay &value) {
    if (!overlayClock.isValid() || value.text != stageOverlay.text || value.scrolling != stageOverlay.scrolling) overlayClock.restart();
    stageOverlay = value; update();
  }
  void setBlackout(bool hidden) { blackout = hidden; update(); }
  void setTextVisible(bool visible) { textVisible = visible; update(); }
  explicit ProjectionPreview(QWidget *parent = nullptr);

  void setActiveScreen(int index) { activeScreen = index; update(); }
  void controlLayerVideo(int layer, int action); // 0 play, 1 pause, 2 stop
  void setLayerText(int layerIdx, const QString &text);
  void setLayerBackground(int layerIdx, Projection::BackgroundType type,
                          const QString &path = "",
                          const QColor &color = Qt::black);
  void setLayoutType(Projection::LayoutType type);
  void clearLayer(int layerIdx);

  void setLayerFormatting(int layerIdx, const Projection::TextFormatting &fmt);
  void setLayerMedia(int layerIdx, Projection::Content::MediaType type,
                     const QString &path, int page = 0,
                     const QImage &rendered = QImage());

  // Legacy API Mappings
  void updateText(const QString &text);
  void setBackgroundImage(const QString &path);
  void setBackgroundVideo(const QString &path);
  void setBackgroundColor(const QColor &color);
  void clear();

protected:
  void resizeEvent(QResizeEvent *event) override;
  void paintEvent(QPaintEvent *event) override;

private:
  Projection::StageOverlay stageOverlay;
  QElapsedTimer overlayClock;
  bool blackout = false;
  bool textVisible = true;
  int activeScreen = 0;
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

    // Scrolling
    float scrollOffset = 0.0f;
  };

  std::vector<LayerState *> layers;
  Projection::LayoutType currentLayout;
  QTimer *renderTimer;

  void drawContent(QPainter &painter, int idx, const QRect &rect,
                   bool drawBg = true);
  void drawBackground(QPainter &painter, int layerIdx, const QRect &rect);
  void drawText(QPainter &painter, const Projection::Content &content,
                const QRect &rect, float scrollOffset = 0.0f);
  void setupLayer(int idx);
};
