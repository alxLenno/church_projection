#pragma once
#include "ObsSceneEditor.h"
#include <QToolButton>
#include <QNativeGestureEvent>
#include <QGestureEvent>
#include <QPinchGesture>
#include <cmath>

// Reuses the normal output-preview canvas. Editor handles remain separate.
class OutputMapPreview:public ObsScenePreview {
public:
  std::function<void(bool)> layoutChanged;
  explicit OutputMapPreview(ProjectionWindow *output):model(output){
    grabGesture(Qt::PinchGesture);
    tools=new QWidget(this);auto *row=new QHBoxLayout(tools);row->setContentsMargins(0,0,0,0);row->setSpacing(2);
    auto button=[&](const QString &text,const QString &tip){auto *b=new QToolButton(tools);b->setText(text);b->setToolTip(tip);b->setStyleSheet("QToolButton{background:#101827;color:white;border:1px solid #64748b;padding:2px 5px;font-size:11px;}");row->addWidget(b);return b;};
    auto *plus=button("+","Zoom in"),*minus=button("−","Zoom out"),*reset=button("Reset","Frame the linked locations again");
    orientation=button("Above","Place the map above or beside the verse");
    connect(plus,&QToolButton::clicked,this,[this]{model->zoomMap(0.8);refresh();});
    connect(minus,&QToolButton::clicked,this,[this]{model->zoomMap(1.25);refresh();});
    connect(reset,&QToolButton::clicked,this,[this]{model->resetMapView();refresh();});
    connect(orientation,&QToolButton::clicked,this,[this]{model->setMapAbove(!model->isMapAbove());if(layoutChanged)layoutChanged(model->isMapAbove());refresh();});
    tools->hide();setToolTip("Map: drag to pan; pinch or scroll to zoom. North is always up.");
  }
  void refresh(){frame=model->obsFrame();positionTools();update();}
protected:
  bool event(QEvent *event)override{
    if(event->type()==QEvent::NativeGesture && model->hasVisibleMap()){
      auto *gesture=static_cast<QNativeGestureEvent*>(event);
      if(gesture->gestureType()==Qt::ZoomNativeGesture && mapRect().contains(gesture->position())){
        model->zoomMap(1.0/qMax(0.1,1.0+gesture->value()));refresh();event->accept();return true;
      }
    }
    if(event->type()==QEvent::Gesture && model->hasVisibleMap()){
      auto *gestures=static_cast<QGestureEvent*>(event);
      if(auto *pinch=static_cast<QPinchGesture*>(gestures->gesture(Qt::PinchGesture))){
        if(!mapRect().contains(pinch->centerPoint()))return ObsScenePreview::event(event);
        const double ratio=pinch->scaleFactor()/qMax(0.01,pinch->lastScaleFactor());
        model->zoomMap(1.0/qMax(0.1,ratio));refresh();gestures->accept(pinch);return true;
      }
    }
    return ObsScenePreview::event(event);
  }
  QRectF mapRect()const{return screenBox(model->mapOutputArea());}
  void mousePressEvent(QMouseEvent *e)override{
    if(model->hasVisibleMap() && e->button()==Qt::LeftButton && mapRect().contains(e->position())){
      const auto point=canvasPoint(e->position());const auto area=model->mapOutputArea();
      if(QRect(area.right()-65,area.top(),65,95).contains(point)){model->resetMapView();refresh();return;}
      draggingMap=true;lastPoint=e->position();grabMouse();setCursor(Qt::ClosedHandCursor);e->accept();return;
    }ObsScenePreview::mousePressEvent(e);
  }
  void mouseMoveEvent(QMouseEvent *e)override{
    if(draggingMap){auto delta=e->position()-lastPoint;lastPoint=e->position();model->panMap(delta.x()/qMax(1.0,mapRect().width()),delta.y()/qMax(1.0,mapRect().width()));refresh();e->accept();return;}
    if(model->hasVisibleMap() && mapRect().contains(e->position()))setCursor(Qt::OpenHandCursor);else unsetCursor();
  }
  void mouseReleaseEvent(QMouseEvent *e)override{if(draggingMap){draggingMap=false;releaseMouse();setCursor(Qt::OpenHandCursor);e->accept();return;}ObsScenePreview::mouseReleaseEvent(e);}
  void wheelEvent(QWheelEvent *e)override{
    if(!model->hasVisibleMap() || !mapRect().contains(e->position())){e->ignore();return;}
    const double steps=e->angleDelta().y()!=0?e->angleDelta().y()/120.0:e->pixelDelta().y()/90.0;
    model->zoomMap(std::pow(0.8,steps));refresh();e->accept();
  }
  void enterEvent(QEnterEvent *e)override{hover=true;positionTools();ObsScenePreview::enterEvent(e);}
  void leaveEvent(QEvent *e)override{hover=false;positionTools();ObsScenePreview::leaveEvent(e);}
  void resizeEvent(QResizeEvent *e)override{ObsScenePreview::resizeEvent(e);positionTools();}
private:
  ProjectionWindow *model;QWidget *tools;QToolButton *orientation;
  bool draggingMap=false,hover=false;QPointF lastPoint;
  void positionTools(){
    orientation->setText(model->isMapAbove()?"Beside":"Above");orientation->setVisible(model->mapMode()==1);
    tools->adjustSize();tools->move(mapRect().topLeft().toPoint()+QPoint(4,4));tools->setVisible(hover && model->hasVisibleMap());tools->raise();
  }
};
