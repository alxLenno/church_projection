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

  // Normalize book name to standard English name (e.g. "Mwanzo" -> "Genesis")
  static QString normalizeBookName(const QString &input);

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

  // Get localized book name (e.g., "Genesis" -> "Mwanzo" for SWAB)
  QString getLocalizedBookName(const QString &book,
                               const QString &version = "NKJV");

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
  // If version is provided, localized names are returned
  std::vector<BookInfo> getCanonicalBooks(const QString &version = "") const;

signals:
  void bibleLoaded();

private:
  explicit BibleManager(QObject *parent = nullptr);

  struct BibleData {
    // Map: Book Name -> Chapter Num -> Verse Num -> Text
    std::map<QString, std::map<int, std::map<int, QString>>> content;
    // Normalized Name -> Localized Name (e.g., "Genesis" -> "Mwanzo")
    std::map<QString, QString> displayNames;
  };

  std::map<QString, BibleData> versions; // Version Name (e.g., "NKJV") -> Data

  void parseXML(const QString &filePath, const QString &versionName);
};
