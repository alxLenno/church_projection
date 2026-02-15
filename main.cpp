#include "core/SongManager.h"
#include "core/ThemeManager.h"
#include "ui/ControlWindow.h"
#include "ui/ProjectionWindow.h"
#include <QApplication>
#include <QMessageBox>
#include <QScreen>
#include <QWindow>

class ChurchApp : public QApplication {
public:
  ChurchApp(int &argc, char **argv) : QApplication(argc, argv) {}

  void setControlWindow(ControlWindow *cw) { m_cw = cw; }

protected:
  // On macOS, clicking the dock icon when no windows are open (or all hidden)
  // triggers this.
  bool event(QEvent *event) override {
    if (event->type() == QEvent::ApplicationActivate) {
      if (m_cw && !m_cw->isVisible()) {
        m_cw->show();
        m_cw->raise();
        m_cw->activateWindow();
      }
    }
    return QApplication::event(event);
  }

private:
  ControlWindow *m_cw = nullptr;
};

int main(int argc, char *argv[]) {
  ChurchApp app(argc, argv);

  // App metadata for QSettings consistency
  app.setApplicationName("ChurchProjection");
  app.setOrganizationName("LennoxKK");
  app.setApplicationVersion("1.0.0");

  // Prevent app from quitting when the last "visible" window (the dashboard) is
  // closed. We handle quitting manually via Cmd+Q or Menu.
  app.setQuitOnLastWindowClosed(false);

  // Verify at least one screen is available
  if (QGuiApplication::screens().isEmpty()) {
    QMessageBox::critical(nullptr, "Church Projection",
                          "No display detected. Cannot start.");
    return 1;
  }

  SongManager songManager;
  ThemeManager themeManager;

  // Create the projection window first
  ProjectionWindow pw;
  pw.show();

  // Pass the pointer to the control window and song manager
  ControlWindow cw(&pw, &songManager, &themeManager);
  cw.show();

  app.setControlWindow(&cw);

  return app.exec();
}
