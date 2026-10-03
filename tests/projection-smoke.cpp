#include "ui/ProjectionWindow.h"
#include <QApplication>
#include <QScreen>
#include <QWindow>
#include <QTimer>
#include <QDebug>

int main(int argc, char **argv) {
  QApplication app(argc, argv);
  ProjectionWindow window;
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
        qInfo() << "PASS: fullscreen, split, reopen and full-width animated ticker"; app.exit(0);
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
