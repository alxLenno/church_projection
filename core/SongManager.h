#pragma once
#include "Song.h"
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QStandardPaths>
#include <QTextStream>
#include <vector>
#include <algorithm>

class SongManager : public QObject {
  Q_OBJECT
public:
  explicit SongManager(QObject *parent = nullptr) : QObject(parent) {
    loadSongs();
    if (songs.empty()) {
      // Initial sample song if none loaded
      Song sample;
      sample.title = "Amazing Grace";
      sample.artist = "John Newton";
      sample.verses
          << "Amazing grace! How sweet the sound\nThat saved a wretch like me!"
          << "Twas grace that taught my heart to fear,\nAnd grace my fears "
             "relieved;"
          << "Through many dangers, toils and snares,\nI have already come;";
      songs.push_back(sample);
      saveSongs();
    }
  }

  void saveSongs() {
    QString path = getStoragePath();
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
      qWarning() << "Failed to save songs to:" << path << file.errorString();
      return;
    }

    QJsonArray array;
    for (const auto &song : songs) {
      QJsonObject obj;
      obj["title"] = song.title;
      obj["artist"] = song.artist;
      obj["pinned"] = song.isPinned();
      obj["lastUsed"] = QString::number(song.lastUsed);
      obj["bilingualLyrics"] = song.bilingualLyrics;
      obj["swahiliLyrics"] = song.swahiliLyrics;
      obj["swahiliOnly"] = song.swahiliOnly;
      obj["verses"] = QJsonArray::fromStringList(song.verses);
      array.append(obj);
    }

    QJsonDocument doc(array);
    file.write(doc.toJson());
  }

  void loadSongs() {
    QString path = getStoragePath();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
      // File doesn't exist yet — not an error on first run
      return;
    }

    QByteArray data = file.readAll();
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
      qWarning() << "Song file JSON parse error:" << parseError.errorString();
      return;
    }
    if (!doc.isArray()) {
      qWarning() << "Song file is not a valid JSON array";
      return;
    }

    songs.clear();
    QJsonArray array = doc.array();
    for (int i = 0; i < array.size(); ++i) {
      QJsonObject obj = array[i].toObject();
      Song s;
      s.title = obj["title"].toString();
      s.artist = obj["artist"].toString();
      s.bilingualLyrics = obj["bilingualLyrics"].toString();
      s.swahiliLyrics = obj["swahiliLyrics"].toString();
      s.swahiliOnly = obj["swahiliOnly"].toBool();
      s.pinned = obj["pinned"].toBool();
      s.lastUsed = obj["lastUsed"].toString().toLongLong();
      QJsonArray verseArray = obj["verses"].toArray();
      for (int v = 0; v < verseArray.size(); ++v) {
        s.verses << verseArray[v].toString();
      }
      songs.push_back(s);
    }
  }

  QString getStoragePath() {
    QString dir =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + "/songs.json";
  }

  const std::vector<Song> &getSongs() const { return songs; }
  void removeFromRecents(int index) {
    if (index < 0 || index >= static_cast<int>(songs.size())) return;
    songs[index].lastUsed = 0;
    saveSongs();
  }
  void markUsed(int index) {
    if (index < 0 || index >= static_cast<int>(songs.size()) || songs[index].isCommonItem()) return;
    qint64 latest = 0;
    for (const auto &song : songs) latest = std::max(latest, song.lastUsed);
    songs[index].lastUsed = latest + 1;
    saveSongs();
  }
  std::vector<int> recentSongIndices() const {
    std::vector<int> indices;
    for (int i = 0; i < static_cast<int>(songs.size()); ++i)
      if (songs[i].lastUsed > 0 && !songs[i].isCommonItem()) indices.push_back(i);
    std::stable_sort(indices.begin(), indices.end(), [this](int a, int b) {
      return songs[a].lastUsed > songs[b].lastUsed;
    });
    if (indices.size() > 5) indices.resize(5);
    return indices;
  }
  void addSong(const Song &song) {
    songs.push_back(song);
    saveSongs();
  }
  void updateSong(int index, const Song &song) {
    if (index >= 0 && index < static_cast<int>(songs.size())) {
      songs[index] = song;
      saveSongs();
    }
  }
  void removeSong(int index) {
    if (index >= 0 && index < static_cast<int>(songs.size())) {
      songs.erase(songs.begin() + index);
      saveSongs();
    }
  }

  bool importFromFile(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
      return false;

    QTextStream in(&file);
    Song newSong;
    if (!in.atEnd()) {
      newSong.title = in.readLine().trimmed();
    }

    QString currentVerse;
    while (!in.atEnd()) {
      QString line = in.readLine();
      if (line.trimmed().isEmpty() && !currentVerse.isEmpty()) {
        newSong.verses << currentVerse.trimmed();
        currentVerse.clear();
      } else {
        currentVerse += line + "\n";
      }
    }
    if (!currentVerse.isEmpty()) {
      newSong.verses << currentVerse.trimmed();
    }

    songs.push_back(newSong);
    return true;
  }

private:
  std::vector<Song> songs;
};
