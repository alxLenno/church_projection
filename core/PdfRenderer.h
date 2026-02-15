#pragma once
#include <QImage>
#include <QString>

// PDF rendering using macOS Core Graphics (no Qt PDF module needed)
class PdfRenderer {
public:
  static bool isAvailable();

  // Open a PDF and return page count, 0 on failure
  static int pageCount(const QString &path);

  // Render a specific page to QImage
  // page is 0-indexed
  // targetSize: desired render size (will maintain aspect ratio)
  static QImage renderPage(const QString &path, int page,
                           QSize targetSize = QSize(1920, 1080));

  // Render thumbnail (smaller, faster)
  static QImage renderThumbnail(const QString &path, int page,
                                QSize targetSize = QSize(300, 400));
};
