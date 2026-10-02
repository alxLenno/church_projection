#pragma once
#include "../core/PdfRenderer.h"
#include "../core/SongManager.h"
#include "../core/ThemeManager.h"
#include "NotesWidget.h"
#include "ProjectionPreview.h"
#include "ProjectionWindow.h"
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QFileDialog>
#include <QFontComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QImageReader>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QShortcut>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWebEngineView>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QStringListModel>
#include <QCompleter>
#include <QTimer>

class VerseWidget : public QWidget {
  Q_OBJECT
public:
  VerseWidget(const QString &book, int chapter, int verse, const QString &text,
              const QString &version, QWidget *parent = nullptr);

signals:
  void versionChanged(const QString &newVersion, const QString &newText);
  void verseClicked();

private:
  QString m_book;
  int m_chapter;
  int m_verse;
  QString m_currentVersion;
  QLabel *contentLabel;
};

class ControlWindow : public QMainWindow {
  Q_OBJECT
public:
  explicit ControlWindow(ProjectionWindow *proj, SongManager *sm,
                         ThemeManager *tm, QWidget *parent = nullptr);

protected:
  void closeEvent(QCloseEvent *event) override;

private slots:
  // Song Management
  void onSongSelected(int index);
  void createNewSong();
  void editLyrics();
  void deleteSong();
  void saveSong();
  void updateSongList();

  // Bible Management (Grid)
  void onBookSelected(const QString &book);
  void onChapterSelected(int chapter);
  void onVerseSelected(int verse);
  void onBibleBackClicked();
  void onBibleVerseSelected(QListWidgetItem *item);
  void onQuickSearch();

  // Projection
  void projectVerse(int index);
  void projectBibleVerse(const QString &text);
  void nextVerse();
  void prevVerse();
  void onClearTextClicked();
  void onBlackOutClicked();
  void clearAll();
  void togglePresentation();

  // Themes
  void selectImage();
  void selectVideo();
  void selectColor();
  void saveCurrentVideoAsTemplate();
  void createNewTheme();
  void updateThemeTab();
  void applyTheme(const QString &themeName);

  // UI
  void onTabChanged(int index);
  void onMediaError(const QString &message);
  void onNotesProject(const QString &text);
  void setGlobalBibleVersion(const QString &version);
  void refreshBibleVersions();

signals:
  void bibleVersionChanged(const QString &version);

private:
  ProjectionWindow *projection;
  ProjectionPreview *preview;
  SongManager *songManager;
  ThemeManager *themeManager;

  // UI Components
  QSplitter *mainSplitter;
  QWidget *sidebarContainer;
  QTabWidget *mainTabWidget;

  // Sidebar (Library) — compact, lazily-populated song list
  QLineEdit *songSearchEdit;
  QListWidget *songList;
  QLabel *songListStatusLabel;
  QList<int> m_filteredSongIndices; // indices into songManager->getSongs()
  int m_songListLoaded = 0;         // how many filtered results are in songList
  QTimer *m_songSearchDebounce = nullptr;
  static constexpr int kSongPageSize = 30;

  void filterSongList(const QString &query);
  void loadMoreSongs();
  void selectSongByIndex(int songIndex);
  void updateSongListStatus();

  // Central (Workspace)
  // -- Bible Tab --
  QSplitter *bibleSplitter;
  QListWidget *bibleVerseList;
  QLineEdit *bibleQuickSearch;
  QButtonGroup *bibleVersionButtons;
  QGridLayout *bibleVersionLayout;
  QString currentBibleVersion;

  // Grid Navigation
  QStackedWidget *bibleNavStack;
  QWidget *bookGridPage;
  QWidget *chapterGridPage;
  QWidget *verseGridPage;
  QLabel *navHeaderLabel;
  QPushButton *navBackBtn;

  // Bottom Navigation
  QPushButton *navBooksBtn;
  QPushButton *navChaptersBtn;
  QPushButton *navVersesBtn;

  // Stage Controls
  QComboBox *projectionLayoutCombo;
  QComboBox *targetLayerCombo;
  QComboBox *screenSelectorCombo; // New: screen selector
  int currentTargetLayer = 0;
  bool chooseContentScreen();
  QString m_screenText[2];

  // Text Formatting Controls
  QSpinBox *fontSizeSpin;
  QSpinBox *marginSpin;
  QFontComboBox *fontCombo;
  QComboBox *alignmentCombo;
  QCheckBox *scrollCheckBox;

  void updateFormatting();
  void loadLayerSettings(int layerIdx);
  void updateBibleNavButtons();
  void setupKeyboardShortcuts(); // New

  // Grid Containers
  QVBoxLayout *bookGridContentLayout;
  QGridLayout *chapterGridLayout;
  QGridLayout *verseGridLayout;

  QString currentBibleBook;
  int currentBibleChapter = 1;

  // -- Song Tab --
  QListWidget *m_songMatches = nullptr;
  QPushButton *m_pinSongBtn = nullptr;
  QTimer *m_songLookupTimer = nullptr;
  void searchSongLibrary();
  void findSongMatches(const QString &query);
  void openSongBrowser(const QString &query);
  QListWidget *verseList;
  QLineEdit *titleEdit;
  QLineEdit *artistEdit;
  QTextEdit *lyricsEdit;

  QPushButton *nextBtn;
  QPushButton *prevBtn;

  // -- Media Tab --
  void setupMediaTab(QWidget *container);
  void addMediaFile();
  void removeMediaFile();
  void onMediaFileSelected(QListWidgetItem *item);
  void onMediaPageSelected(QListWidgetItem *item);

  struct MediaItem {
    QString path;
    Projection::Content::MediaType type;
    int pageCount = 0;
  };
  std::vector<MediaItem> mediaItems;
  QListWidget *mediaFileList;
  QListWidget *mediaPageList;
  int currentMediaIndex = -1;

  // -- Controls --
  QPushButton *presentBtn;
  QLabel *liveStatusLabel;
  QPushButton *clearTextBtn;
  QPushButton *blackOutBtn;

  // Notes
  NotesWidget *notesWidget;

  // -- Browser Tab --
  QLineEdit *m_lyricsSearch;
  QWebEngineView *m_webView;
  QLabel *m_browserStatus;
  QCompleter *m_lyricsCompleter;
  QNetworkAccessManager *m_netManager;
  QStringListModel *m_suggestModel;
  QTimer *m_suggestTimer;

  // -- Browser Tab: Local Bible Lookup Panel --
  QButtonGroup *m_bibleVersionBtns;
  QString m_selectedBibleVersion;
  class ScriptureLookup *m_scriptureLookup = nullptr;
  QLineEdit *m_bibleSearchInput;
  QTimer *m_bibleSearchTimer;
  QListWidget *m_bibleResultsList;

  // -- Browser Tab: AI-powered lyrics cleanup --
  // Cascades through free/cheap options before falling back to local
  // regex-based cleanup: your own Groq key (free) -> your bible_trivia
  // backend (free, no key needed) -> Anthropic (paid, if a key is set) ->
  // heuristicCleanLyrics (always available, no network).
  QPushButton *m_addToSongsBtn = nullptr;
  void cleanLyricsWithAI(const QString &rawText, const QString &title,
                         const QString &artist);
  void tryGroqCleanup(const QString &prompt, const QString &rawText,
                      const QString &title, const QString &artist);
  void tryBibleTriviaBackendCleanup(const QString &prompt,
                                    const QString &rawText,
                                    const QString &title,
                                    const QString &artist);
  void tryAnthropicCleanup(const QString &prompt, const QString &rawText,
                           const QString &title, const QString &artist);
  void finishLyricsCleanup(const QString &title, const QString &artist,
                           const QString &cleanedText,
                           const QString &sourceLabel);
  void finishWithHeuristicCleanup(const QString &rawText,
                                  const QString &title,
                                  const QString &artist);
  QString heuristicCleanLyrics(const QString &pageText);
  void showImportLyricsDialog(const QString &title, const QString &artist,
                              const QString &lyricsText, bool aiCleaned);
  void promptForAiApiKeys();

  // Themes
  QGroupBox *videoThemesGroup;
  QGridLayout *videoThemesLayout;

  void loadMedia();
  void saveMedia();

  void setupSidebar(QWidget *container);
  void setupMainWorkspace(QWidget *container);
  void setupMasterControl(QWidget *container);
  void setupBibleTab(QWidget *container);
  void setupSongTab(QWidget *container);
  void setupLyricsTab(QWidget *container);
  void setupBrowserTab(QWidget *container);

  // Bible Grid Helpers
  void setupBookGrid(QWidget *page);
  void setupChapterGrid(QWidget *page);
  void setupVerseGrid(QWidget *page);
  void refreshBookGrid();
  void populateChapterGrid(const QString &book);
  void populateVerseGrid(int chapter);

  // State
  int currentSongIndex = -1;
  int currentVerseIndex = -1;
  bool isPresenting = false;
  bool isTextVisible = true;
  bool isScreenBlackened = false;

  // Last projected content — for restore after blackout
  QString lastProjectedText;

  QString currentBGPath;
  bool isVideoActive = false;
  QColor currentBGColor;
};
