#include "ui/ProjectionWindow.h"
#include "core/SplitTextFit.h"
#include "core/LocalObsFeed.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QEventLoop>
#include <QApplication>
#include <QScreen>
#include <QWindow>
#include <QTimer>
#include <QDebug>
#include <QSplitter>

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
