#include "ui/NotesWidget.h"
#include "core/LyricFormatter.h"
#include "core/SongLookup.h"
#include <QApplication>
#include <QEventLoop>
#include <QPushButton>
#include <QDebug>
int main(int argc, char **argv) {
  QApplication app(argc, argv);
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
