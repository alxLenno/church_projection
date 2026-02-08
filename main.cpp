#include "core/SongManager.h"
#include "core/ThemeManager.h"
#include "ui/ControlWindow.h"
#include "ui/ProjectionWindow.h"
#include <QApplication>
#include <QScreen>
#include <QWindow>

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);

  SongManager songManager;
  ThemeManager themeManager;

  // Create the projection window first
  ProjectionWindow pw;
  pw.show();

  // Pass the pointer to the control window and song manager
  ControlWindow cw(&pw, &songManager, &themeManager);
  cw.show();

  return app.exec();
}
