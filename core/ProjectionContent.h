#pragma once
#include <QColor>
#include <QPixmap>
#include <QSize>
#include <QString>
#include <QVideoFrame>

namespace Projection {
enum class BackgroundType { None, Image, Video, Color };

struct TextFormatting {
  int fontSize = 0;                       // 0 = Auto
  int margin = 40;                        // Indentation/Padding
  QString fontFamily = "Times New Roman"; // Font Family
  int alignment = Qt::AlignCenter;        // Qt::Alignment flag
  bool isScrolling = false;               // Enable marquee/scrolling
  int scrollSpeed = 2;                    // Pixels per frame (approx)
  bool textShadow = true;                 // Drop shadow for readability
  int outlineWidth = 2;                   // Text outline in pixels (0=off)
};

struct Content {
  BackgroundType bgType = BackgroundType::None;
  QString text;
  QPixmap pixmap;
  QVideoFrame videoFrame;
  QColor bgColor = Qt::black;
  QString bgPath; // Path for image or video
  TextFormatting formatting;

  // Rendering cache — avoids re-scaling pixmap every frame
  QPixmap cachedPixmap;
  QSize cachedPixmapSize;

  // Media Support
  enum class MediaType { None, Image, Pdf };
  MediaType mediaType = MediaType::None;
  QString mediaPath;
  int pageNumber = 0;      // For PDF
  QImage renderedMedia;    // Cache for PDF page render or Image
  QSize renderedMediaSize; // Cache size
};

enum class LayoutType {
  Single,          // Layer 0 fills screen
  SplitHorizontal, // Layer 0 Top, Layer 1 Bottom
  SplitVertical    // Layer 0 Left, Layer 1 Right
};
} // namespace Projection
