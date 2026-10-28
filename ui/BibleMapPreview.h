#pragma once
#include <QWidget>
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <functional>
class BibleMapPreview:public QWidget {
public:
  QImage frame;
  std::function<void(double,double)> pan;
  std::function<void(double)> zoom;
  BibleMapPreview(){setMinimumSize(240,180);setCursor(Qt::OpenHandCursor);}
protected:
  QRect target() const {QSize size=frame.size();size.scale(this->size(),Qt::KeepAspectRatio);return QRect(rect().center()-QPoint(size.width()/2,size.height()/2),size);}
  void paintEvent(QPaintEvent*)override{QPainter p(this);p.fillRect(rect(),QColor("#101827"));p.setRenderHint(QPainter::SmoothPixmapTransform);if(!frame.isNull())p.drawImage(target(),frame);}
  void mousePressEvent(QMouseEvent *e)override{if(e->button()==Qt::LeftButton){last=e->position();dragging=true;setCursor(Qt::ClosedHandCursor);}}
  void mouseMoveEvent(QMouseEvent *e)override{if(!dragging || !pan)return;auto delta=e->position()-last;last=e->position();const auto area=target();pan(delta.x()/qMax(1,area.width()),delta.y()/qMax(1,area.width()));}
  void mouseReleaseEvent(QMouseEvent*)override{dragging=false;setCursor(Qt::OpenHandCursor);}
  void wheelEvent(QWheelEvent *e)override{if(zoom)zoom(e->angleDelta().y()>0?0.8:1.25);e->accept();}
private:
  QPointF last;bool dragging=false;
};
