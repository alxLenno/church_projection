#pragma once
#include <QListWidget>
#include <QMenu>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

class NotesWidget : public QWidget {
  Q_OBJECT
public:
  explicit NotesWidget(QWidget *parent = nullptr);
  void setCurrentVersion(const QString &version);

signals:
  void projectText(const QString &text);
  void versionChanged(const QString &version);

private slots:
  void onTextChanged();
  void onResultClicked(QListWidgetItem *item);
  void onProjectNoteClicked();
  void refreshVersions();

private:
  QTextEdit *editor;
  QListWidget *resultsList;
  class QButtonGroup *versionButtonGroup;
  class QHBoxLayout *versionLayout;

  void performSearch(const QString &query);
  void setupUI();
};
