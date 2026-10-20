#include "ui/NotesWidget.h"
#include "core/LyricFormatter.h"
#include "core/SongLookup.h"
#include "core/Song.h"
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
  NotesWidget notes;
  auto *editor = notes.findChild<QTextEdit *>();
  auto *results = notes.findChild<QListWidget *>();
  auto wait = [] { QEventLoop loop; QTimer::singleShot(250, &loop, &QEventLoop::quit); loop.exec(); };
  for (const auto &version : BibleManager::instance().getVersions()) {
    notes.setCurrentVersion(version);
    editor->setPlainText("@John 3");
    auto cursor = editor->textCursor(); cursor.movePosition(QTextCursor::End); editor->setTextCursor(cursor);
    wait();
    if (!results->count()) qFatal("No Notes results for %s", qPrintable(version));
    for (int i = 0; i < results->count(); ++i) {
      auto ref = results->item(i)->data(Qt::UserRole + 1).toString();
      const auto book = BibleManager::instance().getLocalizedBookName("John", version);
      if (!ref.startsWith(book + " 3:") || !ref.endsWith("(" + version + ")")) qFatal("Wrong Notes reference: %s", qPrintable(ref));
    }
  }
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
