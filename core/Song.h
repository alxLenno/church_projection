#pragma once
#include <QString>
#include <QStringList>
#include <vector>

struct Song {
  bool pinned = false;
  qint64 lastUsed = 0;
  bool isCommonItem() const {
    return title.contains("paybill", Qt::CaseInsensitive) ||
           title.contains("pay bill", Qt::CaseInsensitive) ||
           title.trimmed().compare("Offering", Qt::CaseInsensitive) == 0 ||
           title.trimmed().compare("offering tithe giving", Qt::CaseInsensitive) == 0;
  }
  bool isPinned() const { return pinned || isCommonItem(); }
  QString title;
  QString artist;
  QStringList verses;
  QString bilingualLyrics;
  QString swahiliLyrics;
  bool swahiliOnly = false;
};
