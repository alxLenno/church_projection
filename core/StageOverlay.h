#pragma once
#include "ProjectionContent.h"
#include <QPainter>
#include <QFontMetricsF>
#include <QtMath>
#include <cmath>
#include <QTime>

namespace Projection {
struct StageOverlay {
  bool enabled = false;
  bool scrolling = false;
  bool newsStyle = false, showClock = true;
  QString title, headline, label;
  QColor titleBackground = QColor("#dc2626"), titleForeground = Qt::white;
  QColor headlineBackground = Qt::white, headlineForeground = Qt::black;
  QColor labelBackground = QColor("#e2e8f0"), labelForeground = Qt::black;
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
  if (overlay.newsStyle) {
    const int height = qBound(6, overlay.height, 30);
    const int top = 100-height;
    return QRect(stage.left(),stage.top()+stage.height()*top/100,stage.width(),stage.height()*height/100);
  }
  if (overlay.scrolling)
    return QRect(stage.left(), stage.top()+stage.height()*overlay.y/100, stage.width(), stage.height()*qMin(overlay.height,100-overlay.y)/100);
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
  if (!overlay.enabled || (!overlay.newsStyle && overlay.text.isEmpty())) return region;
  const QRect bar = overlayRect(stage, layout, overlay);
  if (bar.intersects(region) && bar.bottom() >= region.bottom()-1 && bar.left() <= region.left() && bar.right() >= region.right())
    return QRect(region.left(),region.top(),region.width(),qMax(1,bar.top()-region.top()));
  return region;
}
inline void drawNewsStageOverlay(QPainter &painter, const QRect &stage, LayoutType layout, const StageOverlay &overlay, qint64 elapsedMs) {
  const QRect bar = overlayRect(stage, layout, overlay);
  if (bar.isEmpty()) return;
  painter.save(); painter.setClipRect(bar);
  const bool hasTitle = !overlay.title.trimmed().isEmpty(), hasHeadline = !overlay.headline.trimmed().isEmpty();
  const int first = hasTitle ? bar.height()*(hasHeadline ? 27 : 45)/100 : 0;
  const int second = hasHeadline ? bar.height()*(hasTitle ? 43 : 65)/100 : 0;
  QFont titleFont(overlay.font); titleFont.setBold(true);titleFont.setPixelSize(qMax(1,first*68/100));
  const int titleWidth=qMin(bar.width(),qMax(bar.width()*18/100,qRound(QFontMetricsF(titleFont).horizontalAdvance(overlay.title))+first/2));
  const QRect titleRect(bar.left(), bar.top(), titleWidth, first);
  const QRect headlineRect(bar.left(), bar.top()+first, bar.width(), second);
  const QRect bottom(bar.left(), headlineRect.bottom()+1, bar.width(), bar.height()-first-second);
  const int labelWidth = overlay.label.isEmpty() ? 0 : bottom.width()*18/100;
  const int clockWidth = overlay.showClock ? bottom.width()*13/100 : 0;
  const QRect labelRect(bottom.left(), bottom.top(), labelWidth, bottom.height());
  const QRect clockRect(bottom.right()-clockWidth+1, bottom.top(), clockWidth, bottom.height());
  const QRect tickerRect(bottom.left()+labelWidth, bottom.top(), bottom.width()-labelWidth-clockWidth, bottom.height());
  auto fixed = [&](QRect box, const QString &text, QColor bg, QColor fg, bool centered) {
    if (box.isEmpty()) return;
    painter.fillRect(box,bg);
    const int pad = qMax(2, box.height()/8);
    box.adjust(pad,0,-pad,0);
    QFont font(overlay.font); font.setBold(true);
    font.setPixelSize(qMax(1,box.height()*68/100));
    while (font.pixelSize()>1 && (QFontMetricsF(font).horizontalAdvance(text)>box.width() || QFontMetricsF(font).height()>box.height())) font.setPixelSize(font.pixelSize()-1);
    painter.setFont(font); painter.setPen(fg);
    painter.drawText(box,Qt::AlignVCenter|(centered?Qt::AlignHCenter:Qt::AlignLeft),text);
  };
  fixed(titleRect,overlay.title,overlay.titleBackground,overlay.titleForeground,false);
  fixed(headlineRect,overlay.headline,overlay.headlineBackground,overlay.headlineForeground,false);
  fixed(labelRect,overlay.label,overlay.labelBackground,overlay.labelForeground,true);
  if (overlay.showClock) fixed(clockRect,QTime::currentTime().toString("h:mm AP"),overlay.labelBackground,overlay.labelForeground,true);
  painter.fillRect(tickerRect,overlay.background);
  if (!tickerRect.isEmpty() && !overlay.text.isEmpty()) {
    painter.setClipRect(tickerRect);
    const int pad = qMax(2,tickerRect.height()/8);
    const QRect area=tickerRect.adjusted(pad,0,-pad,0);
    QString text=overlay.text; text.replace('\n',"   •   ");
    QFont font(overlay.font); font.setPixelSize(qMax(1,qMin(qMax(qRound(overlay.fontSize*stage.height()/1080.0),tickerRect.height()*55/100),tickerRect.height()*65/100)));
    painter.setFont(font); painter.setPen(overlay.foreground);
    const QFontMetricsF fm(font);
    const qreal span=qMax(1.0,fm.horizontalAdvance(text)+area.width());
    const qreal offset=std::fmod(area.width()+elapsedMs/1000.0*overlay.speed*stage.height()/1080.0,span);
    painter.drawText(QPointF(area.right()-offset,area.center().y()+(fm.ascent()-fm.descent())/2),text);
  }
  painter.restore();
}
inline void drawStageOverlay(QPainter &painter, const QRect &stage, LayoutType layout, const StageOverlay &overlay, qint64 elapsedMs) {
  if (!overlay.enabled || (!overlay.newsStyle && overlay.text.isEmpty())) return;
  if (overlay.newsStyle) { drawNewsStageOverlay(painter,stage,layout,overlay,elapsedMs); return; }
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
  while (overlay.scrolling && font.pixelSize() > 1 && QFontMetricsF(font).height() > area.height()) font.setPixelSize(font.pixelSize()-1);
  painter.setFont(font);painter.setPen(overlay.foreground);
  if (overlay.scrolling) {
    const QFontMetricsF fm(font);
    const qreal span = qMax(1.0, fm.horizontalAdvance(text) + area.width());
    const qreal offset = std::fmod(area.width() + elapsedMs/1000.0*overlay.speed*scale, span);
    const qreal baseline = area.center().y()+(fm.ascent()-fm.descent())/2;
    painter.drawText(QPointF(area.right()-offset, baseline), text);
  } else painter.drawText(area,Qt::AlignLeft|Qt::AlignVCenter|Qt::TextWordWrap,text);
  painter.restore();
}
}
