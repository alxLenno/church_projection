#pragma once
#include "ProjectionContent.h"
#include <QPainter>
#include <QFontMetricsF>
#include <QtMath>
#include <cmath>

namespace Projection {
struct StageOverlay {
  bool enabled = false;
  bool scrolling = false;
  QString text;
  QString font = "Arial";
  int fontSize = 36;
  QColor foreground = Qt::white;
  QColor background = QColor(15, 23, 42, 230);
  int x = 0, y = 90, width = 100, height = 10; // Percent of selected screen
  int speed = 80; // Pixels per second at 1080p
  int screen = -1; // -1 spans both sides
};
inline QRect overlayRect(const QRect &stage, LayoutType layout, const StageOverlay &overlay) {
  QRect area = stage;
  if (layout == LayoutType::SplitVertical && overlay.screen >= 0) {
    const int mid = stage.width()/2;
    area = QRect(stage.left() + (overlay.screen ? mid : 0), stage.top(), overlay.screen ? stage.width()-mid : mid, stage.height());
  } else if (layout == LayoutType::SplitHorizontal && overlay.screen >= 0) {
    const int mid = stage.height()/2;
    area = QRect(stage.left(), stage.top() + (overlay.screen ? mid : 0), stage.width(), overlay.screen ? stage.height()-mid : mid);
  }
  return QRect(area.left()+area.width()*overlay.x/100, area.top()+area.height()*overlay.y/100,
               area.width()*qMin(overlay.width,100-overlay.x)/100, area.height()*qMin(overlay.height,100-overlay.y)/100);
}
inline QRect stageContentRect(const QRect &region, const QRect &stage, LayoutType layout, const StageOverlay &overlay) {
  if (!overlay.enabled || overlay.text.isEmpty()) return region;
  const QRect bar = overlayRect(stage, layout, overlay);
  if (bar.intersects(region) && bar.bottom() >= region.bottom() && bar.left() <= region.left() && bar.right() >= region.right())
    return QRect(region.left(),region.top(),region.width(),qMax(1,bar.top()-region.top()));
  return region;
}
inline void drawStageOverlay(QPainter &painter, const QRect &stage, LayoutType layout, const StageOverlay &overlay, qint64 elapsedMs) {
  if (!overlay.enabled || overlay.text.isEmpty()) return;
  const QRect bar = overlayRect(stage,layout,overlay);
  if (bar.isEmpty()) return;
  painter.save(); painter.setClipRect(bar); painter.fillRect(bar,overlay.background);
  const qreal scale = stage.height()/1080.0;
  const int padding = qMax(3,qRound(18*scale));
  const QRect area = bar.adjusted(padding,padding,-padding,-padding);
  QFont font(overlay.font); font.setPixelSize(qMax(1,qRound(overlay.fontSize*scale)));font.setBold(true);
  QString text = overlay.text; if (overlay.scrolling) text.replace('\n',"   ");
  if (!overlay.scrolling) {
    while(font.pixelSize()>1) {
      const QFontMetricsF fm(font);
      const QRectF bounds=fm.boundingRect(QRectF(area),Qt::TextWordWrap,text);
      if(bounds.height()<=area.height() && bounds.width()<=area.width()) break;
      font.setPixelSize(font.pixelSize()-1);
    }
  }
  painter.setFont(font);painter.setPen(overlay.foreground);
  if (overlay.scrolling) {
    const QFontMetricsF fm(font);const qreal span=fm.horizontalAdvance(text)+area.width();
    const qreal offset=std::fmod(elapsedMs/1000.0*overlay.speed*scale,qMax(1.0,span));
    painter.drawText(QPointF(area.right()-offset,area.center().y()+(fm.ascent()-fm.descent())/2),text);
  } else painter.drawText(area,Qt::AlignLeft|Qt::AlignVCenter|Qt::TextWordWrap,text);
  painter.restore();
}
}
