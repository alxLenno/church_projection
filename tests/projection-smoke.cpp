#include "ui/ProjectionWindow.h"
#include "core/SplitTextFit.h"
#include "core/ObsTextFit.h"
#include "core/LocalObsFeed.h"
#include "core/BibleMap.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QEventLoop>
#include <QApplication>
#include <QScreen>
#include <QWindow>
#include <QTimer>
#include <QDebug>
#include <QSplitter>
#include "ui/ObsSceneEditor.h"
#include "ui/OutputMapPreview.h"
#include "ui/ScriptureViewSelector.h"
#include <QDir>

int main(int argc, char **argv) {
  QApplication app(argc, argv);
  const QString verse = "That Your beloved may be delivered, Save with Your right hand, and hear me.\n\nPsalms 60:5 (NKJV)";
  for (const QSize area : {QSize(1800, 900), QSize(900, 900)}) {
    const int wrapped = fitSplitText(verse, "Times New Roman", area, 150, false, preserveSplitLines(verse));
    const int unwrapped = fitSplitText(verse, "Times New Roman", area, 150, false, true);
    if (wrapped <= unwrapped * 1.4) qFatal("Verse did not grow into readable wrapped lines");
    QFontMetricsF metrics(QFont("Times New Roman", wrapped, QFont::Bold));
    auto bounds = metrics.boundingRect(QRectF(0, 0, area.width(), 100000), Qt::TextWordWrap, verse);
    if (bounds.height() > area.height() || bounds.width() > area.width()) qFatal("Wrapped verse overflows available space");
  }
  ProjectionWindow window;
  auto *animationTimer=window.findChild<QTimer*>("projectionAnimationTimer");
  if(!animationTimer || animationTimer->interval()!=1000)qFatal("Idle projection is waking at animation frequency");
  {
    Projection::TextFormatting scrolling;
    scrolling.isScrolling=true;
    window.setLayerText(0,"Scrolling stays active for covered OBS output");
    window.setLayerFormatting(0,scrolling);
    if(animationTimer->interval()!=16)qFatal("Scrolling failed to wake the animation timer");
    QEventLoop loop;QTimer::singleShot(40,&loop,&QEventLoop::quit);loop.exec();
    if(animationTimer->interval()!=16)qFatal("Hidden OBS scrolling was throttled");
    scrolling.isScrolling=false;window.setLayerFormatting(0,scrolling);
    QEventLoop idle;QTimer::singleShot(40,&idle,&QEventLoop::quit);idle.exec();
    if(animationTimer->interval()!=1000)qFatal("Static projection failed to return to idle frequency");
  }
  window.showOutput(true, app.primaryScreen());
  app.processEvents();
  if (window.isFullScreen() || !window.isVisible() || !window.windowFlags().testFlag(Qt::FramelessWindowHint)) qFatal("OBS output not a borderless window");
  if (window.windowTitle() != "ChurchProjection - OBS Output") qFatal("OBS capture title missing");
  window.resize(800, 450);
  window.setLayerText(0, "OBS content updates");
  window.repaint();
  if (window.grab().isNull()) qFatal("OBS window did not render");
  int closes = 0;
  QObject::connect(&window, &ProjectionWindow::outputCloseRequested, [&] { ++closes; window.hide(); });
  window.close();
  if (closes != 1 || window.isVisible()) qFatal("OBS close did not stop output");
  window.showOutput(false, app.primaryScreen());
  if (!window.isFullScreen()) qFatal("Full-screen mode not restored");
  window.hide();
  LocalObsFeed feed([&] { return window.obsFrame(); });
  feed.setNormalRenderer([&]{return window.normalFrame();});
  feed.setFormattedRenderer([&]{return window.renderObsScene(window.obsScene);});
  if (!feed.start()) qFatal("Local OBS feed failed to listen");
  QNetworkAccessManager network;
  auto get = [&](const QString &path) {
    auto *reply = network.get(QNetworkRequest(QUrl(feed.url() + path)));
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    loop.exec();
    if (!reply->isFinished() || reply->error() != QNetworkReply::NoError) qFatal("OBS HTTP feed request failed");
    QByteArray data = reply->readAll(); reply->deleteLater(); return data;
  };
  if (!get("").contains("/frame?t=")) qFatal("OBS browser page missing refresh");
  if (QImage::fromData(get("frame")).isNull()) qFatal("OBS frame not decodable");
  window.setLayerText(0,"Scripture wraps inside the OBS text panel without covering the phone");
  window.setLayerText(1,"Second screen selected for OBS");
  for(const QString value : {QString("Long scripture passage ").repeated(25),QString(180,'W'),QString("Swahili line\nEnglish translation\n\nNext pair")}) {
    QTextDocument fitted;
    fitObsText(fitted,value,QFont("Arial",80,QFont::Bold),QSize(320,220),80);
    if(fitted.size().height()>220.5 || fitted.idealWidth()>320.5) qFatal("OBS text escapes resized panel");
    if(fitted.toPlainText()!=value) qFatal("OBS fitter removed content");
  }
  const auto normalBefore=window.obsFrame();
  ObsSceneLayout scene;scene.enabled=true;scene.banner=false;
  if (ObsSceneLayout::fromJson(scene.json()).json()!=scene.json()) qFatal("OBS layout settings did not round trip");
  window.obsScene=scene;
  const auto composed=window.obsFrame();
  if(!get("normal").contains("/normal/frame?t=") || !get("obs/").contains("/obs/frame?t="))qFatal("Independent feed pages target wrong endpoint");
  const auto normalBytes=get("normal/frame"), formattedBytes=get("obs/frame");
  if(QImage::fromData(normalBytes).isNull() || QImage::fromData(formattedBytes).size()!=QSize(1920,1080))qFatal("Independent feed frames missing");
  if(QImage::fromData(normalBytes).pixelColor(10,10)==QImage::fromData(formattedBytes).pixelColor(10,10))qFatal("Normal and formatted feeds shared renderer/cache");
  auto altered=scene;altered.background=QColor("#751111");window.applyObsScene(altered);
  {QEventLoop loop;QTimer::singleShot(70,&loop,&QEventLoop::quit);loop.exec();}
  if(get("normal/frame")!=normalBytes)qFatal("Changing OBS layout changed normal feed");
  if(get("obs/frame")==formattedBytes)qFatal("Formatted feed did not update after layout change");
  auto savedA=scene,savedB=altered;
  const auto endpointA=LocalObsFeed::layoutEndpoint("Worship / ibada");
  const auto endpointB=LocalObsFeed::layoutEndpoint("Scripture");
  if(endpointA==endpointB || endpointA!=LocalObsFeed::layoutEndpoint("Worship / ibada"))qFatal("Saved layout endpoints are not stable and unique");
  feed.setLayoutRenderer("Worship / ibada",[&]{return window.renderObsScene(savedA);});
  feed.setLayoutRenderer("Scripture",[&]{return window.renderObsScene(savedB);});
  if(!get(endpointA).contains(("/"+endpointA+"/frame?t=").toUtf8()))qFatal("Saved layout page targets wrong feed");
  const auto beforeA=get(endpointA+"/frame"),beforeB=get(endpointB+"/frame");
  if(endpointB!="layout/Scripture" || endpointA!="layout/Worship%20%2F%20ibada")qFatal("Layout URLs do not preserve encoded names");
  if(get(LocalObsFeed::legacyLayoutEndpoint("Scripture")+"/frame")!=beforeB)qFatal("Existing hashed layout URL stopped working");
  {
    int renders=0;
    feed.setLayoutRenderer("Cache check",[&]{++renders;QImage image(16,16,QImage::Format_RGB32);image.fill(Qt::black);return image;});
    get(LocalObsFeed::layoutEndpoint("Cache check")+"/frame");
    get(LocalObsFeed::legacyLayoutEndpoint("Cache check")+"/frame");
    if(renders!=1)qFatal("Legacy layout URL duplicated frame rendering");
    feed.setLayoutRenderer("Cache check",[]{QImage image(16,16,QImage::Format_RGB32);image.fill(Qt::red);return image;});
    if(get(LocalObsFeed::layoutEndpoint("Cache check")+"/frame")!=get(LocalObsFeed::legacyLayoutEndpoint("Cache check")+"/frame"))qFatal("Replaced saved renderer left a stale legacy URL");
  }
  if(beforeA==beforeB)qFatal("Saved layouts share frames");
  savedA.background=QColor("#117511");
  {QEventLoop loop;QTimer::singleShot(70,&loop,&QEventLoop::quit);loop.exec();}
  if(get(endpointA+"/frame")==beforeA || get(endpointB+"/frame")!=beforeB)qFatal("Saved layout update affected wrong feed");
  window.setLayerText(0,"New live content for all saved layouts");
  {QEventLoop loop;QTimer::singleShot(70,&loop,&QEventLoop::quit);loop.exec();}
  if(get(endpointB+"/frame")==beforeB)qFatal("Saved layout did not receive live content");
  window.setLayerText(0,"Scripture wraps inside the OBS text panel without covering the phone");
  ObsSceneLayout overlay=scene;overlay.transparent=true;overlay.banner=false;
  overlay.text=QRect(96,760,1728,256);overlay.padding=16;
  if(!ObsSceneLayout::fromJson(overlay.json()).transparent)qFatal("Transparency setting lost");
  const QString captionSample="Yahweh, Rapha, Elohim, Shaddai\nJireh, Adonai will manifest Himself\nYour glory is tangible in this place\nLord, You're doing something great";
  window.setLayerText(0,captionSample);
  if(window.obsTextPages(overlay)!=QStringList{captionSample})qFatal("Default caption hides part of selected section");
  if(window.advanceObsText(overlay,1))qFatal("Default captions unexpectedly page");
  overlay.captionPaging=true;
  const auto captionPages=window.obsTextPages(overlay);
  if(captionPages.size()<2)qFatal("Long captions did not split into readable pages");
  if(captionPages.join(' ').simplified()!=QString(captionSample).replace('\n',' ').simplified())qFatal("Caption pagination lost or reordered words");
  const auto fullNormal=window.normalFrame();
  const auto firstCaption=window.renderObsScene(overlay);
  if(!window.advanceObsText(overlay,1) || window.renderObsScene(overlay)==firstCaption)qFatal("Caption next did not advance");
  if(window.normalFrame()!=fullNormal)qFatal("Caption paging changed normal output");
  if(!window.advanceObsText(overlay,-1) || window.renderObsScene(overlay)!=firstCaption)qFatal("Caption previous did not restore page");
  while(window.advanceObsText(overlay,1)){}
  if(window.advanceObsText(overlay,1))qFatal("Caption advanced past last page");
  window.setLayerText(0,captionSample);
  if(window.obsTextPage(overlay)!=0)qFatal("New content did not reset caption page");
  feed.setRenderer("overlay",[&]{return window.renderObsScene(overlay);});
  if(!get("overlay").contains("background:transparent"))qFatal("Browser page blocks transparency");
  const auto overlayBytes=get("overlay/frame");
  if(!overlayBytes.startsWith(QByteArray::fromHex("89504e47")))qFatal("Transparent feed is not PNG");
  const auto overlayFrame=QImage::fromData(overlayBytes);
  if(!overlayFrame.hasAlphaChannel() || overlayFrame.pixelColor(10,10).alpha()!=0 || overlayFrame.pixelColor(overlay.phone.center()).alpha()!=0)qFatal("Overlay background covers camera");
  const QString overlayArtifacts=qEnvironmentVariable("CP_LAYOUT_TEST_OUTPUT");
  if(!overlayArtifacts.isEmpty()){
    QDir().mkpath(overlayArtifacts);overlayFrame.save(overlayArtifacts+"/transparent-overlay.png");
    QImage composite(1920,1080,QImage::Format_RGB32);composite.fill(QColor("#56738a"));
    QPainter p(&composite);p.drawImage(0,0,overlayFrame);p.end();composite.save(overlayArtifacts+"/overlay-composite.png");
  }
  int backingPixels=0;
  int darkOutlinePixels=0,brightTextPixels=0;
  bool foundText=false;
  for(int y=overlay.text.top();y<=overlay.text.bottom();++y)
    for(int x=overlay.text.left();x<=overlay.text.right();++x)if(overlayFrame.pixelColor(x,y).alpha()>0)foundText=true;
  if(!foundText)qFatal("Transparent overlay has no visible text");
  for(int y=0;y<overlayFrame.height();++y)for(int x=0;x<overlayFrame.width();++x){
    const auto c=overlayFrame.pixelColor(x,y);
    if(c.alpha()>=228 && c.alpha()<=232 && c.lightness()==0)++backingPixels;
    if(c.alpha()>240 && c.lightness()<30)++darkOutlinePixels;
    if(c.alpha()>240 && c.lightness()>220)++brightTextPixels;
  }
  if(backingPixels<100)qFatal("Caption backing missing or not semi-transparent");
  if(darkOutlinePixels<brightTextPixels/5 || brightTextPixels<100)qFatal("Caption outline lacks contrast or hides text fill");
  window.setBlackout(true);
  const auto hiddenOverlay=window.renderObsScene(overlay);
  if(hiddenOverlay.pixelColor(overlay.text.center()).alpha()!=0)qFatal("Cleared overlay hides camera");
  window.setBlackout(false);
  window.setLayerText(0,"Scripture wraps inside the OBS text panel without covering the phone");
  window.applyObsScene(scene);
  if(BibleMap::instance().places.size()<100)qFatal("Bundled coordinate places missing");
  BibleMap::Place jerusalem;bool located=false;
  for(const auto &place:BibleMap::instance().places)if(place.name=="Jerusalem"){jerusalem=place;located=true;break;}
  if(!located || jerusalem.lon<34 || jerusalem.lon>36 || jerusalem.lat<30 || jerusalem.lat>33)qFatal("Jerusalem coordinates invalid");
  if(qAbs(BibleMap::latitude(BibleMap::mercator(31.78))-31.78)>0.00001)qFatal("Map coordinate projection is not reversible");
  window.setMapView(jerusalem.lon,jerusalem.lat,3,jerusalem.id,"Jerusalem");
  window.setMapMode(1);
  const auto mapVerse=window.renderMapOutput(1),mapOnly=window.renderMapOutput(2),verseOnly=window.renderMapOutput(0);
  if(mapVerse.size()!=QSize(1920,1080) || mapVerse==mapOnly || mapOnly==verseOnly)qFatal("Map output modes are not independent");
  const auto mapNormal=window.normalFrame();
  if(mapNormal==normalBefore)qFatal("Map mode did not reach direct projection");
  if(window.obsFrame()!=mapVerse)qFatal("Windowed map output ignores selected mode");
  window.setLayerText(0,"A different verse remains complete beside the map\n\nJohn 3:16 (NKJV)");
  if(window.renderMapOutput(2)!=mapOnly || window.renderMapOutput(1)==mapVerse)qFatal("Verse changed map-only feed or failed to update combined output");
  feed.setRenderer("maps/map-only",[&]{return window.renderMapOutput(2);});
  feed.setRenderer("maps/map-and-verse",[&]{return window.renderMapOutput(1);});
  if(!get("maps/map-only").contains("/maps/map-only/frame?t="))qFatal("Map web screen path invalid");
  if(QImage::fromData(get("maps/map-only/frame")).size()!=QSize(1920,1080) || get("maps/map-only/frame")==get("maps/map-and-verse/frame"))qFatal("Map web feeds missing or identical");
  window.setMapView(jerusalem.lon,jerusalem.lat,1,jerusalem.id,"Jerusalem");
  if(window.renderMapOutput(2)==mapOnly)qFatal("Map zoom did not change output");
  const QString mapArtifacts=qEnvironmentVariable("CP_LAYOUT_TEST_OUTPUT");
  if(!mapArtifacts.isEmpty()){QDir().mkpath(mapArtifacts);mapOnly.save(mapArtifacts+"/map-only.png");mapVerse.save(mapArtifacts+"/map-and-verse.png");}
  {
    const auto crowded=BibleMap::instance().linkedPlaces(22,1,10);
    if(crowded.size()<2)qFatal("Crowded-map fixture has no linked locations");
    const auto bounds=BibleMap::instance().framePlaces(crowded);QList<QRectF> labels;
    const auto crowdedImage=BibleMap::instance().render(QSize(1000,900),bounds.lon,bounds.lat,bounds.span,{},crowded,&labels);
    for(int i=0;i<labels.size();++i)for(int j=i+1;j<labels.size();++j)if(labels[i].intersects(labels[j]))qFatal("Map place names still overlap");
    if(!mapArtifacts.isEmpty())crowdedImage.save(mapArtifacts+"/linked-label-spacing.png");
    ScriptureViewSelector selector;int selections=0,lastMode=-1;
    selector.changed=[&](int mode){++selections;lastMode=mode;};
    const auto buttons=selector.findChildren<QPushButton *>();
    if(buttons.size()!=3)qFatal("Compact view selector does not expose three modes");
    buttons[1]->click();if(selector.mode()!=1 || lastMode!=1 || selections!=1)qFatal("View button did not select Bible + Map");
    selector.setMode(2);if(selector.mode()!=2 || selections!=1)qFatal("Synchronizing view selector prompted another mode change");
    selector.setMode(1);selector.show();app.processEvents();
    if(selector.height()>38)qFatal("View selector unexpectedly expands the original UI");
    if(!mapArtifacts.isEmpty())selector.grab().save(mapArtifacts+"/view-selector.png");selector.hide();
  }
  const auto linked=BibleMap::instance().linkedPlaces(42,2,1);
  window.setLayerText(0,"A wedding took place at Cana in Galilee.\n\nJohn 2:1 (NKJV)");
  window.setMapReference(0,linked,"John 2:1");window.setMapMode(1);
  if(window.currentMapPlaces()!=linked || !window.mapFollowsReference())qFatal("Selected scripture did not link its map");
  const auto previewBefore=window.renderMapOutput(1);
  {
    OutputMapPreview interactive(&window);interactive.resize(640,360);interactive.show();interactive.refresh();app.processEvents();
    const QPointF point=QPointF(window.mapOutputArea().center())/3.0;
    QWheelEvent wheel(point,point,QPoint(),QPoint(0,120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);
    QApplication::sendEvent(&interactive,&wheel);
    if(window.renderMapOutput(1)==previewBefore)qFatal("Scrolling the existing preview did not zoom the map");
    const auto zoomed=window.renderMapOutput(1);
    auto sendMap=[&](QEvent::Type type,QPointF position,Qt::MouseButton button,Qt::MouseButtons buttons){QMouseEvent event(type,position,position,button,buttons,Qt::NoModifier);QApplication::sendEvent(&interactive,&event);};
    sendMap(QEvent::MouseButtonPress,point,Qt::LeftButton,Qt::LeftButton);
    sendMap(QEvent::MouseMove,point+QPointF(18,8),Qt::NoButton,Qt::LeftButton);
    sendMap(QEvent::MouseButtonRelease,point+QPointF(18,8),Qt::LeftButton,Qt::NoButton);
    if(window.renderMapOutput(1)==zoomed)qFatal("Dragging the existing preview did not pan the map");
    window.resetMapView();if(window.renderMapOutput(1)!=previewBefore)qFatal("Reset failed to restore linked map framing");
    window.setMapAbove(true);if(window.renderMapOutput(1)==previewBefore || window.mapOutputArea().width()!=1872)qFatal("Map-above layout not applied");
    window.setLayerText(0,"A new verse without changing the chosen view");window.setMapReference(0,linked,"John 2:1");
    if(window.mapMode()!=1 || !window.isMapAbove())qFatal("Projecting a verse changed the persistent map mode or orientation");
    window.setMapAbove(false);interactive.hide();
  }
  const auto linkedMap=window.renderMapOutput(2);
  window.focusMapPlace(linked.first());
  if(window.renderMapOutput(2)==linkedMap)qFatal("Linked place focus did not change map");
  window.setLayerText(0,"A verse with no mapped locations\n\nPsalms 60:5 (NKJV)");
  window.setMapReference(0,{},"Psalms 60:5");
  if(!window.currentMapPlaces().isEmpty() || window.renderMapOutput(1)!=window.renderMapOutput(0) || window.renderMapOutput(2)==linkedMap)qFatal("Unmapped verse retained stale geography");
  window.setLayerText(1,"Jerusalem\n\nActs 2:5 (NKJV)");window.setMapReference(1,{jerusalem.id},"Acts 2:5");
  window.setMapMode(1,0);if(!window.currentMapPlaces().isEmpty())qFatal("Map locations leaked between source layers");
  window.setMapMode(1,1);if(window.currentMapPlaces()!=QStringList{jerusalem.id})qFatal("Second source lost linked locations");
  window.setMapMode(0,0);
  window.setLayerText(0,"Scripture wraps inside the OBS text panel without covering the phone");
  if (composed.size()!=QSize(1920,1080)) qFatal("OBS scene canvas incorrect");
  if (composed.pixelColor(scene.phone.center())!=scene.panel) qFatal("Content leaked into reserved phone area");
  for(int y=scene.phone.top()+10;y<scene.phone.bottom()-10;y+=30)
    for(int x=scene.phone.left()+10;x<scene.phone.right()-10;x+=30)
      if(composed.pixelColor(x,y)!=scene.panel) qFatal("Phone slot not kept clear");
  scene.source=1;
  if(window.renderObsScene(scene)==composed) qFatal("OBS source selection did not change text");
  {
    ObsSceneEditor editor(&window);editor.show();app.processEvents();
    ObsScenePreview *canvas=nullptr;
    for(auto *widget:editor.findChildren<QWidget *>()) if(auto *candidate=dynamic_cast<ObsScenePreview *>(widget)){canvas=candidate;break;}
    if(!canvas)qFatal("OBS drag canvas missing");
    QSize display(1920,1080);display.scale(canvas->size(),Qt::KeepAspectRatio);
    QPoint offset=canvas->rect().center()-QPoint(display.width()/2,display.height()/2);
    auto point=[&](QPoint p){return QPointF(offset)+QPointF(p.x()*display.width()/1920.0,p.y()*display.height()/1080.0);};
    auto send=[&](QEvent::Type type,QPointF p,Qt::MouseButton button,Qt::MouseButtons buttons){QMouseEvent event(type,p,p,button,buttons,Qt::NoModifier);QApplication::sendEvent(canvas,&event);};
    QRect before=editor.draft.phone;
    auto start=point(before.center());auto end=point(before.center()+QPoint(20,0));
    send(QEvent::MouseButtonPress,start,Qt::LeftButton,Qt::LeftButton);
    send(QEvent::MouseMove,end,Qt::NoButton,Qt::LeftButton);
    send(QEvent::MouseButtonRelease,end,Qt::LeftButton,Qt::NoButton);
    if(qAbs(editor.draft.phone.x()-before.x()-20)>2)qFatal("Dragging did not move phone panel");
    before=editor.draft.text;start=point(before.bottomRight()-QPoint(10,10));end=start+QPointF(-display.width()*80/1920.0,0);
    send(QEvent::MouseButtonPress,start,Qt::LeftButton,Qt::LeftButton);
    send(QEvent::MouseMove,end,Qt::NoButton,Qt::LeftButton);
    send(QEvent::MouseButtonRelease,end,Qt::LeftButton,Qt::NoButton);
    if(qAbs(editor.draft.text.width()-before.width()+80)>2)qFatal("Dragging handle did not resize text panel");
    // A handle must remain clickable on the portion outside its rectangle.
    before=editor.draft.phone;start=point(QPoint(before.right()+1,before.center().y()))+QPointF(4,0);end=start-QPointF(display.width()*50/1920.0,0);
    send(QEvent::MouseButtonPress,start,Qt::LeftButton,Qt::LeftButton);
    send(QEvent::MouseMove,end,Qt::NoButton,Qt::LeftButton);
    send(QEvent::MouseButtonRelease,end,Qt::LeftButton,Qt::NoButton);
    if(qAbs(editor.draft.phone.width()-before.width()+50)>2)qFatal("Outside edge handle did not resize phone panel");
    if(window.obsScene.phone!=scene.phone)qFatal("Draft edit changed live OBS scene");
    editor.hide();
  }
  if (!qEnvironmentVariable("CP_LAYOUT_TEST_OUTPUT").isEmpty()) {
    auto folder=qEnvironmentVariable("CP_LAYOUT_TEST_OUTPUT");QDir().mkpath(folder);
    window.renderObsScene(scene,true).save(folder+"/obs-scene.png");
    ObsSceneEditor editor(&window);editor.show();app.processEvents();
    editor.grab().save(folder+"/obs-editor.png");editor.hide();
  }
  window.showOutput(true,app.primaryScreen());window.applyObsScene(scene);window.repaint();
  auto visibleScene=window.grab().toImage();
  QSize targetSize(1920,1080);targetSize.scale(window.size(),Qt::KeepAspectRatio);
  QPoint origin=window.rect().center()-QPoint(targetSize.width()/2,targetSize.height()/2);
  QPoint phonePoint=origin+QPoint(scene.phone.center().x()*targetSize.width()/1920,scene.phone.center().y()*targetSize.height()/1080);
  phonePoint*=visibleScene.devicePixelRatio();
  if(visibleScene.pixelColor(phonePoint)!=scene.panel) qFatal("Applying OBS scene did not change windowed output");
  window.showOutput(false,app.primaryScreen());window.hide();
  window.obsScene.enabled=false;
  if(window.obsFrame()!=normalBefore) qFatal("OBS scene changed normal projector output");
  QSplitter browserHome;
  auto *browser = new QWidget(&browserHome);
  browserHome.addWidget(browser);
  int returns = 0;
  QObject::connect(&window, &ProjectionWindow::browserReturned, [&] { ++returns; });
  window.setLayoutType(Projection::LayoutType::SplitVertical);
  window.setLayerBrowser(1, browser);
  window.repaint();
  if (browser->parentWidget() != &window) qFatal("Browser not on projection");
  window.setLayerText(0, "Other screen unchanged");
  if (browser->parentWidget() != &window) qFatal("Other screen removed browser");
  window.setLayerText(1, "Replace browser");
  if (browser->parentWidget() != &browserHome || returns != 1) qFatal("Browser not restored on replacement");
  window.setLayerBrowser(0, browser);
  window.releaseBrowser();
  if (browser->parentWidget() != &browserHome || returns != 2) qFatal("Browser return failed");
  window.setLayoutType(Projection::LayoutType::Single);
  window.setLayerText(0, "First screen\nContent remains here");
  window.setLayerText(1, "Second screen\nIndependent content");
  Projection::StageOverlay ticker;
  ticker.enabled = true; ticker.scrolling = true; ticker.text = "Hello NIC CHURCH";
  ticker.screen = 1; ticker.x = 38; ticker.width = 62; // Old saved placement must not narrow the ticker.
  window.setStageOverlay(ticker);
  if (Projection::overlayRect(QRect(0, 0, 1920, 1080), Projection::LayoutType::SplitVertical, ticker).width() != 1920) qFatal("Ticker does not span both screens");
  auto oddStage=QRect(0,0,1921,1081);
  ticker.newsStyle=true;ticker.y=70;ticker.height=12;
  auto content=Projection::stageContentRect(oddStage,oddStage,Projection::LayoutType::Single,ticker);
  auto newsBar=Projection::overlayRect(oddStage,Projection::LayoutType::Single,ticker);
  if (newsBar.top()!=oddStage.height()*88/100 || newsBar.width()!=oddStage.width()) qFatal("Compact news bar not bottom anchored");
  if (content.bottom()>=newsBar.top()) qFatal("News bar overlaps content at fractional pixel sizes");
  ticker.height=3;
  if(Projection::overlayRect(oddStage,Projection::LayoutType::Single,ticker).height()!=oddStage.height()*3/100)qFatal("Small editable banner height ignored");
  ticker.height=8;ticker.title="SUNDAY SERVICE";ticker.label="LIVE";
  auto bannerImage=[&](int fontSize){QImage image(1920,1080,QImage::Format_RGB32);image.fill(Qt::black);QPainter painter(&image);ticker.fontSize=fontSize;Projection::drawStageOverlay(painter,image.rect(),Projection::LayoutType::Single,ticker,0);return image;};
  if(bannerImage(12)==bannerImage(36))qFatal("News banner text size ignored");
  ticker.fontSize=36;ticker.height=12;
  ticker.newsStyle=false;
  window.winId();
  int step = 0;
  QTimer timer;
  QObject::connect(&timer, &QTimer::timeout, [&] {
    if (step == 6) {
      timer.stop();
      ticker.newsStyle=true;ticker.title="SUNDAY SERVICE";ticker.headline="THE POWER OF FAITH";ticker.label="LIVE";
      ticker.y=70;ticker.height=12;
      window.setStageOverlay(ticker);window.repaint();
      const auto before = window.grab().toImage();
      QTimer::singleShot(250, &window, [&, before] {
        const auto after=window.grab().toImage();
        if (before == after) qFatal("Ticker did not animate with static lyrics");
        const auto bar=Projection::overlayRect(window.rect(),Projection::LayoutType::SplitVertical,ticker);
        const auto fixed=QRect(bar.left(),bar.top(),bar.width(),bar.height()*70/100);
        if (before.copy(fixed)!=after.copy(fixed)) qFatal("Fixed news title or headline moved");
        window.hide();
        const QImage hiddenBefore = window.obsFrame();
        QTimer::singleShot(250, &window, [&, hiddenBefore] {
          if (window.obsFrame() == hiddenBefore) qFatal("OBS ticker froze with hidden output");
          if (QImage::fromData(get("frame")).isNull()) qFatal("Hidden OBS HTTP frame missing");
          qInfo() << "PASS: fullscreen, split, reopen, HTTP feed and ticker with hidden output"; app.exit(0);
        });
      });
      return;
    }
    auto screens = app.screens();
    auto screen = screens[step % screens.size()];
    window.hide();
    window.windowHandle()->setScreen(screen);
    window.setGeometry(screen->geometry());
    window.setLayoutType(step % 2 ? Projection::LayoutType::SplitVertical : Projection::LayoutType::Single);
    window.showFullScreen();
    window.repaint();
    if (!window.isVisible() || window.grab().isNull()) app.exit(1);
    ++step;
  });
  timer.start(300);
  QTimer::singleShot(15000, &app, [&] { app.exit(2); });
  return app.exec();
}
