#include "NotesWidget.h"
#include "../core/BibleManager.h"
#include <QButtonGroup>
#include <QComboBox>
#include <QDebug>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSplitter>
#include <QTextBlock>
#include <QVBoxLayout>

NotesWidget::NotesWidget(QWidget *parent) : QWidget(parent) {
  setupUI();
  connect(&BibleManager::instance(), &BibleManager::bibleLoaded, this,
          &NotesWidget::refreshVersions);
  // Initial refresh in case already loaded
  refreshVersions();
}

void NotesWidget::setCurrentVersion(const QString &version) {
  // Update the button group to select the correct version
  if (versionButtonGroup) {
    for (auto *btn : versionButtonGroup->buttons()) {
      auto *pushBtn = qobject_cast<QPushButton *>(btn);
      if (pushBtn && pushBtn->text() == version) {
        pushBtn->setChecked(true);
        break;
      }
    }
  }
  // Trigger search refresh with new version
  onTextChanged();
}

void NotesWidget::setupUI() {
  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);

  auto *versionFrame = new QFrame();
  versionFrame->setFixedHeight(24); // Force absolute minimum height
  versionFrame->setStyleSheet(
      "QFrame { background: #1e293b; border-bottom: 1px solid #334155; "
      "padding: 0px; }");
  versionLayout = new QHBoxLayout(versionFrame);
  versionLayout->setSpacing(2);
  versionLayout->setContentsMargins(4, 0, 4, 0);

  // Removed "BIBLE VERSION:" label to save space

  versionButtonGroup = new QButtonGroup(this);
  versionButtonGroup->setExclusive(true);

  // Buttons will be populated by refreshVersions()
  mainLayout->addWidget(versionFrame);

  // Splitter for Notes (Left) and Results (Right)
  QSplitter *splitter = new QSplitter(Qt::Horizontal, this);

  // --- Left Side: Notes Editor ---
  QWidget *leftWidget = new QWidget();
  auto *leftLayout = new QVBoxLayout(leftWidget);
  leftLayout->setContentsMargins(10, 10, 10, 10);

  QLabel *notesLabel = new QLabel("SERMON NOTES");
  notesLabel->setStyleSheet(
      "font-weight: bold; color: #94a3b8; letter-spacing: 1px;");
  leftLayout->addWidget(notesLabel);

  editor = new QTextEdit();
  editor->setPlaceholderText(
      "Type your notes here...\n\nUse @ to search for scriptures (e.g., @John "
      "3:16 or @love). Results will appear on the right.");
  editor->setStyleSheet(
      "QTextEdit { background: rgba(30, 41, 59, 0.6); border: 1px solid "
      "rgba(148, 163, 184, 0.2); border-radius: 8px; color: white; padding: "
      "10px; selection-background-color: #0ea5e9; font-size: 14pt; }");
  leftLayout->addWidget(editor);

  QPushButton *projectNoteBtn = new QPushButton("PROJECT NOTES");
  projectNoteBtn->setStyleSheet(
      "QPushButton { background: #0ea5e9; color: white; font-weight: bold; "
      "padding: 12px; border-radius: 8px; border: none; } QPushButton:hover { "
      "background: #0284c7; }");
  connect(projectNoteBtn, &QPushButton::clicked, this,
          &NotesWidget::onProjectNoteClicked);
  leftLayout->addWidget(projectNoteBtn);

  // --- Right Side: Scripture Results ---
  QWidget *rightWidget = new QWidget();
  auto *rightLayout = new QVBoxLayout(rightWidget);
  rightLayout->setContentsMargins(10, 10, 10, 10);

  QLabel *resultsLabel = new QLabel("SCRIPTURE SUGGESTIONS");
  resultsLabel->setStyleSheet(
      "font-weight: bold; color: #94a3b8; letter-spacing: 1px;");
  rightLayout->addWidget(resultsLabel);

  resultsList = new QListWidget();
  resultsList->setStyleSheet(
      "QListWidget { background: rgba(30, 41, 59, 0.6); border: 1px solid "
      "rgba(148, 163, 184, 0.2); border-radius: 8px; outline: none; } "
      "QListWidget::item { color: #e2e8f0; padding: 12px; border-bottom: 1px "
      "solid rgba(148, 163, 184, 0.1); } QListWidget::item:hover { background: "
      "rgba(56, 189, 248, 0.1); } QListWidget::item:selected { background: "
      "rgba(14, 165, 233, 0.2); border-left: 4px solid #0ea5e9; }");
  resultsList->setWordWrap(true);
  connect(resultsList, &QListWidget::itemClicked, this,
          &NotesWidget::onResultClicked);
  rightLayout->addWidget(resultsList);

  splitter->addWidget(leftWidget);
  splitter->addWidget(rightWidget);
  splitter->setStretchFactor(0, 1);
  splitter->setStretchFactor(1, 1);

  mainLayout->addWidget(splitter);

  connect(editor, &QTextEdit::textChanged, this, &NotesWidget::onTextChanged);
}

void NotesWidget::onTextChanged() {
  QString text = editor->toPlainText();
  QTextCursor cursor = editor->textCursor();
  int pos = cursor.position();

  // Find last '@' before cursor
  int atIndex = text.lastIndexOf('@', pos - 1);

  if (atIndex != -1) {
    QString query = text.mid(atIndex + 1, pos - atIndex - 1);
    // Only trigger if query length is sufficient
    if (query.length() >= 2) {
      performSearch(query);
      return;
    }
  }
  // Optional: Clear results if '@' sequence is broken?
  // deciding to keep them for persistence as requested
}

void NotesWidget::performSearch(const QString &query) {
  // Get selected version from button group
  QString version = "NKJV"; // Default
  if (versionButtonGroup) {
    auto *checkedBtn =
        qobject_cast<QPushButton *>(versionButtonGroup->checkedButton());
    if (checkedBtn) {
      version = checkedBtn->text();
    }
  }

  auto results = BibleManager::instance().search(query, version);

  resultsList->clear();
  for (const auto &verse : results) {
    // Localize book name
    QString displayBook =
        BibleManager::instance().getLocalizedBookName(verse.book, version);

    QString label = QString("%1 %2:%3\n%4")
                        .arg(displayBook)
                        .arg(verse.chapter)
                        .arg(verse.verse)
                        .arg(verse.text);

    QListWidgetItem *item = new QListWidgetItem(label);
    // Store text to project
    item->setData(Qt::UserRole, verse.text);
    item->setData(Qt::UserRole + 1, QString("%1 %2:%3")
                                        .arg(displayBook)
                                        .arg(verse.chapter)
                                        .arg(verse.verse));
    resultsList->addItem(item);
  }
}

void NotesWidget::onResultClicked(QListWidgetItem *item) {
  if (!item)
    return;
  QString verseText = item->data(Qt::UserRole).toString();
  QString verseRef = item->data(Qt::UserRole + 1).toString();

  // Formatting for projection
  QString projectionText = QString("%1\n\n%2").arg(verseText, verseRef);
  emit projectText(projectionText);
}

void NotesWidget::onProjectNoteClicked() {
  emit projectText(editor->toPlainText());
}

void NotesWidget::refreshVersions() {
  if (!versionButtonGroup || !versionLayout)
    return;

  // Clear existing buttons
  QList<QAbstractButton *> buttons = versionButtonGroup->buttons();
  for (auto *btn : buttons) {
    versionButtonGroup->removeButton(btn);
    versionLayout->removeWidget(btn);
    btn->deleteLater();
  }

  // Remove stretch if any
  if (versionLayout->count() > 1) {
    QLayoutItem *item = versionLayout->itemAt(versionLayout->count() - 1);
    if (item->spacerItem()) {
      delete versionLayout->takeAt(versionLayout->count() - 1);
    }
  }

  // Reload versions
  QStringList versions = BibleManager::instance().getVersions();
  QString currentVersion = "NKJV";

  for (const QString &ver : versions) {
    auto *btn = new QPushButton(ver);
    btn->setCheckable(true);
    btn->setAutoExclusive(true);
    btn->setMinimumWidth(40);
    btn->setFixedHeight(20);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setStyleSheet(
        "QPushButton { background: #334155; color: #94a3b8; "
        "border: 1px solid #475569; border-radius: 2px; font-size: 8px; "
        "font-weight: bold; padding: 0 2px; } "
        "QPushButton:hover { background: #475569; color: white; } "
        "QPushButton:checked { background: #38bdf8; color: white; "
        "border-color: #38bdf8; }");

    if (ver == currentVersion) {
      btn->setChecked(true);
    }

    connect(btn, &QPushButton::clicked, [this, btn]() {
      onTextChanged();
      emit versionChanged(btn->text());
    });

    versionButtonGroup->addButton(btn);
    versionLayout->addWidget(btn);
  }

  versionLayout->addStretch();
}
