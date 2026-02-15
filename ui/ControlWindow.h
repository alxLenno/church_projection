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

  // Sidebar (Library)
  QLineEdit *songSearchEdit;
  QListWidget *songList;

  // Central (Workspace)
  // -- Bible Tab --
  QSplitter *bibleSplitter;
  QListWidget *bibleVerseList;
  QLineEdit *bibleQuickSearch;
  QButtonGroup *bibleVersionButtons;
  QHBoxLayout *bibleVersionLayout;
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
