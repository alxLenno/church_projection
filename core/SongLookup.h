#pragma once
#include <QString>
inline QString songMentionQuery(const QString &lineBeforeCursor) {
  const int at = lineBeforeCursor.lastIndexOf('@');
  if (at < 0 || (at > 0 && !lineBeforeCursor[at - 1].isSpace())) return {};
  return lineBeforeCursor.mid(at + 1).trimmed();
}
inline bool hasSongMention(const QString &text) {
  for (const auto &line : text.split('\n')) {
    const int at = line.lastIndexOf('@');
    if (at >= 0 && (at == 0 || line[at - 1].isSpace())) return true;
  }
  return false;
}
