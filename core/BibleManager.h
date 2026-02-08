#pragma once
#include <QObject>
#include <QString>
#include <map>
#include <vector>

struct BibleVerse {
  QString book;
  int chapter;
  int verse;
  QString text;
  QString version;
};

struct BibleBook {
  QString name;
  int chapters;
};

class BibleManager : public QObject {
  Q_OBJECT
public:
  static BibleManager &instance();

  // Loads Bible data from XML files in assets/bible
  void loadBibles();

  // Search for verses by keyword or reference
  // Support queries like "Jesus wept" or "John 3:16"
  // If version is empty, searches all versions
  std::vector<BibleVerse> search(const QString &query,
                                 const QString &version = "");

  // Get list of all loaded version names
  QStringList getVersions() const;

  // Get a specific verse
  QString getVerseText(const QString &book, int chapter, int verse,
                       const QString &version = "NKJV");

  // Get list of books available in a version
  QStringList getBooks(const QString &version = "NKJV");

  // Get number of chapters in a book
  int getChapterCount(const QString &book, const QString &version = "NKJV");

  // Get number of verses in a chapter
  int getVerseCount(const QString &book, int chapter,
                    const QString &version = "NKJV");

  // Get first available version name
  QString getFirstVersion() const;

  enum class Testament { Old, New };
  struct BookInfo {
    QString name;
    Testament testament;
    int chapters;
  };

  // Get books in canonical order with metadata
  std::vector<BookInfo> getCanonicalBooks() const;

signals:
  void bibleLoaded();

private:
  explicit BibleManager(QObject *parent = nullptr);

  struct BibleData {
    // Map: Book Name -> Chapter Num -> Verse Num -> Text
    std::map<QString, std::map<int, std::map<int, QString>>> content;
  };

  std::map<QString, BibleData> versions; // Version Name (e.g., "NKJV") -> Data

  void parseXML(const QString &filePath, const QString &versionName);
};
