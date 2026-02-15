#include "PdfRenderer.h"

#ifdef Q_OS_MACOS
#include <CoreGraphics/CoreGraphics.h>
#include <QUrl>
#endif

bool PdfRenderer::isAvailable() {
#ifdef Q_OS_MACOS
  return true;
#else
  return false;
#endif
}

int PdfRenderer::pageCount(const QString &path) {
#ifdef Q_OS_MACOS
  QUrl fileUrl = QUrl::fromLocalFile(path);
  CFURLRef cfUrl = fileUrl.toCFURL();
  if (!cfUrl)
    return 0;

  CGPDFDocumentRef doc = CGPDFDocumentCreateWithURL(cfUrl);
  CFRelease(cfUrl);
  if (!doc)
    return 0;

  int count = (int)CGPDFDocumentGetNumberOfPages(doc);
  CGPDFDocumentRelease(doc);
  return count;
#else
  Q_UNUSED(path);
  return 0;
#endif
}

QImage PdfRenderer::renderPage(const QString &path, int page,
                               QSize targetSize) {
#ifdef Q_OS_MACOS
  QUrl fileUrl = QUrl::fromLocalFile(path);
  CFURLRef cfUrl = fileUrl.toCFURL();
  if (!cfUrl)
    return QImage();

  CGPDFDocumentRef doc = CGPDFDocumentCreateWithURL(cfUrl);
  CFRelease(cfUrl);
  if (!doc)
    return QImage();

  // PDF pages are 1-indexed in Core Graphics
  CGPDFPageRef pdfPage = CGPDFDocumentGetPage(doc, page + 1);
  if (!pdfPage) {
    CGPDFDocumentRelease(doc);
    return QImage();
  }

  CGRect mediaBox = CGPDFPageGetBoxRect(pdfPage, kCGPDFMediaBox);

  // Calculate scale to fit targetSize while maintaining aspect ratio
  qreal scaleX = targetSize.width() / mediaBox.size.width;
  qreal scaleY = targetSize.height() / mediaBox.size.height;
  qreal scale = qMin(scaleX, scaleY);

  int renderW = (int)(mediaBox.size.width * scale);
  int renderH = (int)(mediaBox.size.height * scale);

  // Create ARGB image
  QImage image(renderW, renderH, QImage::Format_ARGB32_Premultiplied);
  image.fill(Qt::white);

  // Create CG bitmap context from QImage
  CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
  CGContextRef ctx = CGBitmapContextCreate(
      image.bits(), renderW, renderH, 8, image.bytesPerLine(), colorSpace,
      kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host);
  CGColorSpaceRelease(colorSpace);

  if (!ctx) {
    CGPDFDocumentRelease(doc);
    return QImage();
  }

  // Flip coordinate system (CG is bottom-up, Qt is top-down)
  CGContextTranslateCTM(ctx, 0, renderH);
  CGContextScaleCTM(ctx, scale, -scale);

  // Draw the PDF page
  CGContextDrawPDFPage(ctx, pdfPage);

  CGContextRelease(ctx);
  CGPDFDocumentRelease(doc);

  // The user reports pages are "inverted". CGBitmapContext + QImage can cause
  // vertical flips. rgbSwapped was a guess for colors, but "inverted" likely
  // means upside down. We flip it vertically to correct this.
  return image.mirrored(false, true);
#else
  Q_UNUSED(path);
  Q_UNUSED(page);
  Q_UNUSED(targetSize);
  return QImage();
#endif
}

QImage PdfRenderer::renderThumbnail(const QString &path, int page,
                                    QSize targetSize) {
  return renderPage(path, page, targetSize);
}
