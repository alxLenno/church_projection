#pragma once
#include <QAudioOutput>
#include <QColor>
#include <QImage>
#include <QMediaPlayer>
#include <QWidget>
#include <QScreen>
#include <QCloseEvent>
#include <QMouseEvent>
#include <QPointer>
#include <QPainter>
#include <QPixmap>
#include <QResizeEvent>
#include <QString>
#include <QTimer>
#include <QVideoFrame>
#include <QVideoSink>

#include "../core/ProjectionContent.h"
#include "../core/ObsSceneLayout.h"
#include <vector>
#include "../core/StageOverlay.h"
#include <QElapsedTimer>

class ProjectionWindow : public QWidget {
  Q_OBJECT
public:
  void setStageOverlay(const Projection::StageOverlay &value) {
    if (!overlayClock.isValid() || value.text != stageOverlay.text || value.scrolling != stageOverlay.scrolling) overlayClock.restart();
    stageOverlay = value; update();
  }
  void setBlackout(bool hidden) { blackout = hidden; update(); }
  void setTextVisible(bool visible) { textVisible = visible; update(); }
  explicit ProjectionWindow(QWidget *parent = nullptr);

  int obsContentBottom() const { return stageOverlay.enabled ? Projection::overlayRect(QRect(0,0,1920,1080),Projection::LayoutType::Single,stageOverlay).top() : 1080; }
  void applyObsScene(const ObsSceneLayout &value) { obsScene=value; positionBrowser(); update(); }
  ObsSceneLayout obsScene;
  QImage renderObsScene(const ObsSceneLayout &scene, bool guides = false);
  QStringList obsTextPages(const ObsSceneLayout &scene) const;
  int obsTextPage(const ObsSceneLayout &scene) const;
  bool advanceObsText(const ObsSceneLayout &scene,int direction);
  void lastObsTextPage(const ObsSceneLayout &scene);
  void setMapMode(int mode,int source=0,bool followReference=true);
  void setMapReference(int source,const QStringList &ids,const QString &reference);
  void focusMapPlace(const QString &id);
  QRect mapOutputArea(int mode=-1) const;
  void zoomMap(double factor);
  void panMap(double dx,double dy);
  void resetMapView();
  void setMapAbove(bool above){mapAbove=above;if(mapLinked)focusMapPlace(mapPlace);else{mapCache.clear();update();}}
  bool isMapAbove() const {return mapAbove;}
  bool hasVisibleMap() const {return atlasMode>0 && (!mapLinked || !mapRelated.isEmpty());}
  QStringList currentMapPlaces() const {return linkedMapIds[atlasSource];}
  QString currentMapFocus() const {return mapPlace;}
  QString currentMapReference() const {return linkedMapReferences[atlasSource];}
  bool mapFollowsReference() const {return mapLinked;}

  void setMapView(double lon,double lat,double span,const QString &place={},const QString &title={}){
    mapLinked=false;mapRelated.clear();
    mapLon=qBound(-180.0,lon,180.0);mapLat=qBound(-75.0,lat,75.0);mapSpan=qBound(0.05,span,180.0);mapPlace=place;atlasTitle=title;mapCache.clear();update();
  }
  int mapMode() const {return atlasMode;}
  QImage renderMapOutput(int mode);
  QImage normalFrame();
  QImage obsFrame();
  void showOutput(bool windowed, QScreen *screen);
  void setLayerBrowser(int layer, QWidget *browser);
  void releaseBrowser();

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
  void browserReturned();
  void outputCloseRequested();

protected:
  void mousePressEvent(QMouseEvent *event) override;
  void closeEvent(QCloseEvent *event) override;
  void paintEvent(QPaintEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

private slots:
  void handleMediaPlayerError(int layerIdx);
  void onVideoFrameChanged(int layerIdx, const QVideoFrame &frame);

private:
  double mapLon=35.23,mapLat=31.78,mapSpan=9;
  QString mapPlace;
  bool mapAbove=false;
  bool mapLinked=false;
  QStringList mapRelated,linkedMapIds[2];
  QString linkedMapReferences[2];
  bool linkedMapActive[2]={false,false};
  QHash<QString,QImage> mapCache;
  QString atlasTitle;
  int atlasMode=0,atlasSource=0;
  bool obsWindowMode = false;
  QImage obsBackdrop;
  QString obsBackdropPath;
  QPointer<QWidget> liveBrowser, browserHome;
  int browserLayer = 0;
  void positionBrowser();
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

  int captionPage[2]={0,0};
  std::vector<LayerState *> layers;
  Projection::LayoutType currentLayout;
  QTimer *renderTimer; // To drive animation at 60fps

  void drawNormalOutput(QPainter &painter,bool includeAtlas=true);
  void drawContent(QPainter &painter, int layerIdx, const QRect &rect,
                   bool drawBg = true);
  void drawBackground(QPainter &painter, int layerIdx, const QRect &rect);
  void drawText(QPainter &painter, const Projection::Content &content,
                const QRect &rect, float scrollOffset = 0.0f);
  void setupLayer(int idx);
};
