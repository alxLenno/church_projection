#include "ControlWindow.h"
#include "../core/BibleManager.h"
#include "ThemeEditorDialog.h"
#include <QApplication>
#include <QButtonGroup>
#include <QFileDialog>
#include <QInputDialog>
#include <QListWidget>
#include <QMessageBox>
#include <QScreen>
#include <QStandardItemModel>
#include <QStyle>

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
  resize(1400, 900); // Slightly larger for 3-pane layout

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

  // Set Initial Sizes (20% | 50% | 30%)
  mainSplitter->setStretchFactor(0, 1);
  mainSplitter->setStretchFactor(1, 3);
  mainSplitter->setStretchFactor(2, 2);

  setCentralWidget(centralWidget);

  // Initialize Data
  updateSongList();
  // populateBibleBooks(); // Removed
  applyTheme("Glassmorphism 3.0"); // Default

  // Connect Signals
  connect(projection, &ProjectionWindow::mediaError, this,
          &ControlWindow::onMediaError);
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
}

void ControlWindow::setupSidebar(QWidget *container) {
  auto *layout = new QVBoxLayout(container);
  layout->setContentsMargins(10, 10, 10, 10);

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

  bottomLayout->addWidget(navFooter);

  bibleSplitter->addWidget(bottomWidget);

  // Initial Sizes (50/50)
  bibleSplitter->setStretchFactor(0, 1);
  bibleSplitter->setStretchFactor(1, 1);

  updateBibleNavButtons();
}

// --- Grid Setup Helpers ---

void ControlWindow::setupBookGrid(QWidget *page) {
  auto *layout = new QVBoxLayout(page);

  auto *scroll = new QScrollArea();
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);

  auto *content = new QWidget();
  auto *contentLayout = new QVBoxLayout(content);

  // Style for Grid Buttons
  QString btnStyle =
      "QPushButton { "
      "  background: #334155; color: white; border: none; border-radius: 4px; "
      "  padding: 8px; text-align: left; font-weight: bold; "
      "} "
      "QPushButton:hover { background: #38bdf8; color: #0f172a; }";

  auto createSection = [&](const QString &title, BibleManager::Testament t) {
    auto *group = new QGroupBox(title);
    auto *gl = new QGridLayout(group);
    gl->setSpacing(5);

    int row = 0, col = 0;
    int maxCols = 4; // 4 books per row

    auto books = BibleManager::instance().getCanonicalBooks();
    for (const auto &book : books) {
      if (book.testament == t) {
        auto *btn = new QPushButton(book.name);
        btn->setStyleSheet(btnStyle);
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
    contentLayout->addWidget(group);
  };

  createSection("OLD TESTAMENT", BibleManager::Testament::Old);
  createSection("NEW TESTAMENT", BibleManager::Testament::New);

  scroll->setWidget(content);
  layout->addWidget(scroll);
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
  results = BibleManager::instance().search(query, version);

  for (const auto &v : results) {
    if (v.chapter == currentBibleChapter && v.book == currentBibleBook) {
      QListWidgetItem *item = new QListWidgetItem();
      auto *widget =
          new VerseWidget(v.book, v.chapter, v.verse, v.text, v.version);

      item->setData(Qt::UserRole, v.text);
      QString ref = QString("%1 %2:%3 (%4)")
                        .arg(v.book)
                        .arg(v.chapter)
                        .arg(v.verse)
                        .arg(v.version);
      item->setData(Qt::UserRole + 1, ref);

      connect(widget, &VerseWidget::versionChanged,
              [item, v](const QString &newVer, const QString &newText) {
                item->setData(Qt::UserRole, newText);
                QString newRef = QString("%1 %2:%3 (%4)")
                                     .arg(v.book)
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
  // Scroll Top List to this verse
  for (int i = 0; i < bibleVerseList->count(); ++i) {
    auto *item = bibleVerseList->item(i);
    if (item->data(Qt::UserRole + 1).toInt() == verse) {
      bibleVerseList->setCurrentItem(item);
      bibleVerseList->scrollToItem(item, QAbstractItemView::PositionAtTop);
      onBibleVerseSelected(item); // Trigger projection
      break;
    }
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
  auto *layout = new QVBoxLayout(container);
  layout->setContentsMargins(10, 10, 10, 10);

  // 1. Live Preview
  auto *previewGroup = new QGroupBox("LIVE PREVIEW");
  auto *prevLayout = new QVBoxLayout(previewGroup);
  prevLayout->setContentsMargins(0, 10, 0, 0);
  preview = new ProjectionPreview(this);
  prevLayout->addWidget(preview);
  layout->addWidget(previewGroup, 1); // Expand

  // 2. Master Controls
  auto *controlsGroup = new QGroupBox("STAGE CONTROLS");
  auto *cLayout = new QVBoxLayout(controlsGroup);

  presentBtn = new QPushButton("START PRESENTATION");
  presentBtn->setObjectName("presentBtn");
  connect(presentBtn, &QPushButton::clicked, this,
          &ControlWindow::togglePresentation);

  liveStatusLabel = new QLabel("OFFLINE");
  liveStatusLabel->setAlignment(Qt::AlignCenter);
  liveStatusLabel->setObjectName("statusOffline");

  // Toggles
  auto *togglesLayout = new QHBoxLayout();
  clearTextBtn = new QPushButton("Clear Text");
  blackOutBtn = new QPushButton("Black Out");
  auto *clearAllBtn = new QPushButton("Reset All");

  connect(clearTextBtn, &QPushButton::clicked, this,
          &ControlWindow::onClearTextClicked);
  connect(blackOutBtn, &QPushButton::clicked, this,
          &ControlWindow::onBlackOutClicked);
  connect(clearAllBtn, &QPushButton::clicked, this, &ControlWindow::clearAll);

  togglesLayout->addWidget(clearTextBtn);
  togglesLayout->addWidget(blackOutBtn);
  togglesLayout->addWidget(clearAllBtn);

  // --- Multi-Pane Controls ---
  auto *paneLayout = new QHBoxLayout();
  paneLayout->setSpacing(10);

  projectionLayoutCombo = new QComboBox();
  projectionLayoutCombo->addItem("Full Screen",
                                 (int)Projection::LayoutType::Single);
  projectionLayoutCombo->addItem("Split Vertical",
                                 (int)Projection::LayoutType::SplitVertical);
  projectionLayoutCombo->addItem("Split Horizontal",
                                 (int)Projection::LayoutType::SplitHorizontal);
  connect(projectionLayoutCombo, &QComboBox::currentIndexChanged,
          [this](int index) {
            Projection::LayoutType type =
                (Projection::LayoutType)projectionLayoutCombo->itemData(index)
                    .toInt();
            projection->setLayoutType(type);
            preview->setLayoutType(type);
          });

  targetLayerCombo = new QComboBox();
  targetLayerCombo->addItem("Target: Layer 1", 0);
  targetLayerCombo->addItem("Target: Layer 2", 1);
  connect(targetLayerCombo, &QComboBox::currentIndexChanged, [this](int index) {
    currentTargetLayer = targetLayerCombo->itemData(index).toInt();
  });

  paneLayout->addWidget(new QLabel("LAYOUT:"));
  paneLayout->addWidget(projectionLayoutCombo);
  paneLayout->addWidget(targetLayerCombo);

  cLayout->addLayout(paneLayout);
  cLayout->addWidget(presentBtn);
  cLayout->addWidget(liveStatusLabel);
  cLayout->addLayout(togglesLayout);
  layout->addWidget(controlsGroup);

  // 3. Themes
  videoThemesGroup = new QGroupBox("THEMES");
  videoThemesLayout = new QVBoxLayout(videoThemesGroup);

  auto *createThemeBtn = new QPushButton("+ New Theme");
  connect(createThemeBtn, &QPushButton::clicked, this,
          &ControlWindow::createNewTheme);
  videoThemesLayout->addWidget(createThemeBtn);

  // Scroll area for themes if many? For now just VBox
  // We will populate in updateThemeTab

  layout->addWidget(videoThemesGroup, 1);
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

  // Combine Text and Reference
  if (!ref.isEmpty()) {
    QString fullText = QString("%1\n\n%2").arg(text).arg(ref);
    projectBibleVerse(fullText);
  } else {
    projectBibleVerse(text);
  }
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
    auto *widget =
        new VerseWidget(v.book, v.chapter, v.verse, v.text, v.version);

    item->setData(Qt::UserRole, v.text);
    QString ref = QString("%1 %2:%3 (%4)")
                      .arg(v.book)
                      .arg(v.chapter)
                      .arg(v.verse)
                      .arg(v.version);
    item->setData(Qt::UserRole + 1, ref);

    connect(widget, &VerseWidget::versionChanged,
            [item, v](const QString &newVer, const QString &newText) {
              item->setData(Qt::UserRole, newText);
              QString newRef = QString("%1 %2:%3 (%4)")
                                   .arg(v.book)
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
  if (index < 0)
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
  if (index < 0)
    return;
  currentVerseIndex = index;
  QString text = verseList->item(index)->text();

  if (!isTextVisible || isScreenBlackened) {
    projection->setLayerText(currentTargetLayer, "");
    preview->setLayerText(currentTargetLayer, "");
  } else {
    projection->setLayerText(currentTargetLayer, text);
    preview->setLayerText(currentTargetLayer, text);
  }
}

void ControlWindow::projectBibleVerse(const QString &text) {
  if (!isTextVisible || isScreenBlackened) {
    projection->setLayerText(currentTargetLayer, "");
    preview->setLayerText(currentTargetLayer, "");
  } else {
    projection->setLayerText(currentTargetLayer, text);
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
  clearTextBtn->setText(isTextVisible ? "Clear Text" : "Show Text");
  clearTextBtn->setStyleSheet(
      isTextVisible
          ? ""
          : "background-color: #f59e0b; color: black;"); // Amber warning

  if (!isTextVisible) {
    projection->setLayerText(0, "");
    projection->setLayerText(1, "");
    preview->setLayerText(0, "");
    preview->setLayerText(1, "");
  } else {
    // User must re-click verse to show.
  }
}

void ControlWindow::onBlackOutClicked() {
  isScreenBlackened = !isScreenBlackened;
  blackOutBtn->setText(isScreenBlackened ? "Un-Blackout" : "Black Out");
  blackOutBtn->setStyleSheet(
      isScreenBlackened ? "background-color: red; color: white;" : "");

  if (isScreenBlackened) {
    projection->clearLayer(0);
    projection->clearLayer(1);
    preview->clear();
  }
}

void ControlWindow::clearAll() {
  projection->clearLayer(0);
  projection->clearLayer(1);
  preview->clear();
  isTextVisible = true;
  isScreenBlackened = false;
  clearTextBtn->setText("Clear Text");
  clearTextBtn->setStyleSheet("");
  blackOutBtn->setText("Black Out");
  blackOutBtn->setStyleSheet("");
  applyTheme("Glassmorphism 3.0"); // Reset Layer 0 usually
}

void ControlWindow::togglePresentation() {
  isPresenting = !isPresenting;
  if (isPresenting) {
    projection->show();
    presentBtn->setText("STOP PRESENTATION");
    presentBtn->setStyleSheet("background: #ef4444; color: white;");
    liveStatusLabel->setText("•• LIVE ••");
    liveStatusLabel->setObjectName("statusLive");
  } else {
    projection->hide();
    presentBtn->setText("START PRESENTATION");
    presentBtn->setStyleSheet("");
    liveStatusLabel->setText("OFFLINE");
    liveStatusLabel->setObjectName("statusOffline");
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
    if (child->widget())
      delete child->widget();
    delete child;
  }

  // Leave the first button (New Theme)

  for (const auto &t : themeManager->getTemplates()) {
    auto *btn = new QPushButton(t.name);

    // Style button to show type?
    QString color = "#334155";
    if (t.type == ThemeType::Video)
      color = "#475569";
    else if (t.type == ThemeType::Image)
      color = "#0f172a";
    else if (t.type == ThemeType::Color)
      color = t.color.name();

    // btn->setStyleSheet(QString("background-color: %1; border-left: 4px solid
    // white; text-align: left; padding: 5px;").arg(color));

    connect(btn, &QPushButton::clicked, [this, t]() {
      applyTheme(t.name);
      Projection::BackgroundType type;
      if (t.type == ThemeType::Video)
        type = Projection::BackgroundType::Video;
      else if (t.type == ThemeType::Image)
        type = Projection::BackgroundType::Image;
      else if (t.type == ThemeType::Color)
        type = Projection::BackgroundType::Color;
      else
        type = Projection::BackgroundType::None;

      projection->setLayerBackground(currentTargetLayer, type, t.contentPath,
                                     t.color);
      preview->setLayerBackground(currentTargetLayer, type, t.contentPath,
                                  t.color);
    });

    // Add delete context menu?

    videoThemesLayout->addWidget(btn);
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
  QString path = QFileDialog::getOpenFileName(this, "Select Image", "",
                                              "Images (*.png *.jpg)");
  if (!path.isEmpty()) {
    projection->setLayerBackground(currentTargetLayer,
                                   Projection::BackgroundType::Image, path);
    preview->setLayerBackground(currentTargetLayer,
                                Projection::BackgroundType::Image, path);
  }
}

void ControlWindow::selectVideo() {
  QString path = QFileDialog::getOpenFileName(this, "Select Video", "",
                                              "Videos (*.mp4 *.mov)");
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
