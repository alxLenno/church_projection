#pragma once
#include <QAudioOutput>
#include <QImage>
#include <QMediaPlayer>
#include <QOpenGLWidget>
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

class ProjectionPreview : public QOpenGLWidget {
  Q_OBJECT
public:
  explicit ProjectionPreview(QWidget *parent = nullptr);

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
  struct LayerState {
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
