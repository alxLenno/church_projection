#pragma once
#include <QColor>
#include <QRect>
#include <QJsonObject>
#include <QJsonArray>

// All geometry is in the fixed OBS canvas coordinate system (1920 x 1080).
struct ObsSceneLayout {
  bool enabled = false, banner = true, transparent = false;
  bool captionPaging=false;
  int captionScale = 150, panelScale=140, captionBacking=90;
  int source = 0, textSize = 80, padding = 32, border = 2;
  QString title = "", backgroundImage;
  QRect phone{64,120,450,800}, text{558,120,1298,800};
  QColor background{"#101827"}, panel{"#18263b"}, foreground{Qt::white}, accent{"#38bdf8"};
  int outputTextSize() const {return qMax(1,textSize*(transparent?captionScale:panelScale)/100);}
  QJsonObject json() const {
    auto box=[](QRect r) { return QJsonArray{r.x(),r.y(),r.width(),r.height()}; };
    return {{"captionBoxVersion",1},{"panelScale",panelScale},{"captionBacking",captionBacking},{"captionPaging",captionPaging},{"captionScale",captionScale},{"transparent",transparent},{"enabled",enabled},{"banner",banner},{"source",source},{"textSize",textSize},
      {"padding",padding},{"border",border},{"title",title},{"image",backgroundImage},
      {"phone",box(phone)},{"text",box(text)},{"background",background.name()},
      {"panel",panel.name()},{"foreground",foreground.name()},{"accent",accent.name()}};
  }
  static ObsSceneLayout fromJson(const QJsonObject &o) {
    ObsSceneLayout v;
    v.panelScale=qBound(100,o.value("panelScale").toInt(140),250);
    v.captionBacking=qBound(0,o.value("captionBacking").toInt(90),100);
    if(!o.contains("captionBoxVersion") && v.captionBacking==70)v.captionBacking=90;
    v.captionPaging=o.value("captionPaging").toBool(false);
    v.captionScale=qBound(100,o.value("captionScale").toInt(150),250);
    v.transparent=o.value("transparent").toBool(false);
    v.enabled=o.value("enabled").toBool();v.banner=o.value("banner").toBool(true);
    v.source=qBound(0,o.value("source").toInt(),1);
    v.textSize=qBound(12,o.value("textSize").toInt(80),150);
    v.padding=qBound(0,o.value("padding").toInt(32),100);
    v.border=qBound(0,o.value("border").toInt(2),12);
    v.title=o.value("title").toString();v.backgroundImage=o.value("image").toString();
    auto box=[&](const char *key,QRect fallback) {
      auto a=o.value(key).toArray(); if(a.size()!=4) return fallback;
      int x=qBound(0,a[0].toInt(),1919),y=qBound(0,a[1].toInt(),1079);
      return QRect(x,y,qBound(1,a[2].toInt(),1920-x),qBound(1,a[3].toInt(),1080-y));
    };
    v.phone=box("phone",v.phone);v.text=box("text",v.text);
    auto color=[&](const char *k,QColor fallback) {QColor c(o.value(k).toString());return c.isValid()?c:fallback;};
    v.background=color("background",v.background);v.panel=color("panel",v.panel);
    v.foreground=color("foreground",v.foreground);v.accent=color("accent",v.accent);
    return v;
  }
};
