#pragma once
#include "Song.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QStandardPaths>
#include <QTextStream>
#include <vector>

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
    if (!file.open(QIODevice::WriteOnly))
      return;

    QJsonArray array;
    for (const auto &song : songs) {
      QJsonObject obj;
      obj["title"] = song.title;
      obj["artist"] = song.artist;
      obj["verses"] = QJsonArray::fromStringList(song.verses);
      array.append(obj);
    }

    QJsonDocument doc(array);
    file.write(doc.toJson());
  }

  void loadSongs() {
    QString path = getStoragePath();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
      return;

    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isArray())
      return;

    songs.clear();
    QJsonArray array = doc.array();
    for (int i = 0; i < array.size(); ++i) {
      QJsonObject obj = array[i].toObject();
      Song s;
      s.title = obj["title"].toString();
      s.artist = obj["artist"].toString();
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
