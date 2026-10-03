#include "core/BibleManager.h"
#include <QCoreApplication>
#include <QDebug>
int main(int argc, char **argv) {
  QCoreApplication app(argc, argv);
  auto &bible = BibleManager::instance(); bible.loadBibles();
  int checks = 0;
  auto verify = [&](bool ok, QString label) { ++checks; if (!ok) qFatal("FAILED: %s", qPrintable(label)); };
  verify(bible.getVersions().size() == 12, "all bundled translations");
  for (const auto &version : bible.getVersions()) {
    verify(bible.getBooks(version).size() == 66, version + " 66 books");
    for (const auto &book : bible.getBooks(version)) {
      auto result = bible.search(book + " 1:1", version);
      verify(result.size() == 1 && result[0].book == book && result[0].chapter == 1 && result[0].verse == 1 && result[0].version == version && !result[0].text.isEmpty(), version + " " + book);
    }
    auto john = bible.search("John 3", version);
    verify(!john.empty(), version + " John 3 available");
    for (const auto &v : john) verify(v.book == "John" && v.chapter == 3 && v.version == version, "John never James");
    verify(bible.search("jonh 3", version).empty(), "misspelled reference never unrelated scripture");
    verify(bible.search("John 999", version).empty(), "invalid chapter");
    auto keyword = bible.search("love", version);
    for (const auto &v : keyword) verify(v.version == version, "keyword translation isolation");
  }
  verify(bible.search("John 3", "MISSING").empty(), "missing translation never substituted");
  qInfo() << "PASS" << checks << "Bible checks";
}
