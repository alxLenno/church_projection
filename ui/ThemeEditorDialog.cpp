#include "ThemeEditorDialog.h"
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QVBoxLayout>

ThemeEditorDialog::ThemeEditorDialog(QWidget *parent) : QDialog(parent) {
  setWindowTitle("Create New Theme");
  resize(400, 300);

  auto *layout = new QVBoxLayout(this);

  // Name
  layout->addWidget(new QLabel("Theme Name:"));
  nameEdit = new QLineEdit();
  layout->addWidget(nameEdit);

  // Type
  layout->addWidget(new QLabel("Theme Type:"));
  typeCombo = new QComboBox();
  typeCombo->addItem("Video", static_cast<int>(ThemeType::Video));
  typeCombo->addItem("Image", static_cast<int>(ThemeType::Image));
  typeCombo->addItem("Color", static_cast<int>(ThemeType::Color));
  layout->addWidget(typeCombo);

  // Content Selection
  auto *contentLayout = new QHBoxLayout();
  pathLabel = new QLabel("No file selected");
  pathLabel->setWordWrap(true);
  browseBtn = new QPushButton("Browse...");
  colorBtn = new QPushButton("Pick Color...");
  colorBtn->setVisible(false);

  contentLayout->addWidget(pathLabel);
  contentLayout->addWidget(browseBtn);
  contentLayout->addWidget(colorBtn);
  layout->addLayout(contentLayout);

  // Preview
  previewLabel = new QLabel();
  previewLabel->setFixedSize(100, 56); // 16:9 ratio approx
  previewLabel->setStyleSheet("border: 1px solid #555; background: black;");
  previewLabel->setAlignment(Qt::AlignCenter);
  layout->addWidget(previewLabel, 0, Qt::AlignCenter);

  // Buttons
  auto *buttonBox =
      new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  layout->addWidget(buttonBox);

  // Logic
  selectedColor = Qt::black;

  connect(typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &ThemeEditorDialog::updateUI);
  connect(browseBtn, &QPushButton::clicked, this,
          &ThemeEditorDialog::selectContent);
  connect(colorBtn, &QPushButton::clicked, [this]() {
    QColor c = QColorDialog::getColor(selectedColor, this, "Pick Theme Color");
    if (c.isValid()) {
      selectedColor = c;
      previewLabel->setStyleSheet(
          QString("background: %1; border: 1px solid #555;").arg(c.name()));
    }
  });

  connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

  updateUI();
}

void ThemeEditorDialog::updateUI() {
  ThemeType type = static_cast<ThemeType>(typeCombo->currentData().toInt());

  if (type == ThemeType::Color) {
    browseBtn->setVisible(false);
    pathLabel->setVisible(false);
    colorBtn->setVisible(true);
    previewLabel->setText("");
    previewLabel->setStyleSheet(
        QString("background: %1; border: 1px solid #555;")
            .arg(selectedColor.name()));
  } else {
    browseBtn->setVisible(true);
    pathLabel->setVisible(true);
    colorBtn->setVisible(false);
    previewLabel->setStyleSheet("border: 1px solid #555; background: black;");
    previewLabel->setText(type == ThemeType::Video ? "Video\nPreview"
                                                   : "Image\nPreview");

    if (!selectedPath.isEmpty()) {
      pathLabel->setText(QFileInfo(selectedPath).fileName());
      if (type == ThemeType::Image) {
        QPixmap p(selectedPath);
        if (!p.isNull()) {
          previewLabel->setPixmap(p.scaled(previewLabel->size(),
                                           Qt::KeepAspectRatio,
                                           Qt::SmoothTransformation));
          previewLabel->setText("");
        }
      }
    } else {
      pathLabel->setText("No file selected");
    }
  }
}

void ThemeEditorDialog::selectContent() {
  ThemeType type = static_cast<ThemeType>(typeCombo->currentData().toInt());
  QString filter = (type == ThemeType::Video)
                       ? "Videos (*.mp4 *.mov *.avi *.mkv)"
                       : "Images (*.png *.jpg *.jpeg *.bmp)";
  QString path = QFileDialog::getOpenFileName(this, "Select Media", "", filter);

  if (!path.isEmpty()) {
    selectedPath = path;
    updateUI();
  }
}

QString ThemeEditorDialog::getName() const { return nameEdit->text(); }

ThemeType ThemeEditorDialog::getType() const {
  return static_cast<ThemeType>(typeCombo->currentData().toInt());
}

QString ThemeEditorDialog::getContentPath() const { return selectedPath; }

QColor ThemeEditorDialog::getColor() const { return selectedColor; }
