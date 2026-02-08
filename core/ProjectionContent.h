#pragma once
#include <QColor>
#include <QPixmap>
#include <QString>
#include <QVideoFrame>

namespace Projection {
enum class BackgroundType { None, Image, Video, Color };

struct Content {
  BackgroundType bgType = BackgroundType::None;
  QString text;
  QPixmap pixmap;
  QVideoFrame videoFrame;
  QColor bgColor = Qt::black;
  QString bgPath; // Path for image or video
};

enum class LayoutType {
  Single,          // Layer 0 fills screen
  SplitHorizontal, // Layer 0 Top, Layer 1 Bottom
  SplitVertical    // Layer 0 Left, Layer 1 Right
};
} // namespace Projection
