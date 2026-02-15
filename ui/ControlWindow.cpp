#include "ControlWindow.h"
#include "../core/BibleManager.h"
#include "ThemeEditorDialog.h"
#include <QAction>
#include <QApplication>
#include <QAudioOutput>
#include <QButtonGroup>
#include <QEvent> // Added for enterEvent/leaveEvent
#include <QFileDialog>
#include <QGuiApplication>
#include <QInputDialog>
#include <QListWidget>
#include <QMediaPlayer>
#include <QMenu>
#include <QMessageBox>
#include <QScreen>
#include <QShortcut>
#include <QStackedLayout>
#include <QStandardItemModel>
#include <QStyle>
#include <QVideoWidget>
#include <QWindow>
#include <functional> // Added for std::function

// --- Helper Class: ThemePreviewCard ---
// Handles lazy loading of video players to save resources.
class ThemePreviewCard : public QWidget {
public:
  ThemePreviewCard(const ThemeTemplate &theme, int index, ControlWindow *parent)
      : QWidget(parent), m_theme(theme), m_index(index),
        m_controlWindow(parent) {

    setObjectName("themeItem");
    setStyleSheet(
        "QWidget#themeItem { background: #1e293b; border: 1px solid #334155; "
        "border-radius: 8px; margin: 0px; } "
        "QWidget#themeItem:hover { border-color: #38bdf8; background: #334155; "
        "}");

    auto *itemLayout = new QVBoxLayout(this);
    itemLayout->setContentsMargins(6, 6, 6, 6);
    itemLayout->setSpacing(6);

    auto *nameLabel = new QLabel(theme.name);
    nameLabel->setStyleSheet(
        "font-weight: bold; color: #e2e8f0; font-size: 11px;");
    nameLabel->setAlignment(Qt::AlignCenter);
    itemLayout->addWidget(nameLabel);

    // Preview Container
    auto *previewContainer = new QWidget();
    previewContainer->setMinimumHeight(100);
    previewContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_stack = new QStackedLayout(previewContainer);
    m_stack->setStackingMode(QStackedLayout::StackAll);

    // Render Background
    if (theme.type == ThemeType::Video) {
      m_videoWidget = new QVideoWidget();
      m_videoWidget->setStyleSheet("background: black; border-radius: 4px;");
      // Placeholder/Icon when not playing? optional.
      m_stack->addWidget(m_videoWidget);
    } else {
      auto *colorFrame = new QFrame();
      colorFrame->setStyleSheet(
          QString("QFrame { background: %1; border-radius: 4px; border: 1px "
                  "solid rgba(255,255,255,0.1); }")
              .arg(theme.type == ThemeType::Color ? theme.color.name()
                                                  : "#0f172a"));
      m_stack->addWidget(colorFrame);
    }

    // Text Overlay
    auto *dummyText = new QLabel("John 3:16");
    dummyText->setAlignment(Qt::AlignCenter);
    dummyText->setWordWrap(true);
    dummyText->setStyleSheet(
        "color: white; font-weight: bold; font-size: 11px; font-style: italic; "
        "background: transparent;");
    if (theme.type == ThemeType::Color && theme.color.lightness() > 180) {
      dummyText->setStyleSheet(
          "color: #0f172a; font-weight: bold; font-size: 11px; font-style: "
          "italic; background: transparent;");
    }
    m_stack->addWidget(dummyText);

    itemLayout->addWidget(previewContainer);

    // Apply Button
    auto *applyBtn = new QPushButton("Apply");
    applyBtn->setCursor(Qt::PointingHandCursor);
    applyBtn->setStyleSheet(
        "QPushButton { background: #38bdf8; color: #0f172a; border-radius: "
        "4px; "
        "padding: 4px; font-size: 10px; font-weight: bold; } "
        "QPushButton:hover { background: #0ea5e9; }");

    connect(applyBtn, &QPushButton::clicked, this,
            &ThemePreviewCard::onApplyClicked);
    itemLayout->addWidget(applyBtn);

    // Context Menu
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QWidget::customContextMenuRequested, this,
            &ThemePreviewCard::showContextMenu);
  }

  ~ThemePreviewCard() {
    if (m_player) {
      m_player->stop();
      delete m_player;
    }
    if (m_audio) {
      delete m_audio;
    }
  }

protected:
  void enterEvent(QEnterEvent *event) override {
    QWidget::enterEvent(event);
    if (m_theme.type == ThemeType::Video && !m_player) {
      startVideo();
    }
  }

  void leaveEvent(QEvent *event) override {
    QWidget::leaveEvent(event);
    if (m_player) {
      stopVideo();
    }
  }

private slots:
  void onApplyClicked() {
    if (m_applyCallback)
      m_applyCallback(m_theme);
  }

  void showContextMenu(const QPoint &pos) {
    QMenu menu(this);
    QAction *deleteAction = menu.addAction("Delete Theme");
    connect(deleteAction, &QAction::triggered, [this]() {
      if (m_deleteCallback)
        m_deleteCallback(m_index);
    });
    menu.exec(mapToGlobal(pos));
  }

public:
  std::function<void(const ThemeTemplate &)> m_applyCallback;
  std::function<void(int)> m_deleteCallback;

private:
  void startVideo() {
    if (!m_videoWidget)
      return;
    m_player = new QMediaPlayer(this);
    m_audio = new QAudioOutput(this);
    m_audio->setMuted(true);
    m_player->setAudioOutput(m_audio);
    m_player->setVideoOutput(m_videoWidget);
    m_player->setSource(QUrl::fromLocalFile(m_theme.contentPath));

    connect(m_player, &QMediaPlayer::playbackStateChanged,
            [this](QMediaPlayer::PlaybackState state) {
              if (state == QMediaPlayer::StoppedState && m_player)
                m_player->play();
            });

    m_player->play();
  }

  void stopVideo() {
    if (m_player) {
      m_player->stop();
      m_player->deleteLater();
      m_player = nullptr;
    }
    if (m_audio) {
      m_audio->deleteLater();
      m_audio = nullptr;
    }
  }

  ThemeTemplate m_theme;
  int m_index;
  ControlWindow *m_controlWindow;
  QMediaPlayer *m_player = nullptr;
  QAudioOutput *m_audio = nullptr;
  QVideoWidget *m_videoWidget = nullptr;
  QStackedLayout *m_stack = nullptr;
};

VerseWidget::VerseWidget(const QString &book, int chapter, int verse,
                         const QString &text, const QString &version,
                         QWidget *parent)
    : QWidget(parent), m_book(book), m_chapter(chapter), m_verse(verse),
      m_currentVersion(version) {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(10, 8, 10, 8);
  layout->setSpacing(8);

  contentLabel = new QLabel(QString("<b>%1</b> %2").arg(verse).arg(text));
  contentLabel->setWordWrap(true);
  contentLabel->setStyleSheet(
      "color: white; font-size: 16px; background: transparent; "
      "selection-background-color: #38bdf8;");
  layout->addWidget(contentLabel);

  // No stretch here to keep verses compact

  setAttribute(Qt::WA_StyledBackground, true);
  setStyleSheet(
      "VerseWidget { border-bottom: 1px solid rgba(255,255,255,0.05); } "
      "VerseWidget:hover { background: rgba(255,255,255,0.03); }");
}

ControlWindow::ControlWindow(ProjectionWindow *proj, SongManager *sm,
                             ThemeManager *tm, QWidget *parent)
    : QMainWindow(parent), projection(proj), songManager(sm), themeManager(tm) {

  setWindowTitle("Church Projection - Dashboard");

  // Maximize to fill available screen area
  setWindowState(Qt::WindowMaximized);
  setMinimumSize(1024, 600);

  // Central Widget is now just a container for the Main Splitter
  auto *centralWidget = new QWidget();
  auto *mainLayout = new QVBoxLayout(centralWidget);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  // Main Splitter: [Sidebar | Workspace | Controls]
  mainSplitter = new QSplitter(Qt::Horizontal);
  mainLayout->addWidget(mainSplitter);

  // 1. Sidebar (Library)
  sidebarContainer = new QWidget();
  setupSidebar(sidebarContainer);
  mainSplitter->addWidget(sidebarContainer);

  // 2. Main Workspace (Tabs)
  auto *workspaceContainer = new QWidget();
  setupMainWorkspace(workspaceContainer);
  mainSplitter->addWidget(workspaceContainer);

  // 3. Right Pane (Controls + Preview)
  auto *controlsContainer = new QWidget();
  setupMasterControl(controlsContainer);
  mainSplitter->addWidget(controlsContainer);

  // Set Initial Sizes (15% | 55% | 30%)
  mainSplitter->setStretchFactor(0, 1);
  mainSplitter->setStretchFactor(1, 4);
  mainSplitter->setStretchFactor(2, 2);

  setCentralWidget(centralWidget);

  // Initialize Data
  updateSongList();
  applyTheme("Glassmorphism 3.0"); // Default

  // Connect Signals
  if (projection) {
    connect(projection, &ProjectionWindow::mediaError, this,
            &ControlWindow::onMediaError);
  }
  connect(themeManager, &ThemeManager::templatesChanged, this,
          &ControlWindow::updateThemeTab);

  // Initial Theme Tab Update
  updateThemeTab();

  // Connect Bible loading
  connect(&BibleManager::instance(), &BibleManager::bibleLoaded, this,
          &ControlWindow::refreshBibleVersions);
  BibleManager::instance().loadBibles();

  // Connect Notes version changes
  connect(notesWidget, &NotesWidget::versionChanged, this,
          &ControlWindow::setGlobalBibleVersion);

  // Keyboard shortcuts
  setupKeyboardShortcuts();

  // Apply unified dark stylesheet
  setStyleSheet(
      /* Global defaults */
      "QMainWindow, QWidget { background: #0f172a; color: #e2e8f0; }"
      "QGroupBox { background: #1e293b; border: 1px solid #334155; "
      "  border-radius: 8px; margin-top: 14px; padding: 14px 10px 10px; "
      "  font-weight: bold; color: #94a3b8; }"
      "QGroupBox::title { subcontrol-origin: margin; left: 12px; "
      "  padding: 0 6px; color: #38bdf8; font-size: 11px; }"
      /* Tabs */
      "QTabWidget::pane { border: 1px solid #334155; background: #0f172a; }"
      "QTabBar::tab { background: #1e293b; color: #94a3b8; padding: 8px 18px; "
      "  border: 1px solid #334155; border-bottom: none; "
      "border-top-left-radius: 6px; "
      "  border-top-right-radius: 6px; margin-right: 2px; font-weight: bold; }"
      "QTabBar::tab:selected { background: #0f172a; color: #38bdf8; "
      "  border-bottom: 2px solid #38bdf8; }"
      "QTabBar::tab:hover { color: white; }"
      /* Inputs */
      "QLineEdit, QTextEdit, QSpinBox { background: #1e293b; color: white; "
      "  border: 1px solid #334155; border-radius: 6px; padding: 6px 10px; }"
      "QLineEdit:focus, QTextEdit:focus, QSpinBox:focus { border-color: "
      "#38bdf8; }"
      /* Buttons */
      "QPushButton { background: #334155; color: white; border: none; "
      "  border-radius: 6px; padding: 8px 16px; font-weight: bold; }"
      "QPushButton:hover { background: #475569; }"
      "QPushButton:pressed { background: #38bdf8; color: #0f172a; }"
      "QPushButton#presentBtn { background: #22c55e; color: white; "
      "  font-size: 14px; padding: 12px; }"
      "QPushButton#presentBtn:hover { background: #16a34a; }"
      "QPushButton#stopBtn { background: #ef4444; color: white; "
      "  font-size: 14px; padding: 12px; }"
      "QPushButton#stopBtn:hover { background: #dc2626; }"
      "QPushButton#primaryBtn { background: #38bdf8; color: #0f172a; }"
      "QPushButton#primaryBtn:hover { background: #0ea5e9; }"
      /* Lists */
      "QListWidget { background: #1e293b; border: 1px solid #334155; "
      "  border-radius: 6px; color: white; }"
      "QListWidget::item { padding: 6px; border-bottom: 1px solid #1e293b; }"
      "QListWidget::item:selected { background: #38bdf8; color: #0f172a; }"
      "QListWidget::item:hover { background: rgba(56,189,248,0.15); }"
      /* Combos */
      "QComboBox { background: #1e293b; color: white; border: 1px solid "
      "#334155; "
      "  border-radius: 6px; padding: 6px 10px; }"
      "QComboBox:hover { border-color: #38bdf8; }"
      "QComboBox QAbstractItemView { background: #1e293b; color: white; "
      "  selection-background-color: #38bdf8; }"
      /* FontCombo */
      "QFontComboBox { background: #1e293b; color: white; border: 1px solid "
      "#334155; "
      "  border-radius: 6px; padding: 4px 8px; }"
      /* Splitter */
      "QSplitter::handle { background: #334155; }"
      "QSplitter::handle:horizontal { width: 2px; }"
      "QSplitter::handle:vertical { height: 2px; }"
      /* ScrollArea */
      "QScrollArea { border: none; background: transparent; }"
      "QScrollBar:vertical { background: #0f172a; width: 8px; }"
      "QScrollBar::handle:vertical { background: #475569; border-radius: 4px; "
      "min-height: 20px; }"
      "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: "
      "0; }"
      /* Labels */
      "QLabel { color: #e2e8f0; background: transparent; }"
      "QLabel#headerDisplay { color: #38bdf8; font-size: 14px; "
      "  font-weight: bold; padding: 8px; }"
      "QLabel#statusLive { color: #22c55e; font-weight: bold; font-size: 16px; "
      "}"
      "QLabel#statusOffline { color: #94a3b8; font-weight: bold; }"
      /* Checkbox */
      "QCheckBox { color: #e2e8f0; spacing: 8px; }"
      "QCheckBox::indicator { width: 16px; height: 16px; }");
}

void ControlWindow::setupSidebar(QWidget *container) {
  auto *layout = new QVBoxLayout(container);
  layout->setContentsMargins(6, 6, 6, 6);

  auto *header = new QLabel("LYRIC LIBRARY");
  header->setObjectName("headerDisplay");
  header->setAlignment(Qt::AlignCenter);
  layout->addWidget(header);

  // Search
  songSearchEdit = new QLineEdit();
  songSearchEdit->setPlaceholderText("Search songs...");
  connect(songSearchEdit, &QLineEdit::textChanged, [this](const QString &text) {
    // Simple filter implementation
    for (int i = 0; i < songList->count(); ++i) {
      QListWidgetItem *item = songList->item(i);
      item->setHidden(!item->text().contains(text, Qt::CaseInsensitive));
    }
  });
  layout->addWidget(songSearchEdit);

  // List
  songList = new QListWidget();
  layout->addWidget(songList);
  connect(songList, &QListWidget::currentRowChanged, this,
          &ControlWindow::onSongSelected);

  // Buttons
  auto *btnLayout = new QHBoxLayout();
  auto *addBtn = new QPushButton("+ Add");
  auto *editBtn = new QPushButton("Edit");
  auto *delBtn = new QPushButton("Del");

  connect(addBtn, &QPushButton::clicked, this, &ControlWindow::createNewSong);
  connect(editBtn, &QPushButton::clicked, this, &ControlWindow::editLyrics);
  connect(delBtn, &QPushButton::clicked, this, &ControlWindow::deleteSong);

  btnLayout->addWidget(addBtn);
  btnLayout->addWidget(editBtn);
  btnLayout->addWidget(delBtn);
  layout->addLayout(btnLayout);
}

void ControlWindow::setupMainWorkspace(QWidget *container) {
  auto *layout = new QVBoxLayout(container);
  layout->setContentsMargins(0, 0, 0, 0);

  mainTabWidget = new QTabWidget();
  layout->addWidget(mainTabWidget);

  connect(mainTabWidget, &QTabWidget::currentChanged, this,
          &ControlWindow::onTabChanged);

  // --- Tab 1: Scriptures ---
  auto *bibleTab = new QWidget();
  setupBibleTab(bibleTab);
  mainTabWidget->addTab(bibleTab, "Bible");

  // --- Tab 2: Songs (Lyrics) ---
  auto *songTab = new QWidget();
  setupSongTab(songTab);
  mainTabWidget->addTab(songTab, "Songs");

  // --- Tab 3: Sermon Notes ---
  notesWidget = new NotesWidget(this);
  connect(notesWidget, &NotesWidget::projectText, this,
          &ControlWindow::onNotesProject);
  mainTabWidget->addTab(notesWidget, "Notes");

  // --- Tab 4: Media (PDF/Images) ---
  auto *mediaTab = new QWidget();
  setupMediaTab(mediaTab);
  mainTabWidget->addTab(mediaTab, "Media");
}

void ControlWindow::setupBibleTab(QWidget *container) {
  auto *layout = new QVBoxLayout(container);
  layout->setContentsMargins(0, 0, 0, 0);

  // --- Top Navigation & Version (Always Visible) ---
  auto *navContainer = new QWidget();
  auto *navLayout = new QVBoxLayout(navContainer);
  navLayout->setContentsMargins(10, 10, 10, 5);
  navLayout->setSpacing(8);

  // Version selector - horizontal toggle buttons in a frame
  auto *versionFrame = new QFrame();
  versionFrame->setStyleSheet(
      "QFrame { background: #1e293b; border: 1px solid #334155; "
      "border-radius: 6px; padding: 8px; }");
  bibleVersionLayout = new QHBoxLayout(versionFrame);
  bibleVersionLayout->setSpacing(6);
  bibleVersionLayout->setContentsMargins(8, 8, 8, 8);

  auto *versionLabel = new QLabel("VERSION:");
  versionLabel->setStyleSheet("color: white; font-weight: bold; font-size: "
                              "12px; background: transparent; border: none;");
  bibleVersionLayout->addWidget(versionLabel);

  bibleVersionButtons = new QButtonGroup(this);
  bibleVersionButtons->setExclusive(true);

  // Buttons will be populated by refreshBibleVersions()
  navLayout->addWidget(versionFrame);

  // Quick search bar
  auto *searchLayout = new QHBoxLayout();
  auto *searchLabel = new QLabel("SCRIPTURE:");
  searchLabel->setStyleSheet(
      "color: #94a3b8; font-weight: bold; font-size: 11px;");
  searchLayout->addWidget(searchLabel);

  bibleQuickSearch = new QLineEdit();
  bibleQuickSearch->setPlaceholderText("Quick Search (e.g. John 3:16)");
  connect(bibleQuickSearch, &QLineEdit::returnPressed, this,
          &ControlWindow::onQuickSearch);
  searchLayout->addWidget(bibleQuickSearch);
  navLayout->addLayout(searchLayout);

  layout->addWidget(navContainer);

  // Vertical Splitter: Top (Content) | Bottom (Navigation)
  bibleSplitter = new QSplitter(Qt::Vertical);
  layout->addWidget(bibleSplitter);

  // --- Top Pane: Content ---
  auto *topWidget = new QWidget();
  auto *topLayout = new QVBoxLayout(topWidget);
  topLayout->setContentsMargins(0, 5, 0, 0);

  bibleVerseList = new QListWidget();
  bibleVerseList->setWordWrap(true);
  bibleVerseList->setAlternatingRowColors(true);
  bibleVerseList->setStyleSheet(
      "QListWidget::item { padding: 8px; border-bottom: 1px solid #334155; }");
  connect(bibleVerseList, &QListWidget::itemClicked, this,
          &ControlWindow::onBibleVerseSelected);
  topLayout->addWidget(bibleVerseList);

  bibleSplitter->addWidget(topWidget);

  // --- Bottom Pane: Navigation Grid ---
  auto *bottomWidget = new QWidget();
  auto *bottomLayout = new QVBoxLayout(bottomWidget);
  bottomLayout->setContentsMargins(0, 0, 0, 0);

  // Nav Header (Back Button + Label)
  auto *navHeader = new QWidget();
  navHeader->setStyleSheet(
      "background: #1e293b; border-bottom: 1px solid #334155;");
  auto *navHeaderLayout = new QHBoxLayout(navHeader);

  navBackBtn = new QPushButton("◀ BACK");
  navBackBtn->setFixedWidth(80);
  navBackBtn->setVisible(false); // Hidden on Book Grid
  connect(navBackBtn, &QPushButton::clicked, this,
          &ControlWindow::onBibleBackClicked);

  navHeaderLabel = new QLabel("SELECT BOOK");
  navHeaderLabel->setAlignment(Qt::AlignCenter);
  navHeaderLabel->setStyleSheet("font-weight: bold; color: #94a3b8;");

  navHeaderLayout->addWidget(navBackBtn);
  navHeaderLayout->addWidget(navHeaderLabel);
  navHeaderLayout->addStretch(); // Balance ? or center label?
  // To center label properly with left button:
  // Add dummy right widget? Or just use stretch.

  bottomLayout->addWidget(navHeader);

  // Stacked Widget for Grids
  bibleNavStack = new QStackedWidget();

  bookGridPage = new QWidget();
  setupBookGrid(bookGridPage);
  bibleNavStack->addWidget(bookGridPage);

  chapterGridPage = new QWidget();
  setupChapterGrid(chapterGridPage);
  bibleNavStack->addWidget(chapterGridPage);

  verseGridPage = new QWidget();
  setupVerseGrid(verseGridPage);
  bibleNavStack->addWidget(verseGridPage);

  bottomLayout->addWidget(bibleNavStack);

  // --- Footer Navigation Bar ---
  auto *navFooter = new QFrame();
  navFooter->setObjectName("navFooter");
  navFooter->setStyleSheet(
      "QFrame#navFooter { background: #0f172a; border-top: 2px solid #38bdf8; "
      "padding: 5px; }");
  auto *navFooterLayout = new QHBoxLayout(navFooter);
  navFooterLayout->setContentsMargins(10, 5, 10, 5);
  navFooterLayout->setSpacing(10);

  navBooksBtn = new QPushButton("BOOKS");
  navChaptersBtn = new QPushButton("CHAPTERS");
  navVersesBtn = new QPushButton("VERSES");

  QString navBtnStyle =
      "QPushButton { background: transparent; color: #94a3b8; border: none; "
      "font-weight: bold; padding: 10px; font-size: 13px; } "
      "QPushButton:enabled:hover { color: #38bdf8; } "
      "QPushButton:checked { color: #38bdf8; border-bottom: 2px solid #38bdf8; "
      "background: #1e293b; } "
      "QPushButton:disabled { color: #334155; }";

  navBooksBtn->setCheckable(true);
  navChaptersBtn->setCheckable(true);
  navVersesBtn->setCheckable(true);

  navBooksBtn->setStyleSheet(navBtnStyle);
  navChaptersBtn->setStyleSheet(navBtnStyle);
  navVersesBtn->setStyleSheet(navBtnStyle);

  connect(navBooksBtn, &QPushButton::clicked, [this]() {
    bibleNavStack->setCurrentWidget(bookGridPage);
    navHeaderLabel->setText("SELECT BOOK");
    updateBibleNavButtons();
  });
  connect(navChaptersBtn, &QPushButton::clicked, [this]() {
    bibleNavStack->setCurrentWidget(chapterGridPage);
    navHeaderLabel->setText(currentBibleBook + " > Select Chapter");
    updateBibleNavButtons();
  });
  connect(navVersesBtn, &QPushButton::clicked, [this]() {
    bibleNavStack->setCurrentWidget(verseGridPage);
    navHeaderLabel->setText(QString("%1 %2 > Select Verse")
                                .arg(currentBibleBook)
                                .arg(currentBibleChapter));
    updateBibleNavButtons();
  });

  navFooterLayout->addWidget(navBooksBtn);
  navFooterLayout->addWidget(
      new QLabel("<span style='color:#334155;'>|</span>"));
  navFooterLayout->addWidget(navChaptersBtn);
  navFooterLayout->addWidget(
      new QLabel("<span style='color:#334155;'>|</span>"));
  navFooterLayout->addWidget(navVersesBtn);
  navFooterLayout->addStretch();

  // bottomLayout->addWidget(navFooter); // Hidden to fit screen better (bottom
  // app tiles)

  bibleSplitter->addWidget(bottomWidget);

  // Initial Sizes (50/50)
  bibleSplitter->setStretchFactor(0, 1);
  bibleSplitter->setStretchFactor(1, 1);

  updateBibleNavButtons();
}

// --- Grid Setup Helpers ---

// --- Grid Setup Helpers ---

void ControlWindow::setupBookGrid(QWidget *page) {
  auto *layout = new QVBoxLayout(page);

  auto *scroll = new QScrollArea();
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);

  auto *content = new QWidget();
  bookGridContentLayout = new QVBoxLayout(content); // Assign to member

  scroll->setWidget(content);
  layout->addWidget(scroll);

  // Initial Population
  refreshBookGrid();
}

void ControlWindow::refreshBookGrid() {
  if (!bookGridContentLayout)
    return;

  // Clear existing items
  QLayoutItem *child;
  while ((child = bookGridContentLayout->takeAt(0)) != nullptr) {
    if (child->widget())
      delete child->widget();
    delete child;
  }

  // Style for Grid Buttons
  QString btnStyle =
      "QPushButton { "
      "  background: #334155; color: white; border: none; border-radius: 4px; "
      "  padding: 8px; text-align: left; font-weight: bold; "
      "} "
      "QPushButton:hover { background: #38bdf8; color: #0f172a; }";

  // Use current version for localization
  // If empty, BibleManager defaults to English names if passed "" or defaults
  QString version =
      currentBibleVersion.isEmpty() ? "NKJV" : currentBibleVersion;

  // Helper to create sections
  auto createSection = [&](const QString &title, BibleManager::Testament t) {
    auto *group = new QGroupBox(title);
    auto *gl = new QGridLayout(group);
    gl->setSpacing(5);

    int row = 0, col = 0;
    int maxCols = 4; // 4 books per row

    // Pass version to get localized names
    auto books = BibleManager::instance().getCanonicalBooks(version);

    for (const auto &book : books) {
      if (book.testament == t) {
        // book.name is now LOCALIZED if available (e.g. "Mwanzo")
        // But we probably want to store the key (English) for logic?
        // Or does BibleManager handle localized names in queries?
        // BibleManager::search/getVerseCount usually expects standard keys or
        // normalized. The localized name is for DISPLAY. Logic: We need to map
        // Display Name -> Key for onBookSelected. BUT
        // BibleManager::normalizeBookName handles "Mwanzo" -> "Genesis". So
        // passing "Mwanzo" to onBookSelected -> populateChapterGrid ->
        // getChapterCount("Mwanzo") normalizeBookName("Mwanzo") -> "Genesis".
        // So it should work mostly automatic!

        auto *btn = new QPushButton(book.name);
        btn->setStyleSheet(btnStyle);
        // Pass the name (Display Name) to the handler.
        // Handler logic needs to ensure it normalizes it.
        connect(btn, &QPushButton::clicked,
                [this, b = book.name]() { onBookSelected(b); });

        gl->addWidget(btn, row, col);
        col++;
        if (col >= maxCols) {
          col = 0;
          row++;
        }
      }
    }
    bookGridContentLayout->addWidget(group);
  };

  createSection("OLD TESTAMENT", BibleManager::Testament::Old);
  createSection("NEW TESTAMENT", BibleManager::Testament::New);
}

void ControlWindow::setupChapterGrid(QWidget *page) {
  auto *layout = new QVBoxLayout(page);
  auto *scroll = new QScrollArea();
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);

  auto *content = new QWidget();
  chapterGridLayout = new QGridLayout(content);
  chapterGridLayout->setSpacing(5);
  // Will be populated dynamically

  scroll->setWidget(content);
  layout->addWidget(scroll);
}

void ControlWindow::setupVerseGrid(QWidget *page) {
  auto *layout = new QVBoxLayout(page);
  auto *scroll = new QScrollArea();
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);

  auto *content = new QWidget();
  verseGridLayout = new QGridLayout(content);
  verseGridLayout->setSpacing(5);
  // Will be populated dynamically

  scroll->setWidget(content);
  layout->addWidget(scroll);
}

// --- Navigation Logic ---

void ControlWindow::onBookSelected(const QString &book) {
  currentBibleBook = book;
  populateChapterGrid(book);
  bibleNavStack->setCurrentWidget(chapterGridPage);
  navHeaderLabel->setText(book + " > Select Chapter");
  navBackBtn->setVisible(true);
  updateBibleNavButtons();
}

void ControlWindow::populateChapterGrid(const QString &book) {
  // Clear existing
  QLayoutItem *child;
  while ((child = chapterGridLayout->takeAt(0)) != nullptr) {
    if (child->widget())
      delete child->widget();
    delete child;
  }

  QString version =
      currentBibleVersion.isEmpty() ? "NKJV" : currentBibleVersion;
  if (version.isEmpty())
    version = "NKJV";

  int count = BibleManager::instance().getChapterCount(book, version);
  int maxCols = 6;

  for (int i = 1; i <= count; ++i) {
    auto *btn = new QPushButton(QString::number(i));
    btn->setFixedSize(50, 50);
    btn->setStyleSheet(
        "QPushButton { background: #334155; color: white; border-radius: 4px; "
        "font-size: 14px; font-weight: bold; } QPushButton:hover { background: "
        "#38bdf8; color: #0f172a; }");
    connect(btn, &QPushButton::clicked, [this, i]() { onChapterSelected(i); });
    chapterGridLayout->addWidget(btn, (i - 1) / maxCols, (i - 1) % maxCols);
  }
}

void ControlWindow::onChapterSelected(int chapter) {
  currentBibleChapter = chapter;

  // 1. Load Content in Top Pane
  // Reuse existing logic but manual call
  // We need to fetch verses and populate bibleVerseList

  QString query =
      QString("%1 %2").arg(currentBibleBook).arg(currentBibleChapter);
  auto results = BibleManager::instance().search(query);

  bibleVerseList->clear();
  QString version =
      currentBibleVersion.isEmpty() ? "NKJV" : currentBibleVersion;
  if (version.isEmpty())
    version = "NKJV";

  // Re-search with version filter
  // Re-search with version filter
  results = BibleManager::instance().search(query, version);

  // Normalize current book to match search results (which are normalized)
  QString normalizedCurrent = BibleManager::normalizeBookName(currentBibleBook);

  for (const auto &v : results) {
    if (v.chapter == currentBibleChapter && v.book == normalizedCurrent) {
      QListWidgetItem *item = new QListWidgetItem();

      // Get localized book name for display
      QString displayBook =
          BibleManager::instance().getLocalizedBookName(v.book, v.version);

      auto *widget =
          new VerseWidget(displayBook, v.chapter, v.verse, v.text, v.version);

      item->setData(Qt::UserRole, v.text);
      QString ref = QString("%1 %2:%3 (%4)")
                        .arg(displayBook)
                        .arg(v.chapter)
                        .arg(v.verse)
                        .arg(v.version);
      item->setData(Qt::UserRole + 1, ref);

      connect(widget, &VerseWidget::versionChanged,
              [item, v](const QString &newVer, const QString &newText) {
                item->setData(Qt::UserRole, newText);
                QString newDisplayBook =
                    BibleManager::instance().getLocalizedBookName(v.book,
                                                                  newVer);
                QString newRef = QString("%1 %2:%3 (%4)")
                                     .arg(newDisplayBook) // FIX: Localize
                                     .arg(v.chapter)
                                     .arg(v.verse)
                                     .arg(newVer);
                item->setData(Qt::UserRole + 1, newRef);
              });

      connect(widget, &VerseWidget::verseClicked, [this, item]() {
        bibleVerseList->setCurrentItem(item);
        onBibleVerseSelected(item);
      });

      item->setSizeHint(widget->sizeHint());
      bibleVerseList->addItem(item);
      bibleVerseList->setItemWidget(item, widget);
    }
  }

  // 2. Switch to Verse Grid
  populateVerseGrid(chapter);
  bibleNavStack->setCurrentWidget(verseGridPage);
  navHeaderLabel->setText(QString("%1 %2 > Select Verse")
                              .arg(currentBibleBook)
                              .arg(currentBibleChapter));
  updateBibleNavButtons();
}

void ControlWindow::populateVerseGrid(int chapter) {
  QLayoutItem *child;
  while ((child = verseGridLayout->takeAt(0)) != nullptr) {
    if (child->widget())
      delete child->widget();
    delete child;
  }

  int count = BibleManager::instance().getVerseCount(currentBibleBook, chapter);
  int maxCols = 6;

  for (int i = 1; i <= count; ++i) {
    auto *btn = new QPushButton(QString::number(i));
    btn->setFixedSize(50, 50);
    btn->setStyleSheet(
        "QPushButton { background: #334155; color: white; border-radius: 4px; "
        "font-size: 14px; font-weight: bold; } QPushButton:hover { background: "
        "#38bdf8; color: #0f172a; }");
    connect(btn, &QPushButton::clicked, [this, i]() { onVerseSelected(i); });
    verseGridLayout->addWidget(btn, (i - 1) / maxCols, (i - 1) % maxCols);
  }
}

void ControlWindow::onVerseSelected(int verse) {
  // Match verse number — data at UserRole+1 is a ref string like "Book Ch:V
  // (Ver)" So we match by row index (verse-1) since verses are 1-indexed and
  // list is 0-indexed
  int targetRow = verse - 1;
  if (targetRow >= 0 && targetRow < bibleVerseList->count()) {
    auto *item = bibleVerseList->item(targetRow);
    bibleVerseList->setCurrentItem(item);
    bibleVerseList->scrollToItem(item, QAbstractItemView::PositionAtTop);
    onBibleVerseSelected(item);
  }
}

void ControlWindow::onBibleBackClicked() {
  if (bibleNavStack->currentWidget() == verseGridPage) {
    // Back to Chapter Grid
    bibleNavStack->setCurrentWidget(chapterGridPage);
    navHeaderLabel->setText(currentBibleBook + " > Select Chapter");
  } else if (bibleNavStack->currentWidget() == chapterGridPage) {
    // Back to Book Grid
    bibleNavStack->setCurrentWidget(bookGridPage);
    navHeaderLabel->setText("SELECT BOOK");
    navBackBtn->setVisible(false);
  }
  updateBibleNavButtons();
}

void ControlWindow::updateBibleNavButtons() {
  navBooksBtn->setChecked(bibleNavStack->currentWidget() == bookGridPage);
  navChaptersBtn->setChecked(bibleNavStack->currentWidget() == chapterGridPage);
  navVersesBtn->setChecked(bibleNavStack->currentWidget() == verseGridPage);

  navChaptersBtn->setEnabled(!currentBibleBook.isEmpty());
  navVersesBtn->setEnabled(!currentBibleBook.isEmpty() &&
                           currentBibleChapter > 0);
}

void ControlWindow::setupSongTab(QWidget *container) {
  auto *layout = new QVBoxLayout(container);

  // Editor Fields (Hidden/Visible based on mode? Or just always visible?)
  // Let's keep them visible for easy editing
  auto *metaLayout = new QHBoxLayout();
  titleEdit = new QLineEdit();
  titleEdit->setPlaceholderText("Title");
  artistEdit = new QLineEdit();
  artistEdit->setPlaceholderText("Artist");
  auto *saveBtn = new QPushButton("Save Changes");
  connect(saveBtn, &QPushButton::clicked, this, &ControlWindow::saveSong);

  metaLayout->addWidget(titleEdit);
  metaLayout->addWidget(artistEdit);
  metaLayout->addWidget(saveBtn);
  layout->addLayout(metaLayout);

  // Lyrics Editor (or List?)
  // User wants "Lyric View". Let's show the verses in a list for projection,
  // and maybe a text edit for bulk editing.
  // For "Professional", maybe a splitter here too?
  // Let's stick to the list for projection mostly.

  QSplitter *songSplitter = new QSplitter(Qt::Vertical);
  layout->addWidget(songSplitter);

  verseList = new QListWidget();
  verseList->setWordWrap(true);
  connect(verseList, &QListWidget::currentRowChanged, this,
          &ControlWindow::projectVerse);
  connect(verseList, &QListWidget::itemClicked, [this](QListWidgetItem *item) {
    projectVerse(verseList->row(item));
  });

  lyricsEdit = new QTextEdit();
  lyricsEdit->setPlaceholderText(
      "Paste lyrics here...\n\nVerse 1\n...\n\nChorus\n...");

  songSplitter->addWidget(verseList);
  songSplitter->addWidget(lyricsEdit);

  // Navigation
  auto *navLayout = new QHBoxLayout();
  prevBtn = new QPushButton("PREV (B)");
  nextBtn = new QPushButton("NEXT (SPACE)");
  nextBtn->setObjectName("primaryBtn");

  connect(prevBtn, &QPushButton::clicked, this, &ControlWindow::prevVerse);
  connect(nextBtn, &QPushButton::clicked, this, &ControlWindow::nextVerse);

  navLayout->addWidget(prevBtn);
  navLayout->addWidget(nextBtn);
  layout->addLayout(navLayout);
}

void ControlWindow::setupMasterControl(QWidget *container) {
  auto *outerLayout = new QVBoxLayout(container);
  outerLayout->setContentsMargins(0, 0, 0, 0);

  // Wrap right panel in scroll area so it never clips
  auto *scrollArea = new QScrollArea();
  scrollArea->setWidgetResizable(true);
  scrollArea->setFrameShape(QFrame::NoFrame);
  auto *scrollContent = new QWidget();
  auto *layout = new QVBoxLayout(scrollContent);
  layout->setContentsMargins(6, 6, 6, 6);
  layout->setSpacing(6);

  // 1. Live Preview
  auto *previewGroup = new QGroupBox("LIVE PREVIEW");
  previewGroup->setMaximumHeight(220);
  auto *prevLayout = new QVBoxLayout(previewGroup);
  prevLayout->setContentsMargins(0, 4, 0, 0);
  preview = new ProjectionPreview(this);
  prevLayout->addWidget(preview);
  layout->addWidget(previewGroup); // No stretch — capped height

  // 2. Master Controls — Modern Stage Panel
  auto *controlsGroup = new QGroupBox();
  controlsGroup->setTitle("");
  controlsGroup->setStyleSheet(
      "QGroupBox { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
      "  stop:0 #1e293b, stop:1 #0f172a); "
      "  border: 1px solid #334155; border-radius: 10px; "
      "  padding: 12px 10px 10px; margin: 0; }");
  auto *cLayout = new QVBoxLayout(controlsGroup);
  cLayout->setSpacing(8);
  cLayout->setContentsMargins(10, 10, 10, 10);

  // Section header
  auto *stageHeader = new QLabel("⚡ STAGE");
  stageHeader->setStyleSheet(
      "color: #38bdf8; font-size: 11px; font-weight: bold; "
      "letter-spacing: 2px; background: transparent; padding: 0;");
  cLayout->addWidget(stageHeader);

  // --- Live Status + Present Button Row ---
  auto *statusRow = new QHBoxLayout();
  statusRow->setSpacing(8);

  // Status indicator with dot
  liveStatusLabel = new QLabel("● OFFLINE");
  liveStatusLabel->setAlignment(Qt::AlignCenter);
  liveStatusLabel->setObjectName("statusOffline");
  liveStatusLabel->setStyleSheet(
      "color: #64748b; font-weight: bold; font-size: 11px; "
      "background: rgba(15,23,42,0.6); border: 1px solid #334155; "
      "border-radius: 14px; padding: 6px 14px;");
  statusRow->addWidget(liveStatusLabel);

  presentBtn = new QPushButton("▶  START PRESENTATION");
  presentBtn->setObjectName("presentBtn");
  presentBtn->setCursor(Qt::PointingHandCursor);
  presentBtn->setStyleSheet(
      "QPushButton { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, "
      "  stop:0 #22c55e, stop:1 #16a34a); color: white; "
      "  border: none; border-radius: 8px; padding: 10px 20px; "
      "  font-weight: bold; font-size: 13px; } "
      "QPushButton:hover { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, "
      "  stop:0 #16a34a, stop:1 #15803d); } "
      "QPushButton:pressed { background: #15803d; }");
  connect(presentBtn, &QPushButton::clicked, this,
          &ControlWindow::togglePresentation);
  statusRow->addWidget(presentBtn, 1);

  cLayout->addLayout(statusRow);

  // --- Separator ---
  auto *sep1 = new QFrame();
  sep1->setFrameShape(QFrame::HLine);
  sep1->setStyleSheet("background: #334155; max-height: 1px; border: none;");
  cLayout->addWidget(sep1);

  // --- Quick Actions Row ---
  auto *actionsLabel = new QLabel("ACTIONS");
  actionsLabel->setStyleSheet(
      "color: #64748b; font-size: 10px; font-weight: bold; "
      "letter-spacing: 1px; background: transparent; padding: 2px 0 0 0;");
  cLayout->addWidget(actionsLabel);

  auto *togglesLayout = new QHBoxLayout();
  togglesLayout->setSpacing(6);

  clearTextBtn = new QPushButton("✕  Clear Text");
  clearTextBtn->setCursor(Qt::PointingHandCursor);
  clearTextBtn->setStyleSheet(
      "QPushButton { background: #334155; color: #fbbf24; "
      "  border: 1px solid #475569; border-radius: 6px; "
      "  padding: 7px 12px; font-weight: bold; font-size: 11px; } "
      "QPushButton:hover { background: #475569; border-color: #fbbf24; }");

  blackOutBtn = new QPushButton("■  Black Out");
  blackOutBtn->setCursor(Qt::PointingHandCursor);
  blackOutBtn->setStyleSheet(
      "QPushButton { background: #334155; color: #f87171; "
      "  border: 1px solid #475569; border-radius: 6px; "
      "  padding: 7px 12px; font-weight: bold; font-size: 11px; } "
      "QPushButton:hover { background: #475569; border-color: #f87171; }");

  auto *clearAllBtn = new QPushButton("↺  Reset All");
  clearAllBtn->setCursor(Qt::PointingHandCursor);
  clearAllBtn->setStyleSheet(
      "QPushButton { background: #334155; color: #94a3b8; "
      "  border: 1px solid #475569; border-radius: 6px; "
      "  padding: 7px 12px; font-weight: bold; font-size: 11px; } "
      "QPushButton:hover { background: #475569; color: #e2e8f0; }");

  connect(clearTextBtn, &QPushButton::clicked, this,
          &ControlWindow::onClearTextClicked);
  connect(blackOutBtn, &QPushButton::clicked, this,
          &ControlWindow::onBlackOutClicked);
  connect(clearAllBtn, &QPushButton::clicked, this, &ControlWindow::clearAll);

  togglesLayout->addWidget(clearTextBtn);
  togglesLayout->addWidget(blackOutBtn);
  togglesLayout->addWidget(clearAllBtn);
  cLayout->addLayout(togglesLayout);

  // --- Separator ---
  auto *sep2 = new QFrame();
  sep2->setFrameShape(QFrame::HLine);
  sep2->setStyleSheet("background: #334155; max-height: 1px; border: none;");
  cLayout->addWidget(sep2);

  // --- Display Settings Grid ---
  auto *settingsLabel = new QLabel("DISPLAY");
  settingsLabel->setStyleSheet(
      "color: #64748b; font-size: 10px; font-weight: bold; "
      "letter-spacing: 1px; background: transparent; padding: 2px 0 0 0;");
  cLayout->addWidget(settingsLabel);

  QString comboModernStyle =
      "QComboBox { background: #1e293b; color: #e2e8f0; "
      "  border: 1px solid #475569; border-radius: 6px; "
      "  padding: 5px 10px; font-size: 11px; font-weight: bold; } "
      "QComboBox:hover { border-color: #38bdf8; } "
      "QComboBox::drop-down { border: none; padding-right: 8px; } "
      "QComboBox QAbstractItemView { background: #1e293b; color: white; "
      "  selection-background-color: #38bdf8; border: 1px solid #475569; }";

  auto *settingsGrid = new QGridLayout();
  settingsGrid->setSpacing(6);

  // Layout combo
  auto *layoutLabel = new QLabel("Layout");
  layoutLabel->setStyleSheet(
      "color: #94a3b8; font-size: 10px; background: transparent;");
  projectionLayoutCombo = new QComboBox();
  projectionLayoutCombo->setStyleSheet(comboModernStyle);
  projectionLayoutCombo->addItem("⬜ Full Screen",
                                 (int)Projection::LayoutType::Single);
  projectionLayoutCombo->addItem("◫ Split Vertical",
                                 (int)Projection::LayoutType::SplitVertical);
  projectionLayoutCombo->addItem("⬒ Split Horizontal",
                                 (int)Projection::LayoutType::SplitHorizontal);
  connect(projectionLayoutCombo, &QComboBox::currentIndexChanged,
          [this](int index) {
            Projection::LayoutType type =
                (Projection::LayoutType)projectionLayoutCombo->itemData(index)
                    .toInt();
            projection->setLayoutType(type);
            preview->setLayoutType(type);
          });

  // Layer combo
  auto *layerLabel = new QLabel("Target");
  layerLabel->setStyleSheet(
      "color: #94a3b8; font-size: 10px; background: transparent;");
  targetLayerCombo = new QComboBox();
  targetLayerCombo->setStyleSheet(comboModernStyle);
  targetLayerCombo->addItem("Layer 1", 0);
  targetLayerCombo->addItem("Layer 2", 1);
  connect(targetLayerCombo, &QComboBox::currentIndexChanged, [this](int index) {
    currentTargetLayer = targetLayerCombo->itemData(index).toInt();
    loadLayerSettings(currentTargetLayer);
  });

  // Screen combo
  auto *screenLabel = new QLabel("Screen");
  screenLabel->setStyleSheet(
      "color: #94a3b8; font-size: 10px; background: transparent;");
  screenSelectorCombo = new QComboBox();
  screenSelectorCombo->setStyleSheet(comboModernStyle);
  screenSelectorCombo->addItem("Auto Detect");
  QList<QScreen *> screens = QGuiApplication::screens();
  for (int i = 0; i < screens.size(); ++i) {
    screenSelectorCombo->addItem(
        QString("Screen %1: %2").arg(i + 1).arg(screens[i]->name()));
  }

  settingsGrid->addWidget(layoutLabel, 0, 0);
  settingsGrid->addWidget(projectionLayoutCombo, 0, 1);
  settingsGrid->addWidget(layerLabel, 1, 0);
  settingsGrid->addWidget(targetLayerCombo, 1, 1);
  settingsGrid->addWidget(screenLabel, 2, 0);
  settingsGrid->addWidget(screenSelectorCombo, 2, 1);
  settingsGrid->setColumnStretch(1, 1);

  cLayout->addLayout(settingsGrid);
  layout->addWidget(controlsGroup);

  // --- Collapsible toggle button style ---
  QString toggleBtnStyle =
      "QPushButton { background: #1e293b; color: #38bdf8; border: 1px solid "
      "#334155; "
      "  border-radius: 6px; padding: 8px 12px; font-weight: bold; font-size: "
      "12px; text-align: left; } "
      "QPushButton:hover { background: #334155; }";

  // 3. Themes (Collapsible — starts collapsed)
  auto *themesToggleBtn = new QPushButton("▶ THEMES");
  themesToggleBtn->setStyleSheet(toggleBtnStyle);
  layout->addWidget(themesToggleBtn);

  videoThemesGroup = new QGroupBox();
  videoThemesGroup->setTitle(
      ""); // No title since the toggle button acts as header
  videoThemesLayout = new QGridLayout(videoThemesGroup);
  videoThemesLayout->setSpacing(10);

  auto *createThemeBtn = new QPushButton("+ New Theme");
  connect(createThemeBtn, &QPushButton::clicked, this,
          &ControlWindow::createNewTheme);
  videoThemesLayout->addWidget(createThemeBtn, 0, 0, 1, 2); // Span 2 columns

  // Toggle visibility on click
  connect(themesToggleBtn, &QPushButton::clicked, [themesToggleBtn, this]() {
    bool visible = !videoThemesGroup->isVisible();
    videoThemesGroup->setVisible(visible);
    themesToggleBtn->setText(visible ? "▼ THEMES" : "▶ THEMES");
  });

  videoThemesGroup->setVisible(false); // Start collapsed
  layout->addWidget(videoThemesGroup);

  // 4. Text Formatting (Collapsible)
  auto *formatToggleBtn = new QPushButton("▶ TEXT FORMATTING");
  formatToggleBtn->setStyleSheet(toggleBtnStyle);
  layout->addWidget(formatToggleBtn);

  auto *formatGroup = new QGroupBox();
  formatGroup->setTitle("");
  auto *fmtLayout = new QGridLayout(formatGroup);

  fmtLayout->addWidget(new QLabel("Font:"), 0, 0);
  fontCombo = new QFontComboBox();
  fontCombo->setCurrentFont(QFont("Times New Roman")); // Default
  fmtLayout->addWidget(fontCombo, 0, 1, 1, 3);

  fmtLayout->addWidget(new QLabel("Size (0=Auto):"), 1, 0);
  fontSizeSpin = new QSpinBox();
  fontSizeSpin->setRange(0, 200);
  fontSizeSpin->setValue(0);
  fmtLayout->addWidget(fontSizeSpin, 1, 1);

  fmtLayout->addWidget(new QLabel("Margin:"), 1, 2);
  marginSpin = new QSpinBox();
  marginSpin->setRange(0, 500);
  marginSpin->setValue(40);
  fmtLayout->addWidget(marginSpin, 1, 3);

  fmtLayout->addWidget(new QLabel("Align:"), 2, 0);
  alignmentCombo = new QComboBox();
  alignmentCombo->addItem("Left", (int)Qt::AlignLeft);
  alignmentCombo->addItem("Center", (int)Qt::AlignCenter);
  alignmentCombo->addItem("Right", (int)Qt::AlignRight);
  alignmentCombo->setCurrentIndex(1); // Center default
  fmtLayout->addWidget(alignmentCombo, 2, 1, 1, 3);

  scrollCheckBox = new QCheckBox("Scrolling (Vertical)");
  fmtLayout->addWidget(scrollCheckBox, 3, 0, 1, 4);

  // Start collapsed
  formatGroup->setVisible(false);

  connect(formatToggleBtn, &QPushButton::clicked,
          [formatToggleBtn, formatGroup]() {
            bool visible = !formatGroup->isVisible();
            formatGroup->setVisible(visible);
            formatToggleBtn->setText(visible ? "▼ TEXT FORMATTING"
                                             : "▶ TEXT FORMATTING");
          });

  layout->addWidget(formatGroup);

  // Connect Formatting Signals
  auto applyFmt = [this]() { updateFormatting(); };
  connect(fontCombo, &QFontComboBox::currentFontChanged, applyFmt);
  connect(fontSizeSpin, &QSpinBox::valueChanged, applyFmt);
  connect(marginSpin, &QSpinBox::valueChanged, applyFmt);
  connect(alignmentCombo, &QComboBox::currentIndexChanged, applyFmt);
  connect(scrollCheckBox, &QCheckBox::toggled, applyFmt);

  // Finalize scroll area
  scrollArea->setWidget(scrollContent);
  outerLayout->addWidget(scrollArea);

  // Initial Load Use Default Layer 0
  loadLayerSettings(0);
}

void ControlWindow::loadLayerSettings(int layerIdx) {
  if (!projection)
    return;

  auto fmt = projection->getLayerFormatting(layerIdx);

  // Block signals to prevent triggering updateFormatting loop
  fontCombo->blockSignals(true);
  fontSizeSpin->blockSignals(true);
  marginSpin->blockSignals(true);
  alignmentCombo->blockSignals(true);
  scrollCheckBox->blockSignals(true);

  // Apply values
  fontCombo->setCurrentFont(QFont(fmt.fontFamily));
  fontSizeSpin->setValue(fmt.fontSize);
  marginSpin->setValue(fmt.margin);

  // Find alignment index
  int alignIdx = alignmentCombo->findData(fmt.alignment);
  if (alignIdx != -1)
    alignmentCombo->setCurrentIndex(alignIdx);

  scrollCheckBox->setChecked(fmt.isScrolling);

  // Unblock
  fontCombo->blockSignals(false);
  fontSizeSpin->blockSignals(false);
  marginSpin->blockSignals(false);
  alignmentCombo->blockSignals(false);
  scrollCheckBox->blockSignals(false);
}

void ControlWindow::updateFormatting() {
  Projection::TextFormatting fmt;
  fmt.fontFamily = fontCombo->currentFont().family();
  fmt.fontSize = fontSizeSpin->value();
  fmt.margin = marginSpin->value();
  fmt.alignment = alignmentCombo->currentData().toInt();
  fmt.isScrolling = scrollCheckBox->isChecked();

  // Apply to current target layer
  projection->setLayerFormatting(currentTargetLayer, fmt);

  if (preview) {
    preview->setLayerFormatting(currentTargetLayer, fmt);
  }
}

// --- Logic ---

void ControlWindow::setGlobalBibleVersion(const QString &version) {
  if (currentBibleVersion == version)
    return;

  currentBibleVersion = version;

  // Sync button group in Bible tab
  if (bibleVersionButtons) {
    for (auto *btn : bibleVersionButtons->buttons()) {
      auto *pushBtn = qobject_cast<QPushButton *>(btn);
      if (pushBtn && pushBtn->text() == version) {
        pushBtn->setChecked(true);
        break;
      }
    }
  }

  // Sync Notes Widget
  if (notesWidget) {
    notesWidget->setCurrentVersion(version);
  }

  // Reload current Bible view if applicable
  if (!currentBibleBook.isEmpty() && currentBibleChapter > 0) {
    onChapterSelected(currentBibleChapter);
  }

  emit bibleVersionChanged(version);
}

// Obsolete methods removed

void ControlWindow::onBibleVerseSelected(QListWidgetItem *item) {
  if (!item)
    return;
  QString text = item->data(Qt::UserRole).toString();
  QString ref = item->data(Qt::UserRole + 1).toString();

  QString fullText;
  if (!ref.isEmpty()) {
    fullText = QString("%1\n\n%2").arg(text).arg(ref);
  } else {
    fullText = text;
  }
  lastProjectedText = fullText;
  projectBibleVerse(fullText);
}
void ControlWindow::onQuickSearch() {
  QString query = bibleQuickSearch->text().trimmed();
  if (query.isEmpty())
    return;

  QString version =
      currentBibleVersion.isEmpty() ? "NKJV" : currentBibleVersion;
  auto results = BibleManager::instance().search(query, version);
  if (results.empty())
    return;

  bibleVerseList->clear();
  for (const auto &v : results) {
    QListWidgetItem *item = new QListWidgetItem();

    // Get localized book name for display
    QString displayBook =
        BibleManager::instance().getLocalizedBookName(v.book, v.version);

    auto *widget =
        new VerseWidget(displayBook, v.chapter, v.verse, v.text, v.version);

    item->setData(Qt::UserRole, v.text);
    QString ref = QString("%1 %2:%3 (%4)")
                      .arg(displayBook)
                      .arg(v.chapter)
                      .arg(v.verse)
                      .arg(v.version);
    item->setData(Qt::UserRole + 1, ref);

    connect(widget, &VerseWidget::versionChanged,
            [item, v](const QString &newVer, const QString &newText) {
              item->setData(Qt::UserRole, newText);
              QString newDisplayBook =
                  BibleManager::instance().getLocalizedBookName(v.book, newVer);
              QString newRef = QString("%1 %2:%3 (%4)")
                                   .arg(newDisplayBook) // FIX: Localize
                                   .arg(v.chapter)
                                   .arg(v.verse)
                                   .arg(newVer);
              item->setData(Qt::UserRole + 1, newRef);
            });

    connect(widget, &VerseWidget::verseClicked, [this, item]() {
      bibleVerseList->setCurrentItem(item);
      onBibleVerseSelected(item);
    });

    item->setSizeHint(widget->sizeHint());
    bibleVerseList->addItem(item);
    bibleVerseList->setItemWidget(item, widget);
  }
}

// ... Song Logic (adapted) ...

void ControlWindow::updateSongList() {
  songList->clear();
  for (const auto &song : songManager->getSongs()) {
    songList->addItem(song.title);
  }
}

void ControlWindow::onSongSelected(int index) {
  if (index < 0 || index >= (int)songManager->getSongs().size())
    return;
  currentSongIndex = index;
  const auto &song = songManager->getSongs()[index];

  titleEdit->setText(song.title);
  artistEdit->setText(song.artist);
  lyricsEdit->setText(song.verses.join("\n\n"));

  verseList->clear();
  verseList->addItems(song.verses);

  // Auto-switch to Songs tab if clicking on sidebar?
  mainTabWidget->setCurrentIndex(1);
}

void ControlWindow::createNewSong() {
  bool ok;
  QString title = QInputDialog::getText(
      this, "New Song", "Enter song title:", QLineEdit::Normal, "", &ok);
  if (ok && !title.isEmpty()) {
    Song s;
    s.title = title;
    songManager->addSong(s);
    updateSongList();
    songList->setCurrentRow(songList->count() - 1);
  }
}

void ControlWindow::editLyrics() {
  // Just select the song and focus tab
  mainTabWidget->setCurrentIndex(1);
  // Focus lyrics edit?
}

void ControlWindow::saveSong() {
  if (currentSongIndex < 0)
    return;
  Song s;
  s.title = titleEdit->text();
  s.artist = artistEdit->text();
  s.verses = lyricsEdit->toPlainText().split("\n\n", Qt::SkipEmptyParts);
  songManager->updateSong(currentSongIndex, s);
  updateSongList();
  onSongSelected(currentSongIndex); // Refresh lists
}

void ControlWindow::deleteSong() {
  if (currentSongIndex < 0)
    return;
  auto reply = QMessageBox::question(this, "Delete", "Delete this song?",
                                     QMessageBox::Yes | QMessageBox::No);
  if (reply == QMessageBox::Yes) {
    songManager->removeSong(currentSongIndex);
    updateSongList();
    verseList->clear();
    titleEdit->clear();
    artistEdit->clear();
    lyricsEdit->clear();
    currentSongIndex = -1;
  }
}

// ... Projection Logic ...

void ControlWindow::projectVerse(int index) {
  if (index < 0 || index >= verseList->count())
    return;
  currentVerseIndex = index;
  QString text = verseList->item(index)->text();
  lastProjectedText = text;

  if (!isTextVisible || isScreenBlackened) {
    if (projection)
      projection->setLayerText(currentTargetLayer, "");
    if (preview)
      preview->setLayerText(currentTargetLayer, "");
  } else {

    if (projection && isPresenting)
      projection->setLayerText(currentTargetLayer, text);
    if (preview)
      preview->setLayerText(currentTargetLayer, text);
  }
}

void ControlWindow::projectBibleVerse(const QString &text) {
  if (!isTextVisible || isScreenBlackened) {
    if (projection)
      projection->setLayerText(currentTargetLayer, "");
    if (preview)
      preview->setLayerText(currentTargetLayer, "");
  } else {

    if (projection && isPresenting)
      projection->setLayerText(currentTargetLayer, text);
    if (preview)
      preview->setLayerText(currentTargetLayer, text);
  }
}

void ControlWindow::nextVerse() {
  // Decide if we are in Song or Bible mode?
  // Simply check active tab
  if (mainTabWidget->currentIndex() == 1) { // Songs
    if (currentSongIndex >= 0 && currentVerseIndex < verseList->count() - 1) {
      verseList->setCurrentRow(currentVerseIndex + 1);
    }
  } else if (mainTabWidget->currentIndex() == 0) { // Bible
    // Logic for next bible verse?
    int row = bibleVerseList->currentRow();
    if (row < bibleVerseList->count() - 1) {
      bibleVerseList->setCurrentRow(row + 1);
      onBibleVerseSelected(bibleVerseList->currentItem());
    }
  }
}

void ControlWindow::prevVerse() {
  if (mainTabWidget->currentIndex() == 1) { // Songs
    if (currentVerseIndex > 0) {
      verseList->setCurrentRow(currentVerseIndex - 1);
    }
  } else if (mainTabWidget->currentIndex() == 0) { // Bible
    int row = bibleVerseList->currentRow();
    if (row > 0) {
      bibleVerseList->setCurrentRow(row - 1);
      onBibleVerseSelected(bibleVerseList->currentItem());
    }
  }
}

void ControlWindow::onClearTextClicked() {
  isTextVisible = !isTextVisible;
  clearTextBtn->setText(isTextVisible ? "✕  Clear Text" : "👁  Show Text");
  clearTextBtn->setStyleSheet(
      isTextVisible
          ? "QPushButton { background: #334155; color: #fbbf24; "
            "border: 1px solid #475569; border-radius: 6px; "
            "padding: 7px 12px; font-weight: bold; font-size: 11px; } "
            "QPushButton:hover { background: #475569; border-color: #fbbf24; }"
          : "QPushButton { background: #fbbf24; color: #0f172a; "
            "border: 1px solid #fbbf24; border-radius: 6px; "
            "padding: 7px 12px; font-weight: bold; font-size: 11px; } "
            "QPushButton:hover { background: #f59e0b; }");

  if (!isTextVisible) {
    if (projection && isPresenting) {
      projection->setLayerText(0, "");
      projection->setLayerText(1, "");
    }
    if (preview) {
      preview->setLayerText(0, "");
      preview->setLayerText(1, "");
    }
  } else {
    // Restore last projected content
    if (!lastProjectedText.isEmpty()) {
      if (projection && isPresenting)
        projection->setLayerText(currentTargetLayer, lastProjectedText);
      if (preview)
        preview->setLayerText(currentTargetLayer, lastProjectedText);
    }
  }
}

void ControlWindow::onBlackOutClicked() {
  isScreenBlackened = !isScreenBlackened;
  blackOutBtn->setText(isScreenBlackened ? "■  Un-Black" : "■  Black Out");
  blackOutBtn->setStyleSheet(
      isScreenBlackened
          ? "QPushButton { background: #ef4444; color: white; "
            "border: 1px solid #ef4444; border-radius: 6px; "
            "padding: 7px 12px; font-weight: bold; font-size: 11px; } "
            "QPushButton:hover { background: #dc2626; }"
          : "QPushButton { background: #334155; color: #f87171; "
            "border: 1px solid #475569; border-radius: 6px; "
            "padding: 7px 12px; font-weight: bold; font-size: 11px; } "
            "QPushButton:hover { background: #475569; border-color: #f87171; "
            "}");

  if (isScreenBlackened) {
    if (projection && isPresenting) {
      projection->clearLayer(0);
      projection->clearLayer(1);
    }
    if (preview)
      preview->clear();
  } else {
    // Restore last projected content
    if (!lastProjectedText.isEmpty()) {
      if (projection && isPresenting)
        projection->setLayerText(currentTargetLayer, lastProjectedText);
      if (preview)
        preview->setLayerText(currentTargetLayer, lastProjectedText);
    }
  }
}

void ControlWindow::clearAll() {
  if (projection) {
    projection->clearLayer(0);
    projection->clearLayer(1);
  }
  if (preview)
    preview->clear();
  isTextVisible = true;
  isScreenBlackened = false;
  lastProjectedText.clear();
  clearTextBtn->setText("✕  Clear Text");
  clearTextBtn->setStyleSheet(
      "QPushButton { background: #334155; color: #fbbf24; "
      "border: 1px solid #475569; border-radius: 6px; "
      "padding: 7px 12px; font-weight: bold; font-size: 11px; } "
      "QPushButton:hover { background: #475569; border-color: #fbbf24; }");
  blackOutBtn->setText("■  Black Out");
  blackOutBtn->setStyleSheet(
      "QPushButton { background: #334155; color: #f87171; "
      "border: 1px solid #475569; border-radius: 6px; "
      "padding: 7px 12px; font-weight: bold; font-size: 11px; } "
      "QPushButton:hover { background: #475569; border-color: #f87171; }");
  applyTheme("Glassmorphism 3.0");
}

void ControlWindow::togglePresentation() {
  if (!projection)
    return;
  isPresenting = !isPresenting;

  if (isPresenting) {
    presentBtn->setText("◼  STOP PRESENTATION");
    presentBtn->setStyleSheet(
        "QPushButton { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, "
        "  stop:0 #ef4444, stop:1 #dc2626); color: white; "
        "  border: none; border-radius: 8px; padding: 10px 20px; "
        "  font-weight: bold; font-size: 13px; } "
        "QPushButton:hover { background: qlineargradient(x1:0, y1:0, x2:1, "
        "y2:0, "
        "  stop:0 #dc2626, stop:1 #b91c1c); } "
        "QPushButton:pressed { background: #b91c1c; }");
    liveStatusLabel->setText("● LIVE");
    liveStatusLabel->setStyleSheet(
        "color: #22c55e; font-weight: bold; font-size: 11px; "
        "background: rgba(34,197,94,0.1); border: 1px solid #22c55e; "
        "border-radius: 14px; padding: 6px 14px;");

    // Screen Detection: use screen selector or auto-detect
    QList<QScreen *> screens = QGuiApplication::screens();
    QScreen *targetScreen = qApp->primaryScreen();

    if (screenSelectorCombo && screenSelectorCombo->currentIndex() > 0) {
      int idx = screenSelectorCombo->currentIndex() - 1; // 0 = "Auto"
      if (idx >= 0 && idx < screens.size()) {
        targetScreen = screens[idx];
      }
    } else if (screens.size() > 1) {
      // Auto: prefer screen different from control window
      QScreen *controlScreen = this->screen();
      for (auto *s : screens) {
        if (s != controlScreen) {
          targetScreen = s;
          break;
        }
      }
    }

    projection->windowHandle()->setScreen(targetScreen);
    projection->setGeometry(targetScreen->geometry());
    projection->showFullScreen();
    projection->raise();
    projection->activateWindow();

    // Sync state to projection since we block updates when offline
    if (isScreenBlackened) {
      projection->clearLayer(0);
      projection->clearLayer(1);
    } else if (!isTextVisible) {
      projection->setLayerText(0, "");
      projection->setLayerText(1, "");
    } else if (!lastProjectedText.isEmpty()) {
      projection->setLayerText(currentTargetLayer, lastProjectedText);
    }

  } else {
    presentBtn->setText("▶  START PRESENTATION");
    presentBtn->setStyleSheet(
        "QPushButton { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, "
        "  stop:0 #22c55e, stop:1 #16a34a); color: white; "
        "  border: none; border-radius: 8px; padding: 10px 20px; "
        "  font-weight: bold; font-size: 13px; } "
        "QPushButton:hover { background: qlineargradient(x1:0, y1:0, x2:1, "
        "y2:0, "
        "  stop:0 #16a34a, stop:1 #15803d); } "
        "QPushButton:pressed { background: #15803d; }");
    liveStatusLabel->setText("● OFFLINE");
    liveStatusLabel->setStyleSheet(
        "color: #64748b; font-weight: bold; font-size: 11px; "
        "background: rgba(15,23,42,0.6); border: 1px solid #334155; "
        "border-radius: 14px; padding: 6px 14px;");
    projection->hide();
  }
}

// --- Themes ---

void ControlWindow::createNewTheme() {
  ThemeEditorDialog dialog(this);
  if (dialog.exec() == QDialog::Accepted) {
    QString name = dialog.getName();
    ThemeType type = dialog.getType();
    QString path = dialog.getContentPath();
    QColor color = dialog.getColor();

    if (!name.isEmpty()) {
      themeManager->addTemplate(name, type, path, color);
    }
  }
}

void ControlWindow::updateThemeTab() {
  // Clear layout
  QLayoutItem *child;
  while ((child = videoThemesLayout->takeAt(1)) !=
         nullptr) { // Keep "New Theme" button at index 0
    if (child->widget()) {
      // Stop any media players before deleting
      // With ThemePreviewCard, its destructor handles this.
      delete child->widget();
    }
    delete child;
  }

  // Leave the first button (New Theme)

  int index = 0;
  for (const auto &t : themeManager->getTemplates()) {
    auto *card = new ThemePreviewCard(t, index, this);

    // Callbacks
    card->m_applyCallback = [this](const ThemeTemplate &tm) {
      applyTheme(tm.name);
      Projection::BackgroundType type;
      if (tm.type == ThemeType::Video)
        type = Projection::BackgroundType::Video;
      else if (tm.type == ThemeType::Image)
        type = Projection::BackgroundType::Image;
      else if (tm.type == ThemeType::Color)
        type = Projection::BackgroundType::Color;
      else
        type = Projection::BackgroundType::None;

      projection->setLayerBackground(currentTargetLayer, type, tm.contentPath,
                                     tm.color);
      preview->setLayerBackground(currentTargetLayer, type, tm.contentPath,
                                  tm.color);
    };

    card->m_deleteCallback = [this](int idx) {
      QMessageBox::StandardButton reply;
      reply = QMessageBox::question(
          this, "Delete Theme", "Are you sure you want to delete this theme?",
          QMessageBox::Yes | QMessageBox::No);
      if (reply == QMessageBox::Yes) {
        themeManager->removeTemplate(idx);
        updateThemeTab();
      }
    };

    int row = (index / 2) + 1;
    int col = index % 2;
    videoThemesLayout->addWidget(card, row, col);
    index++;
  }
}

void ControlWindow::applyTheme(const QString &themeName) {
  if (themeName == "Glassmorphism 3.0") {
    projection->setLayerBackground(
        currentTargetLayer, Projection::BackgroundType::Color, "", Qt::black);
    preview->setLayerBackground(
        currentTargetLayer, Projection::BackgroundType::Color, "", Qt::black);
  }
}

void ControlWindow::selectImage() {
  QString path = QFileDialog::getOpenFileName(
      this, "Select Image", QDir::homePath(),
      "Images (*.png *.jpg *.jpeg *.bmp *.gif);;All Files (*.*)", nullptr,
      QFileDialog::DontUseNativeDialog);
  if (!path.isEmpty()) {
    projection->setLayerBackground(currentTargetLayer,
                                   Projection::BackgroundType::Image, path);
    preview->setLayerBackground(currentTargetLayer,
                                Projection::BackgroundType::Image, path);
  }
}

void ControlWindow::selectVideo() {
  QString path = QFileDialog::getOpenFileName(
      this, "Select Video", QDir::homePath(),
      "Videos (*.mp4 *.mov *.avi *.mkv *.webm);;All Files (*.*)", nullptr,
      QFileDialog::DontUseNativeDialog);
  if (!path.isEmpty()) {
    projection->setLayerBackground(currentTargetLayer,
                                   Projection::BackgroundType::Video, path);
    preview->setLayerBackground(currentTargetLayer,
                                Projection::BackgroundType::Video, path);
  }
}

void ControlWindow::selectColor() {
  // ...
}

void ControlWindow::saveCurrentVideoAsTemplate() {
  // Deprecated
}

void ControlWindow::onTabChanged(int index) {
  // Maybe hide sidebar if needed?
  // User wants adjustable panes. Sidebar can stay.
}

void ControlWindow::onMediaError(const QString &message) {
  liveStatusLabel->setText("MEDIA ERROR");
  liveStatusLabel->setStyleSheet("color: red; font-weight: bold;");
  QMessageBox::warning(this, "Media Error", message);
}

void ControlWindow::onNotesProject(const QString &text) {
  projectBibleVerse(text); // Reuse
}

void ControlWindow::refreshBibleVersions() {
  if (!bibleVersionButtons || !bibleVersionLayout)
    return;

  // Clear existing buttons
  QList<QAbstractButton *> buttons = bibleVersionButtons->buttons();
  for (auto *btn : buttons) {
    bibleVersionButtons->removeButton(btn);
    bibleVersionLayout->removeWidget(btn);
    btn->deleteLater();
  }

  // Remove stretch if any
  if (bibleVersionLayout->count() > 1) {
    QLayoutItem *item =
        bibleVersionLayout->itemAt(bibleVersionLayout->count() - 1);
    if (item->spacerItem()) {
      delete bibleVersionLayout->takeAt(bibleVersionLayout->count() - 1);
    }
  }

  // Reload versions
  QStringList versions = BibleManager::instance().getVersions();
  if (currentBibleVersion.isEmpty())
    currentBibleVersion = "NKJV";

  for (const QString &ver : versions) {
    auto *btn = new QPushButton(ver);
    btn->setCheckable(true);
    btn->setAutoExclusive(true);
    btn->setMinimumWidth(60);
    btn->setFixedHeight(32);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setStyleSheet(
        "QPushButton { background: #334155; color: #94a3b8; "
        "border: 1px solid #475569; border-radius: 4px; font-size: 11px; "
        "font-weight: bold; padding: 0 14px; } "
        "QPushButton:hover { background: #475569; color: white; } "
        "QPushButton:checked { background: #38bdf8; color: white; "
        "border-color: #38bdf8; }");

    if (ver == currentBibleVersion) {
      btn->setChecked(true);
    }

    connect(btn, &QPushButton::clicked,
            [this, ver]() { setGlobalBibleVersion(ver); });

    bibleVersionButtons->addButton(btn);
    bibleVersionLayout->addWidget(btn);
  }

  bibleVersionLayout->addStretch();
}

void ControlWindow::setupKeyboardShortcuts() {
  // Space / Right Arrow → Next verse
  auto *nextShortcut = new QShortcut(QKeySequence(Qt::Key_Space), this);
  connect(nextShortcut, &QShortcut::activated, this, &ControlWindow::nextVerse);
  auto *nextArrow = new QShortcut(QKeySequence(Qt::Key_Right), this);
  connect(nextArrow, &QShortcut::activated, this, &ControlWindow::nextVerse);

  // Left Arrow / B → Previous verse
  auto *prevShortcut = new QShortcut(QKeySequence(Qt::Key_Left), this);
  connect(prevShortcut, &QShortcut::activated, this, &ControlWindow::prevVerse);
  auto *prevB = new QShortcut(QKeySequence(Qt::Key_B), this);
  connect(prevB, &QShortcut::activated, this, &ControlWindow::prevVerse);

  // Escape → Clear text
  auto *escShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
  connect(escShortcut, &QShortcut::activated, this,
          &ControlWindow::onClearTextClicked);

  // F5 → Toggle presentation
  auto *f5Shortcut = new QShortcut(QKeySequence(Qt::Key_F5), this);
  connect(f5Shortcut, &QShortcut::activated, this,
          &ControlWindow::togglePresentation);
}

void ControlWindow::closeEvent(QCloseEvent *event) {
  // If projection is visible, just hide the dashboard
  if (projection && projection->isVisible()) {
    hide();
    event->ignore();
  } else {
    event->accept();
  }
}

// --- Media Logic ---

void ControlWindow::setupMediaTab(QWidget *container) {
  auto *layout = new QVBoxLayout(container);

  // 1. File Controls
  auto *fileControls = new QHBoxLayout();
  auto *addBtn = new QPushButton("+ Add File");
  auto *removeBtn = new QPushButton("- Remove");

  connect(addBtn, &QPushButton::clicked, this, &ControlWindow::addMediaFile);
  connect(removeBtn, &QPushButton::clicked, this,
          &ControlWindow::removeMediaFile);

  fileControls->addWidget(addBtn);
  fileControls->addWidget(removeBtn);
  fileControls->addStretch();
  layout->addLayout(fileControls);

  // 2. Splitter: File List | Page Preview
  auto *splitter = new QSplitter(Qt::Horizontal);
  layout->addWidget(splitter);

  // Left: File List
  mediaFileList = new QListWidget();
  mediaFileList->setUniformItemSizes(true);
  connect(mediaFileList, &QListWidget::currentItemChanged, this,
          &ControlWindow::onMediaFileSelected);
  splitter->addWidget(mediaFileList);

  // Right: Page List (Grid or List)
  mediaPageList = new QListWidget();
  mediaPageList->setViewMode(QListWidget::IconMode);
  mediaPageList->setIconSize(QSize(150, 200));
  mediaPageList->setResizeMode(QListWidget::Adjust);
  mediaPageList->setSpacing(10);
  connect(mediaPageList, &QListWidget::itemClicked, this,
          &ControlWindow::onMediaPageSelected);
  splitter->addWidget(mediaPageList);

  splitter->setStretchFactor(0, 1);
  splitter->setStretchFactor(1, 4);

  // Load persisted media
  loadMedia();
}

void ControlWindow::loadMedia() {
  QSettings settings("ChurchProjection", "Media");
  int size = settings.beginReadArray("Files");
  for (int i = 0; i < size; ++i) {
    settings.setArrayIndex(i);
    QString path = settings.value("path").toString();

    // Verify existence
    if (QFile::exists(path)) {
      MediaItem item;
      item.path = path;
      QFileInfo fi(path);
      QString ext = fi.suffix().toLower();

      if (ext == "pdf") {
        item.type = Projection::Content::MediaType::Pdf;
        item.pageCount = PdfRenderer::pageCount(path);
        if (item.pageCount <= 0)
          continue; // Skip invalid
      } else {
        item.type = Projection::Content::MediaType::Image;
        item.pageCount = 1;
      }

      mediaItems.push_back(item);

      QString label = fi.fileName();
      if (item.type == Projection::Content::MediaType::Pdf) {
        label =
            QString("📄 %1 (%2 pages)").arg(fi.fileName()).arg(item.pageCount);
      } else {
        label = QString("🖼 %1").arg(fi.fileName());
      }
      auto *newItem = new QListWidgetItem(label);
      newItem->setToolTip(path);
      mediaFileList->addItem(newItem);
    }
  }
  settings.endArray();
}

void ControlWindow::saveMedia() {
  QSettings settings("ChurchProjection", "Media");
  settings.beginWriteArray("Files");
  for (int i = 0; i < (int)mediaItems.size(); ++i) {
    settings.setArrayIndex(i);
    settings.setValue("path", mediaItems[i].path);
  }
  settings.endArray();
}

void ControlWindow::addMediaFile() {
  QString path = QFileDialog::getOpenFileName(
      this, "Open Media", QDir::homePath(),
      "All Supported (*.png *.jpg *.jpeg *.bmp *.gif *.pdf);;"
      "Images (*.png *.jpg *.jpeg *.bmp *.gif);;"
      "PDF Documents (*.pdf)",
      nullptr, QFileDialog::DontUseNativeDialog);
  if (path.isEmpty())
    return;

  QFileInfo fi(path);
  QString ext = fi.suffix().toLower();

  MediaItem item;
  item.path = path;

  if (ext == "pdf") {
    // PDF file
    int pages = PdfRenderer::pageCount(path);
    if (pages <= 0) {
      QMessageBox::warning(
          this, "Error",
          "Failed to load PDF. File may be corrupt or unsupported.");
      return;
    }
    item.type = Projection::Content::MediaType::Pdf;
    item.pageCount = pages;
  } else {
    // Image file
    QImageReader reader(path);
    if (!reader.canRead()) {
      QMessageBox::warning(
          this, "Error",
          "Failed to load image. Format not supported or file is corrupt.");
      return;
    }
    item.type = Projection::Content::MediaType::Image;
    item.pageCount = 1;
  }

  // Copy to assets/media
  QString assetsDir =
      QString("%1/assets/media").arg(CHURCH_PROJECTION_SOURCE_DIR);
  QDir dir(assetsDir);
  if (!dir.exists())
    dir.mkpath(".");

  QString destPath = dir.filePath(fi.fileName());

  // If explicitly not adding the file that is already there
  if (path != destPath) {
    // Handle overwrite? Auto-rename? For now, overwrite or assume unique.
    // Let's copy.
    if (QFile::exists(destPath))
      QFile::remove(destPath);
    QFile::copy(path, destPath);
  }

  // Use the destination path for the item
  item.path = destPath;

  mediaItems.push_back(item);

  // Add to list with type indicator
  QString label = fi.fileName();
  if (item.type == Projection::Content::MediaType::Pdf) {
    label = QString("📄 %1 (%2 pages)").arg(fi.fileName()).arg(item.pageCount);
  } else {
    label = QString("🖼 %1").arg(fi.fileName());
  }
  auto *newItem = new QListWidgetItem(label);
  newItem->setToolTip(destPath);
  mediaFileList->addItem(newItem);

  // Robust selection
  mediaFileList->setCurrentItem(newItem);
  mediaFileList->setFocus();

  saveMedia();
}

void ControlWindow::removeMediaFile() {
  int row = mediaFileList->currentRow();
  if (row >= 0 && row < (int)mediaItems.size()) {
    QString path = mediaItems[row].path;

    // Check if file is in assets/media and delete it
    // Only delete if it is actually in our managed folder
    if (path.contains("/assets/media/")) {
      QMessageBox::StandardButton reply;
      reply = QMessageBox::question(
          this, "Delete File",
          "Execute permanent deletion from disk?\nFile: " + path,
          QMessageBox::Yes | QMessageBox::No);
      if (reply == QMessageBox::Yes) {
        QFile::remove(path);
      }
    }

    mediaItems.erase(mediaItems.begin() + row);
    delete mediaFileList->takeItem(row);
    mediaPageList->clear();
    currentMediaIndex = -1;
    saveMedia();
  }
}

void ControlWindow::onMediaFileSelected(QListWidgetItem *item) {
  int row = mediaFileList->row(item);
  if (row < 0 || row >= (int)mediaItems.size())
    return;

  currentMediaIndex = row;
  mediaPageList->clear();

  MediaItem &m = mediaItems[row];

  if (m.type == Projection::Content::MediaType::Pdf) {
    // Render PDF page thumbnails
    for (int i = 0; i < m.pageCount; ++i) {
      QImage img = PdfRenderer::renderThumbnail(m.path, i, QSize(300, 400));
      if (img.isNull())
        continue;

      QListWidgetItem *pItem = new QListWidgetItem();
      pItem->setIcon(QPixmap::fromImage(img));
      pItem->setText(QString("Page %1").arg(i + 1));
      pItem->setData(Qt::UserRole, i); // Page index
      mediaPageList->addItem(pItem);
    }
  } else {
    // Image
    QImage img(m.path);
    if (!img.isNull()) {
      QListWidgetItem *pItem = new QListWidgetItem();
      pItem->setIcon(
          QPixmap::fromImage(img.scaled(300, 300, Qt::KeepAspectRatio)));
      pItem->setText("Image");
      pItem->setData(Qt::UserRole, 0);
      mediaPageList->addItem(pItem);
    }
  }
}

void ControlWindow::onMediaPageSelected(QListWidgetItem *item) {
  if (currentMediaIndex < 0 || !item)
    return;

  int page = item->data(Qt::UserRole).toInt();
  const MediaItem &m = mediaItems[currentMediaIndex];

  QImage rendered;

  if (m.type == Projection::Content::MediaType::Pdf) {
    // Render high-res PDF page for projection
    rendered = PdfRenderer::renderPage(m.path, page, QSize(1920, 1080));
  } else {
    rendered = QImage(m.path);
  }

  // Project
  // Check live state
  if (!isTextVisible || isScreenBlackened) {
    // Do not project if offline/blackened, but allow preview update
    if (preview)
      preview->setLayerMedia(currentTargetLayer, m.type, m.path, page,
                             rendered);
  } else {
    if (projection && isPresenting)
      projection->setLayerMedia(currentTargetLayer, m.type, m.path, page,
                                rendered);
    if (preview)
      preview->setLayerMedia(currentTargetLayer, m.type, m.path, page,
                             rendered);
  }
}
