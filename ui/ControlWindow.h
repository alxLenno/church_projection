#pragma once
#include "../core/SongManager.h"
#include "../core/ThemeManager.h"
#include "NotesWidget.h"
#include "ProjectionPreview.h"
#include "ProjectionWindow.h"
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QPushButton>
#include <QScrollArea>
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
  void onBibleBackClicked(); // Go up one level
  void onBibleVerseSelected(QListWidgetItem *item);
  void onQuickSearch(); // Keep search bar maybe?

  // Projection
  void projectVerse(int index);                // From Song
  void projectBibleVerse(const QString &text); // From Bible
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
  void saveCurrentVideoAsTemplate(); // Deprecated? Replaced by createNewTheme
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
  QSplitter *bibleSplitter; // Vertical: Content | Navigation
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
  QLabel *navHeaderLabel;  // "Genesis > Chapter 1"
  QPushButton *navBackBtn; // To be removed or kept as secondary? Plan says
                           // "seamless bottom navigation". User request: "nav
                           // at the bottom". Let's keep back button in header
                           // for now, but focus on bottom nav.

  // Bottom Navigation
  QPushButton *navBooksBtn;
  QPushButton *navChaptersBtn;
  QPushButton *navVersesBtn;

  // Stage Controls (Layout & Layer Targeting)
  QComboBox *projectionLayoutCombo;
  QComboBox *targetLayerCombo;
  int currentTargetLayer = 0;

  void updateBibleNavButtons();

  // Grid Containers
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

  // -- Controls --
  QPushButton *presentBtn;
  QLabel *liveStatusLabel;
  QPushButton *clearTextBtn;
  QPushButton *blackOutBtn;

  // Notes
  NotesWidget *notesWidget;

  // Themes
  QGroupBox *videoThemesGroup;
  QVBoxLayout *videoThemesLayout;

  void setupSidebar(QWidget *container);
  void setupMainWorkspace(QWidget *container);
  void setupMasterControl(QWidget *container);
  void setupBibleTab(QWidget *container);
  void setupSongTab(QWidget *container);

  // Bible Grid Helpers
  void setupBookGrid(QWidget *page);
  void setupChapterGrid(QWidget *page);
  void setupVerseGrid(QWidget *page);
  void populateChapterGrid(const QString &book);
  void populateVerseGrid(int chapter);

  // State
  int currentSongIndex = -1;
  int currentVerseIndex = -1;
  bool isPresenting = false;
  bool isTextVisible = true;
  bool isScreenBlackened = false;

  QString currentBGPath;
  bool isVideoActive = false;
  QColor currentBGColor;
};
