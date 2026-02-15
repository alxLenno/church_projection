#pragma once
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QObject>
#include <QSettings>
#include <QStandardPaths>
#include <QString>
#include <vector>

#include <QColor>
#include <algorithm>
#include <random>

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
    loadDefaultThemes();
  }

  const std::vector<ThemeTemplate> &getTemplates() const { return templates; }

  void addTemplate(const QString &name, ThemeType type, const QString &path,
                   const QColor &color = Qt::black) {
    QString finalPath = path;

    // Validate content path for file-based themes
    if (type != ThemeType::Color && !path.isEmpty()) {
      QFileInfo fi(path);
      if (!fi.exists()) {
        qWarning() << "Theme content path does not exist:" << path;
        return;
      }

      // Setup Destination Directory: AppData/themes
      QString dataLocation =
          QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
      QDir dir(dataLocation);
      if (!dir.exists())
        dir.mkpath(".");
      if (!dir.exists("themes"))
        dir.mkpath("themes");

      // Generate unique filename to avoid collisions
      QString timestamp = QString::number(QDateTime::currentMSecsSinceEpoch());
      QString newName = QString("%1_%2").arg(timestamp).arg(fi.fileName());
      // Sanitize spaces/special chars if needed, but Qt handles paths well.

      QString destPath = dir.filePath("themes/" + newName);

      // Copy the file
      if (QFile::copy(path, destPath)) {
        finalPath = destPath;
        qDebug() << "Copied theme file to:" << finalPath;
      } else {
        qWarning() << "Failed to copy theme file to:" << destPath
                   << "- Using original path.";
      }
    }

    ThemeTemplate t{name, type, finalPath, color};
    templates.push_back(t);
    saveTemplates();
    emit templatesChanged();
  }

  void removeTemplate(int index) {
    if (index >= 0 && index < static_cast<int>(templates.size())) {
      const auto &t = templates[index];

      // Attempt to delete the file if it is stored in AppData/themes
      if (!t.contentPath.isEmpty()) {
        QString dataLocation =
            QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        if (t.contentPath.startsWith(dataLocation)) {
          QFile::remove(t.contentPath);
          qDebug() << "Deleted theme file:" << t.contentPath;
        }
      }

      templates.erase(templates.begin() + index);
      saveTemplates();
      emit templatesChanged();
    } else {
      qWarning() << "removeTemplate: index out of range:" << index;
    }
  }

  // Check if a theme's content file still exists on disk
  bool isThemeValid(int index) const {
    if (index < 0 || index >= static_cast<int>(templates.size()))
      return false;
    const auto &t = templates[index];
    if (t.type == ThemeType::Color)
      return true; // Colors are always valid
    return QFileInfo::exists(t.contentPath);
  }

signals:
  void templatesChanged();

private:
  void loadTemplates() {
    templates.clear();
    bool needsSave = false;
    int size = settings.beginReadArray("templates");
    for (int i = 0; i < size; ++i) {
      settings.setArrayIndex(i);
      ThemeTemplate t;
      t.name = settings.value("name").toString();

      // Skip entries with missing name (corrupt data)
      if (t.name.isEmpty()) {
        qWarning() << "Skipping corrupt theme entry at index" << i;
        needsSave = true;
        continue;
      }

      int typeInt = settings.value("type", 0).toInt();

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

      // VALIDATION: Check if file exists
      if (t.type != ThemeType::Color && !t.contentPath.isEmpty()) {
        QFileInfo fi(t.contentPath);
        if (!fi.exists()) {
          qWarning() << "Removing invalid theme:" << t.name
                     << "Path:" << t.contentPath;
          needsSave = true;
          continue;
        }
      }

      templates.push_back(t);
    }
    settings.endArray();

    if (needsSave) {
      saveTemplates();
    }
  }

  void loadDefaultThemes() {
    QString assetPath =
        QString("%1/assets/default_themes").arg(CHURCH_PROJECTION_SOURCE_DIR);
    QDir dir(assetPath);
    if (!dir.exists()) {
      qWarning() << "Default themes directory not found at:" << assetPath;
      return;
    }

    QStringList filters;
    filters << "*.mp4" << "*.mov" << "*.avi" << "*.mkv" << "*.webm";
    QFileInfoList files = dir.entryInfoList(filters, QDir::Files);

    if (files.isEmpty())
      return;

    bool changed = false;
    QStringList names = {
        "Serene Sky",     "Mountain Mist",  "Golden Hour",   "Calm Waters",
        "Starlit Night",  "Forest Whisper", "Deep Ocean",    "Sunset Glow",
        "Breezy Morning", "Autumn Leaves",  "Ethereal Flow", "Liquid Light",
        "Purple Haze",    "Azure Drift",    "Emerald Dream", "Peaceful Path",
        "Divine Light",   "Morning Glory",  "Night Watch",   "Crystal Stream"};

    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(names.begin(), names.end(), g);

    int nameIdx = 0;

    for (const auto &fi : files) {
      QString path = fi.absoluteFilePath();

      // Check if already exists
      bool exists = std::any_of(
          templates.begin(), templates.end(),
          [&](const ThemeTemplate &t) { return t.contentPath == path; });

      if (!exists) {
        QString themeName =
            (nameIdx < names.size()) ? names[nameIdx++] : fi.baseName();
        ThemeTemplate t{themeName, ThemeType::Video, path, Qt::black};
        templates.push_back(t);
        changed = true;
      }
    }

    if (changed) {
      saveTemplates();
      emit templatesChanged();
    }
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
