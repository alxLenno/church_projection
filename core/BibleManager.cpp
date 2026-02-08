#include "BibleManager.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QXmlStreamReader>

// Forward declaration
QString normalizeBookName(const QString &input);

BibleManager &BibleManager::instance() {
  static BibleManager instance;
  return instance;
}

BibleManager::BibleManager(QObject *parent) : QObject(parent) {}

void BibleManager::loadBibles() {
  // Look for assets relative to the executable (standard deployment)
  // or in the current directory (development/IDE)
  QString appDir = QCoreApplication::applicationDirPath();

  QStringList searchPaths = {
      // 1. "assets" next to executable (Windows/Linux standard, or Mac inside
      // bundle if placed there)
      appDir + "/assets/bible",

      // 2. "../assets" (Dev environment where build dir is sibling to assets)
      appDir + "/../assets/bible",

      // 3. "../../assets" (Shadow build depth 2)
      appDir + "/../../assets/bible",

      // 4. Mac Bundle standard resources location: Contents/Resources/assets
      appDir + "/../Resources/assets/bible",

      // 5. Current working directory fallback
      QDir::currentPath() + "/assets/bible"};

  QString bibleDir;
  for (const auto &path : searchPaths) {
    if (QDir(path).exists()) {
      bibleDir = path;
      break;
    }
  }

  if (bibleDir.isEmpty()) {
    qWarning() << "Bible assets directory not found!";
    return;
  }

  QDir dir(bibleDir);
  QStringList filters;
  filters << "*.xml";
  dir.setNameFilters(filters);

  QStringList files = dir.entryList(QDir::Files);
  for (const QString &file : files) {
    QString versionName = QFileInfo(file).baseName(); // e.g., "NKJV"
    parseXML(dir.absoluteFilePath(file), versionName);
  }

  emit bibleLoaded();
}

void BibleManager::parseXML(const QString &filePath,
                            const QString &versionName) {
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    qWarning() << "Failed to open Bible XML:" << filePath;
    return;
  }

  QXmlStreamReader xml(&file);
  QString currentBook;
  int currentChapter = 0;
  int depth = 0;

  BibleData &data = versions[versionName];

  while (!xml.atEnd() && !xml.hasError()) {
    QXmlStreamReader::TokenType token = xml.readNext();

    if (token == QXmlStreamReader::StartElement) {
      QString name = xml.name().toString();

      if (name == "b") {
        currentBook = normalizeBookName(xml.attributes().value("n").toString());
      } else if (name == "c") {
        currentChapter = xml.attributes().value("n").toInt();
      } else if (name == "v") {
        int verseNum = xml.attributes().value("n").toInt();
        QString text = xml.readElementText();
        data.content[currentBook][currentChapter][verseNum] = text;
      }
    }
  }

  if (xml.hasError()) {
    qWarning() << "XML Parse Error in" << filePath << ":" << xml.errorString();
  } else {
    qDebug() << "Loaded Bible:" << versionName << "with" << data.content.size()
             << "books";
  }
}

// Helper to normalize book names
QString normalizeBookName(const QString &input) {
  static const std::map<QString, QString> bookMap = {
      {"gen", "Genesis"},
      {"genesis", "Genesis"},
      {"exo", "Exodus"},
      {"exodus", "Exodus"},
      {"lev", "Leviticus"},
      {"leviticus", "Leviticus"},
      {"num", "Numbers"},
      {"numbers", "Numbers"},
      {"deu", "Deuteronomy"},
      {"deuteronomy", "Deuteronomy"},
      {"jos", "Joshua"},
      {"joshua", "Joshua"},
      {"jdg", "Judges"},
      {"judges", "Judges"},
      {"rut", "Ruth"},
      {"ruth", "Ruth"},
      {"1sa", "1 Samuel"},
      {"1 samuel", "1 Samuel"},
      {"2sa", "2 Samuel"},
      {"2 samuel", "2 Samuel"},
      {"1ki", "1 Kings"},
      {"1 kings", "1 Kings"},
      {"2ki", "2 Kings"},
      {"2 kings", "2 Kings"},
      {"1ch", "1 Chronicles"},
      {"1 chronicles", "1 Chronicles"},
      {"2ch", "2 Chronicles"},
      {"2 chronicles", "2 Chronicles"},
      {"ezr", "Ezra"},
      {"ezra", "Ezra"},
      {"neh", "Nehemiah"},
      {"nehemiah", "Nehemiah"},
      {"est", "Esther"},
      {"esther", "Esther"},
      {"job", "Job"},
      {"ps", "Psalms"},
      {"psa", "Psalms"},
      {"psalm", "Psalms"},
      {"psalms", "Psalms"},
      {"pro", "Proverbs"},
      {"proverbs", "Proverbs"},
      {"ecc", "Ecclesiastes"},
      {"ecclesiastes", "Ecclesiastes"},
      {"son", "Song of Solomon"},
      {"song", "Song of Solomon"},
      {"isa", "Isaiah"},
      {"isaiah", "Isaiah"},
      {"jer", "Jeremiah"},
      {"jeremiah", "Jeremiah"},
      {"lam", "Lamentations"},
      {"lamentations", "Lamentations"},
      {"eze", "Ezekiel"},
      {"ezekiel", "Ezekiel"},
      {"dan", "Daniel"},
      {"daniel", "Daniel"},
      {"hos", "Hosea"},
      {"hosea", "Hosea"},
      {"joe", "Joel"},
      {"joel", "Joel"},
      {"amo", "Amos"},
      {"amos", "Amos"},
      {"oba", "Obadiah"},
      {"obadiah", "Obadiah"},
      {"jon", "Jonah"},
      {"jonah", "Jonah"},
      {"mic", "Micah"},
      {"micah", "Micah"},
      {"nah", "Nahum"},
      {"nahum", "Nahum"},
      {"hab", "Habakkuk"},
      {"habakkuk", "Habakkuk"},
      {"zep", "Zephaniah"},
      {"zephaniah", "Zephaniah"},
      {"hag", "Haggai"},
      {"haggai", "Haggai"},
      {"zec", "Zechariah"},
      {"zechariah", "Zechariah"},
      {"mal", "Malachi"},
      {"malachi", "Malachi"},
      {"mat", "Matthew"},
      {"matt", "Matthew"},
      {"matthew", "Matthew"},
      {"mar", "Mark"},
      {"mark", "Mark"},
      {"luk", "Luke"},
      {"luke", "Luke"},
      {"joh", "John"},
      {"john", "John"},
      {"act", "Acts"},
      {"acts", "Acts"},
      {"rom", "Romans"},
      {"romans", "Romans"},
      {"1co", "1 Corinthians"},
      {"1 corinthians", "1 Corinthians"},
      {"2co", "2 Corinthians"},
      {"2 corinthians", "2 Corinthians"},
      {"gal", "Galatians"},
      {"galatians", "Galatians"},
      {"eph", "Ephesians"},
      {"ephesians", "Ephesians"},
      {"phi", "Philippians"},
      {"philippians", "Philippians"},
      {"col", "Colossians"},
      {"colossians", "Colossians"},
      {"1th", "1 Thessalonians"},
      {"1 thessalonians", "1 Thessalonians"},
      {"2th", "2 Thessalonians"},
      {"2 thessalonians", "2 Thessalonians"},
      {"1ti", "1 Timothy"},
      {"1 timothy", "1 Timothy"},
      {"2ti", "2 Timothy"},
      {"2 timothy", "2 Timothy"},
      {"tit", "Titus"},
      {"titus", "Titus"},
      {"phm", "Philemon"},
      {"philemon", "Philemon"},
      {"heb", "Hebrews"},
      {"hebrews", "Hebrews"},
      {"jam", "James"},
      {"james", "James"},
      {"1pe", "1 Peter"},
      {"1 peter", "1 Peter"},
      {"2pe", "2 Peter"},
      {"2 peter", "2 Peter"},
      {"1jo", "1 John"},
      {"1 john", "1 John"},
      {"2jo", "2 John"},
      {"2 john", "2 John"},
      {"3jo", "3 John"},
      {"3 john", "3 John"},
      {"jud", "Jude"},
      {"jude", "Jude"},
      {"rev", "Revelation"},
      {"revelation", "Revelation"},
      // Swahili normalization
      {"mwanzo", "Genesis"},
      {"kutoka", "Exodus"},
      {"walawi", "Leviticus"},
      {"hesabu", "Numbers"},
      {"kumbukumbu", "Deuteronomy"},
      {"kumbukumbu la torati", "Deuteronomy"},
      {"yoshua", "Joshua"},
      {"waamuzi", "Judges"},
      {"rutu", "Ruth"},
      {"1 samweli", "1 Samuel"},
      {"2 samweli", "2 Samuel"},
      {"1 wafalme", "1 Kings"},
      {"2 wafalme", "2 Kings"},
      {"1 mambo ya nyakati", "1 Chronicles"},
      {"2 mambo ya nyakati", "2 Chronicles"},
      {"ezra", "Ezra"},
      {"nehemia", "Nehemiah"},
      {"esta", "Esther"},
      {"ayubu", "Job"},
      {"zaburi", "Psalms"},
      {"mithali", "Proverbs"},
      {"mhubiri", "Ecclesiastes"},
      {"wimbo ulio bora", "Song of Solomon"},
      {"isaya", "Isaiah"},
      {"yeremia", "Jeremiah"},
      {"maombolezo", "Lamentations"},
      {"ezekieli", "Ezekiel"},
      {"danieli", "Daniel"},
      {"hotea", "Hosea"},
      {"yoeli", "Joel"},
      {"amosi", "Amos"},
      {"obadia", "Obadiah"},
      {"yona", "Jonah"},
      {"mika", "Micah"},
      {"nahumu", "Nahum"},
      {"habakuki", "Habakkuk"},
      {"sefania", "Zephaniah"},
      {"hagai", "Haggai"},
      {"zakaria", "Zechariah"},
      {"malaki", "Malachi"},
      {"mathayo", "Matthew"},
      {"marko", "Mark"},
      {"luka", "Luke"},
      {"yohana", "John"},
      {"matendo", "Acts"},
      {"matendo ya mitume", "Acts"},
      {"warumi", "Romans"},
      {"1 wakorintho", "1 Corinthians"},
      {"2 wakorintho", "2 Corinthians"},
      {"wagalatia", "Galatians"},
      {"waefeso", "Ephesians"},
      {"wafilipi", "Philippians"},
      {"wakolosai", "Colossians"},
      {"1 wathesalonike", "1 Thessalonians"},
      {"2 wathesalonike", "2 Thessalonians"},
      {"1 timotheo", "1 Timothy"},
      {"2 timotheo", "2 Timothy"},
      {"tito", "Titus"},
      {"filemoni", "Philemon"},
      {"waebrania", "Hebrews"},
      {"yakobo", "James"},
      {"1 petro", "1 Peter"},
      {"2 petro", "2 Peter"},
      {"1 yohana", "1 John"},
      {"2 yohana", "2 John"},
      {"3 yohana", "3 John"},
      {"yuda", "Jude"},
      {"ufunuo", "Revelation"}};

  QString lower = input.toLower().remove('.');
  // Try strict match
  if (bookMap.count(lower))
    return bookMap.at(lower);

  // Try prefix match if len >= 2
  if (lower.length() >= 2) {
    for (const auto &[key, val] : bookMap) {
      if (key.startsWith(lower))
        return val;
    }
  }
  return input; // Return original if no match
}

std::vector<BibleVerse> BibleManager::search(const QString &query,
                                             const QString &version) {
  std::vector<BibleVerse> results;
  if (versions.empty())
    return results;

  // Filter versions to search
  std::vector<QString> versionsToSearch;
  if (!version.isEmpty() && versions.count(version)) {
    versionsToSearch.push_back(version);
  } else {
    for (const auto &[name, data] : versions) {
      versionsToSearch.push_back(name);
    }
  }

  // 1. Try parsing as Reference: "Book Chapter:Verse" or "Book
  // Chapter:Verse-Verse" Flexible Regex: Group 1: Book name, Group 2: Chapter,
  // Group 3: Start Verse, Group 4: End Verse
  QRegularExpression refRegex(
      R"(^([1-3]?\s*[a-zA-Z\x80-\xff\.]+)\s*(\d*)\s*[:\s]?\s*(\d*)?\s*-?\s*(\d*)?$)");
  QRegularExpressionMatch match = refRegex.match(query.trimmed());

  if (match.hasMatch()) {
    QString rawBook = match.captured(1).trimmed();
    QString book = normalizeBookName(rawBook);

    int chapter = match.captured(2).toInt();    // 0 if empty
    int startVerse = match.captured(3).toInt(); // 0 if empty
    int endVerse = match.captured(4).toInt();   // 0 if empty

    for (const QString &verName : versionsToSearch) {
      const auto &data = versions[verName];
      QString targetBook = "";
      for (const auto &[bKey, val] : data.content) {
        if (bKey.compare(book, Qt::CaseInsensitive) == 0) {
          targetBook = bKey;
          break;
        }
      }

      if (!targetBook.isEmpty()) {
        const auto &chapterData = data.content.at(targetBook);
        if (chapter > 0) {
          if (chapterData.count(chapter)) {
            const auto &versesMap = chapterData.at(chapter);
            if (startVerse > 0) {
              int finalEnd = (endVerse > 0) ? endVerse : startVerse;
              for (int v = startVerse; v <= finalEnd; ++v) {
                if (versesMap.count(v)) {
                  results.push_back(
                      {targetBook, chapter, v, versesMap.at(v), verName});
                }
              }
            } else {
              for (const auto &[vNum, txt] : versesMap) {
                results.push_back({targetBook, chapter, vNum, txt, verName});
                if (results.size() >= 50)
                  break;
              }
            }
          }
        } else {
          // No chapter, default to Ch 1
          if (chapterData.count(1)) {
            const auto &versesMap = chapterData.at(1);
            int count = 0;
            for (const auto &[vNum, txt] : versesMap) {
              results.push_back({targetBook, 1, vNum, txt, verName});
              if (++count >= 20 || results.size() >= 50)
                break;
            }
          }
        }
      }
    }
  }

  // 2. Fallback: Keyword search
  // Only if no results found above AND query looks like a keyword (not a failed
  // reference)
  if (results.empty() && query.length() > 3) {
    QString lowerQuery = query.toLower();
    for (const auto &[verName, data] : versions) {
      for (const auto &[book, chapters] : data.content) {
        for (const auto &[chapNum, verses] : chapters) {
          for (const auto &[verseNum, text] : verses) {
            if (text.toLower().contains(lowerQuery)) {
              BibleVerse v{book, chapNum, verseNum, text, verName};
              results.push_back(v);
              if (results.size() >= 50)
                return results;
            }
          }
        }
      }
    }
  }

  return results;
}

QString BibleManager::getVerseText(const QString &book, int chapter, int verse,
                                   const QString &version) {
  if (versions.count(version)) {
    const auto &data = versions.at(version);
    if (data.content.count(book) && data.content.at(book).count(chapter) &&
        data.content.at(book).at(chapter).count(verse)) {
      return data.content.at(book).at(chapter).at(verse);
    }
  }
  return "";
}

QStringList BibleManager::getBooks(const QString &version) {
  if (versions.count(version)) {
    QStringList books;
    for (const auto &[bookName, _] : versions.at(version).content) {
      books.append(bookName);
    }
    // Ideally sort them canonically, but map sorts alphabetically by default
    // key? Wait, std::map sorts by key. Bible books are not alphabetical. The
    // XML parsing order might not be preserved if we use std::map<QString,
    // ...>. We should probably rely on a fixed list of books for order or
    // change structure. For now, return keys.
    return books;
  }
  // Fallback: return keys from first available version if specific one not
  // found?
  if (!versions.empty()) {
    QStringList books;
    for (const auto &[bookName, _] : versions.begin()->second.content) {
      books.append(bookName);
    }
    return books;
  }
  return {};
}

int BibleManager::getChapterCount(const QString &book, const QString &version) {
  if (versions.count(version)) {
    const auto &data = versions.at(version);
    if (data.content.count(book)) {
      return data.content.at(book).size();
    }
  }
  return 0;
}

int BibleManager::getVerseCount(const QString &book, int chapter,
                                const QString &version) {
  if (versions.count(version)) {
    const auto &data = versions.at(version);
    if (data.content.count(book) && data.content.at(book).count(chapter)) {
      return data.content.at(book).at(chapter).size();
    }
  }
  return 0;
}

QString BibleManager::getFirstVersion() const {
  if (versions.empty())
    return "";
  return versions.begin()->first;
}

std::vector<BibleManager::BookInfo> BibleManager::getCanonicalBooks() const {
  static const std::vector<BookInfo> books = {
      // Old Testament
      {"Genesis", Testament::Old, 50},
      {"Exodus", Testament::Old, 40},
      {"Leviticus", Testament::Old, 27},
      {"Numbers", Testament::Old, 36},
      {"Deuteronomy", Testament::Old, 34},
      {"Joshua", Testament::Old, 24},
      {"Judges", Testament::Old, 21},
      {"Ruth", Testament::Old, 4},
      {"1 Samuel", Testament::Old, 31},
      {"2 Samuel", Testament::Old, 24},
      {"1 Kings", Testament::Old, 22},
      {"2 Kings", Testament::Old, 25},
      {"1 Chronicles", Testament::Old, 29},
      {"2 Chronicles", Testament::Old, 36},
      {"Ezra", Testament::Old, 10},
      {"Nehemiah", Testament::Old, 13},
      {"Esther", Testament::Old, 10},
      {"Job", Testament::Old, 42},
      {"Psalms", Testament::Old, 150},
      {"Proverbs", Testament::Old, 31},
      {"Ecclesiastes", Testament::Old, 12},
      {"Song of Solomon", Testament::Old, 8},
      {"Isaiah", Testament::Old, 66},
      {"Jeremiah", Testament::Old, 52},
      {"Lamentations", Testament::Old, 5},
      {"Ezekiel", Testament::Old, 48},
      {"Daniel", Testament::Old, 12},
      {"Hosea", Testament::Old, 14},
      {"Joel", Testament::Old, 3},
      {"Amos", Testament::Old, 9},
      {"Obadiah", Testament::Old, 1},
      {"Jonah", Testament::Old, 4},
      {"Micah", Testament::Old, 7},
      {"Nahum", Testament::Old, 3},
      {"Habakkuk", Testament::Old, 3},
      {"Zephaniah", Testament::Old, 3},
      {"Haggai", Testament::Old, 2},
      {"Zechariah", Testament::Old, 14},
      {"Malachi", Testament::Old, 4},
      // New Testament
      {"Matthew", Testament::New, 28},
      {"Mark", Testament::New, 16},
      {"Luke", Testament::New, 24},
      {"John", Testament::New, 21},
      {"Acts", Testament::New, 28},
      {"Romans", Testament::New, 16},
      {"1 Corinthians", Testament::New, 16},
      {"2 Corinthians", Testament::New, 13},
      {"Galatians", Testament::New, 6},
      {"Ephesians", Testament::New, 6},
      {"Philippians", Testament::New, 4},
      {"Colossians", Testament::New, 4},
      {"1 Thessalonians", Testament::New, 5},
      {"2 Thessalonians", Testament::New, 3},
      {"1 Timothy", Testament::New, 6},
      {"2 Timothy", Testament::New, 4},
      {"Titus", Testament::New, 3},
      {"Philemon", Testament::New, 1},
      {"Hebrews", Testament::New, 13},
      {"James", Testament::New, 5},
      {"1 Peter", Testament::New, 5},
      {"2 Peter", Testament::New, 3},
      {"1 John", Testament::New, 5},
      {"2 John", Testament::New, 1},
      {"3 John", Testament::New, 1},
      {"Jude", Testament::New, 1},
      {"Revelation", Testament::New, 22}};
  return books;
}

QStringList BibleManager::getVersions() const {
  QStringList names;
  for (const auto &[name, _] : versions) {
    names.append(name);
  }
  return names;
}
