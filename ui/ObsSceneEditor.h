#pragma once
#include "ProjectionWindow.h"
#include <QDialog>
#include <QLabel>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QColorDialog>
#include <QFileDialog>
#include <QInputDialog>
#include <QSettings>
#include <QJsonDocument>
#include <QTimer>
#include <QSignalBlocker>
#include <QScrollArea>

class ObsScenePreview : public QWidget {
public:
  QImage frame;
  const ObsSceneLayout *scene=nullptr;
  std::function<void(bool,Qt::Edges,QPoint)> drag;
  explicit ObsScenePreview(QWidget *parent=nullptr):QWidget(parent){setMinimumSize(560,315);setMouseTracking(true);}
protected:
  QRect target() const {QSize s(1920,1080);s.scale(size(),Qt::KeepAspectRatio);return QRect(rect().center()-QPoint(s.width()/2,s.height()/2),s);}
  QPoint canvasPoint(QPointF p) const {auto r=target();return QPoint(qRound((p.x()-r.x())*1920/r.width()),qRound((p.y()-r.y())*1080/r.height()));}
  QRectF screenBox(QRect r) const {auto a=target();return QRectF(a.x()+r.x()*a.width()/1920.0,a.y()+r.y()*a.height()/1080.0,r.width()*a.width()/1920.0,r.height()*a.height()/1080.0);}
  QList<QPair<QPointF,Qt::Edges>> handles(QRectF r) const {
    return {{r.topLeft(),Qt::TopEdge|Qt::LeftEdge},{r.topRight(),Qt::TopEdge|Qt::RightEdge},
      {r.bottomLeft(),Qt::BottomEdge|Qt::LeftEdge},{r.bottomRight(),Qt::BottomEdge|Qt::RightEdge},
      {QPointF(r.center().x(),r.top()),Qt::TopEdge},{QPointF(r.center().x(),r.bottom()),Qt::BottomEdge},
      {QPointF(r.left(),r.center().y()),Qt::LeftEdge},{QPointF(r.right(),r.center().y()),Qt::RightEdge}};
  }
  Qt::Edges hit(QPointF point,QRect box) const {
    for(auto handle:handles(screenBox(box)))if(QRectF(handle.first-QPointF(10,10),QSizeF(20,20)).contains(point))return handle.second;
    return {};
  }
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);p.fillRect(rect(),QColor("#0b1220"));p.setRenderHint(QPainter::SmoothPixmapTransform);
    if(!frame.isNull())p.drawImage(target(),frame);
    if(scene)for(const QRect box:scene->transparent?QList<QRect>{scene->text}:QList<QRect>{scene->phone,scene->text}) {
      p.setBrush(Qt::NoBrush);p.setPen(QPen(QColor("#38bdf8"),1,Qt::DashLine));p.drawRect(screenBox(box));
      p.setPen(QPen(QColor("#38bdf8"),1));p.setBrush(Qt::white);
      for(auto handle:handles(screenBox(box)))p.drawRect(QRectF(handle.first-QPointF(5,5),QSizeF(10,10)));
    }
  }
  void mousePressEvent(QMouseEvent *e) override {
    if(!scene||e->button()!=Qt::LeftButton)return;
    last=canvasPoint(e->position());edges={};active=false;
    // Handles are hit-tested in screen pixels BEFORE rectangle interiors,
    // including the half of a handle that lies outside its rectangle.
    for(bool isPhone:{true,false}) {if(isPhone && scene->transparent)continue;auto box=isPhone?scene->phone:scene->text;auto handle=hit(e->position(),box);
      if(handle){phone=isPhone;edges=handle;active=true;break;}
    }
    if(!active){phone=!scene->transparent && scene->phone.contains(last);active=phone||scene->text.contains(last);}
    if(active){grabMouse();e->accept();}
  }
  void mouseMoveEvent(QMouseEvent *e) override {
    auto point=canvasPoint(e->position());
    if(active&&drag){drag(phone,edges,point-last);last=point;return;}
    if(!scene)return;
    for(const QRect box:scene->transparent?QList<QRect>{scene->text}:QList<QRect>{scene->phone,scene->text}) {auto edge=hit(e->position(),box);
      if(edge){bool h=edge.testFlag(Qt::LeftEdge)||edge.testFlag(Qt::RightEdge),v=edge.testFlag(Qt::TopEdge)||edge.testFlag(Qt::BottomEdge);
        setCursor(h&&v ? ((edge.testFlag(Qt::LeftEdge)==edge.testFlag(Qt::TopEdge))?Qt::SizeFDiagCursor:Qt::SizeBDiagCursor) : h?Qt::SizeHorCursor:Qt::SizeVerCursor);return;}
      if(box.contains(point)){setCursor(Qt::SizeAllCursor);return;}
    }unsetCursor();
  }
  void mouseReleaseEvent(QMouseEvent *) override {if(active)releaseMouse();active=false;}
private:
  QPoint last;bool active=false,phone=false;Qt::Edges edges;
};

class ObsSceneEditor : public QDialog {
public:
  ObsSceneLayout draft;
  ObsSceneEditor(ProjectionWindow *output, QWidget *parent=nullptr) : QDialog(parent), draft(output->obsScene) {
    setWindowTitle("OBS Scene Layout");resize(1200,760);
    setStyleSheet("QLabel,QCheckBox{font-size:12px;} QLineEdit,QComboBox,QSpinBox{font-size:12px;padding:4px 6px;min-width:0;min-height:24px;} QPushButton{font-size:12px;padding:6px 8px;min-width:0;} QScrollArea{border:none;}");
    auto *outer=new QVBoxLayout(this);
    auto *hint=new QLabel("1920 × 1080 canvas. Choose a panel layout or a transparent bottom-text overlay. Drag panels to move; use any white edge or corner handle to resize.");
    hint->setWordWrap(true);outer->addWidget(hint);
    auto *row=new QHBoxLayout();outer->addLayout(row,1);
    auto *left=new QVBoxLayout();row->addLayout(left,1);
    auto *preview=new ObsScenePreview();preview->scene=&draft;left->addWidget(preview,1);
    auto *instructions=new QLabel("Drag the phone frame and text panel to arrange them. Resize with any white edge or corner handle. Text wraps and fits as the panel changes. In OBS, drag the phone picture into the reserved frame.");instructions->setWordWrap(true);left->addWidget(instructions);
    auto *coordinates=new QLabel();coordinates->setWordWrap(true);
    auto *warning=new QLabel();warning->setWordWrap(true);warning->setStyleSheet("color:#fbbf24");left->addWidget(warning);
    auto *settingsPane=new QWidget();auto *right=new QFormLayout(settingsPane);right->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);right->setRowWrapPolicy(QFormLayout::WrapLongRows);
    auto *scroll=new QScrollArea();scroll->setWidgetResizable(true);scroll->setMinimumWidth(340);scroll->setMaximumWidth(440);scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);scroll->setWidget(settingsPane);row->addWidget(scroll);
    auto *enabled=new QCheckBox("Use separate OBS scene");right->addRow(enabled);
    auto *presets=new QComboBox();presets->addItems({"Portrait phone + content","Landscape phone + content","Landscape phone above lyrics","Transparent bottom text over camera"});
    QJsonObject saved=QJsonDocument::fromJson(QSettings().value("OBS/ScenePresets").toByteArray()).object();
    for (const auto &name:saved.keys()) presets->addItem(name);
    right->addRow("Preset",presets);
    auto *transparent=new QCheckBox("Transparent text overlay (camera fills OBS)");right->addRow(transparent);
    auto *source=new QComboBox();source->addItems({"Content from Screen 1","Content from Screen 2"});right->addRow("Content",source);
    auto *title=new QLineEdit();right->addRow("Heading",title);
    auto *advancedToggle=new QCheckBox("Advanced numeric positioning (optional)");right->addRow(advancedToggle);
    auto *advanced=new QWidget();auto *geometry=new QFormLayout(advanced);right->addRow(advanced);advanced->hide();
    geometry->addRow(coordinates);
    connect(advancedToggle,&QCheckBox::toggled,advanced,&QWidget::setVisible);
    auto boxSpin=[&](const QString &label,int lo,int hi){auto *v=new QSpinBox();v->setRange(lo,hi);geometry->addRow(label,v);return v;};
    auto *px=boxSpin("Phone left",0,1919),*py=boxSpin("Phone top",0,1079),*pw=boxSpin("Phone width",100,1920),*ph=boxSpin("Phone height",100,1080);
    auto *tx=boxSpin("Text left",0,1919),*ty=boxSpin("Text top",0,1079),*tw=boxSpin("Text width",100,1920),*th=boxSpin("Text height",100,1080);
    auto spin=[&](const QString &label,int lo,int hi){auto *v=new QSpinBox();v->setRange(lo,hi);right->addRow(label,v);return v;};
    auto *size=spin("Maximum text size",12,150),*padding=spin("Text padding",0,100),*border=spin("Border width",0,12);
    auto *backing=spin("Caption black box (%)",0,100);backing->setToolTip("0 = no box; 90 = dark uniform box behind the complete text section.");
    auto *panelScale=spin("Panel text size (%)",100,250);
    auto *captionPaging=new QCheckBox("Split captions into pages (optional)");right->addRow(captionPaging);
    auto *captionScale=spin("Caption size (%)",100,250);captionScale->setToolTip("Transparent captions: 150% by default. Full sections grow upward to stay visible; very long sections fit within the lower screen.");
    auto *banner=new QCheckBox("Include current stage banner");right->addRow(banner);
    auto *colors=new QGridLayout();right->addRow("Colours",colors);
    auto *bg=new QPushButton("Stage"),*panel=new QPushButton("Panels"),*fg=new QPushButton("Text"),*accent=new QPushButton("Border");
    colors->addWidget(bg,0,0);colors->addWidget(panel,0,1);colors->addWidget(fg,1,0);colors->addWidget(accent,1,1);
    auto *image=new QPushButton("Choose background image…"),*clearImage=new QPushButton("Clear image");right->addRow(image);right->addRow(clearImage);
    auto *save=new QPushButton("Save as preset…");right->addRow(save);
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Apply|QDialogButtonBox::Cancel);outer->addWidget(buttons);
    auto *apply=buttons->button(QDialogButtonBox::Apply);apply->setText("Apply to OBS feed");
    auto load=[=] {
      const QList<QObject *> objects{backing,panelScale,captionPaging,captionScale,transparent,enabled,source,title,px,py,pw,ph,tx,ty,tw,th,size,padding,border,banner};
      for(auto *o:objects)o->blockSignals(true);
      backing->setValue(draft.captionBacking);panelScale->setValue(draft.panelScale);
      captionPaging->setChecked(draft.captionPaging);
      captionScale->setValue(draft.captionScale);
      transparent->setChecked(draft.transparent);
      enabled->setChecked(draft.enabled);source->setCurrentIndex(draft.source);title->setText(draft.title);
      px->setValue(draft.phone.x());py->setValue(draft.phone.y());pw->setValue(draft.phone.width());ph->setValue(draft.phone.height());
      tx->setValue(draft.text.x());ty->setValue(draft.text.y());tw->setValue(draft.text.width());th->setValue(draft.text.height());
      size->setValue(draft.textSize);padding->setValue(draft.padding);border->setValue(draft.border);banner->setChecked(draft.banner);
      for(auto *o:objects)o->blockSignals(false);
    };
    auto read=[=] {
      draft.captionBacking=backing->value();draft.panelScale=panelScale->value();
      draft.captionPaging=captionPaging->isChecked();
      draft.captionScale=captionScale->value();
      draft.transparent=transparent->isChecked();
      draft.enabled=enabled->isChecked();draft.source=source->currentIndex();draft.title=title->text();
      draft.phone=QRect(px->value(),py->value(),pw->value(),ph->value());draft.text=QRect(tx->value(),ty->value(),tw->value(),th->value());
      draft.textSize=size->value();draft.padding=padding->value();draft.border=border->value();draft.banner=banner->isChecked();
    };
    auto refresh=[=] {
      const QRect canvas(0,0,1920,1080);
      QString error;
      if(!canvas.contains(draft.phone)||!canvas.contains(draft.text))error="Keep both panels inside the canvas.";
      else if(!draft.title.trimmed().isEmpty() && qMin(draft.phone.top(),draft.text.top())<100)error="Move panels below the heading (top 100 or more), or clear the heading.";
      else if(!draft.transparent && draft.phone.intersects(draft.text))error="The phone and text panels overlap. Adjust their positions or dimensions.";
      else if(draft.text.width()<=draft.padding*2 || draft.text.height()<=draft.padding*2)error="Reduce padding to leave space for text.";
      else if(draft.banner && qMax(draft.phone.bottom(),draft.text.bottom())>=output->obsContentBottom())error="Panels overlap the stage banner. Reduce their height or turn off the banner here.";
      warning->setText(error);apply->setEnabled(!draft.enabled || error.isEmpty());save->setEnabled(error.isEmpty());
      instructions->setText(draft.transparent?"Transparent overlay: place this app Browser source ABOVE the full-screen camera in OBS. Drag or resize the text panel; the full selected section wraps and grows upward as needed. Paging is optional. The dark preview area is transparent in OBS.":"Drag the phone frame and text panel to arrange them. Resize with any white edge or corner handle. Place your phone above the background in OBS.");
      coordinates->setText(QString("OBS phone source → Edit Transform:\nPosition X %1, Y %2 · Bounding box %3 × %4\nUse Scale to inner bounds to show the whole camera picture, or Scale to outer bounds plus Crop to Bounding Box to fill it. Match the phone orientation to this area.\nAudio stays in OBS as a separate source.").arg(draft.phone.x()).arg(draft.phone.y()).arg(draft.phone.width()).arg(draft.phone.height()));
      preview->frame=output->renderObsScene(draft,true);preview->update();
    };
    auto keepAboveBanner=[=]{
      int top=draft.title.trimmed().isEmpty()?24:100;
      int bottom=draft.banner?qMin(1056,output->obsContentBottom()-20):1056;
      bottom=qMax(top+120,bottom);
      int oldBottom=qMax(draft.phone.bottom(),draft.text.bottom())+1;
      if(oldBottom>bottom){
        double factor=double(bottom-top)/qMax(1,oldBottom-top);
        for(QRect *box:{&draft.phone,&draft.text}) {
          box->setRect(box->x(),top+qRound((box->y()-top)*factor),box->width(),qMax(100,qRound(box->height()*factor)));
        }
      }
      for(QRect *box:{&draft.phone,&draft.text}) {
        box->setSize(QSize(qBound(100,box->width(),1872),qBound(100,box->height(),bottom-top)));
        box->moveTopLeft(QPoint(qBound(24,box->x(),1896-box->width()),qBound(top,box->y(),bottom-box->height())));
      }
      load();
    };
    preview->drag=[=](bool phone,Qt::Edges edges,QPoint delta){
      QRect &box=phone?draft.phone:draft.text;const QRect other=phone?draft.text:draft.phone;
      QRect next=box;int top=draft.title.trimmed().isEmpty()?24:100;
      int bottom=draft.banner?qMin(1056,output->obsContentBottom()-20):1056;
      bottom=qMax(top+120,bottom);
      if(edges) {
        if(edges.testFlag(Qt::LeftEdge))next.setLeft(qBound(24,box.left()+delta.x(),box.right()-99));
        if(edges.testFlag(Qt::RightEdge))next.setRight(qBound(next.left()+99,box.right()+delta.x(),1895));
        if(edges.testFlag(Qt::TopEdge))next.setTop(qBound(top,box.top()+delta.y(),box.bottom()-99));
        if(edges.testFlag(Qt::BottomEdge))next.setBottom(qBound(next.top()+99,box.bottom()+delta.y(),bottom-1));
      } else next.moveTopLeft(QPoint(qBound(24,box.x()+delta.x(),1896-box.width()),qBound(top,box.y()+delta.y(),bottom-box.height())));
      if(draft.transparent || !next.adjusted(-8,-8,8,8).intersects(other))box=next;
      load();refresh();
    };
    for(auto *s:{backing,panelScale,captionScale,px,py,pw,ph,tx,ty,tw,th,size,padding,border})connect(s,&QSpinBox::valueChanged,this,[=]{read();refresh();});
    connect(title,&QLineEdit::textChanged,this,[=]{read();refresh();});
    connect(source,&QComboBox::currentIndexChanged,this,[=]{read();refresh();});
    connect(captionPaging,&QCheckBox::toggled,this,[=]{read();refresh();});
    connect(transparent,&QCheckBox::toggled,this,[=]{read();refresh();});
    connect(enabled,&QCheckBox::toggled,this,[=]{read();refresh();});
    connect(banner,&QCheckBox::toggled,this,[=]{read();keepAboveBanner();refresh();});
    connect(presets,&QComboBox::activated,this,[=](int i){
      if(i>=4) draft=ObsSceneLayout::fromJson(QJsonDocument::fromJson(QSettings().value("OBS/ScenePresets").toByteArray()).object().value(presets->itemText(i)).toObject());
      else {draft=ObsSceneLayout();draft.enabled=true;
        if(i==1){draft.phone=QRect(64,160,1040,585);draft.text=QRect(1144,120,712,800);}
        if(i==3){draft.transparent=true;draft.banner=false;draft.border=0;draft.padding=16;draft.text=QRect(96,760,1728,256);}
        if(i==2){draft.phone=QRect(424,28,1072,603);draft.text=QRect(64,660,1792,260);}
      }
      keepAboveBanner();refresh();
    });
    auto choose=[=](QPushButton *button,QColor ObsSceneLayout::*field){connect(button,&QPushButton::clicked,this,[=]{auto c=QColorDialog::getColor(draft.*field,this);if(c.isValid()){draft.*field=c;refresh();}});};
    choose(bg,&ObsSceneLayout::background);choose(panel,&ObsSceneLayout::panel);choose(fg,&ObsSceneLayout::foreground);choose(accent,&ObsSceneLayout::accent);
    connect(image,&QPushButton::clicked,this,[=]{auto f=QFileDialog::getOpenFileName(this,"OBS background",{},"Images (*.png *.jpg *.jpeg *.webp)");if(!f.isEmpty()){draft.backgroundImage=f;refresh();}});
    connect(clearImage,&QPushButton::clicked,this,[=]{draft.backgroundImage.clear();refresh();});
    connect(save,&QPushButton::clicked,this,[=]() mutable {bool ok=false;auto name=QInputDialog::getText(this,"Save OBS preset","Name",QLineEdit::Normal,presets->currentIndex()>=4?presets->currentText():QString(),&ok).trimmed();if(!ok||name.isEmpty())return;auto all=QJsonDocument::fromJson(QSettings().value("OBS/ScenePresets").toByteArray()).object();all[name]=draft.json();QSettings().setValue("OBS/ScenePresets",QJsonDocument(all).toJson(QJsonDocument::Compact));saved=all;if(presets->findText(name)<0)presets->addItem(name);});
    connect(apply,&QPushButton::clicked,this,[=]{read();accept();});connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);
    auto *timer=new QTimer(this);connect(timer,&QTimer::timeout,this,refresh);timer->start(250);
    keepAboveBanner();refresh();
  }
};
