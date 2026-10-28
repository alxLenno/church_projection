#pragma once
#include <QTextDocument>
#include <QTextOption>
#include <QFont>
#include <QSize>

// Use the same document for measuring and painting so paragraph spacing and
// long-word wrapping cannot diverge between the fit calculation and output.
inline int fitObsText(QTextDocument &document, const QString &text,
                      QFont font, QSize area, int maximum) {
  document.setDocumentMargin(0);
  QTextOption option(Qt::AlignHCenter);
  option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
  document.setDefaultTextOption(option);
  document.setPlainText(text);
  document.setTextWidth(qMax(1,area.width()));
  int low=1,high=qMax(1,maximum),best=1;
  while(low<=high) {
    int size=(low+high)/2;font.setPointSize(size);document.setDefaultFont(font);
    if(document.size().height()<=area.height() && document.idealWidth()<=area.width()+0.5) {best=size;low=size+1;}
    else high=size-1;
  }
  font.setPointSize(best);document.setDefaultFont(font);
  return best;
}

// Split by rendered lines, preserving the order and every word. This keeps
// transparent captions readable instead of shrinking an entire stanza.
#include <QTextLayout>
#include <QStringList>
inline QStringList obsCaptionPages(const QString &text,QFont font,QSize area,int maximum) {
  if(text.trimmed().isEmpty())return {text};
  QTextDocument measure;
  // Keep the requested caption size. Use one line when two would force
  // a smaller font in the reserved bottom area.
  const int twoLineSize=fitObsText(measure,"Ag\nAg",font,area,maximum);
  const int linesPerPage=twoLineSize>=maximum?2:1;
  const int size=fitObsText(measure,linesPerPage==2?"Ag\nAg":"Ag",font,area,maximum);
  font.setPointSize(size);
  QStringList lines;
  for(const QString &paragraph:text.split('\n')){
    if(paragraph.trimmed().isEmpty())continue;
    QTextLayout layout(paragraph,font);QTextOption option;
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);layout.setTextOption(option);
    layout.beginLayout();
    while(true){auto line=layout.createLine();if(!line.isValid())break;line.setLineWidth(qMax(1,area.width()));
      lines.append(paragraph.mid(line.textStart(),line.textLength()).trimmed());}
    layout.endLayout();
  }
  QStringList pages;
  for(int i=0;i<lines.size();i+=linesPerPage)pages.append(lines.mid(i,linesPerPage).join('\n'));
  return pages.isEmpty()?QStringList{text}:pages;
}
