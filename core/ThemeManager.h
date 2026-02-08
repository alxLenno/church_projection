#pragma once
#include <QObject>
#include <QSettings>
#include <QString>
#include <vector>

#include <QColor>

enum class ThemeType { Video, Image, Color };

struct ThemeTemplate {
  QString name;
  ThemeType type;
  QString contentPath; // Path for Video/Image
  QColor color;        // For Color type
};

class ThemeManager : public QObject {
  Q_OBJECT
public:
  explicit ThemeManager(QObject *parent = nullptr)
      : QObject(parent), settings("ChurchProjection", "Themes") {
    loadTemplates();
  }

  const std::vector<ThemeTemplate> &getTemplates() const { return templates; }

  void addTemplate(const QString &name, ThemeType type, const QString &path,
                   const QColor &color = Qt::black) {
    ThemeTemplate t{name, type, path, color};
    templates.push_back(t);
    saveTemplates();
    emit templatesChanged();
  }

  void removeTemplate(int index) {
    if (index >= 0 && index < static_cast<int>(templates.size())) {
      templates.erase(templates.begin() + index);
      saveTemplates();
      emit templatesChanged();
    }
  }

signals:
  void templatesChanged();

private:
  void loadTemplates() {
    templates.clear();
    int size = settings.beginReadArray("templates");
    for (int i = 0; i < size; ++i) {
      settings.setArrayIndex(i);
      ThemeTemplate t;
      t.name = settings.value("name").toString();
      int typeInt = settings.value("type", 0)
                        .toInt(); // Default to 0 (Video) logic if old

      // Backward compatibility: check if "videoPath" exists and "type" is
      // missing
      if (settings.contains("videoPath") && !settings.contains("type")) {
        t.type = ThemeType::Video;
        t.contentPath = settings.value("videoPath").toString();
      } else {
        t.type = static_cast<ThemeType>(typeInt);
        t.contentPath = settings.value("contentPath").toString();
        t.color = settings.value("color", QColor(Qt::black)).value<QColor>();
      }

      templates.push_back(t);
    }
    settings.endArray();
  }

  void saveTemplates() {
    settings.beginWriteArray("templates");
    for (int i = 0; i < static_cast<int>(templates.size()); ++i) {
      settings.setArrayIndex(i);
      settings.setValue("name", templates[i].name);
      settings.setValue("type", static_cast<int>(templates[i].type));
      settings.setValue("contentPath", templates[i].contentPath);
      settings.setValue("color", templates[i].color);
    }
    settings.endArray();
  }

  std::vector<ThemeTemplate> templates;
  QSettings settings;
};
