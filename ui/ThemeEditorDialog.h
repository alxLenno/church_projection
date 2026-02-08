#pragma once
#include "../core/ThemeManager.h"
#include <QColor>
#include <QComboBox>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

class ThemeEditorDialog : public QDialog {
  Q_OBJECT
public:
  explicit ThemeEditorDialog(QWidget *parent = nullptr);

  QString getName() const;
  ThemeType getType() const;
  QString getContentPath() const;
  QColor getColor() const;

private:
  QLineEdit *nameEdit;
  QComboBox *typeCombo;
  QLabel *pathLabel;
  QPushButton *browseBtn;
  QPushButton *colorBtn;
  QLabel *previewLabel; // To show color or image thumb

  QString selectedPath;
  QColor selectedColor;

  void updateUI();
  void selectContent();
};
