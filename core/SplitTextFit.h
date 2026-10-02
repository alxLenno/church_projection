#pragma once
#include <QFont>
#include <QFontMetricsF>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QtMath>

// Preserve authored line breaks, fitting the longest line and whole block.
inline int fitSplitText(const QString &text, const QString &family,
                        const QSize &area, int maximum, bool scrolling, bool preserveLines = true) {
  const QStringList lines = text.split('\n');
  for (int size = qMax(1, maximum); size >= 1; --size) {
    const QFontMetricsF metrics(QFont(family, size, QFont::Bold));
    qreal width = 0;
    for (const auto &line : lines) width = qMax(width, metrics.horizontalAdvance(line));
    if (preserveLines) {
      if (width <= area.width() && (scrolling || metrics.lineSpacing() * lines.size() <= area.height())) return size;
    } else {
      const QRectF bounds = metrics.boundingRect(QRectF(0, 0, area.width(), 100000), Qt::TextWordWrap, text);
      if (bounds.width() <= area.width() && (scrolling || bounds.height() <= area.height())) return size;
    }
  }
  return 1;
}

inline bool preserveSplitLines(const QString &text) {
  for (const auto &line : text.split('\n'))
    if (line.trimmed().size() > 80) return false;
  return true;
}
