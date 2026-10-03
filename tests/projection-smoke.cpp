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
  window.winId();
  int step = 0;
  QTimer timer;
  QObject::connect(&timer, &QTimer::timeout, [&] {
    if (step == 6) { qInfo() << "PASS: projection fullscreen, split, hide and reopen"; app.exit(0); return; }
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
