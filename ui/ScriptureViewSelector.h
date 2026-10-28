#pragma once
#include <QWidget>
#include <QHBoxLayout>
#include <QPushButton>
#include <QButtonGroup>
#include <functional>

class ScriptureViewSelector:public QWidget {
public:
  std::function<void(int)> changed;
  explicit ScriptureViewSelector(QWidget *parent=nullptr):QWidget(parent){
    setAttribute(Qt::WA_StyledBackground,true);
    setObjectName("scriptureViewSegments");setSizePolicy(QSizePolicy::Maximum,QSizePolicy::Fixed);setFixedHeight(34);
    setStyleSheet("#scriptureViewSegments{background:#172638;border:1px solid #34465b;border-radius:7px;}"
      "QPushButton{background:transparent;color:#cbd5e1;border:0;border-radius:5px;padding:4px 13px;font-size:12px;font-weight:600;min-height:0;min-width:0;}"
      "QPushButton:hover{background:#293e53;color:white;}"
      "QPushButton:checked{background:#0ea5e9;color:#061827;font-weight:800;}"
      "QPushButton:focus{border:1px solid #7dd3fc;}");
    auto *layout=new QHBoxLayout(this);layout->setContentsMargins(3,3,3,3);layout->setSpacing(2);
    group=new QButtonGroup(this);group->setExclusive(true);
    const QStringList labels{"Bible","Bible + Map","Map"};
    for(int i=0;i<labels.size();++i){auto *button=new QPushButton(labels[i]);button->setCheckable(true);button->setCursor(Qt::PointingHandCursor);button->setFixedHeight(26);
      button->setToolTip(i==0?"Project Bible text only":i==1?"Project Bible text with its linked map":"Project the linked map only");
      button->setAccessibleName(button->toolTip());group->addButton(button,i);layout->addWidget(button);}
    setMode(0);
    connect(group,&QButtonGroup::idClicked,this,[this](int mode){if(changed)changed(mode);});
  }
  void setMode(int mode){if(auto *button=group->button(qBound(0,mode,2)))button->setChecked(true);}
  int mode()const{return group->checkedId();}
private:
  QButtonGroup *group;
};
