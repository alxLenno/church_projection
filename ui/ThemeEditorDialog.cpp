#include "ThemeEditorDialog.h"
#include <QColorDialog>
#include <QDebug>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QStandardPaths>
#include <QVBoxLayout>

ThemeEditorDialog::ThemeEditorDialog(QWidget *parent) : QDialog(parent) {
  setWindowTitle("Create New Theme");
  resize(400, 300);

  // Dark theme matching ControlWindow
  setStyleSheet(
      "QDialog { background: #0f172a; color: #e2e8f0; }"
      "QLabel { color: #94a3b8; font-weight: bold; font-size: 12px; }"
      "QLineEdit { background: #1e293b; color: white; border: 1px solid "
      "#334155; "
      "  border-radius: 6px; padding: 8px; }"
      "QLineEdit:focus { border-color: #38bdf8; }"
      "QComboBox { background: #1e293b; color: white; border: 1px solid "
      "#334155; "
      "  border-radius: 6px; padding: 6px; }"
      "QPushButton { background: #334155; color: white; border: none; "
      "  border-radius: 6px; padding: 8px 16px; font-weight: bold; }"
      "QPushButton:hover { background: #475569; }"
      "QPushButton:pressed { background: #38bdf8; color: #0f172a; }"
      "QDialogButtonBox QPushButton { min-width: 80px; }");
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

  connect(browseBtn, &QPushButton::clicked, this, [this]() {
    qDebug() << "Browse button clicked";
    selectContent();
  });
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
  qDebug() << "Selecting content. Current Type:" << (int)type;

  // Allow both filters but prioritize current type
  QString filters;
  QString videoFilter = "Videos (*.mp4 *.mov *.avi *.mkv *.webm)";
  QString imageFilter = "Images (*.png *.jpg *.jpeg *.bmp *.gif)";
  QString allFilter = "All Files (*)";

  if (type == ThemeType::Video) {
    filters = videoFilter + ";;" + imageFilter + ";;" + allFilter;
  } else {
    filters = imageFilter + ";;" + videoFilter + ";;" + allFilter;
  }

  // Default to Downloads folder
  QString defaultDir =
      QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
  qDebug() << "Opening File Dialog in:" << defaultDir;

  QString path = QFileDialog::getOpenFileName(
      this, "Select Media", defaultDir, filters, nullptr,
      QFileDialog::DontUseNativeDialog); // Force non-native to debug macOS
                                         // issue

  qDebug() << "Selected path:" << path;

  if (!path.isEmpty()) {
    selectedPath = path;

    // Auto-detect type mismatch?
    QFileInfo fi(path);
    QString ext = fi.suffix().toLower();
    if (ext == "mp4" || ext == "mov" || ext == "avi" || ext == "mkv" ||
        ext == "webm") {
      if (type != ThemeType::Video) {
        int idx = typeCombo->findData(static_cast<int>(ThemeType::Video));
        if (idx >= 0)
          typeCombo->setCurrentIndex(idx);
      }
    } else if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "bmp" ||
               ext == "gif") {
      if (type != ThemeType::Image) {
        int idx = typeCombo->findData(static_cast<int>(ThemeType::Image));
        if (idx >= 0)
          typeCombo->setCurrentIndex(idx);
      }
    }

    updateUI();
  }
}

QString ThemeEditorDialog::getName() const { return nameEdit->text(); }

ThemeType ThemeEditorDialog::getType() const {
  return static_cast<ThemeType>(typeCombo->currentData().toInt());
}

QString ThemeEditorDialog::getContentPath() const { return selectedPath; }

QColor ThemeEditorDialog::getColor() const { return selectedColor; }
