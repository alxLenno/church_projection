#include "ui/NotesWidget.h"
#include "core/LyricFormatter.h"
#include "core/SongLookup.h"
#include "core/Song.h"
#include "core/BibleGeography.h"
#include <QApplication>
#include <QEventLoop>
#include <QPushButton>
#include <QDebug>
int main(int argc, char **argv) {
  QApplication app(argc, argv);
  Song daily;
  daily.title = "Test song"; daily.pinned = true;
  daily.pinnedDate = QDate::currentDate();
  if (!daily.isPinned()) qFatal("Today pin missing");
  daily.pinnedDate = QDate::currentDate().addDays(-1);
  if (daily.isPinned()) qFatal("Yesterday pin still visible");
  daily.title = "Paybill";
  if (!daily.isPinned()) qFatal("Always pinned item expired");
  if (LyricFormatter::keepsPhrases("Nataka nimjue Yesu", "Nataka\nnimjue\nYesu\nMore\nabout\nJesus")) qFatal("Fragmented output accepted");
  if (!LyricFormatter::keepsPhrases("Zaidi, zaidi (More, more)", "Zaidi, zaidi\nMore, more")) qFatal("Translation pair rejected");
  const QString refrain = "Zaidi, zaidi\nNimfahamu Yesu\nNijue pendo lake na\nWokovu wake kamili";
  const QString sections = "Zaidi, zaidi\nNimfahamu Yesu\n\nNijue pendo lake na\nWokovu wake kamili";
  const QString paired = "Zaidi, zaidi\nMore, more\n\nNimfahamu Yesu\nMore about Jesus";
  if (LyricFormatter::shortSections(paired) != paired) qFatal("Translation pairs changed");
  if (LyricFormatter::shortSections(refrain) != sections) qFatal("Two-line sections not formed");
  if (LyricFormatter::shortSections(sections) != sections) qFatal("Existing sections changed");
  if (LyricFormatter::words(refrain) != LyricFormatter::words(sections)) qFatal("Section formatting changed lyrics");
  BibleManager::instance().loadBibles();
  auto johnRef=geographyReference("John",2,1),swahiliRef=geographyReference("Yohana",2,1);
  if(johnRef.book!=42 || swahiliRef.book!=42)qFatal("Geography canonical/localized book indexes differ");
  const auto johnPlaces=BibleMap::instance().linkedPlaces(johnRef.book,2,1);
  if(johnPlaces.size()!=2 || !johnPlaces.contains("a031bda") || !johnPlaces.contains("a9cf1e8"))qFatal("BiAY exact verse-location mapping missing");
  if(BibleMap::instance().linkedPlaces(19,60,5).size()!=0)qFatal("Unmapped verse acquired an unrelated place");
  const auto noteRefs=geographyReferences("Study John 2:1–2 and Mwanzo 12:6. Meet at 10:30.");
  if(noteRefs.size()!=2 || noteRefs[0].book!=42 || noteRefs[0].endVerse!=2 || noteRefs[1].book!=0)qFatal("Notes geography reference extraction failed");
  if(!geographyReferences("We arrived in Jerusalem today without a Bible reference").isEmpty())qFatal("Place-name text was mistaken for verse identity");
  if(geographyReference("John",999,1).book>=0)qFatal("Invalid chapter accepted for map lookup");
  NotesWidget notes;
  int scriptureEvents=0;QString emittedBook;int emittedChapter=0,emittedVerse=0;
  QObject::connect(&notes,&NotesWidget::projectScripture,[&](const QString &,const QString &book,int chapter,int verse){++scriptureEvents;emittedBook=book;emittedChapter=chapter;emittedVerse=verse;});
  auto *editor = notes.findChild<QTextEdit *>();
  auto *results = notes.findChild<QListWidget *>();
  auto wait = [] { QEventLoop loop; QTimer::singleShot(250, &loop, &QEventLoop::quit); loop.exec(); };
  for (const auto &version : BibleManager::instance().getVersions()) {
    notes.setCurrentVersion(version);
    editor->setPlainText("@John 3");
    auto cursor = editor->textCursor(); cursor.movePosition(QTextCursor::End); editor->setTextCursor(cursor);
    wait();
    if (!results->count()) qFatal("No Notes results for %s", qPrintable(version));
    QMetaObject::invokeMethod(&notes,"onResultClicked",Qt::DirectConnection,Q_ARG(QListWidgetItem*,results->item(0)));
    if(emittedBook!="John" || emittedChapter!=3 || emittedVerse!=1)qFatal("Notes projection lost canonical scripture identity in %s",qPrintable(version));
    for (int i = 0; i < results->count(); ++i) {
      auto ref = results->item(i)->data(Qt::UserRole + 1).toString();
      const auto book = BibleManager::instance().getLocalizedBookName("John", version);
      if (!ref.startsWith(book + " 3:") || !ref.endsWith("(" + version + ")")) qFatal("Wrong Notes reference: %s", qPrintable(ref));
    }
  }
  notes.setCurrentVersion("NKJV");editor->setPlainText("@John 2:1");
  {auto end=editor->textCursor();end.movePosition(QTextCursor::End);editor->setTextCursor(end);}wait();
  if(!results->count() || results->item(0)->icon().isNull())qFatal("Location pin missing from linked Notes scripture");
  editor->setPlainText("@Psalms 60:5");
  {auto end=editor->textCursor();end.movePosition(QTextCursor::End);editor->setTextCursor(end);}wait();
  if(!results->count() || !results->item(0)->icon().isNull())qFatal("Unlinked Notes verse incorrectly displays a location pin");
  notes.setCurrentVersion("MSG"); editor->setPlainText("@love");
  auto cursor = editor->textCursor(); cursor.movePosition(QTextCursor::End); editor->setTextCursor(cursor); wait();
  for (int i = 0; i < results->count(); ++i) if (!results->item(i)->data(Qt::UserRole + 1).toString().endsWith("(MSG)")) qFatal("MSG Notes keyword leaked another version");
  if (songMentionQuery("@Amazing Grace") != "Amazing Grace" || songMentionQuery("Find @Amazing Grace") != "Amazing Grace" || !songMentionQuery("mail@example.com").isNull() || !hasSongMention("Lyrics\n@Jesus")) qFatal("Song mention regression");
  const QString pasted = "Zaidi, zaidi ***(More, more)***\\\nNimfahamu Yes&#x75; ***(More about Jesus)***";
  const QString formatted = "Zaidi, zaidi\nMore, more\n\nNimfahamu Yesu\nMore about Jesus";
  if (LyricFormatter::words(pasted) != LyricFormatter::words(formatted)) qFatal("Bilingual cleanup failed word preservation");
  if (LyricFormatter::words(pasted) == LyricFormatter::words(formatted + " invented words")) qFatal("Added lyrics were accepted");
  qInfo() << "PASS: Notes @John 3 in all translations and MSG keyword lookup";
}
