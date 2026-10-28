#pragma once
#include "BibleManager.h"
#include "BibleMap.h"
#include <QRegularExpression>

struct BibleGeographyReference {
  int book=-1,chapter=0,verse=0,endVerse=0;
  QString label;
};
// Resolve references, not words in translated scripture, against BiAY's
// canonical zero-based book indexes. Structured Bible/Notes metadata wins.
inline BibleGeographyReference geographyReference(const QString &name,int chapter,int verse,int endVerse=0){
  BibleGeographyReference ref;const QString normalized=BibleManager::normalizeBookName(name);
  const auto books=BibleManager::instance().getCanonicalBooks();
  for(int i=0;i<int(books.size());++i)if(books[i].name.compare(normalized,Qt::CaseInsensitive)==0){
    if(chapter<1 || chapter>books[i].chapters || verse<1 || verse>200)return ref;
    ref={i,chapter,verse,qBound(verse,endVerse>0?endVerse:verse,200),QString("%1 %2:%3").arg(books[i].name).arg(chapter).arg(verse)};
    if(ref.endVerse>verse)ref.label+=QString("–%1").arg(ref.endVerse);break;
  }return ref;
}
inline QList<BibleGeographyReference> geographyReferences(const QString &text){
  QList<BibleGeographyReference> refs;
  static const QRegularExpression expression(QString::fromUtf8("([1-3]?[ \\t]*[\\p{L}]+(?:[ \\t]+[\\p{L}]+){0,3})[ \\t]+(\\d{1,3})[ \\t]*:[ \\t]*(\\d{1,3})(?:[ \\t]*[-–][ \\t]*(\\d{1,3}))?"));
  auto matches=expression.globalMatch(text);
  while(matches.hasNext()){
    auto match=matches.next();auto words=match.captured(1).simplified().split(' ');
    for(int start=0;start<words.size();++start){
      const auto ref=geographyReference(words.mid(start).join(' '),match.captured(2).toInt(),match.captured(3).toInt(),match.captured(4).toInt());
      if(ref.book>=0){refs.append(ref);break;}
    }
  }return refs;
}
