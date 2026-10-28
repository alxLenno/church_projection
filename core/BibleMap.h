#pragma once
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>
#include <algorithm>
#include <cmath>
#include <QRegularExpression>
#include <QSet>

// Local geographic data shared with BiAY. No tile server or API key required.
class BibleMap {
public:
  struct Place {QString id,name,modern,type;double lon=0,lat=0;int confidence=0;};
  QList<Place> places;
  QStringList linkedPlaces(int book,int chapter,int verse,int endVerse=0){
    if(book<0 || book>=66 || chapter<1 || verse<1)return {};
    if(!books.contains(book))books.insert(book,load("book_"+QString::number(book)).object());
    QStringList ids;for(int v=verse;v<=qMin(200,qMax(verse,endVerse));++v){
      for(const auto &raw:books[book].value(QString("%1:%2").arg(chapter).arg(v)).toArray()){
        const auto id=raw.toString();if(!ids.contains(id) && find(id))ids.append(id);
      }
    }return ids;
  }
  const Place *find(const QString &id) const {for(const auto &place:places)if(place.id==id)return &place;return nullptr;}
  struct Frame {double lon=35.23,lat=31.78,span=9;};
  Frame framePlaces(const QStringList &ids) const {
    QList<QPointF> points;for(const auto &id:ids)if(const auto *place=find(id))points.append({place->lon,mercator(place->lat)});
    if(points.isEmpty())return {};
    const double anchor=points.first().x();
    double west=anchor,east=anchor,south=points.first().y(),north=south;
    for(auto point:points){while(point.x()-anchor>180)point.rx()-=360;while(point.x()-anchor< -180)point.rx()+=360;
      west=qMin(west,point.x());east=qMax(east,point.x());south=qMin(south,point.y());north=qMax(north,point.y());}
    double lon=(west+east)/2;while(lon>180)lon-=360;while(lon< -180)lon+=360;
    return {lon,latitude((south+north)/2),qBound(1.2,qMax((east-west)*1.5,(north-south)*2.2),180.0)};
  }
  static BibleMap &instance(){static BibleMap value;return value;}
  static double mercator(double latitude){return qRadiansToDegrees(std::asinh(qTan(qDegreesToRadians(qBound(-80.0,latitude,80.0)))));}
  static double latitude(double y){return qRadiansToDegrees(qAtan(std::sinh(qDegreesToRadians(y))));}
  QImage render(QSize size,double lon,double lat,double span,const QString &selected={},const QStringList &related={},QList<QRectF> *labelBounds=nullptr) const {
    QImage image(size,QImage::Format_RGB32);image.fill(QColor("#132129"));
    QPainter p(&image);p.setRenderHint(QPainter::Antialiasing);p.setClipRect(image.rect());
    const double scale=size.width()/span;
    QTransform transform;transform.translate(size.width()/2.0,size.height()/2.0);transform.scale(scale,-scale);transform.translate(-lon,-mercator(lat));
    const QRectF visible=transform.inverted().mapRect(QRectF(image.rect()));
    auto paths=[&](const QList<QPainterPath> &items,QColor fill,QColor stroke,double width){
      p.setBrush(fill);p.setPen(QPen(stroke,width));
      for(const auto &path:items)if(path.boundingRect().intersects(visible))p.drawPath(transform.map(path));
    };
    paths(land,QColor("#3d493e"),QColor(255,255,255,33),0.7);
    paths(lakes,QColor("#193844"),QColor("#193844"),1);
    paths(rivers,Qt::transparent,QColor("#6aacc0"),1.5);
    QFont font("Arial");font.setPixelSize(qMax(18,size.width()/62));font.setWeight(QFont::ExtraBold);p.setFont(font);
    QList<QRectF> occupied{QRectF(size.width()-65,4,61,90),QRectF(0,size.height()-44,qMin(size.width(),520),44)};
    QSet<QString> painted;
    if(labelBounds)labelBounds->clear();
    // Adapt BiAY's offline hierarchy: muted context, warm ancient places,
    // italic physical features and a pink highlight only for the focused site.
    auto label=[&](const Place &place,bool focus,bool linked=false){
      auto point=transform.map(QPointF(place.lon,mercator(place.lat)));
      if(!QRectF(image.rect()).adjusted(10,10,-10,-10).contains(point))return;
      QString name=place.name;name.remove(QRegularExpression("\\s+\\d+$"));
      if(place.confidence<350)name+=" ?";
      const bool water=place.type.contains("river") || place.type.contains("sea") || place.type.contains("lake");
      const bool terrain=place.type.contains("mountain") || place.type.contains("valley") || place.type.contains("hill");
      const QString key=name+QString("/%1/%2").arg(qRound(point.x()/3)).arg(qRound(point.y()/3));
      if(painted.contains(key))return;
      QFont style=font;style.setItalic(water||terrain);style.setWeight(focus?QFont::Black:QFont::ExtraBold);p.setFont(style);
      const QSizeF extent=p.fontMetrics().boundingRect(name).size()+QSizeF(12,10);
      QRectF box;bool placed=false;const QRectF safe=QRectF(image.rect()).adjusted(8,8,-8,-8);
      const QList<int> radii=(focus||linked)?QList<int>{10,30,60,100,150}:QList<int>{10,30};
      for(int radius:radii){
        const QList<QPointF> candidates{
          point+QPointF(radius,-extent.height()/2),point+QPointF(-radius-extent.width(),-extent.height()/2),
          point+QPointF(-extent.width()/2,-radius-extent.height()),point+QPointF(-extent.width()/2,radius),
          point+QPointF(radius,radius),point+QPointF(-radius-extent.width(),radius),
          point+QPointF(radius,-radius-extent.height()),point+QPointF(-radius-extent.width(),-radius-extent.height())};
        for(const auto &candidate:candidates){
          QRectF proposed(candidate,extent);if(!safe.contains(proposed))continue;
          bool collision=false;for(const auto &previous:occupied)if(previous.intersects(proposed.adjusted(-5,-4,5,4))){collision=true;break;}
          if(!collision){box=proposed;placed=true;break;}
        }
        if(placed)break;
      }
      if(!placed)return;
      if(focus||linked){
        const QPointF anchor(qBound(box.left(),point.x(),box.right()),qBound(box.top(),point.y(),box.bottom()));
        if(QLineF(point,anchor).length()>18){p.setPen(QPen(QColor(247,217,232,150),1));p.drawLine(point,anchor);}
      }
      if(focus || linked){p.setPen(QPen(focus?QColor("#ff83b9"):QColor("#f7d9e8"),1));p.setBrush(QColor(146,46,93,focus?150:70));p.drawRoundedRect(box,5,5);}
      if(terrain){p.setPen(Qt::NoPen);p.setBrush(QColor("#b7bba3"));QPolygonF triangle;triangle<<point+QPointF(-5,4)<<point+QPointF(0,-6)<<point+QPointF(5,4);p.drawPolygon(triangle);}
      p.setPen(focus?QColor("#ff83b9"):linked?QColor("#f7d9e8"):water?QColor("#9bc5d3"):terrain?QColor("#b7bba3"):QColor("#e7cba6"));
      p.drawText(box,Qt::AlignCenter,name);occupied.append(box);painted.insert(key);if(labelBounds)labelBounds->append(box);
    };
    for(const auto &place:places)if(place.id==selected)label(place,true,true);
    for(const auto &id:related)if(id!=selected)if(const auto *place=find(id))label(*place,false,true);
    int count=0;
    for(const auto &place:places)if(place.id!=selected && !related.contains(place.id) && ++count<2500 && occupied.size()<48)label(place,false);
    // This offline map is always north-up; the marker is not an estimate.
    const QPointF north(size.width()-36,46);
    p.setPen(Qt::NoPen);p.setBrush(QColor(19,33,41,220));p.drawRoundedRect(QRectF(north.x()-23,10,46,78),8,8);
    p.setPen(QPen(QColor("#f7d9b4"),2));p.drawLine(north+QPointF(0,24),north-QPointF(0,8));
    QPolygonF arrow;arrow<<north+QPointF(-7,0)<<north-QPointF(0,14)<<north+QPointF(7,0);p.setBrush(QColor("#f7d9b4"));p.drawPolygon(arrow);
    QFont compass("Arial");compass.setPixelSize(18);compass.setBold(true);p.setFont(compass);p.drawText(QRectF(north.x()-16,12,32,22),Qt::AlignCenter,"N");
    QFont coordinates("Arial");coordinates.setPixelSize(qMax(13,size.width()/100));p.setFont(coordinates);
    const QString position=QString("Lat %1° · Lon %2° · Offline Bible geography").arg(lat,0,'f',4).arg(lon,0,'f',4);
    auto box=p.fontMetrics().boundingRect(position).adjusted(-8,-5,8,5);box.moveBottomLeft(QPoint(10,size.height()-10));p.fillRect(box,QColor(19,33,41,225));p.setPen(QColor("#b4babb"));p.drawText(box,Qt::AlignCenter,position);
    return image;
  }
private:
  QHash<int,QJsonObject> books;
  QList<QPainterPath> land,lakes,rivers;
  static QJsonDocument load(const QString &name){QFile file(":/geography/"+name+".json");if(!file.open(QIODevice::ReadOnly))return {};return QJsonDocument::fromJson(file.readAll());}
  static QList<QPainterPath> paths(const QJsonArray &rings,bool close){
    QList<QPainterPath> result;
    for(const auto &ring:rings){QPainterPath path;bool first=true;for(const auto &value:ring.toArray()){
      auto point=value.toArray();if(point.size()<2)continue;QPointF p(point[0].toDouble(),mercator(point[1].toDouble()));
      if(first){path.moveTo(p);first=false;}else path.lineTo(p);
    }if(close)path.closeSubpath();result.append(path);}return result;
  }
  BibleMap(){
    land=paths(load("land").array(),true);const auto physical=load("physical").object();
    lakes=paths(physical.value("lakes").toArray(),true);rivers=paths(physical.value("rivers").toArray(),false);
    const auto data=load("places").object();
    for(auto it=data.begin();it!=data.end();++it){auto a=it.value().toArray();if(a.size()<6 || !a[2].isDouble() || !a[3].isDouble())continue;
      places.append({it.key(),a[0].toString(),a[1].toString(),a[4].toString(),a[2].toDouble(),a[3].toDouble(),a[5].toInt()});}
    std::sort(places.begin(),places.end(),[](const Place &a,const Place &b){return a.confidence>b.confidence;});
  }
};
