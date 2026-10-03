#include "ControlWindow.h"
#include "../core/BibleManager.h"
#include "../core/ScriptureLookup.h"
#include "ThemeEditorDialog.h"
#include <QAction>
#include <QDialog>
#include <QSignalBlocker>
#include <QTextBlock>
#include <QTextDocument>
#include <QtMath>
#include <QApplication>
#include <QCryptographicHash>
#include <QAudioOutput>
#include <QButtonGroup>
#include <QEvent> // Added for enterEvent/leaveEvent
#include <QFile>
#include <QFormLayout>
#include <QColorDialog>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QInputDialog>
#include <QListWidget>
#include <QMediaPlayer>
#include <QMenu>
#include <QMessageBox>
#include <QJsonObject>
#include <QPointer>
#include <QResizeEvent>
#include <QScreen>
#include <QScrollBar>
#include <QSettings>
#include <QShortcut>
#include <QStackedLayout>
#include <QStandardItemModel>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QVideoSink>
#include <QVideoWidget>
#include <QWebEngineLoadingInfo>
#include <QWebEngineProfile>
#include <QWindow>
#include <algorithm> // Added for std::min
#include <functional> // Added for std::function
#include <memory> // Added for std::shared_ptr (ThemePreviewCard thumbnail capture)

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
      // Static first-frame thumbnail — shown by default so every theme
      // always has a visible preview, not just on hover. The live
      // QVideoWidget itself is created lazily in startVideo()/destroyed in
      // stopVideo(), not here: it's a native-backed widget, and with 30+
      // theme cards sitting inside this panel's QScrollArea, permanently
      // instantiating one per card (even hidden) caused ghosting/tearing
      // artifacts on scroll — the same native-surface-in-QScrollArea issue
      // as the LIVE PREVIEW panel's QOpenGLWidget.
      m_thumbLabel = new QLabel();
      m_thumbLabel->setAlignment(Qt::AlignCenter);
      m_thumbLabel->setText("Loading video preview…");
      m_thumbLabel->setStyleSheet("background: #0f172a; border-radius: 4px;");
      // Without these, the label's size hint follows the captured frame's
      // raw pixel size (KeepAspectRatioByExpanding can even overflow the
      // target box on one axis) instead of the card's actual layout box —
      // that's what was inflating every card. setScaledContents alone
      // wasn't enough — Ignored fully removes the label's own sizeHint
      // from layout negotiation, so previewContainer's fixed 100px height
      // is what actually governs the box size, not the pixmap.
      m_thumbLabel->setScaledContents(true);
      m_thumbLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
      m_stack->addWidget(m_thumbLabel);
      captureThumbnail();
    } else if (theme.type == ThemeType::Image) {
      // Images load synchronously and cheaply — no async capture dance
      // needed like the video case, just show the file directly.
      auto *imgLabel = new QLabel();
      imgLabel->setAlignment(Qt::AlignCenter);
      imgLabel->setStyleSheet("background: #0f172a; border-radius: 4px;");
      imgLabel->setScaledContents(true);
      imgLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
      QPixmap pix(theme.contentPath);
      if (!pix.isNull())
        imgLabel->setPixmap(pix);
      else imgLabel->setText("Image unavailable");
      m_stack->addWidget(imgLabel);
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
    auto *actions = new QHBoxLayout();
    actions->setSpacing(6);
    actions->addWidget(applyBtn, 1);
    auto *deleteBtn = new QPushButton("Delete");
    deleteBtn->setToolTip("Delete this theme without applying it");
    deleteBtn->setStyleSheet(
        "QPushButton { background: #334155; color: #fca5a5; border-radius: 4px; padding: 4px 8px; font-size: 10px; } "
        "QPushButton:hover { background: #7f1d1d; color: white; }");
    connect(deleteBtn, &QPushButton::clicked, this, [this]() {
      const auto callback = m_deleteCallback;
      const int index = m_index;
      if (callback) QTimer::singleShot(0, m_controlWindow, [callback, index]() { callback(index); });
    });
    actions->addWidget(deleteBtn);
    itemLayout->addLayout(actions);

    // Context Menu
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QWidget::customContextMenuRequested, this,
            &ThemePreviewCard::showContextMenu);
  }

  ~ThemePreviewCard() { stopVideo(); }

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
    const auto callback = m_deleteCallback;
    const int index = m_index;
    QPointer<ControlWindow> owner(m_controlWindow);
    QAction *selected = menu.exec(mapToGlobal(pos));
    if (selected == deleteAction && owner && callback) {
      QTimer::singleShot(0, owner, [callback, index]() { callback(index); });
    }
  }

public:
  std::function<void(const ThemeTemplate &)> m_applyCallback;
  std::function<void(int)> m_deleteCallback;

private:
  void startVideo() {
    if (m_thumbLabel)
      m_thumbLabel->hide();

    // Created here, not in the constructor: a native-backed QVideoWidget
    // sitting permanently in this scrollable list (even hidden) is what
    // caused the ghosting/tearing on scroll. Only exists while hovered.
    m_videoWidget = new QVideoWidget();
    m_videoWidget->setStyleSheet("background: black; border-radius: 4px;");
    // QVideoWidget defaults to Expanding — same fix as m_thumbLabel, so
    // hovering doesn't balloon the card the way the static thumbnail did.
    m_videoWidget->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    m_stack->addWidget(m_videoWidget);
    m_videoWidget->show();

    m_player = new QMediaPlayer(this);
    m_audio = new QAudioOutput(this);
    m_audio->setMuted(true);
    m_player->setAudioOutput(m_audio);
    m_player->setVideoOutput(m_videoWidget);
    m_player->setSource(QUrl::fromLocalFile(m_theme.contentPath));

    connect(m_player, &QMediaPlayer::playbackStateChanged,
            this, [this](QMediaPlayer::PlaybackState state) {
              if (state == QMediaPlayer::StoppedState && m_player)
                m_player->play();
            });

    m_player->play();
  }

  void stopVideo() {
    if (m_player) {
      disconnect(m_player, nullptr, this, nullptr);
      m_player->setVideoOutput(nullptr);
      m_player->stop();
      m_player->deleteLater();
      m_player = nullptr;
    }
    if (m_audio) {
      m_audio->deleteLater();
      m_audio = nullptr;
    }
    if (m_videoWidget) {
      m_stack->removeWidget(m_videoWidget);
      m_videoWidget->deleteLater();
      m_videoWidget = nullptr;
    }
    if (m_thumbLabel)
      m_thumbLabel->show();
  }

  // Static preview for a video theme. Two paths:
  //  1. A sibling image with the same base name (every bundled default
  //     theme ships one, e.g. foo.mp4 + foo.jpg) — load it directly, no
  //     decoding needed at all.
  //  2. No sibling image (custom-uploaded videos) — grab the first frame
  //     via a throwaway QMediaPlayer, but queued one-at-a-time (see
  //     s_captureQueue below). Firing 20+ of these simultaneously on
  //     startup crashed the app with SIGSEGV inside CoreMedia/MediaToolbox
  //     — macOS's AVFoundation backend doesn't tolerate that many
  //     concurrent decode-then-teardown cycles at once.
  void captureThumbnail() {
    QFileInfo info(m_theme.contentPath);
    QString base = info.absolutePath() + "/" + info.completeBaseName() + ".";
    for (const char *ext : {"jpg", "jpeg", "png"}) {
      QString candidate = base + ext;
      if (QFile::exists(candidate)) {
        QPixmap pix(candidate);
        if (!pix.isNull()) {
          m_thumbLabel->setPixmap(pix);
          return;
        }
      }
    }

    const QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/theme-previews";
    QDir().mkpath(cacheDir);
    const QByteArray identity = (info.absoluteFilePath() + QString::number(info.size()) + info.lastModified().toString(Qt::ISODateWithMs)).toUtf8();
    m_thumbnailPath = cacheDir + "/" + QCryptographicHash::hash(identity, QCryptographicHash::Sha256).toHex() + ".png";
    QPixmap cached(m_thumbnailPath);
    if (!cached.isNull()) { m_thumbLabel->setPixmap(cached); return; }
    s_captureQueue.append(this);
    processQueue();
  }

  static void processQueue() {
    if (s_captureActive)
      return;
    QPointer<ThemePreviewCard> card;
    while (!s_captureQueue.isEmpty()) {
      card = s_captureQueue.takeFirst();
      if (card)
        break;
    }
    if (!card)
      return;
    s_captureActive = true;
    card->doCaptureThumbnail();
  }

  void doCaptureThumbnail() {
    // The capture outlives its card so deleting a theme cannot strand the
    // queue or tear down a decoder inside its own frame callback.
    auto *session = new QObject(qApp);
    auto *player = new QMediaPlayer(session);
    auto *sink = new QVideoSink(session);
    auto *timer = new QTimer(session);
    timer->setSingleShot(true);
    player->setVideoSink(sink);
    QPointer<ThemePreviewCard> card(this);
    auto finished = std::make_shared<bool>(false);
    auto finish = [session, player, sink, timer, card, finished](const QImage &image) {
      if (*finished) return;
      *finished = true;
      timer->stop();
      QObject::disconnect(sink, nullptr, session, nullptr);
      QObject::disconnect(player, nullptr, session, nullptr);
      if (card && card->m_thumbLabel) {
        if (!image.isNull()) {
          card->m_thumbLabel->setPixmap(QPixmap::fromImage(image));
          image.save(card->m_thumbnailPath);
        } else card->m_thumbLabel->setText("Video preview unavailable");
      }
      QTimer::singleShot(0, session, [session, player]() {
        player->stop();
        player->setVideoSink(nullptr);
        QTimer::singleShot(150, qApp, [session]() {
          delete session;
          s_captureActive = false;
          processQueue();
        });
      });
    };
    connect(sink, &QVideoSink::videoFrameChanged, session, [finish](const QVideoFrame &frame) {
      if (frame.isValid()) { const QImage image = frame.toImage(); if (!image.isNull()) finish(image); }
    });
    connect(player, &QMediaPlayer::errorOccurred, session, [finish]() { finish(QImage()); });
    connect(timer, &QTimer::timeout, session, [finish]() { finish(QImage()); });
    timer->start(6000);
    player->setSource(QUrl::fromLocalFile(m_theme.contentPath));
    player->play();
  }

  inline static QList<QPointer<ThemePreviewCard>> s_captureQueue;
  inline static bool s_captureActive = false;

  QString m_thumbnailPath;
  ThemeTemplate m_theme;
  int m_index;
  ControlWindow *m_controlWindow;
  QMediaPlayer *m_player = nullptr;
  QAudioOutput *m_audio = nullptr;
  QVideoWidget *m_videoWidget = nullptr;
  QLabel *m_thumbLabel = nullptr;
  QStackedLayout *m_stack = nullptr;
};

// Keep keyboard focus and selection while omitting the native Windows
// focus frame; the selected card already provides the visual indication.
class LyricCardDelegate : public QStyledItemDelegate {
public:
  using QStyledItemDelegate::QStyledItemDelegate;
protected:
  void initStyleOption(QStyleOptionViewItem *option,
                       const QModelIndex &index) const override {
    QStyledItemDelegate::initStyleOption(option, index);
    option->state &= ~QStyle::State_HasFocus;
  }
};

// Native list selection keeps click and keyboard projection behavior together.
class LyricGridList : public QListWidget {
public:
  explicit LyricGridList(QWidget *parent = nullptr) : QListWidget(parent) {
    setItemDelegate(new LyricCardDelegate(this));
    setViewMode(QListView::IconMode);
    setFlow(QListView::LeftToRight);
    setWrapping(true);
    setMovement(QListView::Static);
    setResizeMode(QListView::Adjust);
    setWordWrap(true);
    setSpacing(4);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    connect(model(), &QAbstractItemModel::rowsInserted, this, [this]() { scheduleReflow(); });
  }
protected:
  void resizeEvent(QResizeEvent *event) override {
    QListWidget::resizeEvent(event);
    scheduleReflow();
  }
private:
  bool pending = false;
  void scheduleReflow() {
    if (pending) return;
    pending = true;
    QTimer::singleShot(0, this, [this]() {
      pending = false;
      const int available = qMax(100, viewport()->width() - 8);
      const int columns = available >= 560 ? 2 : 1;
      const int cellWidth = qMax(80, available / columns - 4 * spacing());
      QVector<int> heights;
      for (int i = 0; i < count(); ++i) {
        QTextDocument doc;
        doc.setDefaultFont(font());
        doc.setDocumentMargin(0);
        doc.setPlainText(item(i)->text());
        doc.setTextWidth(qMax(40, cellWidth - 20));
        heights.append(qMax(48, qCeil(doc.size().height()) + 20));
      }
      // Let each row fit its own sections instead of using the longest
      // section's height for every row in the song.
      setGridSize(QSize());
      for (int row = 0; row < count(); row += columns) {
        int rowHeight = 0;
        for (int j = row; j < qMin(row + columns, count()); ++j) rowHeight = qMax(rowHeight, heights[j]);
        for (int j = row; j < qMin(row + columns, count()); ++j) item(j)->setSizeHint(QSize(cellWidth, rowHeight));
      }
      doItemsLayout();
    });
  }
};

// --- Helper Class: SongListItemWidget ---
// Compact card-style row for the sidebar song library (title + artist,
// elided to fit so long titles never blow out the compact layout).
class SongListItemWidget : public QWidget {
public:
  SongListItemWidget(const QString &title, const QString &artist,
                      QWidget *parent = nullptr)
      : QWidget(parent), m_fullTitle(title.trimmed().isEmpty() ? "Untitled" : title.trimmed()),
        m_fullArtist(artist.trimmed()) {
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setToolTip(m_fullTitle + (m_fullArtist.isEmpty() ? QString() : "\n" + m_fullArtist));
    setStyleSheet("background: transparent;");
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 9, 12, 9);
    layout->setSpacing(4);

    m_titleLabel = new QLabel(m_fullTitle, this);
    m_titleLabel->setTextFormat(Qt::PlainText);
    m_titleLabel->setMinimumWidth(0);
    m_titleLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_titleLabel->setStyleSheet(
        "color: #f1f5f9; font-size: 13px; font-weight: 600; "
        "background: transparent;");
    layout->addWidget(m_titleLabel);

    QString trimmedArtist = artist.trimmed();
    if (!trimmedArtist.isEmpty()) {
      m_artistLabel = new QLabel(trimmedArtist, this);
      m_artistLabel->setTextFormat(Qt::PlainText);
      m_artistLabel->setMinimumWidth(0);
      m_artistLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
      m_artistLabel->setStyleSheet(
          "color: #94a3b8; font-size: 11px; background: transparent;");
      layout->addWidget(m_artistLabel);
    }
  }

protected:
  void resizeEvent(QResizeEvent *event) override {
    QWidget::resizeEvent(event);
    QFontMetrics fm(m_titleLabel->font());
    m_titleLabel->setText(
        fm.elidedText(m_fullTitle, Qt::ElideRight, qMax(0, width() - 24)));
    if (m_artistLabel) m_artistLabel->setText(QFontMetrics(m_artistLabel->font()).elidedText(m_fullArtist, Qt::ElideRight, qMax(0, width() - 24)));
  }

private:
  QString m_fullTitle;
  QString m_fullArtist;
  QLabel *m_titleLabel;
  QLabel *m_artistLabel = nullptr;
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
  mainSplitter->setChildrenCollapsible(false);
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
  // Keep enough room for the STAGE panel's buttons/dropdowns so dragging the
  // splitter narrower can't force it into horizontal-scroll territory.
  controlsContainer->setMinimumWidth(340);
  setupMasterControl(controlsContainer);
  mainSplitter->addWidget(controlsContainer);

  // Set Initial Sizes (~18% | 45% | 36%) — previously 15/55/30, which left the
  // STAGE/preview column comparatively squeezed. Explicit setSizes() controls
  // the actual first-launch split; the stretch factors just keep that same
  // balance on subsequent window resizes.
  mainSplitter->setStretchFactor(0, 2);
  mainSplitter->setStretchFactor(1, 5);
  mainSplitter->setStretchFactor(2, 4);
  mainSplitter->setSizes({260, 650, 520});

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
  layout->setSpacing(6);

  auto *header = new QLabel("PINNED RECENTS");
  header->setObjectName("headerDisplay");
  header->setAlignment(Qt::AlignCenter);
  layout->addWidget(header);

  // Search — matches title, artist AND the lyrics text itself
  songSearchEdit = new QLineEdit();
  songSearchEdit->setPlaceholderText("Filter pinned songs…");
  songSearchEdit->setToolTip("Press Enter to search all saved songs");
  layout->addWidget(songSearchEdit);
  connect(songSearchEdit, &QLineEdit::returnPressed, this, [this]() {
    m_songLookupTimer->stop();
    mainTabWidget->setCurrentIndex(1);
    findSongMatches(songSearchEdit->text().trimmed());
  });

  // Debounce so fast typing doesn't re-scan every song's lyrics per keystroke
  m_songSearchDebounce = new QTimer(this);
  m_songSearchDebounce->setSingleShot(true);
  m_songSearchDebounce->setInterval(150);
  connect(m_songSearchDebounce, &QTimer::timeout, this,
          [this]() { filterSongList(songSearchEdit->text()); });
  connect(songSearchEdit, &QLineEdit::textChanged, this,
          [this]() { m_songSearchDebounce->start(); });

  // List — populated a page at a time, never the whole library up front
  songList = new QListWidget();
  songList->setObjectName("songListWidget");
  // Use native item spacing rather than a CSS "margin" on ::item — margins on
  // list items conflict with setItemWidget's geometry sync in Qt and clip the
  // embedded widget's left edge.
  songList->setSpacing(5);
  songList->setWordWrap(false);
  songList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  songList->setStyleSheet(
      "QListWidget#songListWidget { background: #1e293b; "
      "  border: 1px solid #334155; border-radius: 8px; padding: 5px; }"
      "QListWidget#songListWidget::item { border: none; border-radius: 6px; "
      "  padding: 0; color: #f1f5f9; background: #172235; }"
      "QListWidget#songListWidget::item:hover { background: "
      "rgba(56,189,248,0.10); }"
      "QListWidget#songListWidget::item:selected { background: "
      "#15374b; color: #f1f5f9; border: 1px solid #38bdf8; }");
  layout->addWidget(songList, 1);

  connect(songList, &QListWidget::currentItemChanged, this,
          [this](QListWidgetItem *current, QListWidgetItem *) {
            if (current)
              onSongSelected(current->data(Qt::UserRole).toInt());
          });

  // Infinite-scroll: load the next page as the user nears the bottom
  connect(songList->verticalScrollBar(), &QScrollBar::valueChanged, this,
          [this](int value) {
            auto *sb = songList->verticalScrollBar();
            if (sb->maximum() - value < 60 &&
                m_songListLoaded < (int)m_filteredSongIndices.size())
              loadMoreSongs();
          });

  // Status line — how many are showing / matched, so lazy loading is obvious
  songListStatusLabel = new QLabel();
  songListStatusLabel->setAlignment(Qt::AlignCenter);
  songListStatusLabel->setStyleSheet(
      "color: #64748b; font-size: 10px; padding: 2px;");
  layout->addWidget(songListStatusLabel);

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

  // --- Tab 5: Browser (Lyrics Search) ---
  auto *browserTab = new QWidget();
  setupBrowserTab(browserTab);
  mainTabWidget->addTab(browserTab, "Browser");
}

void ControlWindow::setupBibleTab(QWidget *container) {
  auto *layout = new QVBoxLayout(container);
  layout->setContentsMargins(0, 0, 0, 0);

  // --- Top Navigation & Version (Always Visible) ---
  auto *navContainer = new QWidget();
  auto *navLayout = new QVBoxLayout(navContainer);
  navLayout->setContentsMargins(10, 10, 10, 5);
  navLayout->setSpacing(8);

  // Version selector - toggle buttons that wrap onto multiple rows instead
  // of forcing one long row (which, with a dozen+ versions, was wide enough
  // to push the whole window past the screen edge).
  auto *versionFrame = new QFrame();
  versionFrame->setStyleSheet(
      "QFrame { background: #1e293b; border: 1px solid #334155; "
      "border-radius: 6px; padding: 8px; }");
  auto *versionOuterLayout = new QVBoxLayout(versionFrame);
  versionOuterLayout->setContentsMargins(8, 8, 8, 8);
  versionOuterLayout->setSpacing(6);

  auto *versionLabel = new QLabel("VERSION:");
  versionLabel->setStyleSheet("color: white; font-weight: bold; font-size: "
                              "12px; background: transparent; border: none;");
  versionOuterLayout->addWidget(versionLabel);

  bibleVersionLayout = new QGridLayout();
  bibleVersionLayout->setSpacing(6);
  versionOuterLayout->addLayout(bibleVersionLayout);

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
  bibleSplitter->setChildrenCollapsible(false);
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
      child->widget()->hide();
      child->widget()->deleteLater();
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

void ControlWindow::searchSongLibrary() {
  // A lookup is a line beginning with @, read up to the editing cursor.
  // Ordinary lyrics and email addresses remain ordinary text.
  const auto cursor = lyricsEdit->textCursor();
  QString line = cursor.block().text().left(cursor.positionInBlock()).trimmed();
  if (!lyricsEdit->hasFocus() || !line.startsWith('@')) {
    m_songMatches->clear();
    m_songMatches->hide();
    return;
  }
  findSongMatches(line.mid(1).trimmed());
}

void ControlWindow::findSongMatches(const QString &query) {
  m_songMatches->clear();
  m_songMatches->hide();
  const auto &songs = songManager->getSongs();
  for (int i = (int)songs.size() - 1; i >= 0; --i) {
    const auto &song = songs[i];
    if (query.isEmpty() || song.title.contains(query, Qt::CaseInsensitive) ||
        song.artist.contains(query, Qt::CaseInsensitive) ||
        song.verses.join("\n").contains(query, Qt::CaseInsensitive)) {
      auto *item = new QListWidgetItem(song.title + (song.isPinned() ? " • Pinned" : ""), m_songMatches);
      item->setData(Qt::UserRole, i);
    }
  }
  if (m_songMatches->count()) m_songMatches->show();
  else if (!query.isEmpty()) openSongBrowser(query);
}

void ControlWindow::openSongBrowser(const QString &query) {
  // Reuse the full browser, including its lyrics import controls, in a popup.
  const int browserIndex = mainTabWidget->count() - 1;
  QWidget *browser = mainTabWidget->widget(browserIndex);
  const QString tabTitle = mainTabWidget->tabText(browserIndex);
  mainTabWidget->removeTab(browserIndex);
  QDialog dialog(this);
  dialog.setWindowTitle("Find song lyrics — " + query);
  dialog.resize(1100, 750);
  auto *layout = new QVBoxLayout(&dialog);
  layout->addWidget(browser);
  // A non-current QTabWidget page is explicitly hidden. Reparenting it
  // does not clear that state, so show it before displaying the dialog.
  browser->show();
  m_lyricsSearch->setText(query);
  m_lyricsSearch->returnPressed();
  dialog.exec();
  layout->removeWidget(browser);
  mainTabWidget->insertTab(browserIndex, browser, tabTitle);
}

void ControlWindow::setupSongTab(QWidget *container) {
  auto *layout = new QVBoxLayout(container);
  layout->setContentsMargins(14, 14, 14, 14);
  layout->setSpacing(10);
  auto *meta = new QHBoxLayout();
  titleEdit = new QLineEdit();
  titleEdit->setPlaceholderText("Song title");
  artistEdit = new QLineEdit();
  artistEdit->setPlaceholderText("Artist / author");
  auto *save = new QPushButton("Save Changes");
  save->setObjectName("primaryBtn");
  connect(save, &QPushButton::clicked, this, &ControlWindow::saveSong);
  meta->addWidget(titleEdit, 1);
  meta->addWidget(artistEdit, 1);
  meta->addWidget(save);
  layout->addLayout(meta);
  auto *actions = new QHBoxLayout();
  m_pinSongBtn = new QPushButton("Pin to Recents");
  connect(m_pinSongBtn, &QPushButton::clicked, this, [this]() {
    if (currentSongIndex < 0) return;
    Song song = songManager->getSongs()[currentSongIndex];
    if (song.isCommonItem()) return;
    song.pinned = !song.pinned;
    songManager->updateSong(currentSongIndex, song);
    updateSongList();
    m_pinSongBtn->setText(song.isPinned() ? "Unpin from Recents" : "Pin to Recents");
  });
  auto *remove = new QPushButton("Delete Song");
  connect(remove, &QPushButton::clicked, this, &ControlWindow::deleteSong);
  actions->addWidget(m_pinSongBtn);
  actions->addWidget(remove);
  actions->addStretch();
  layout->addLayout(actions);
  auto *splitter = new QSplitter(Qt::Vertical);
  splitter->setChildrenCollapsible(false);
  splitter->setHandleWidth(7);
  auto *lyricPane = new QWidget();
  auto *lyricLayout = new QVBoxLayout(lyricPane);
  lyricLayout->setContentsMargins(0, 0, 0, 0);
  auto *lyricLabel = new QLabel("LYRICS · Click a section to project");
  lyricLabel->setStyleSheet("color: #94a3b8; font-size: 11px;");
  lyricLayout->addWidget(lyricLabel);
  verseList = new LyricGridList();
  verseList->setWordWrap(true);
  verseList->setSpacing(4);
  verseList->setMinimumHeight(100);
  verseList->setStyleSheet("QListWidget { background: #1b293b; border: 1px solid #334155; border-radius: 8px; padding: 6px; } QListWidget::item { padding: 8px; border-radius: 5px; } QListWidget::item:hover { background: #253b50; } QListWidget::item:selected { background: #15374b; color: #e2e8f0; border-left: 3px solid #38bdf8; }");

  connect(verseList, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) { projectVerse(verseList->row(item)); });
  lyricLayout->addWidget(verseList, 1);
  splitter->addWidget(lyricPane);
  auto *editorPane = new QWidget();
  auto *editorLayout = new QVBoxLayout(editorPane);
  editorLayout->setContentsMargins(0, 0, 0, 0);
  auto *editorLabel = new QLabel("LYRICS EDITOR · Type @song name to find a song");
  editorLabel->setStyleSheet("color: #94a3b8; font-size: 11px;");
  editorLayout->addWidget(editorLabel);
  lyricsEdit = new QTextEdit();
  lyricsEdit->setAcceptRichText(false);
  lyricsEdit->setPlaceholderText("Paste lyrics here… Use blank lines between sections.\nType @song name on a new line to search.");
  lyricsEdit->setMinimumHeight(100);
  editorLayout->addWidget(lyricsEdit, 1);
  m_songMatches = new QListWidget();
  m_songMatches->setMaximumHeight(130);
  m_songMatches->hide();
  editorLayout->addWidget(m_songMatches);
  m_songLookupTimer = new QTimer(this);
  m_songLookupTimer->setSingleShot(true);
  m_songLookupTimer->setInterval(1000);
  connect(m_songLookupTimer, &QTimer::timeout, this, &ControlWindow::searchSongLibrary);
  auto schedule = [this]() { m_songMatches->hide(); m_songLookupTimer->start(); };
  connect(lyricsEdit, &QTextEdit::textChanged, this, schedule);
  connect(lyricsEdit, &QTextEdit::cursorPositionChanged, this, schedule);
  connect(m_songMatches, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
    const int index = item->data(Qt::UserRole).toInt();
    onSongSelected(index);
    lyricsEdit->setFocus();
  });
  splitter->addWidget(editorPane);
  splitter->setStretchFactor(0, 1);
  splitter->setStretchFactor(1, 1);
  splitter->setSizes({350, 350});
  layout->addWidget(splitter, 1);
  auto *navigation = new QHBoxLayout();
  prevBtn = new QPushButton("PREV (B)");
  nextBtn = new QPushButton("NEXT (SPACE)");
  nextBtn->setObjectName("primaryBtn");
  connect(prevBtn, &QPushButton::clicked, this, &ControlWindow::prevVerse);
  connect(nextBtn, &QPushButton::clicked, this, &ControlWindow::nextVerse);
  navigation->addWidget(prevBtn);
  navigation->addWidget(nextBtn);
  layout->addLayout(navigation);
}

void ControlWindow::setupMasterControl(QWidget *container) {
  auto *outerLayout = new QVBoxLayout(container);
  outerLayout->setContentsMargins(0, 0, 0, 0);

  // Wrap right panel in scroll area so it never clips
  auto *scrollArea = new QScrollArea();
  scrollArea->setWidgetResizable(true);
  scrollArea->setFrameShape(QFrame::NoFrame);
  // Vertical only — the panel's minimum width is guaranteed to fit its
  // content, so a horizontal scrollbar should never be needed.
  scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
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

  presentBtn = new QPushButton("▶  GO LIVE");
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

  // 2-column grid (not a single row) so the panel stays usable at a
  // realistic minimum width instead of needing 3 buttons' worth of space.
  auto *togglesLayout = new QGridLayout();
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

  togglesLayout->addWidget(clearTextBtn, 0, 0);
  togglesLayout->addWidget(blackOutBtn, 0, 1);
  togglesLayout->addWidget(clearAllBtn, 1, 0, 1, 2);
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
  projectionLayoutCombo->addItem("Split Screen · Left / Right",
                                 (int)Projection::LayoutType::SplitVertical);
  projectionLayoutCombo->addItem("Split Screen · Top / Bottom",
                                 (int)Projection::LayoutType::SplitHorizontal);
  connect(projectionLayoutCombo, &QComboBox::currentIndexChanged,
          [this](int index) {
            Projection::LayoutType type =
                (Projection::LayoutType)projectionLayoutCombo->itemData(index)
                    .toInt();
            projection->setLayoutType(type);
            preview->setLayoutType(type);
            if (targetLayerCombo) {
              targetLayerCombo->setEnabled(type != Projection::LayoutType::Single);
              if (type == Projection::LayoutType::Single) targetLayerCombo->setCurrentIndex(0);
            }
          });

  // Layer combo
  auto *layerLabel = new QLabel("Target");
  layerLabel->setStyleSheet(
      "color: #94a3b8; font-size: 10px; background: transparent;");
  targetLayerCombo = new QComboBox();
  targetLayerCombo->setStyleSheet(comboModernStyle);
  targetLayerCombo->addItem("Screen 1", 0);
  targetLayerCombo->addItem("Screen 2", 1);
  targetLayerCombo->setEnabled(false);
  connect(targetLayerCombo, &QComboBox::currentIndexChanged, [this](int index) {
    currentTargetLayer = targetLayerCombo->itemData(index).toInt();
    if (preview) preview->setActiveScreen(currentTargetLayer);
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
  videoThemesLayout->addWidget(createThemeBtn, 0, 0, 1, 2); // span both columns

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

  // One label+control pair per row (not 2 side by side) — packing "Size" and
  // "Margin" into a single 4-column row needed ~365px minimum, which didn't
  // fit the panel and got silently clipped with no way to scroll to it.
  fmtLayout->addWidget(new QLabel("Size (0=Auto):"), 1, 0);
  fontSizeSpin = new QSpinBox();
  fontSizeSpin->setRange(0, 200);
  fontSizeSpin->setValue(0);
  fmtLayout->addWidget(fontSizeSpin, 1, 1, 1, 3);

  fmtLayout->addWidget(new QLabel("Margin:"), 2, 0);
  marginSpin = new QSpinBox();
  marginSpin->setRange(0, 500);
  marginSpin->setValue(40);
  fmtLayout->addWidget(marginSpin, 2, 1, 1, 3);

  fmtLayout->addWidget(new QLabel("Align:"), 3, 0);
  alignmentCombo = new QComboBox();
  alignmentCombo->addItem("Left", (int)Qt::AlignLeft);
  alignmentCombo->addItem("Center", (int)Qt::AlignCenter);
  alignmentCombo->addItem("Right", (int)Qt::AlignRight);
  alignmentCombo->setCurrentIndex(1); // Center default
  fmtLayout->addWidget(alignmentCombo, 3, 1, 1, 3);

  scrollCheckBox = new QCheckBox("Scrolling (Vertical)");
  fmtLayout->addWidget(scrollCheckBox, 4, 0, 1, 4);

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

  auto *stageToggle = new QPushButton("▶ STAGE TEXT BAR");
  layout->addWidget(stageToggle);
  auto *stageGroup = new QGroupBox();
  auto *stageForm = new QFormLayout(stageGroup);
  stageForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  auto *stageEnabled = new QCheckBox("Show bottom text bar");
  auto *stageText = new QTextEdit();
  stageText->setAcceptRichText(false);
  stageText->setMaximumHeight(85);
  stageText->setPlaceholderText("Announcement, sermon title, speaker name…");
  auto *stageMode = new QComboBox(); stageMode->addItems({"Static text", "Scrolling ticker"});
  auto *stageFont = new QFontComboBox();
  auto *stageSize = new QSpinBox(); stageSize->setRange(12,120); stageSize->setSuffix(" px at 1080p");
  auto *stageSpeed = new QSpinBox(); stageSpeed->setRange(10,300);stageSpeed->setSuffix(" px / sec");
  auto *stageScreen = new QComboBox();stageScreen->addItems({"Whole stage", "Screen 1", "Screen 2"});
  auto *stageX = new QSpinBox();stageX->setRange(0,90);stageX->setSuffix(" %");
  auto *stageY = new QSpinBox();stageY->setRange(0,95);stageY->setSuffix(" %");
  auto *stageWidth = new QSpinBox();stageWidth->setRange(10,100);stageWidth->setSuffix(" %");
  auto *stageHeight = new QSpinBox();stageHeight->setRange(5,40);stageHeight->setSuffix(" %");
  auto *stageForeground = new QPushButton("Text colour");
  auto *stageBackground = new QPushButton("Bar colour / opacity");
  auto colours = std::make_shared<QPair<QColor,QColor>>();
  QSettings savedStage;
  stageEnabled->setChecked(savedStage.value("StageBar/enabled",false).toBool());
  stageText->setPlainText(savedStage.value("StageBar/text").toString());
  stageMode->setCurrentIndex(savedStage.value("StageBar/mode",0).toInt());
  stageFont->setCurrentFont(QFont(savedStage.value("StageBar/font","Arial").toString()));
  stageSize->setValue(savedStage.value("StageBar/size",36).toInt());
  stageSpeed->setValue(savedStage.value("StageBar/speed",80).toInt());
  stageScreen->setCurrentIndex(savedStage.value("StageBar/screen",0).toInt());
  stageX->setValue(savedStage.value("StageBar/x",0).toInt());
  stageY->setValue(savedStage.value("StageBar/y",90).toInt());
  stageWidth->setValue(savedStage.value("StageBar/width",100).toInt());
  stageHeight->setValue(savedStage.value("StageBar/height",10).toInt());
  colours->first=savedStage.value("StageBar/foreground",QColor(Qt::white)).value<QColor>();
  colours->second=savedStage.value("StageBar/background",QColor(15,23,42,230)).value<QColor>();
  stageForm->addRow(stageEnabled);
  stageForm->addRow("Text",stageText);stageForm->addRow("Mode",stageMode);
  stageForm->addRow("Font",stageFont);stageForm->addRow("Text size",stageSize);
  stageForm->addRow("Ticker speed",stageSpeed);stageForm->addRow("Placement",stageScreen);
  stageForm->addRow("Left",stageX);stageForm->addRow("Top",stageY);
  stageForm->addRow("Width",stageWidth);stageForm->addRow("Height",stageHeight);
  stageForm->addRow(stageForeground);stageForm->addRow(stageBackground);
  auto *saveStage = new QPushButton("Save stage layout");stageForm->addRow(saveStage);
  auto applyStage = [=]() {
    Projection::StageOverlay bar;
    bar.enabled=stageEnabled->isChecked();bar.text=stageText->toPlainText();bar.scrolling=stageMode->currentIndex()==1;
    bar.font=stageFont->currentFont().family();bar.fontSize=stageSize->value();bar.speed=stageSpeed->value();
    bar.screen=stageScreen->currentIndex()-1;bar.x=stageX->value();bar.y=stageY->value();
    bar.width=stageWidth->value();bar.height=stageHeight->value();bar.foreground=colours->first;bar.background=colours->second;
    stageSpeed->setEnabled(bar.scrolling);
    if(projection)projection->setStageOverlay(bar);if(preview)preview->setStageOverlay(bar);
  };
  connect(stageEnabled,&QCheckBox::toggled,this,applyStage);
  connect(stageText,&QTextEdit::textChanged,this,applyStage);
  connect(stageMode,&QComboBox::currentIndexChanged,this,applyStage);
  connect(stageScreen,&QComboBox::currentIndexChanged,this,applyStage);
  connect(stageFont,&QFontComboBox::currentFontChanged,this,applyStage);
  for(auto *spin:{stageSize,stageSpeed,stageX,stageY,stageWidth,stageHeight})connect(spin,&QSpinBox::valueChanged,this,applyStage);
  connect(stageForeground,&QPushButton::clicked,this,[=](){auto colour=QColorDialog::getColor(colours->first,this,"Text colour");if(colour.isValid()){colours->first=colour;applyStage();}});
  connect(stageBackground,&QPushButton::clicked,this,[=](){auto colour=QColorDialog::getColor(colours->second,this,"Bar colour and opacity",QColorDialog::ShowAlphaChannel);if(colour.isValid()){colours->second=colour;applyStage();}});
  connect(saveStage,&QPushButton::clicked,this,[=](){
    QSettings settings;
    settings.setValue("StageBar/enabled",stageEnabled->isChecked());settings.setValue("StageBar/text",stageText->toPlainText());
    settings.setValue("StageBar/mode",stageMode->currentIndex());settings.setValue("StageBar/font",stageFont->currentFont().family());
    settings.setValue("StageBar/size",stageSize->value());settings.setValue("StageBar/speed",stageSpeed->value());
    settings.setValue("StageBar/screen",stageScreen->currentIndex());settings.setValue("StageBar/x",stageX->value());settings.setValue("StageBar/y",stageY->value());
    settings.setValue("StageBar/width",stageWidth->value());settings.setValue("StageBar/height",stageHeight->value());
    settings.setValue("StageBar/foreground",colours->first);settings.setValue("StageBar/background",colours->second);
    saveStage->setText("Stage layout saved");QTimer::singleShot(2000,saveStage,[saveStage](){saveStage->setText("Save stage layout");});
  });
  stageGroup->hide();layout->addWidget(stageGroup);
  connect(stageToggle,&QPushButton::clicked,this,[=](){stageGroup->setVisible(!stageGroup->isVisible());stageToggle->setText(stageGroup->isVisible()?"▼ STAGE TEXT BAR":"▶ STAGE TEXT BAR");});
  applyStage();


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
  // Rebuild the filtered set from scratch, respecting whatever search text
  // (if any) is currently in the box.
  filterSongList(songSearchEdit ? songSearchEdit->text() : QString());
}

void ControlWindow::filterSongList(const QString &query) {
  const auto &songs = songManager->getSongs();
  QString q = query.trimmed();

  m_filteredSongIndices.clear();
  m_filteredSongIndices.reserve((int)songs.size());
  // Match pinned entries; common items stay at the top of the sidebar.
  for (int i = (int)songs.size() - 1; i >= 0; --i) {
    const auto &song = songs[i];
    if (!song.isPinned()) continue;
    bool matches = q.isEmpty() || song.title.contains(q, Qt::CaseInsensitive) ||
                   song.artist.contains(q, Qt::CaseInsensitive);
    if (!matches) {
      // Search inside the lyrics themselves, not just title/artist
      for (const QString &verse : song.verses) {
        if (verse.contains(q, Qt::CaseInsensitive)) {
          matches = true;
          break;
        }
      }
    }
    if (matches)
      m_filteredSongIndices.push_back(i);
  }

  std::stable_sort(m_filteredSongIndices.begin(), m_filteredSongIndices.end(), [&songs](int a, int b) {
    if (songs[a].isCommonItem() != songs[b].isCommonItem()) return songs[a].isCommonItem();
    return songs[a].lastUsed > songs[b].lastUsed;
  });

  songList->clear();
  m_songListLoaded = 0;
  loadMoreSongs(); // only the first page — rest loads as the user scrolls
}

void ControlWindow::loadMoreSongs() {
  const auto &songs = songManager->getSongs();
  int end = std::min(m_songListLoaded + kSongPageSize,
                      (int)m_filteredSongIndices.size());

  for (int i = m_songListLoaded; i < end; ++i) {
    int songIdx = m_filteredSongIndices[i];
    const auto &song = songs[songIdx];

    auto *item = new QListWidgetItem();
    item->setData(Qt::UserRole, songIdx);
    item->setData(Qt::UserRole + 1, -1);
    item->setData(Qt::UserRole + 2, false);
    item->setToolTip(song.title + (song.artist.isEmpty() ? QString() : "\n" + song.artist));
    item->setSizeHint(QSize(0, song.artist.trimmed().isEmpty() ? 44 : 62));
    songList->addItem(item);
    songList->setItemWidget(item, new SongListItemWidget(song.title, song.artist));
  }
  m_songListLoaded = end;
  updateSongListStatus();
}

void ControlWindow::updateSongListStatus() {
  int total = (int)m_filteredSongIndices.size();
  if (total == 0) {
    songListStatusLabel->setText(songManager->getSongs().empty()
                                      ? "No songs yet — click + Add"
                                      : "No pinned matches — use @ in Songs to find and pin songs");
  } else if (m_songListLoaded >= total) {
    songListStatusLabel->setText(
        QString("%1 song%2").arg(total).arg(total == 1 ? "" : "s"));
  } else {
    songListStatusLabel->setText(QString("Showing %1 of %2 — scroll for more")
                                      .arg(m_songListLoaded)
                                      .arg(total));
  }
}

void ControlWindow::selectSongByIndex(int songIndex) {
  int row = -1;
  for (int i = 0; i < songList->count(); ++i) {
    if (songList->item(i)->data(Qt::UserRole).toInt() == songIndex) {
      row = i;
      break;
    }
  }
  // Not loaded yet (further down the filtered list) — keep paging it in
  while (row < 0 && m_songListLoaded < (int)m_filteredSongIndices.size()) {
    int before = m_songListLoaded;
    loadMoreSongs();
    for (int i = before; i < songList->count(); ++i) {
      if (songList->item(i)->data(Qt::UserRole).toInt() == songIndex) {
        row = i;
        break;
      }
    }
  }
  if (row >= 0) {
    songList->setCurrentRow(row);
    songList->scrollToItem(songList->item(row));
  }
}

void ControlWindow::onSongSelected(int index) {
  if (index < 0 || index >= (int)songManager->getSongs().size())
    return;
  currentSongIndex = index;
  const auto recent = songManager->recentSongIndices();
  if (std::find(recent.begin(), recent.end(), index) == recent.end()) songManager->markUsed(index);
  const auto &song = songManager->getSongs()[index];

  m_pinSongBtn->setText(song.isCommonItem() ? "Always pinned" :
                         song.isPinned() ? "Unpin from Recents" : "Pin to Recents");
  m_pinSongBtn->setEnabled(!song.isCommonItem());
  titleEdit->setText(song.title);
  artistEdit->setText(song.artist);
  m_songLookupTimer->stop();
  m_songMatches->hide();
  const QSignalBlocker lyricsSignals(lyricsEdit);
  lyricsEdit->setPlainText(song.verses.join("\n\n"));

  const QSignalBlocker verseSignals(verseList);
  verseList->clear();
  for (int i = 0; i < song.verses.size(); ++i) {
    auto *item = new QListWidgetItem(QString("Section %1\n%2").arg(i + 1).arg(song.verses[i]), verseList);
    item->setTextAlignment(Qt::AlignLeft | Qt::AlignTop);
    item->setData(Qt::UserRole, song.verses[i]);
  }

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
    int newIndex = (int)songManager->getSongs().size() - 1;

    songSearchEdit->clear(); // ensure the new song isn't hidden by a filter
    filterSongList(QString());
    onSongSelected(newIndex);
  }
}

void ControlWindow::editLyrics() {
  // Just select the song and focus tab
  mainTabWidget->setCurrentIndex(1);
  lyricsEdit->setFocus();
}

void ControlWindow::saveSong() {
  if (currentSongIndex < 0)
    return;
  Song s = songManager->getSongs()[currentSongIndex];
  s.title = titleEdit->text();
  s.artist = artistEdit->text();
  s.verses = lyricsEdit->toPlainText().split("\n\n", Qt::SkipEmptyParts);
  songManager->updateSong(currentSongIndex, s);
  updateSongList();
  selectSongByIndex(currentSongIndex);
  onSongSelected(currentSongIndex); // Refresh edit fields even if the song
                                     // no longer matches the active search
}

void ControlWindow::deleteSong() {
  if (currentSongIndex < 0)
    return;
  auto reply = QMessageBox::question(this, "Delete", "Delete this song?",
                                     QMessageBox::Yes | QMessageBox::No);
  if (reply == QMessageBox::Yes) {
    songManager->removeSong(currentSongIndex);
    m_songLookupTimer->stop();
    m_songMatches->clear();
    m_songMatches->hide();
    m_pinSongBtn->setText("Pin to Recents");
    m_pinSongBtn->setEnabled(true);
    updateSongList();
    verseList->clear();
    titleEdit->clear();
    artistEdit->clear();
    lyricsEdit->clear();
    currentSongIndex = -1;
  }
}

// ... Projection Logic ...

bool ControlWindow::chooseContentScreen() {
  const auto type = static_cast<Projection::LayoutType>(projectionLayoutCombo->currentData().toInt());
  if (type == Projection::LayoutType::Single) {
    targetLayerCombo->setCurrentIndex(0);
    currentTargetLayer = 0;
    return true;
  }
  QMessageBox chooser(this);
  chooser.setWindowTitle("Choose projection screen");
  chooser.setText("Where should this content appear?");
  chooser.setInformativeText(type == Projection::LayoutType::SplitVertical ? "Screen 1: left · Screen 2: right" : "Screen 1: top · Screen 2: bottom");
  auto *one = chooser.addButton("Screen 1", QMessageBox::AcceptRole);
  auto *two = chooser.addButton("Screen 2", QMessageBox::AcceptRole);
  chooser.addButton(QMessageBox::Cancel);
  chooser.exec();
  if (chooser.clickedButton() != one && chooser.clickedButton() != two) return false;
  currentTargetLayer = chooser.clickedButton() == one ? 0 : 1;
  targetLayerCombo->setCurrentIndex(currentTargetLayer);
  return true;
}

void ControlWindow::projectVerse(int index) {
  if (index < 0 || index >= verseList->count())
    return;
  if (!chooseContentScreen()) return;
  currentVerseIndex = index;
  QString text = verseList->item(index)->data(Qt::UserRole).toString();
  lastProjectedText = text;
  m_screenText[currentTargetLayer] = text;

  if (projection) projection->setLayerText(currentTargetLayer, text);
  if (preview) preview->setLayerText(currentTargetLayer, text);
}

void ControlWindow::projectBibleVerse(const QString &text) {
  if (!chooseContentScreen()) return;
  lastProjectedText = text;
  m_screenText[currentTargetLayer] = text;
  if (projection) projection->setLayerText(currentTargetLayer, text);
  if (preview) preview->setLayerText(currentTargetLayer, text);
}

void ControlWindow::nextVerse() {
  // Decide if we are in Song or Bible mode?
  // Simply check active tab
  if (mainTabWidget->currentIndex() == 1) { // Songs
    if (currentSongIndex >= 0 && currentVerseIndex < verseList->count() - 1) {
      verseList->setCurrentRow(currentVerseIndex + 1);
      projectVerse(verseList->currentRow());
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
      projectVerse(verseList->currentRow());
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

  if (projection) projection->setTextVisible(isTextVisible);
  if (preview) preview->setTextVisible(isTextVisible);
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

  if (projection) projection->setBlackout(isScreenBlackened);
  if (preview) preview->setBlackout(isScreenBlackened);
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
  m_screenText[0].clear();
  m_screenText[1].clear();
  if (projection) { projection->setBlackout(false); projection->setTextVisible(true); }
  if (preview) { preview->setBlackout(false); preview->setTextVisible(true); }
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
    presentBtn->setText("◼  STOP");
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

    // Create the native handle before selecting the output display.
    projection->winId();
    if (!targetScreen || !projection->windowHandle()) {
      isPresenting = false;
      return;
    }
    projection->windowHandle()->setScreen(targetScreen);
    projection->setGeometry(targetScreen->geometry());
    projection->showFullScreen();
    projection->raise();
    projection->activateWindow();

    // Both screens already retain their content, including staged media.
    projection->setBlackout(isScreenBlackened);
    projection->setTextVisible(isTextVisible);

  } else {
    presentBtn->setText("▶  GO LIVE");
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
      if (!chooseContentScreen()) return;
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
      }
    };

    // 2-column grid. Previously single-column because the preview boxes
    // had no upper size bound, so a 2-up grid needed ~520px to avoid
    // clipping — now that each card's preview is capped to a fixed height
    // (Ignored size policy, see ThemePreviewCard), the cards scale down to
    // fit the panel width instead of forcing it wider.
    static constexpr int kThemeColumns = 2;
    int row = (index / kThemeColumns) + 1; // +1 to skip the New Theme row
    int col = index % kThemeColumns;
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

  // Reload versions
  QStringList versions = BibleManager::instance().getVersions();
  if (currentBibleVersion.isEmpty())
    currentBibleVersion = "NKJV";

  const int kVersionColumns = 6;
  int row = 0, col = 0;
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
    bibleVersionLayout->addWidget(btn, row, col);
    col++;
    if (col >= kVersionColumns) {
      col = 0;
      row++;
    }
  }
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
  for (int action = 0; action < 3; ++action) {
    auto *button = new QPushButton(QStringList{"Play", "Pause", "Stop"}[action]);
    button->setToolTip("Control video on the selected projection screen");
    connect(button, &QPushButton::clicked, this, [this, action]() {
      projection->controlLayerVideo(currentTargetLayer, action);
      preview->controlLayerVideo(currentTargetLayer, action);
    });
    fileControls->addWidget(button);
  }
  auto *muteVideo = new QCheckBox("Mute video");
  muteVideo->setChecked(true);
  muteVideo->setToolTip("Mute video audio on the selected screen. Preview audio stays muted.");
  auto *videoVolume = new QSlider(Qt::Horizontal);
  videoVolume->setRange(0, 100);
  videoVolume->setValue(100);
  videoVolume->setMaximumWidth(120);
  videoVolume->setToolTip("Video volume for the selected screen");
  auto applyVideoAudio = [this, muteVideo, videoVolume]() {
    projection->setLayerVideoAudio(currentTargetLayer, muteVideo->isChecked(), videoVolume->value() / 100.0f);
  };
  connect(muteVideo, &QCheckBox::toggled, this, [applyVideoAudio](bool) { applyVideoAudio(); });
  connect(videoVolume, &QSlider::valueChanged, this, [applyVideoAudio](int) { applyVideoAudio(); });
  auto *audioSync = new QTimer(container);
  connect(audioSync, &QTimer::timeout, this, [this, muteVideo, videoVolume]() {
    const QSignalBlocker muteBlock(muteVideo), volumeBlock(videoVolume);
    muteVideo->setChecked(projection->layerVideoMuted(currentTargetLayer));
    videoVolume->setValue(qRound(projection->layerVideoVolume(currentTargetLayer) * 100));
  });
  audioSync->start(150);
  fileControls->addWidget(muteVideo);
  fileControls->addWidget(new QLabel("Volume"));
  fileControls->addWidget(videoVolume);
  fileControls->addStretch();
  layout->addLayout(fileControls);

  // 2. Splitter: File List | Page Preview
  auto *splitter = new QSplitter(Qt::Horizontal);
  splitter->setChildrenCollapsible(false);
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
      } else if (QStringList{"mp4","mov","m4v","avi","mkv","webm"}.contains(ext)) {
        item.type = Projection::Content::MediaType::Video;
        item.pageCount = 1;
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
        label = QString(item.type == Projection::Content::MediaType::Video ? "▶ %1" : "🖼 %1").arg(fi.fileName());
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
      "All Supported (*.png *.jpg *.jpeg *.bmp *.gif *.pdf *.mp4 *.mov *.m4v *.avi *.mkv *.webm);;"
      "Images (*.png *.jpg *.jpeg *.bmp *.gif);;"
      "PDF Documents (*.pdf);;Videos (*.mp4 *.mov *.m4v *.avi *.mkv *.webm)",
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
  } else if (QStringList{"mp4","mov","m4v","avi","mkv","webm"}.contains(ext)) {
    item.type = Projection::Content::MediaType::Video;
    item.pageCount = 1;
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

  // Store imported media in the user data directory for installed apps.
  QString assetsDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/assets/media";
  QDir dir(assetsDir);
  if (!dir.exists())
    dir.mkpath(".");

  QString destPath = dir.filePath(fi.fileName());

  // If explicitly not adding the file that is already there
  if (path != destPath) {
    // Handle overwrite? Auto-rename? For now, overwrite or assume unique.
    // Let's copy.
    if (QFile::exists(destPath)) destPath = dir.filePath(QString::number(QDateTime::currentMSecsSinceEpoch()) + "_" + fi.fileName());
    if (!QFile::copy(path, destPath)) {
      QMessageBox::warning(this, "Import failed", "Could not copy the media file.");
      return;
    }
  }

  // Use the destination path for the item
  item.path = destPath;

  mediaItems.push_back(item);

  // Add to list with type indicator
  QString label = fi.fileName();
  if (item.type == Projection::Content::MediaType::Pdf) {
    label = QString("📄 %1 (%2 pages)").arg(fi.fileName()).arg(item.pageCount);
  } else {
    label = QString(item.type == Projection::Content::MediaType::Video ? "▶ %1" : "🖼 %1").arg(fi.fileName());
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
  } else if (m.type == Projection::Content::MediaType::Video) {
    auto *video = new QListWidgetItem("▶ Play video\n" + QFileInfo(m.path).fileName(), mediaPageList);
    video->setData(Qt::UserRole, 0);
    video->setSizeHint(QSize(230, 100));
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
  if (currentMediaIndex < 0 || currentMediaIndex >= mediaItems.size() || !item)
    return;

  if (!chooseContentScreen()) return;
  m_screenText[currentTargetLayer].clear();
  int page = item->data(Qt::UserRole).toInt();
  const MediaItem &m = mediaItems[currentMediaIndex];

  QImage rendered;

  if (m.type == Projection::Content::MediaType::Pdf) {
    // Render high-res PDF page for projection
    rendered = PdfRenderer::renderPage(m.path, page, QSize(1920, 1080));
  } else if (m.type == Projection::Content::MediaType::Image) {
    rendered = QImage(m.path);
  }

  if (projection) projection->setLayerMedia(currentTargetLayer, m.type, m.path, page, rendered);
  if (preview) preview->setLayerMedia(currentTargetLayer, m.type, m.path, page, rendered);
}

void ControlWindow::setupBrowserTab(QWidget *container) {
  auto *layout = new QVBoxLayout(container);
  layout->setContentsMargins(6, 6, 6, 6);
  layout->setSpacing(6);

  // --- Network manager for autocomplete ---
  m_netManager = new QNetworkAccessManager(this);

  // --- Suggestion model & completer ---
  m_suggestModel = new QStringListModel(this);
  m_lyricsCompleter = new QCompleter(m_suggestModel, this);
  m_lyricsCompleter->setCaseSensitivity(Qt::CaseInsensitive);
  m_lyricsCompleter->setCompletionMode(QCompleter::PopupCompletion);
  m_lyricsCompleter->setMaxVisibleItems(8);

  // Style the completer popup
  m_lyricsCompleter->popup()->setStyleSheet(
      "QListView { background: #1e293b; color: white; border: 1px solid "
      "#38bdf8; "
      "border-radius: 6px; padding: 4px; font-size: 13px; }"
      "QListView::item { padding: 6px 10px; }"
      "QListView::item:selected { background: #38bdf8; color: #0f172a; }");

  // --- Debounce timer for suggestions ---
  m_suggestTimer = new QTimer(this);
  m_suggestTimer->setSingleShot(true);
  m_suggestTimer->setInterval(300);

  // --- Bible version toggle at top ---
  auto *versionFrame = new QFrame();
  versionFrame->setStyleSheet(
      "QFrame { background: #1e293b; border: 1px solid #334155; "
      "border-radius: 6px; padding: 4px; }");
  auto *versionLayout = new QHBoxLayout(versionFrame);
  versionLayout->setSpacing(3);
  versionLayout->setContentsMargins(4, 4, 4, 4);

  auto *versionLabel = new QLabel("📖");
  versionLabel->setStyleSheet(
      "font-size: 14px; background: transparent; border: none;");
  versionLayout->addWidget(versionLabel);

  m_bibleVersionBtns = new QButtonGroup(this);
  m_bibleVersionBtns->setExclusive(true);
  m_selectedBibleVersion = "NKJV";

  auto &bm = BibleManager::instance();
  QStringList versions = bm.getVersions();
  for (const auto &ver : versions) {
    auto *btn = new QPushButton(ver);
    btn->setCheckable(true);
    btn->setStyleSheet(
        "QPushButton { background: #334155; color: #94a3b8; border: none; "
        "border-radius: 4px; padding: 5px 10px; font-size: 12px; "
        "font-weight: bold; }"
        "QPushButton:checked { background: #38bdf8; color: #0f172a; }"
        "QPushButton:hover { background: #475569; }");
    if (ver == m_selectedBibleVersion)
      btn->setChecked(true);
    m_bibleVersionBtns->addButton(btn);
    versionLayout->addWidget(btn);
  }
  versionLayout->addStretch();
  layout->addWidget(versionFrame);

  // Connect version toggle
  connect(m_bibleVersionBtns,
          QOverload<QAbstractButton *>::of(&QButtonGroup::buttonClicked),
          [this](QAbstractButton *btn) {
            m_selectedBibleVersion = btn->text();
            if (!m_bibleSearchInput->text().trimmed().isEmpty())
              m_bibleSearchTimer->start();
          });

  // --- Search bar ---
  auto *searchBar = new QHBoxLayout();
  searchBar->setSpacing(6);

  m_lyricsSearch = new QLineEdit();
  m_lyricsSearch->setPlaceholderText(
      "🔍 Search song lyrics... (e.g. 'Amazing Grace')");
  m_lyricsSearch->setStyleSheet(
      "QLineEdit { background: white; color: #1e293b; border: 2px solid "
      "#38bdf8; "
      "border-radius: 8px; padding: 10px 14px; font-size: 15px; "
      "font-weight: bold; }"
      "QLineEdit:focus { border-color: #0ea5e9; }");
  m_lyricsSearch->setCompleter(m_lyricsCompleter);

  auto *searchBtn = new QPushButton("🔍 Search");
  searchBtn->setObjectName("primaryBtn");
  searchBtn->setStyleSheet(
      "QPushButton { background: #38bdf8; color: #0f172a; border-radius: 8px; "
      "padding: 10px 20px; font-size: 14px; font-weight: bold; }"
      "QPushButton:hover { background: #0ea5e9; }");

  m_addToSongsBtn = new QPushButton("➕ Add to Songs");
  m_addToSongsBtn->setStyleSheet(
      "QPushButton { background: #22c55e; color: white; border-radius: 8px; "
      "padding: 10px 20px; font-size: 14px; font-weight: bold; }"
      "QPushButton:hover { background: #16a34a; }"
      "QPushButton:disabled { background: #334155; color: #94a3b8; }");

  auto *aiKeyBtn = new QPushButton("🔑");
  aiKeyBtn->setToolTip("Manage AI keys used to clean up imported lyrics "
                       "(free Groq key, and/or paid Anthropic key)");
  aiKeyBtn->setFixedWidth(40);
  aiKeyBtn->setStyleSheet(
      "QPushButton { background: #334155; color: white; border-radius: 8px; "
      "font-size: 14px; }"
      "QPushButton:hover { background: #475569; }");
  connect(aiKeyBtn, &QPushButton::clicked, this,
          [this]() { promptForAiApiKeys(); });

  searchBar->addWidget(m_lyricsSearch, 1);
  searchBar->addWidget(searchBtn);
  searchBar->addWidget(m_addToSongsBtn);
  searchBar->addWidget(aiKeyBtn);
  layout->addLayout(searchBar);

  // --- Status label ---
  m_browserStatus =
      new QLabel("Type a song name above — suggestions appear as you type");
  m_browserStatus->setAlignment(Qt::AlignCenter);
  m_browserStatus->setStyleSheet(
      "color: #94a3b8; font-size: 12px; padding: 2px;");
  layout->addWidget(m_browserStatus);

  // --- Content area: Web View (left) + Bible Panel (right) ---
  auto *contentSplitter = new QSplitter(Qt::Horizontal, container);
  contentSplitter->setHandleWidth(3);
  contentSplitter->setChildrenCollapsible(false);
  contentSplitter->setStyleSheet("QSplitter::handle { background: #334155; }");

  // == Left: Web View ==
  m_webView = new QWebEngineView(contentSplitter);
  m_webView->setMinimumHeight(300);
  m_webView->setMinimumWidth(300);
  m_webView->page()->setBackgroundColor(Qt::white);
  // Qt WebEngine's default UA includes a "QtWebEngine/x.x" token that
  // identifies it as an embedded/automated browser rather than a normal
  // desktop one — Google's abuse detection flags that and returns 429s
  // (visible as "Code: 429" in the status bar) far more readily than it
  // would for a real Chrome UA.
  m_webView->page()->profile()->setHttpUserAgent(
      "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 "
      "(KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36");

  // Landing page
  m_webView->setHtml(
      "<html><body style='background:white;color:#333;font-family:system-ui,"
      "sans-serif;display:flex;align-items:center;justify-content:center;"
      "height:100vh;margin:0;'>"
      "<div style='text-align:center;'>"
      "<div style='font-size:64px;margin-bottom:16px;'>🎵</div>"
      "<h2 style='color:#38bdf8;margin:0 0 8px;'>Lyrics Search</h2>"
      "<p style='color:#94a3b8;max-width:400px;line-height:1.5;'>"
      "Start typing a song name in the search bar above.<br>"
      "Suggestions will appear in <b>real-time</b>.<br>"
      "Press <b>Enter</b> or click <b>Search</b> to find lyrics.</p>"
      "</div></body></html>");

  contentSplitter->addWidget(m_webView);

  // == Right: Bible Search Panel ==
  auto *biblePanel = new QWidget(contentSplitter);
  auto *bibleLayout = new QVBoxLayout(biblePanel);
  bibleLayout->setContentsMargins(6, 6, 6, 6);
  bibleLayout->setSpacing(6);

  // Panel header
  auto *panelHeader = new QLabel("📖 Bible Verse Lookup");
  panelHeader->setStyleSheet(
      "font-size: 15px; font-weight: bold; color: #38bdf8; padding: 4px;");
  bibleLayout->addWidget(panelHeader);

  // Bible search input
  m_scriptureLookup = new ScriptureLookup(this);
  m_bibleSearchInput = new QLineEdit();
  m_bibleSearchInput->setPlaceholderText(
      "Type reference or phrase (e.g. John 3:16, love)");
  m_bibleSearchInput->setStyleSheet(
      "QLineEdit { background: #1e293b; color: white; border: 1px solid "
      "#334155; border-radius: 6px; padding: 8px 10px; font-size: 13px; }"
      "QLineEdit:focus { border-color: #38bdf8; }");
  bibleLayout->addWidget(m_bibleSearchInput);

  // Debounce timer for Bible search
  m_bibleSearchTimer = new QTimer(this);
  m_bibleSearchTimer->setSingleShot(true);
  m_bibleSearchTimer->setInterval(900);

  // Results list
  m_bibleResultsList = new QListWidget();
  connect(m_bibleResultsList, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
    const QString text = item->data(Qt::UserRole).toString();
    if (!text.isEmpty()) projectBibleVerse(text);
  });
  m_bibleResultsList->setWordWrap(true);
  m_bibleResultsList->setStyleSheet(
      "QListWidget { background: #0f172a; border: 1px solid #334155; "
      "border-radius: 6px; }"
      "QListWidget::item { padding: 0; border-bottom: 1px solid #1e293b; }");
  bibleLayout->addWidget(m_bibleResultsList, 1);

  contentSplitter->addWidget(biblePanel);

  // Set splitter proportions: 60% web, 40% bible
  contentSplitter->setSizes({600, 400});

  layout->addWidget(contentSplitter, 1);

  // ========== BIBLE SEARCH LOGIC (Local) ==========

  auto doBibleSearch = [this]() {
    m_scriptureLookup->cancel();
    QString query = m_bibleSearchInput->text().trimmed();
    if (query.startsWith('@')) {
      query = query.mid(1).trimmed();
      m_bibleResultsList->clear();
      if (query.size() < 2) return;
      auto *loading = new QListWidgetItem("AI is finding scripture…", m_bibleResultsList);
      loading->setFlags(Qt::NoItemFlags);
      m_scriptureLookup->search(query, m_selectedBibleVersion, [this](std::vector<BibleVerse> results, const QString &error) {
        m_bibleResultsList->clear();
        for (const auto &verse : results) {
          const QString ref = BibleManager::instance().getLocalizedBookName(verse.book, verse.version) + QString(" %1:%2 (%3)").arg(verse.chapter).arg(verse.verse).arg(verse.version);
          auto *item = new QListWidgetItem(ref + "\n" + verse.text, m_bibleResultsList);
          item->setData(Qt::UserRole, verse.text + "\n\n" + ref);
        }
        if (!error.isEmpty()) { auto *item = new QListWidgetItem(error, m_bibleResultsList); item->setFlags(Qt::NoItemFlags); }
      });
      return;
    }
    if (query.isEmpty()) {
      m_bibleResultsList->clear();
      return;
    }

    auto &bible = BibleManager::instance();
    auto results = bible.search(query, m_selectedBibleVersion);

    m_bibleResultsList->clear();

    if (results.empty()) {
      auto *emptyItem =
          new QListWidgetItem("No verses found for \"" + query + "\"");
      emptyItem->setFlags(Qt::NoItemFlags);
      emptyItem->setForeground(QColor("#64748b"));
      m_bibleResultsList->addItem(emptyItem);
      return;
    }

    for (const auto &verse : results) {
      auto *itemWidget = new QWidget();
      itemWidget->setCursor(Qt::PointingHandCursor);
      auto *itemLayout = new QVBoxLayout(itemWidget);
      itemLayout->setContentsMargins(8, 6, 8, 6);
      itemLayout->setSpacing(3);

      // Reference label
      QString ref = QString("%1 %2:%3")
                        .arg(verse.book)
                        .arg(verse.chapter)
                        .arg(verse.verse);
      auto *refLabel = new QLabel(ref);
      refLabel->setStyleSheet(
          "font-weight: bold; color: #38bdf8; font-size: 12px;");
      itemLayout->addWidget(refLabel);

      // Text preview — click to project in current version
      QString preview = verse.text.left(150);
      if (verse.text.length() > 150)
        preview += "...";
      auto *textLabel = new QLabel(preview);
      textLabel->setWordWrap(true);
      textLabel->setStyleSheet(
          "color: #cbd5e1; font-size: 12px; cursor: pointer;");
      itemLayout->addWidget(textLabel);

      // Capture book/chapter/verse for version buttons
      QString book = verse.book;
      int chapter = verse.chapter;
      int verseNum = verse.verse;

      // Click text to project in current version
      QString defaultText = QString("%1\n\n%2 %3:%4 (%5)")
                                .arg(verse.text)
                                .arg(book)
                                .arg(chapter)
                                .arg(verseNum)
                                .arg(verse.version);
      connect(textLabel, &QLabel::linkActivated,
              [this, defaultText]() { projectBibleVerse(defaultText); });

      // Version buttons row
      auto *versionRow = new QHBoxLayout();
      versionRow->setSpacing(3);

      auto &bible = BibleManager::instance();
      QStringList allVersions = bible.getVersions();
      for (const auto &ver : allVersions) {
        auto *verBtn = new QPushButton(ver);
        verBtn->setFixedHeight(22);
        verBtn->setStyleSheet(
            "QPushButton { background: #334155; color: #94a3b8; border: none; "
            "border-radius: 3px; padding: 2px 6px; font-size: 10px; "
            "font-weight: bold; }"
            "QPushButton:hover { background: #8b5cf6; color: white; }");

        connect(verBtn, &QPushButton::clicked,
                [this, book, chapter, verseNum, ver, &bible]() {
                  QString text =
                      bible.getVerseText(book, chapter, verseNum, ver);
                  if (text.isEmpty()) {
                    m_browserStatus->setText("⚠️ Verse not found in " + ver);
                    m_browserStatus->setStyleSheet(
                        "color: #f59e0b; font-size: 12px;");
                    return;
                  }
                  QString fullText = QString("%1\n\n%2 %3:%4 (%5)")
                                         .arg(text)
                                         .arg(book)
                                         .arg(chapter)
                                         .arg(verseNum)
                                         .arg(ver);
                  projectBibleVerse(fullText);
                  m_browserStatus->setText(
                      "📖 Projected: " + book + " " + QString::number(chapter) +
                      ":" + QString::number(verseNum) + " (" + ver + ")");
                  m_browserStatus->setStyleSheet(
                      "color: #22c55e; font-size: 12px;");
                });

        versionRow->addWidget(verBtn);
      }
      versionRow->addStretch();
      itemLayout->addLayout(versionRow);

      auto *listItem = new QListWidgetItem();
      listItem->setSizeHint(itemWidget->sizeHint());
      m_bibleResultsList->addItem(listItem);
      m_bibleResultsList->setItemWidget(listItem, itemWidget);
    }
  };

  // Search on Enter
  connect(m_bibleSearchInput, &QLineEdit::returnPressed, doBibleSearch);

  // Search on debounce (e.g. version change)
  connect(m_bibleSearchTimer, &QTimer::timeout, doBibleSearch);

  // Re-search on text change (debounced)
  connect(m_bibleSearchInput, &QLineEdit::textChanged,
          [this](const QString &text) {
            m_scriptureLookup->cancel();
            m_bibleSearchTimer->stop();
            if (text.trimmed().length() < 3) {
              m_bibleResultsList->clear();
              return;
            }
            m_bibleSearchTimer->start();
          });

  // ========== REAL-TIME AUTOCOMPLETE ==========

  // On text change → debounce → fetch suggestions
  connect(m_lyricsSearch, &QLineEdit::textChanged, [this](const QString &text) {
    if (text.trimmed().length() < 2) {
      m_suggestModel->setStringList({});
      return;
    }
    m_suggestTimer->start(); // restart debounce
  });

  // On debounce timeout → fetch Google suggestions
  connect(m_suggestTimer, &QTimer::timeout, [this]() {
    QString query = m_lyricsSearch->text().trimmed();
    if (query.isEmpty())
      return;

    // Google Suggest API (returns JSON with Firefox client)
    QString url =
        "https://suggestqueries.google.com/complete/search?client=firefox&q=" +
        QUrl::toPercentEncoding(query + " lyrics");

    QNetworkRequest req{QUrl(url)};
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) "
                  "AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0.0.0 "
                  "Safari/537.36");

    QNetworkReply *reply = m_netManager->get(req);
    connect(reply, &QNetworkReply::finished, [this, reply]() {
      reply->deleteLater();
      if (reply->error() != QNetworkReply::NoError)
        return;

      QByteArray data = reply->readAll();
      // Parse: ["query", ["suggestion1", "suggestion2", ...]]
      QJsonDocument doc = QJsonDocument::fromJson(data);
      if (!doc.isArray())
        return;

      QJsonArray arr = doc.array();
      if (arr.size() < 2 || !arr[1].isArray())
        return;

      QStringList suggestions;
      QJsonArray sugArr = arr[1].toArray();
      for (int i = 0; i < sugArr.size() && i < 8; ++i) {
        QString s = sugArr[i].toString();
        // Clean up: remove " lyrics" suffix for cleaner display
        s.replace(" lyrics", "", Qt::CaseInsensitive);
        suggestions << s;
      }

      m_suggestModel->setStringList(suggestions);

      // Force popup to show if we have suggestions
      if (!suggestions.isEmpty() && m_lyricsSearch->hasFocus()) {
        m_lyricsCompleter->complete();
      }
    });
  });

  // ========== SEARCH ==========

  auto doSearch = [this]() {
    // Close the autocomplete popup (a separate native window) before
    // navigating — leaving it open while the web view's process grabs
    // window focus for the new page is what was making the whole app
    // window bounce, as if two windows were fighting to become key.
    m_lyricsCompleter->popup()->hide();

    QString query = m_lyricsSearch->text().trimmed();
    if (query.isEmpty())
      return;
    if (!query.contains("lyrics", Qt::CaseInsensitive))
      query += " lyrics";
    m_browserStatus->setText("🔍 Searching: " + query);
    m_browserStatus->setStyleSheet("color: #38bdf8; font-size: 12px;");
    m_webView->setUrl(QUrl("https://www.google.com/search?q=" +
                           QUrl::toPercentEncoding(query)));
  };

  connect(searchBtn, &QPushButton::clicked, doSearch);
  connect(m_lyricsSearch, &QLineEdit::returnPressed, doSearch);

  // Also search when a suggestion is selected
  connect(m_lyricsCompleter,
          QOverload<const QString &>::of(&QCompleter::activated),
          [this, doSearch](const QString &text) {
            m_lyricsSearch->setText(text);
            doSearch();
          });

  // ========== LOAD STATUS ==========

  connect(m_webView, &QWebEngineView::loadStarted, [this]() {
    m_browserStatus->setText("⏳ Loading...");
    m_browserStatus->setStyleSheet("color: #38bdf8; font-size: 12px;");
  });

  connect(
      m_webView->page(), &QWebEnginePage::loadingChanged,
      [this](const QWebEngineLoadingInfo &info) {
        if (info.status() == QWebEngineLoadingInfo::LoadSucceededStatus) {
          m_browserStatus->setText("✅ Page loaded — find the lyrics and click "
                                   "\"➕ Add to Songs\" to import");
          m_browserStatus->setStyleSheet("color: #22c55e; font-size: 12px;");
        } else if (info.status() == QWebEngineLoadingInfo::LoadFailedStatus) {
          int code = static_cast<int>(info.errorCode());
          QString errorMsg =
              code == 429
                  ? "❌ Google is rate-limiting search requests right now — "
                    "wait a minute and try again"
                  : QString("❌ Error: %1 (Code: %2)")
                        .arg(info.errorString())
                        .arg(code);
          m_browserStatus->setText(errorMsg);
          m_browserStatus->setStyleSheet("color: #ef4444; font-size: 12px;");
          qDebug() << "Browser load error:" << info.errorString()
                   << "Code:" << info.errorCode() << "URL:" << info.url();
        }
      });

  // ========== ADD TO SONGS ==========

  connect(m_addToSongsBtn, &QPushButton::clicked, [this]() {
    // Extract text from the current page via JavaScript
    m_webView->page()->runJavaScript(
        "document.body.innerText", [this](const QVariant &result) {
          QString pageText = result.toString();

          // Determine title and artist from search bar
          QString rawSearch = m_lyricsSearch->text().trimmed();
          rawSearch.replace(
              QRegularExpression("\\s+lyrics$",
                                 QRegularExpression::CaseInsensitiveOption),
              "");

          QString title = rawSearch;
          QString artist;

          // Parse "by" keyword: "song title by artist name"
          int byIdx = rawSearch.lastIndexOf(" by ", -1, Qt::CaseInsensitive);
          if (byIdx > 0) {
            title = rawSearch.left(byIdx).trimmed();
            artist = rawSearch.mid(byIdx + 4).trimmed();
          }

          cleanLyricsWithAI(pageText, title, artist);
        });
  });
}

// Old regex/word-list based cleanup — kept as the offline fallback for when
// no Anthropic API key is configured or the request fails. cleanLyricsWithAI
// tries Claude first, which does a much better job of judging what's
// boilerplate vs. lyrics than a static noise-word list ever can.
QString ControlWindow::heuristicCleanLyrics(const QString &pageText) {
  QStringList lines = pageText.split('\n', Qt::SkipEmptyParts);
  QStringList cleanedLines;

  // Boilerplate filters
  static const QStringList noiseWords = {"Share",
                                         "Follow",
                                         "Contact",
                                         "Privacy",
                                         "Related",
                                         "Discover",
                                         "Video",
                                         "Lyrics",
                                         "Submit",
                                         "Requests",
                                         "Chords",
                                         "Download",
                                         "Comment",
                                         "Reply",
                                         "Anonymous",
                                         "Jan ",
                                         "Feb ",
                                         "Mar ",
                                         "Apr ",
                                         "May ",
                                         "Jun ",
                                         "Jul ",
                                         "Aug ",
                                         "Sep ",
                                         "Oct ",
                                         "Nov ",
                                         "Dec ",
                                         "Spotify",
                                         "Apple ",
                                         "Deezer",
                                         "Genius",
                                         "Musixmatch",
                                         "TuneCore",
                                         "Hymnary",
                                         "Boomplay",
                                         "TikTok",
                                         "YouTube",
                                         "Gafkosoft",
                                         "Sifa ",
                                         "Home",
                                         "About",
                                         "Search",
                                         "Gospel",
                                         "Christian",
                                         "Church",
                                         "Worship",
                                         "Twitter",
                                         "Facebook",
                                         "Instagram",
                                         "WhatsApp",
                                         "Telegram",
                                         "Email",
                                         "Subscribe",
                                         "Type your email",
                                         "©",
                                         "all rights",
                                         "Discover more",
                                         "View Lyrics",
                                         "Instrumental",
                                         "Interlude",
                                         "Intro",
                                         "Outro",
                                         "Chorus",
                                         "Verse",
                                         "Bridge",
                                         "Pre-Chorus",
                                         "Refrain",
                                         "Tag",
                                         "Ending",
                                         "Tafsiri",
                                         "Maoni",
                                         "Usaidizi",
                                         "Vichujio",
                                         "Mada",
                                         "Matumizi ya AI",
                                         "Zote",
                                         "Picha",
                                         "Wavuti",
                                         "Vitabu",
                                         "Mengineyo",
                                         "Zana",
                                         "Dibaji",
                                         "Maneno ya wimbo",
                                         "Matokeo ya Utafutaji",
                                         "Kuhusu",
                                         "Msanii:",
                                         "Albamu:",
                                         "Ilitolewa:",
                                         "Sikiliza",
                                         "Watunzi:",
                                         "Viungo",
                                         "Nenda kwenye",
                                         "maudhui",
                                         "Ingia",
                                         "Kwa nini",
                                         "Msaada",
                                         "Play Audio",
                                         "Suggest correction",
                                         "Suggest a correction",
                                         "Request song info"};

  for (QString line : lines) {
    line = line.trimmed();
    if (line.isEmpty())
      continue;

    // STOP MARKERS: If we hit these, we are out of the lyrics section.
    // Keyword lists like this only ever cover the sites they were tuned
    // against — "Chart Methodology"/"Copyright & Corrections"/"Browse All"
    // are common enough footer wording across lyrics-aggregator sites in
    // general to be worth hardcoding too, not just this one site's exact
    // labels.
    static const QStringList stopMarkers = {"Watu pia wanatafuta",
                                            "Tafsiri kwa Kiswahili",
                                            "Video",
                                            "Sikiliza",
                                            "Discover more",
                                            "Explore more",
                                            "People also ask",
                                            "Maneno ya wimbo wa",
                                            "Watunzi",
                                            "Chanzo",
                                            "Chart Methodology",
                                            "Copyright &",
                                            "Browse All",
                                            "Charts by"};
    bool hitStop = false;
    for (const auto &stop : stopMarkers) {
      if (line.startsWith(stop, Qt::CaseInsensitive)) {
        hitStop = true;
        break;
      }
    }
    if (hitStop)
      break;

    bool isNoise = false;

    // General structural signals, independent of any specific site's exact
    // wording: real lyric lines are essentially never ALL-CAPS section
    // headers ("CHARTS BY COUNTRY"), colon-terminated metadata labels
    // ("ARTIST:", "UPDATED:"), or bare breadcrumb separators ("/").
    QString lettersOnly = line;
    lettersOnly.remove(QRegularExpression("[^A-Za-z]"));
    bool isAllCaps = lettersOnly.length() > 2 &&
                     !line.contains(QRegularExpression("[a-z]"));
    bool isLabelLine = line.endsWith(':') && line.length() < 40;
    bool isBareSeparator =
        line == "/" || QRegularExpression("^[/|•\\-–—]+$").match(line).hasMatch();
    if (isAllCaps || isLabelLine || isBareSeparator) {
      isNoise = true;
    }

    // Filter noise patterns
    if (!isNoise) {
      for (const auto &noise : noiseWords) {
        if (line.compare(noise, Qt::CaseInsensitive) == 0 ||
            line.startsWith(noise + ":", Qt::CaseInsensitive) ||
            line.startsWith(noise + " ", Qt::CaseInsensitive)) {
          isNoise = true;
          break;
        }

        if (line.length() < 60 &&
            line.contains(noise, Qt::CaseInsensitive)) {
          if (line.contains("Jesus", Qt::CaseInsensitive) ||
              line.contains("Lord", Qt::CaseInsensitive) ||
              line.contains("You have", Qt::CaseInsensitive)) {
            continue;
          }
          isNoise = true;
          break;
        }
      }
    }

    // Filter out search snippet artifacts
    line.remove("...");
    line.remove("·");
    line = line.trimmed();
    if (line.isEmpty())
      continue;

    if (line.contains(QRegularExpression("^\\d+\\s+Likes?$")) ||
        line.contains(QRegularExpression("^\\d+\\s+Views?$")) ||
        line.contains(QRegularExpression("^\\d+\\s+\\w+\\s+\\d{4}$"))) {
      isNoise = true;
    }

    if (!isNoise)
      cleanedLines << line;
  }

  // Group into stanzas and deduplicate
  QStringList finalVerses;
  QSet<QString> seenVerses;

  QString currentStanza;
  int lineCountInStanza = 0;

  for (int i = 0; i < cleanedLines.size(); ++i) {
    currentStanza += cleanedLines[i] + "\n";
    lineCountInStanza++;

    bool isLastLine = (i == cleanedLines.size() - 1);
    // Group into 4-line blocks or end of song
    if (lineCountInStanza >= 4 || isLastLine) {
      QString verse = currentStanza.trimmed();
      QString normalized =
          verse.toLower().remove(QRegularExpression("[^a-z]"));

      if (!seenVerses.contains(normalized) &&
          normalized.length() > 15) {
        finalVerses << verse;
        seenVerses.insert(normalized);
      }
      currentStanza.clear();
      lineCountInStanza = 0;
    }
  }

  // fallback if stanza splitting was too aggressive (rare)
  if (finalVerses.isEmpty()) {
    QStringList rawBlocks = pageText.split(
        QRegularExpression("\\n\\s*\\n"), Qt::SkipEmptyParts);
    for (const auto &block : rawBlocks) {
      QString verse = block.trimmed();
      QString normalized =
          verse.toLower().remove(QRegularExpression("[^a-z0-9]"));
      if (!seenVerses.contains(normalized) && normalized.length() > 5) {
        finalVerses << verse;
        seenVerses.insert(normalized);
      }
    }
  }

  return finalVerses.join("\n\n");
}

void ControlWindow::promptForAiApiKeys() {
  QSettings settings;

  QDialog dlg(this);
  dlg.setWindowTitle("AI Lyrics Cleanup — API Keys");
  dlg.setMinimumWidth(440);
  dlg.setStyleSheet(
      "QDialog { background: #0f172a; }"
      "QLabel { color: #e2e8f0; }"
      "QLineEdit { background: #1e293b; color: white; border: 1px "
      "solid #334155; border-radius: 6px; padding: 8px; }"
      "QPushButton { background: #334155; color: white; "
      "border-radius: 6px; padding: 8px 16px; font-weight: bold; }"
      "QPushButton:hover { background: #475569; }");

  auto *layout = new QVBoxLayout(&dlg);
  layout->setSpacing(10);

  auto *info = new QLabel(
      "Lyrics import tries these in order, and falls back automatically "
      "if one fails or isn't set: your free Groq key, then your "
      "Bible Trivia AI backend (no key needed), then Claude. Leave a "
      "field blank to skip it.");
  info->setWordWrap(true);
  info->setStyleSheet("color: #94a3b8; font-size: 12px;");
  layout->addWidget(info);

  layout->addWidget(new QLabel("Groq API Key (free — console.groq.com/keys):"));
  auto *groqInput = new QLineEdit(settings.value("AI/GroqApiKey").toString());
  groqInput->setEchoMode(QLineEdit::Password);
  layout->addWidget(groqInput);

  layout->addWidget(
      new QLabel("Anthropic API Key (paid — console.anthropic.com/settings/keys):"));
  auto *anthropicInput =
      new QLineEdit(settings.value("AI/AnthropicApiKey").toString());
  anthropicInput->setEchoMode(QLineEdit::Password);
  layout->addWidget(anthropicInput);

  auto *btnRow = new QHBoxLayout();
  auto *cancelBtn = new QPushButton("Cancel");
  auto *saveBtn = new QPushButton("Save");
  saveBtn->setStyleSheet("QPushButton { background: #22c55e; } "
                        "QPushButton:hover { background: #16a34a; }");
  btnRow->addStretch();
  btnRow->addWidget(cancelBtn);
  btnRow->addWidget(saveBtn);
  layout->addLayout(btnRow);

  connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
  connect(saveBtn, &QPushButton::clicked, &dlg, &QDialog::accept);

  if (dlg.exec() == QDialog::Accepted) {
    settings.setValue("AI/GroqApiKey", groqInput->text().trimmed());
    settings.setValue("AI/AnthropicApiKey", anthropicInput->text().trimmed());
  }
}

void ControlWindow::cleanLyricsWithAI(const QString &rawText,
                                      const QString &title,
                                      const QString &artist) {
  m_addToSongsBtn->setEnabled(false);
  m_browserStatus->setText("🤖 Cleaning lyrics...");
  m_browserStatus->setStyleSheet("color: #38bdf8; font-size: 12px;");

  // Cap the input — lyric pages rarely need more than this to capture the
  // actual song, and it keeps every request below small and cheap.
  QString trimmedInput = rawText.left(8000);

  QString prompt =
      "You are cleaning up song lyrics scraped from a web page that also "
      "contains unrelated site content (navigation, ads, related songs, "
      "artist bios, comments, \"people also search for\", metadata like "
      "\"Artist:\"/\"Album:\", chart links, copyright notices, etc).\n\n"
      "Extract ONLY the actual song lyrics from the text below, formatted "
      "for on-screen projection:\n"
      "- Remove everything that is not part of the song itself.\n"
      "- Keep the lyrics' own line breaks; separate stanzas/sections with "
      "a single blank line.\n"
      "- If a line or short phrase is meant to be sung twice/three times "
      "in a row, write it once followed by \"x2\" / \"x3\" instead of "
      "writing it out multiple times.\n"
      "- If the same verse or chorus block recurs again later in the song "
      "(not just an immediately-repeated line — the whole section coming "
      "back around after other verses), include it only the FIRST time. "
      "Do not retype it again at each later recurrence — whoever is "
      "operating the projector will just re-select that same earlier "
      "slide when it's time to show that section again, so repeating it "
      "in this list only adds clutter, not new slides.\n"
      "- This still counts as the SAME recurring block even when it has "
      "small ad-libs layered on top — call-and-response tags like \"(all "
      "the brothers)\"/\"(all the ladies)\", interjections like \"Eh\"/"
      "\"Eze\", or minor wording drift like \"You made possible\" vs "
      "\"You've made possible\". A chorus sung 8 times with slightly "
      "different ad-libs each time is still ONE chorus, appearing ONCE "
      "in your output — not 8 near-duplicate blocks. Only keep a repeat "
      "as its own separate block if it introduces genuinely new lyric "
      "content beyond ad-libs (e.g. \"(what the doctors could not do)\" "
      "followed by new lines is a distinct call-and-response verse, not "
      "a chorus repeat).\n"
      "- Output ONLY the cleaned lyrics text — no headers, labels, or "
      "commentary of any kind.\n\n"
      "Raw scraped page text:\n\"\"\"\n" +
      trimmedInput + "\n\"\"\"";

  tryGroqCleanup(prompt, rawText, title, artist);
}

// 1. Free — user's own Groq key, called directly from this desktop app (no
// backend needed, same pattern as the Anthropic path below).
void ControlWindow::tryGroqCleanup(const QString &prompt,
                                   const QString &rawText,
                                   const QString &title,
                                   const QString &artist) {
  QSettings settings;
  QString groqKey = settings.value("AI/GroqApiKey").toString();
  if (groqKey.isEmpty()) {
    tryBibleTriviaBackendCleanup(prompt, rawText, title, artist);
    return;
  }

  QJsonObject sysMsg;
  sysMsg["role"] = "system";
  sysMsg["content"] = "You clean up messy scraped web text into plain song "
                      "lyrics. Follow the user's formatting instructions "
                      "exactly.";
  QJsonObject userMsg;
  userMsg["role"] = "user";
  userMsg["content"] = prompt;

  QJsonObject body;
  body["model"] = "openai/gpt-oss-120b"; // same model bible_trivia uses
  body["temperature"] = 0.3;
  body["max_tokens"] = 4096;
  body["messages"] = QJsonArray{sysMsg, userMsg};

  QNetworkRequest req{
      QUrl("https://api.groq.com/openai/v1/chat/completions")};
  req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
  req.setRawHeader("Authorization", ("Bearer " + groqKey).toUtf8());

  QNetworkReply *reply =
      m_netManager->post(req, QJsonDocument(body).toJson());
  connect(reply, &QNetworkReply::finished, this,
          [this, reply, prompt, rawText, title, artist]() {
            reply->deleteLater();
            if (reply->error() == QNetworkReply::NoError) {
              QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
              QJsonArray choices = doc.object().value("choices").toArray();
              if (!choices.isEmpty()) {
                QString cleaned = choices.first()
                                      .toObject()
                                      .value("message")
                                      .toObject()
                                      .value("content")
                                      .toString()
                                      .trimmed();
                if (!cleaned.isEmpty()) {
                  finishLyricsCleanup(title, artist, cleaned, "Groq (free)");
                  return;
                }
              }
            }
            tryBibleTriviaBackendCleanup(prompt, rawText, title, artist);
          });
}

// 2. Free — your already-deployed bible_trivia Flask backend, which keeps
// its own Groq/Hugging Face key server-side. No key needed here at all,
// but it depends on that server being deployed and reachable.
void ControlWindow::tryBibleTriviaBackendCleanup(const QString &prompt,
                                                 const QString &rawText,
                                                 const QString &title,
                                                 const QString &artist) {
  QJsonObject body;
  body["message"] = prompt;
  body["history"] = QJsonArray{};
  // Low temperature: this is a precise formatting/extraction task, not
  // creative writing — the default 0.7 (tuned for this endpoint's other
  // use as a general Bible-chat assistant) made instruction-following
  // inconsistent, e.g. the "don't repeat a recurring chorus" rule getting
  // applied on some runs and ignored on others for the identical prompt.
  body["temperature"] = 0.2;

  QNetworkRequest req{
      QUrl("https://abytrivia.pythonanywhere.com/api/ai/chat")};
  req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

  QNetworkReply *reply =
      m_netManager->post(req, QJsonDocument(body).toJson());
  connect(reply, &QNetworkReply::finished, this,
          [this, reply, prompt, rawText, title, artist]() {
            reply->deleteLater();
            if (reply->error() == QNetworkReply::NoError) {
              QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
              QJsonObject obj = doc.object();
              if (obj.value("success").toBool()) {
                QString cleaned = obj.value("response").toString().trimmed();
                if (!cleaned.isEmpty()) {
                  finishLyricsCleanup(title, artist, cleaned,
                                      "your Bible Trivia AI backend");
                  return;
                }
              }
            }
            tryAnthropicCleanup(prompt, rawText, title, artist);
          });
}

// 3. Paid — Anthropic, if the user has set a key. Highest quality, tried
// last since the two options above are free.
void ControlWindow::tryAnthropicCleanup(const QString &prompt,
                                        const QString &rawText,
                                        const QString &title,
                                        const QString &artist) {
  QSettings settings;
  QString apiKey = settings.value("AI/AnthropicApiKey").toString();
  if (apiKey.isEmpty()) {
    finishWithHeuristicCleanup(rawText, title, artist);
    return;
  }

  QJsonObject outputConfig;
  outputConfig["effort"] = "low";

  QJsonObject userMsg;
  userMsg["role"] = "user";
  userMsg["content"] = prompt;

  QJsonObject body;
  body["model"] = "claude-opus-5";
  body["max_tokens"] = 4096;
  body["thinking"] = QJsonObject{{"type", "disabled"}};
  body["output_config"] = outputConfig;
  body["messages"] = QJsonArray{userMsg};

  QNetworkRequest req{QUrl("https://api.anthropic.com/v1/messages")};
  req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
  req.setRawHeader("x-api-key", apiKey.toUtf8());
  req.setRawHeader("anthropic-version", "2023-06-01");

  QNetworkReply *reply =
      m_netManager->post(req, QJsonDocument(body).toJson());
  connect(reply, &QNetworkReply::finished, this,
          [this, reply, rawText, title, artist]() {
            reply->deleteLater();

            if (reply->error() != QNetworkReply::NoError) {
              finishWithHeuristicCleanup(rawText, title, artist);
              return;
            }

            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            QJsonArray content = doc.object().value("content").toArray();
            QString cleaned;
            if (!content.isEmpty())
              cleaned =
                  content.first().toObject().value("text").toString().trimmed();

            if (cleaned.isEmpty()) {
              finishWithHeuristicCleanup(rawText, title, artist);
              return;
            }

            finishLyricsCleanup(title, artist, cleaned, "Claude");
          });
}

void ControlWindow::finishLyricsCleanup(const QString &title,
                                        const QString &artist,
                                        const QString &cleanedText,
                                        const QString &sourceLabel) {
  m_addToSongsBtn->setEnabled(true);
  m_browserStatus->setText("✨ Lyrics cleaned by " + sourceLabel);
  m_browserStatus->setStyleSheet("color: #22c55e; font-size: 12px;");
  showImportLyricsDialog(title, artist, cleanedText, true);
}

void ControlWindow::finishWithHeuristicCleanup(const QString &rawText,
                                               const QString &title,
                                               const QString &artist) {
  m_addToSongsBtn->setEnabled(true);
  m_browserStatus->setText(
      "⚠️ AI cleanup unavailable — using basic local cleanup (add a free "
      "Groq key via 🔑 for better results)");
  m_browserStatus->setStyleSheet("color: #f59e0b; font-size: 12px;");
  showImportLyricsDialog(title, artist, heuristicCleanLyrics(rawText),
                         false);
}

void ControlWindow::showImportLyricsDialog(const QString &title,
                                           const QString &artist,
                                           const QString &lyricsText,
                                           bool aiCleaned) {
  QDialog dlg(this);
  dlg.setWindowTitle("Import Lyrics to Song Library");
  dlg.setMinimumSize(600, 600);
  dlg.setStyleSheet(
      "QDialog { background: #0f172a; }"
      "QLabel { color: #e2e8f0; }"
      "QLineEdit { background: #1e293b; color: white; border: 1px "
      "solid #334155; border-radius: 6px; padding: 10px; }"
      "QTextEdit { background: #1e293b; color: white; border: 1px "
      "solid #334155; border-radius: 6px; padding: 12px; "
      "font-size: 14px; line-height: 1.4; }"
      "QPushButton { background: #22c55e; color: white; "
      "border-radius: 6px; padding: 10px 20px; font-weight: bold; "
      "font-size: 14px; }"
      "QPushButton:hover { background: #16a34a; }");

  auto *dlgLayout = new QVBoxLayout(&dlg);
  dlgLayout->setSpacing(12);

  // Info badge
  auto *badge = new QLabel(aiCleaned
                               ? "✨ Cleaned by Claude"
                               : "🧹 Cleaned locally — set an AI key (🔑) "
                                 "for smarter cleanup");
  badge->setStyleSheet(
      "background: #1e293b; color: #38bdf8; border: 1px solid #334155; "
      "padding: 4px 10px; border-radius: 12px; font-size: 11px; "
      "font-weight: bold;");
  badge->setAlignment(Qt::AlignCenter);
  dlgLayout->addWidget(badge);

  // Title
  dlgLayout->addWidget(new QLabel("Song Title:"));
  auto *titleInput = new QLineEdit(title);
  dlgLayout->addWidget(titleInput);

  // Artist
  dlgLayout->addWidget(new QLabel("Artist:"));
  auto *artistInput = new QLineEdit(artist);
  dlgLayout->addWidget(artistInput);

  // Lyrics
  dlgLayout->addWidget(new QLabel("Lyrics (review before saving):"));
  auto *lyricsEdit = new QTextEdit();
  lyricsEdit->setPlainText(lyricsText);
  dlgLayout->addWidget(lyricsEdit, 1);

  // Buttons
  auto *btnLayout = new QHBoxLayout();
  auto *saveBtn = new QPushButton("💾 Save to Library");
  auto *cancelBtn = new QPushButton("Cancel");
  cancelBtn->setStyleSheet(
      "QPushButton { background: #475569; }"
      "QPushButton:hover { background: #64748b; }");
  btnLayout->addStretch();
  btnLayout->addWidget(cancelBtn);
  btnLayout->addWidget(saveBtn);
  dlgLayout->addLayout(btnLayout);

  connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
  connect(saveBtn, &QPushButton::clicked, [&]() {
    Song newSong;
    newSong.title = titleInput->text().trimmed();
    newSong.artist = artistInput->text().trimmed();

    QString rawLyrics = lyricsEdit->toPlainText();
    QStringList blocks = rawLyrics.split(
        QRegularExpression("\\n\\s*\\n"), Qt::SkipEmptyParts);

    for (const auto &block : blocks) {
      QString verse = block.trimmed();
      if (!verse.isEmpty())
        newSong.verses << verse;
    }

    if (newSong.title.isEmpty()) {
      QMessageBox::warning(&dlg, "Missing Title",
                           "Please enter a song title.");
      return;
    }
    if (newSong.verses.isEmpty()) {
      QMessageBox::warning(&dlg, "No Lyrics",
                           "Please enter some lyrics.");
      return;
    }

    // Duplicate lookup — deterministic, not AI-based, so it still works
    // even when every AI cleanup tier is down. Titles alone aren't
    // reliable (misspellings, transliteration variants, or a failed
    // title/artist split dumping the whole search string into the title
    // field), so this also fingerprints the actual lyrics content: if the
    // opening ~100 normalized characters of one song's lyrics appear
    // anywhere in the other's, they're almost certainly the same song
    // regardless of what the title says.
    auto normalizeTitle = [](const QString &t) {
      QString n = t.trimmed().toLower();
      n.remove(QRegularExpression("[^a-z0-9]"));
      return n;
    };
    auto normalizeContent = [](const QStringList &verses) {
      QString n = verses.join(' ').toLower();
      n.remove(QRegularExpression("[^a-z0-9]"));
      return n;
    };
    QString newTitleNorm = normalizeTitle(newSong.title);
    QString newContentNorm = normalizeContent(newSong.verses);
    QString newFingerprint =
        newContentNorm.left(qMin(100, newContentNorm.length()));
    int existingIndex = -1;
    const auto &existingSongs = songManager->getSongs();
    for (int i = 0; i < (int)existingSongs.size(); ++i) {
      QString existingTitleNorm = normalizeTitle(existingSongs[i].title);
      bool titleMatch = false;
      if (!existingTitleNorm.isEmpty() && !newTitleNorm.isEmpty()) {
        titleMatch = (existingTitleNorm == newTitleNorm);
        if (!titleMatch && newTitleNorm.length() > 5 &&
            existingTitleNorm.length() > 5) {
          titleMatch = existingTitleNorm.contains(newTitleNorm) ||
                       newTitleNorm.contains(existingTitleNorm);
        }
      }

      bool contentMatch = false;
      if (!titleMatch && newFingerprint.length() >= 30) {
        QString existingContentNorm =
            normalizeContent(existingSongs[i].verses);
        if (existingContentNorm.length() >= 30) {
          QString existingFingerprint = existingContentNorm.left(
              qMin(100, existingContentNorm.length()));
          contentMatch = existingContentNorm.contains(newFingerprint) ||
                         newContentNorm.contains(existingFingerprint);
        }
      }

      if (titleMatch || contentMatch) {
        existingIndex = i;
        break;
      }
    }

    bool updated = false;
    if (existingIndex >= 0) {
      QMessageBox::StandardButton reply = QMessageBox::question(
          &dlg, "Song Already in Library",
          "\"" + existingSongs[existingIndex].title +
              "\" is already in your library. Update it with this cleaned "
              "version instead of adding a duplicate?",
          QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
      if (reply == QMessageBox::Yes) {
        newSong.pinned = existingSongs[existingIndex].isPinned();
        newSong.lastUsed = existingSongs[existingIndex].lastUsed;
        songManager->updateSong(existingIndex, newSong);
        updated = true;
      }
    }
    if (!updated)
      songManager->addSong(newSong);
    updateSongList();

    m_browserStatus->setText("✅ \"" + newSong.title + "\" " +
                             (updated ? "updated in" : "added to") +
                             " your library!");
    m_browserStatus->setStyleSheet(
        "color: #22c55e; font-size: 12px; font-weight: bold;");

    dlg.accept();
  });

  dlg.exec();
}
