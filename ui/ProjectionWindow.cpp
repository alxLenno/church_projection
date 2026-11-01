#include "../core/SplitTextFit.h"
#include "../core/ObsTextFit.h"
#include "../core/BibleMap.h"
#include <QAbstractTextDocumentLayout>
#include "ProjectionWindow.h"
#include <QPainter>
#include <QPainterPath>
#include <QTextOption>
#include <QTextCursor>
#include <QTextCharFormat>
#include <QSplitter>
#include <QWindow>

using namespace Projection;

QStringList ProjectionWindow::obsTextPages(const ObsSceneLayout &scene) const {
  const auto &content=layers[qBound(0,scene.source,1)]->content;
  if(!scene.transparent || !scene.captionPaging)return {content.text};
  QRect area=scene.text.adjusted(scene.padding,scene.padding,-scene.padding,-scene.padding);
  if(scene.banner && stageOverlay.enabled)area.setBottom(qMin(area.bottom(),obsContentBottom()-1));
  return obsCaptionPages(content.text,QFont(content.formatting.fontFamily,scene.outputTextSize(),QFont::Bold),area.size(),scene.outputTextSize());
}
int ProjectionWindow::obsTextPage(const ObsSceneLayout &scene) const {
  return qBound(0,captionPage[qBound(0,scene.source,1)],obsTextPages(scene).size()-1);
}
bool ProjectionWindow::advanceObsText(const ObsSceneLayout &scene,int direction){
  if(!scene.transparent)return false;
  const int current=obsTextPage(scene),next=current+direction;
  if(next<0 || next>=obsTextPages(scene).size())return false;
  captionPage[qBound(0,scene.source,1)]=next;update();return true;
}
void ProjectionWindow::lastObsTextPage(const ObsSceneLayout &scene){
  captionPage[qBound(0,scene.source,1)]=qMax(0,obsTextPages(scene).size()-1);update();
}

QImage ProjectionWindow::renderObsScene(const ObsSceneLayout &scene, bool guides) {
  QImage image(1920,1080,scene.transparent?QImage::Format_ARGB32_Premultiplied:QImage::Format_RGB32);
  image.fill(scene.transparent?QColor(Qt::transparent):scene.background);
  QPainter painter(&image); painter.setRenderHint(QPainter::Antialiasing); painter.setRenderHint(QPainter::TextAntialiasing);
  if (blackout && !guides) return image;
  if (obsBackdropPath != scene.backgroundImage) { obsBackdropPath=scene.backgroundImage;obsBackdrop=QImage(obsBackdropPath); }
  if (!scene.transparent && !obsBackdrop.isNull()) {
    QSize size=obsBackdrop.size();size.scale(image.size(),Qt::KeepAspectRatioByExpanding);
    painter.drawImage(QRect(image.rect().center()-QPoint(size.width()/2,size.height()/2),size),obsBackdrop);
  }
  if(!scene.transparent){
  painter.fillRect(scene.phone,scene.panel);
  painter.fillRect(scene.text,scene.panel);
  painter.setPen(QPen(scene.accent,scene.border));
  if (scene.border) { painter.drawRect(scene.phone); painter.drawRect(scene.text); }
  }
  const auto &content=layers[qBound(0,scene.source,1)]->content;
  QRect textArea=scene.text.adjusted(scene.padding,scene.padding,-scene.padding,-scene.padding);
  if (scene.banner && stageOverlay.enabled) textArea.setBottom(qMin(textArea.bottom(),obsContentBottom()-1));
  if(scene.transparent && !scene.captionPaging && textArea.width()>0){
    // Grow upwards rather than hiding the rest of the selected section.
    QTextDocument measure;
    fitObsText(measure,content.text,QFont(content.formatting.fontFamily,scene.outputTextSize(),QFont::Bold),QSize(textArea.width(),1080),scene.outputTextSize());
    const int wanted=qCeil(measure.size().height())+8;
    const int available=textArea.bottom()-qMin(textArea.top(),432)+1;
    textArea.setTop(textArea.bottom()-qMin(available,qMax(textArea.height(),wanted))+1);
  }
  if (textArea.width()>0 && textArea.height()>0) {
    painter.save();painter.setClipRect(scene.transparent?textArea.adjusted(-14,-8,14,8):textArea);
    if (!scene.transparent && content.mediaType != Content::MediaType::None) {
      QImage media = content.mediaType==Content::MediaType::Video ? layers[scene.source]->contentFrame.toImage() : content.renderedMedia;
      if (!media.isNull()) { QSize size=media.size();size.scale(textArea.size(),Qt::KeepAspectRatio);painter.drawImage(QRect(textArea.center()-QPoint(size.width()/2,size.height()/2),size),media); }
    }
    if (textVisible || guides) {
      QString words=content.text.isEmpty() && guides ? QString("Lyrics / scripture preview\nSelected content appears here") : content.text;
      if(scene.transparent && !content.text.isEmpty())words=obsTextPages(scene).value(obsTextPage(scene));
      QTextDocument document;
      fitObsText(document,words,QFont(content.formatting.fontFamily,scene.outputTextSize(),QFont::Bold),textArea.size(),scene.outputTextSize());
      QAbstractTextDocumentLayout::PaintContext context;
      context.palette.setColor(QPalette::Text,scene.foreground);
      painter.translate(textArea.left(),textArea.top()+qMax(0.0,(textArea.height()-document.size().height())/(scene.transparent?1:2)));
      if(scene.transparent){
        if(scene.captionBacking>0 && !words.trimmed().isEmpty()){
          const QColor backing(0,0,0,qRound(255*scene.captionBacking/100.0));
          painter.fillRect(QRectF(-14,-8,textArea.width()+28,document.size().height()+16),backing);
        }
        // Stroke the glyphs themselves so diagonal edges have the same
        // contrast as vertical edges, including over bright camera footage.
        QTextCursor cursor(&document);cursor.select(QTextCursor::Document);
        QTextCharFormat outline;
        QPen pen(Qt::black,8,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin);
        outline.setTextOutline(pen);cursor.mergeCharFormat(outline);
        context.palette.setColor(QPalette::Text,Qt::black);
        painter.save();painter.translate(4,6);painter.setOpacity(0.8);
        document.documentLayout()->draw(&painter,context);painter.restore();
        context.palette.setColor(QPalette::Text,scene.foreground);
        document.documentLayout()->draw(&painter,context);
        // Paint a clean fill over the inner half of the stroke so the
        // stronger outline does not consume the white letter interiors.
        outline.setTextOutline(QPen(Qt::NoPen));cursor.mergeCharFormat(outline);
      }
      document.documentLayout()->draw(&painter,context);
    }
    painter.restore();
  }
  if (!scene.title.isEmpty()) {
    painter.setPen(scene.foreground);QFont font("Arial");font.setPixelSize(38);font.setBold(true);painter.setFont(font);
    painter.drawText(QRect(64,20,1792,64),Qt::AlignCenter,scene.title);
  }
  if (scene.banner) Projection::drawStageOverlay(painter,image.rect(),LayoutType::Single,stageOverlay,overlayClock.isValid()?overlayClock.elapsed():0);
  if (guides && !scene.transparent) {
    painter.setPen(scene.foreground);QFont font("Arial");font.setPixelSize(30);painter.setFont(font);
    painter.drawText(scene.phone.adjusted(12,12,-12,-12),Qt::AlignCenter|Qt::TextWordWrap,"PHONE / CAMERA\nReserved area — phone remains in OBS");
  }
  return image;
}

void ProjectionWindow::setMapMode(int mode,int source,bool followReference){
  atlasMode=qBound(0,mode,2);atlasSource=qBound(0,source,1);
  if(followReference && linkedMapActive[atlasSource])focusMapPlace({});
  else if(!followReference)mapLinked=false;
  positionBrowser();update();
}
void ProjectionWindow::setMapReference(int source,const QStringList &ids,const QString &reference){
  source=qBound(0,source,1);linkedMapIds[source]=ids;linkedMapReferences[source]=reference;linkedMapActive[source]=true;
  atlasSource=source;focusMapPlace({});positionBrowser();update();
}
void ProjectionWindow::focusMapPlace(const QString &id){
  const auto ids=linkedMapIds[atlasSource];
  const auto frame=BibleMap::instance().framePlaces(id.isEmpty()?ids:QStringList{id});
  mapLon=frame.lon;mapLat=frame.lat;
  const auto area=mapOutputArea(atlasMode==0?1:atlasMode);
  const double aspect=double(area.width())/qMax(1,area.height());
  mapSpan=qMin(180.0,frame.span*qMax(1.0,aspect/1.8));mapRelated=ids;
  mapPlace=id.isEmpty() && ids.size()==1?ids.first():id;
  mapLinked=true;atlasTitle=linkedMapReferences[atlasSource];
  if(const auto *place=BibleMap::instance().find(mapPlace))atlasTitle=place->name+" · "+atlasTitle;
  mapCache.clear();update();
}

QRect ProjectionWindow::mapOutputArea(int mode) const {
  if(mode<0)mode=atlasMode;
  const int bottom=stageOverlay.enabled?obsContentBottom():1080;
  if(mode==1 && mapAbove)return QRect(24,70,1872,qMax(1,qRound((bottom-124)*0.56)));
  return QRect(24,70,mode==1?1000:1872,qMax(1,bottom-124));
}
void ProjectionWindow::zoomMap(double factor){mapSpan=qBound(0.05,mapSpan*factor,180.0);mapCache.clear();update();}
void ProjectionWindow::panMap(double dx,double dy){
  mapLon-=dx*mapSpan;while(mapLon>180)mapLon-=360;while(mapLon< -180)mapLon+=360;
  mapLat=qBound(-75.0,BibleMap::latitude(BibleMap::mercator(mapLat)+dy*mapSpan),75.0);mapCache.clear();update();
}
void ProjectionWindow::resetMapView(){
  if(mapLinked)focusMapPlace(currentMapFocus());
  else {mapSpan=9;mapCache.clear();update();}
}

QImage ProjectionWindow::renderMapOutput(int mode){
  QImage image(1920,1080,QImage::Format_RGB32);image.fill(QColor("#101827"));
  QPainter painter(&image);painter.setRenderHint(QPainter::SmoothPixmapTransform);
  painter.setRenderHint(QPainter::TextAntialiasing);
  if(blackout)return image;
  if(mode==0){painter.scale(1920.0/qMax(1,width()),1080.0/qMax(1,height()));drawNormalOutput(painter,false);return image;}
  if(mapLinked && mapRelated.isEmpty()){
    if(mode==1){painter.end();return renderMapOutput(0);}
    QFont font("Arial");font.setPixelSize(40);painter.setFont(font);painter.setPen(Qt::white);
    painter.drawText(image.rect().adjusted(80,80,-80,-80),Qt::AlignCenter|Qt::TextWordWrap,
      linkedMapReferences[atlasSource].isEmpty()?"No scripture reference selected — project a verse from Bible or Notes":linkedMapReferences[atlasSource]+"\n\nNo location is linked to this verse in the bundled map data");
    return image;
  }
  const int bottom=stageOverlay.enabled?obsContentBottom():1080;
  const QRect mapArea=mapOutputArea(mode);
  const QString key=QString::number(mapArea.width())+"x"+QString::number(mapArea.height());
  if(!mapCache.contains(key))mapCache.insert(key,BibleMap::instance().render(mapArea.size(),mapLon,mapLat,mapSpan,mapPlace,mapRelated));
  painter.drawImage(mapArea,mapCache.value(key));
  painter.setPen(Qt::white);QFont heading("Arial");heading.setPixelSize(32);heading.setBold(true);painter.setFont(heading);
  painter.drawText(QRect(24,12,1872,48),Qt::AlignCenter,atlasTitle.isEmpty()?"Offline Bible map":atlasTitle);
  if(mode==1 && textVisible){
    const auto &content=layers[atlasSource]->content;
    QRect area=mapAbove?QRect(44,mapArea.bottom()+26,1832,qMax(1,bottom-mapArea.bottom()-90)):QRect(1060,90,820,qMax(1,bottom-170));
    QTextDocument doc;fitObsText(doc,content.text,QFont(content.formatting.fontFamily,110,QFont::Bold),area.size(),110);
    QAbstractTextDocumentLayout::PaintContext context;context.palette.setColor(QPalette::Text,Qt::white);
    painter.save();painter.setClipRect(area);painter.translate(area.left(),area.top()+qMax(0.0,(area.height()-doc.size().height())/2));doc.documentLayout()->draw(&painter,context);painter.restore();
  }
  QFont credits("Arial");credits.setPixelSize(18);painter.setFont(credits);painter.setPen(QColor("#cbd5e1"));
  painter.drawText(QRect(24,bottom-42,1872,30),Qt::AlignCenter,"Places: OpenBible.info (CC BY 4.0) · Geography: Natural Earth (public domain) · Locations may be uncertain; geography provides modern context");
  Projection::drawStageOverlay(painter,image.rect(),LayoutType::Single,stageOverlay,overlayClock.isValid()?overlayClock.elapsed():0);
  return image;
}

QImage ProjectionWindow::normalFrame() {
  QImage image(size(),QImage::Format_RGB32);image.fill(Qt::black);
  QPainter painter(&image);drawNormalOutput(painter);return image;
}

QImage ProjectionWindow::obsFrame() {
  return atlasMode>0 ? renderMapOutput(atlasMode) : obsScene.enabled ? renderObsScene(obsScene) : normalFrame();
}

void ProjectionWindow::showOutput(bool windowed, QScreen *screen) {
  if (!screen) return;
  hide();
  obsWindowMode = windowed;
  setWindowFlag(Qt::FramelessWindowHint, true);
  setWindowTitle(windowed ? "ChurchProjection - OBS Output" : "ChurchProjection - Projection Output");
  winId();
  if (!windowHandle()) return;
  windowHandle()->setScreen(screen);
  if (windowed) {
    setWindowState(Qt::WindowNoState);
    const QRect available = screen->availableGeometry();
    QSize size(960, 540);
    size.scale(available.size() * 0.8, Qt::KeepAspectRatio);
    size = size.boundedTo(QSize(1280, 720));
    setGeometry(QRect(available.center() - QPoint(size.width()/2, size.height()/2), size));
    showNormal();
  } else {
    setGeometry(screen->geometry());
    showFullScreen();
  }
  raise();
}

void ProjectionWindow::mousePressEvent(QMouseEvent *event) {
  if (!isFullScreen() && event->button() == Qt::LeftButton && windowHandle()) {
    const QPoint point = event->position().toPoint();
    Qt::Edges edges;
    if (point.x() < 10) edges |= Qt::LeftEdge;
    if (point.x() >= width() - 10) edges |= Qt::RightEdge;
    if (point.y() < 10) edges |= Qt::TopEdge;
    if (point.y() >= height() - 10) edges |= Qt::BottomEdge;
    if (edges) windowHandle()->startSystemResize(edges);
    else windowHandle()->startSystemMove();
    event->accept();
    return;
  }
  QWidget::mousePressEvent(event);
}

void ProjectionWindow::closeEvent(QCloseEvent *event) {
  event->ignore();
  emit outputCloseRequested();
}

void ProjectionWindow::setLayerBrowser(int layer, QWidget *browser) {
  if (!browser || layer < 0 || layer >= int(layers.size())) return;
  if (liveBrowser != browser) {
    releaseBrowser();
    browserHome = browser->parentWidget();
    liveBrowser = browser;
    browser->setParent(this);
  }
  browserLayer = layer;
  layers[layer]->contentPlayer->stop();
  layers[layer]->mediaPlayer->pause();
  browser->setMinimumSize(0, 0);
  positionBrowser();
  update();
}

void ProjectionWindow::releaseBrowser() {
  if (!liveBrowser) return;
  auto *browser = liveBrowser.data();
  browser->hide();
  browser->setParent(browserHome);
  if (auto *splitter = qobject_cast<QSplitter *>(browserHome.data())) splitter->insertWidget(0, browser);
  browser->setMinimumSize(300, 300);
  browser->show();
  liveBrowser.clear(); browserHome.clear();
  emit browserReturned();
  update();
}

void ProjectionWindow::positionBrowser() {
  if (!liveBrowser) return;
  if (atlasMode>0 || (obsWindowMode && obsScene.enabled)) { liveBrowser->hide(); return; }
  QRect area = rect();
  if (currentLayout == LayoutType::SplitVertical) {
    int mid = width()/2;
    area = QRect(browserLayer ? mid : 0, 0, browserLayer ? width()-mid : mid, height());
  } else if (currentLayout == LayoutType::SplitHorizontal) {
    int mid = height()/2;
    area = QRect(0, browserLayer ? mid : 0, width(), browserLayer ? height()-mid : mid);
  }
  liveBrowser->setGeometry(Projection::stageContentRect(area, rect(), currentLayout, stageOverlay));
  liveBrowser->setVisible(!blackout && (currentLayout != LayoutType::Single || browserLayer == 0));
  liveBrowser->raise();
}

ProjectionWindow::ProjectionWindow(QWidget *parent) : QWidget(parent) {
  setWindowFlag(Qt::FramelessWindowHint);
  resize(1920, 1080);
  // Stays hidden until togglePresentation() explicitly geometries and shows
  // it on the target screen — but its native window handle exists from
  // startup (main.cpp forces that), so keep it off the visible desktop in
  // the meantime rather than overlapping the dashboard at the default (0,0).
  move(-10000, -10000);

  // Start with black screen to avoid white flash
  setAttribute(Qt::WA_OpaquePaintEvent);

  // Set up layers (2 layers for now)
  setupLayer(0);
  setupLayer(1);

  currentLayout = LayoutType::Single;

  // Start Animation Loop
  renderTimer = new QTimer(this);
  renderTimer->setObjectName("projectionAnimationTimer");
  connect(renderTimer, &QTimer::timeout, this, [this]() {
    // Only update if scrolling is active
    bool needsUpdate = stageOverlay.enabled && (stageOverlay.newsStyle || (stageOverlay.scrolling && !stageOverlay.text.isEmpty()));
    for (auto len : layers) {
      if (len->content.formatting.isScrolling && !len->content.text.isEmpty()) {
        len->scrollOffset += (float)len->content.formatting.scrollSpeed;
        needsUpdate = true;
      } else {
        len->scrollOffset = 0;
      }
    }
    // Static output needs no 60 Hz wakeups; OBS has its own pull feed.
    renderTimer->setInterval(needsUpdate ? 16 : 1000);
    if (needsUpdate)
      update();
  });
  renderTimer->start(1000); // ~60 FPS
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

  ls->contentPlayer = new QMediaPlayer(this);
  ls->contentAudio = new QAudioOutput(this);
  ls->contentAudio->setMuted(true);
  ls->contentPlayer->setAudioOutput(ls->contentAudio);
  ls->contentSink = new QVideoSink(this);
  ls->contentPlayer->setVideoSink(ls->contentSink);
  connect(ls->contentSink, &QVideoSink::videoFrameChanged, this, [this, ls](const QVideoFrame &frame) {
    ls->contentFrame = frame; update();
  });
  connect(ls->contentPlayer, &QMediaPlayer::errorOccurred, this, [this, ls]() { emit mediaError(ls->contentPlayer->errorString()); });
  layers.push_back(ls);
}

void ProjectionWindow::setLayerText(int layerIdx, const QString &text) {
  renderTimer->start(16);
  if (liveBrowser && browserLayer == layerIdx) releaseBrowser();
  if (layerIdx < 0 || layerIdx >= (int)layers.size())
    return;
  if (!text.isEmpty()) {
    layers[layerIdx]->contentPlayer->stop();
    layers[layerIdx]->contentFrame = QVideoFrame();
    layers[layerIdx]->content.mediaType = Content::MediaType::None;
    layers[layerIdx]->content.renderedMedia = QImage();
  }
  if(linkedMapActive[layerIdx]){
    linkedMapIds[layerIdx].clear();linkedMapReferences[layerIdx].clear();
    if(atlasSource==layerIdx)focusMapPlace({});
  }
  captionPage[layerIdx]=0;
  layers[layerIdx]->content.text = text;
  update();
}

void ProjectionWindow::setLayerFormatting(
    int layerIdx, const Projection::TextFormatting &fmt) {
  renderTimer->start(16);
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
  if (liveBrowser && browserLayer == layerIdx) releaseBrowser();
  if (layerIdx < 0 || layerIdx >= (int)layers.size())
    return;

  LayerState *ls = layers[layerIdx];
  ls->contentPlayer->stop();
  ls->contentFrame = QVideoFrame();
  if (type == Content::MediaType::Video) {
    ls->contentPlayer->setSource(QUrl::fromLocalFile(path));
    ls->contentPlayer->play();
  }
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
  if (liveBrowser && browserLayer == layerIdx) releaseBrowser();
  if (layerIdx < 0 || layerIdx >= (int)layers.size())
    return;
  LayerState *ls = layers[layerIdx];

  ls->contentPlayer->stop();
  ls->contentFrame = QVideoFrame();

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
  positionBrowser();
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setRenderHint(QPainter::TextAntialiasing);
  painter.setRenderHint(QPainter::SmoothPixmapTransform);

  // Always fill with black first
  painter.fillRect(rect(), Qt::black);
  if (blackout) return;
  if (obsWindowMode && obsScene.enabled && atlasMode==0) {
    const QImage scene=renderObsScene(obsScene);
    QSize size=scene.size();size.scale(this->size(),Qt::KeepAspectRatio);
    painter.drawImage(QRect(rect().center()-QPoint(size.width()/2,size.height()/2),size),scene);
    return;
  }

  drawNormalOutput(painter);
}

void ProjectionWindow::drawNormalOutput(QPainter &painter,bool includeAtlas) {
  if(includeAtlas && atlasMode>0){painter.drawImage(rect(),renderMapOutput(atlasMode));return;}
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setRenderHint(QPainter::TextAntialiasing);
  painter.setRenderHint(QPainter::SmoothPixmapTransform);
  painter.fillRect(rect(),Qt::black);
  if(blackout)return;
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
  Projection::drawStageOverlay(painter, this->rect(), currentLayout, stageOverlay, overlayClock.isValid() ? overlayClock.elapsed() : 0);

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
        img.size().scaled(rect.size(), currentLayout == LayoutType::Single ? Qt::KeepAspectRatioByExpanding : Qt::KeepAspectRatio);
    QRect targetRect(rect.center().x() - scaledSize.width() / 2,
                     rect.center().y() - scaledSize.height() / 2,
                     scaledSize.width(), scaledSize.height());
    painter.drawImage(targetRect, img);
  } else if (!c.pixmap.isNull()) {
    // Use cached scaled pixmap for performance
    if (c.cachedPixmapSize != rect.size()) {
      QSize scaledSize =
          c.pixmap.size().scaled(rect.size(), currentLayout == LayoutType::Single ? Qt::KeepAspectRatioByExpanding : Qt::KeepAspectRatio);
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
                                   const QRect &region, bool drawBg) {
  if (idx < 0 || idx >= (int)layers.size())
    return;
  LayerState *ls = layers[idx];
  Content &c = ls->content;
  const QRect rect = Projection::stageContentRect(region, this->rect(), currentLayout, stageOverlay);
  painter.save();
  painter.setClipRect(rect);

  // 1. Draw Background (Optional)
  if (drawBg) {
    drawBackground(painter, idx, region);
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

  if (c.mediaType == Content::MediaType::Video && ls->contentFrame.isValid()) {
    const QImage frame = ls->contentFrame.toImage();
    if (!frame.isNull()) {
      painter.fillRect(rect, Qt::black);
      const QSize size = frame.size().scaled(rect.size(), Qt::KeepAspectRatio);
      painter.drawImage(QRect(QPoint(rect.center().x()-size.width()/2, rect.center().y()-size.height()/2), size), frame);
    }
  }

  // 2. Draw Text (on top)
  if (textVisible && !c.text.isEmpty()) {
    drawText(painter, c, rect, ls->scrollOffset);
  }
  painter.restore();
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
  if (currentLayout != LayoutType::Single) {
    const bool preserveLines = preserveSplitLines(text);
    option.setWrapMode(preserveLines ? QTextOption::NoWrap : QTextOption::WordWrap);
    // Each screen fits its own content; a longer verse on the other
    // screen must not force this screen's shorter content to shrink.
    const int cap = qMax(1, qRound(150 * rect.height() / 1080.0));
    fontSize = fitSplitText(text, fmt.fontFamily, textRect.size(),
                           cap, fmt.isScrolling, false);
  } else {
    // Fit wrapped text against the actual available area, without an
    // estimate that prematurely shrinks long verses into a single line.
    const int cap = qMax(1, qRound(150 * rect.height() / 1080.0));
    const int requested = fmt.fontSize;
    fontSize = fitSplitText(text, fmt.fontFamily, textRect.size(),
                           requested > 0 ? qMin(requested, cap) : cap,
                           fmt.isScrolling, false);
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
  QWidget::resizeEvent(event);
}

void ProjectionWindow::handleMediaPlayerError(int layerIdx) {
  if (layerIdx < 0 || layerIdx >= (int)layers.size())
    return;
  QString errorMsg = layers[layerIdx]->mediaPlayer->errorString();
  qWarning() << "Media player error on layer" << layerIdx << ":" << errorMsg;
  emit mediaError(QString("Layer %1 Error: %2").arg(layerIdx).arg(errorMsg));
}

void ProjectionWindow::controlLayerVideo(int layer, int action) {
  if (layer < 0 || layer >= static_cast<int>(layers.size())) return;
  auto *player = layers[layer]->contentPlayer;
  if (action == 0) player->play();
  else if (action == 1) player->pause();
  else player->stop();
}

void ProjectionWindow::setLayerVideoAudio(int layer, bool muted, float volume) {
  if (layer < 0 || layer >= static_cast<int>(layers.size())) return;
  layers[layer]->contentAudio->setVolume(qBound(0.0f, volume, 1.0f));
  layers[layer]->contentAudio->setMuted(muted);
}

bool ProjectionWindow::layerVideoMuted(int layer) const {
  return layer < 0 || layer >= static_cast<int>(layers.size()) || layers[layer]->contentAudio->isMuted();
}
float ProjectionWindow::layerVideoVolume(int layer) const {
  return layer < 0 || layer >= static_cast<int>(layers.size()) ? 1.0f : layers[layer]->contentAudio->volume();
}
