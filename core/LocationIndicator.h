#pragma once
#include <QIcon>
#include <QPainter>
#include <QPainterPath>
inline QIcon locationPinIcon(){
  static const QIcon icon=[](){QPixmap image(32,32);image.fill(Qt::transparent);QPainter p(&image);p.setRenderHint(QPainter::Antialiasing);
    QPainterPath pin;pin.moveTo(16,30);pin.cubicTo(12,25,5,18,5,12);pin.cubicTo(5,-2,27,-2,27,12);pin.cubicTo(27,18,20,25,16,30);
    p.setPen(Qt::NoPen);p.setBrush(QColor("#38bdf8"));p.drawPath(pin);p.setBrush(QColor("#101827"));p.drawEllipse(QPointF(16,11),4,4);return QIcon(image);
  }();return icon;
}
