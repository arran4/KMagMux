#ifndef MAXWIDTHDELEGATE_H
#define MAXWIDTHDELEGATE_H

#include <QApplication>
#include <QPainter>
#include <QStyledItemDelegate>

class MaxWidthDelegate : public QStyledItemDelegate {
  Q_OBJECT
public:
  explicit MaxWidthDelegate(QObject *parent = nullptr)
      : QStyledItemDelegate(parent) {}
  QSize sizeHint(const QStyleOptionViewItem &option,
                 const QModelIndex &index) const override {
    QSize hint = QStyledItemDelegate::sizeHint(option, index);
    if (option.widget) {
      int max_w = option.widget->width() * 0.75;
      if (hint.width() > max_w) {
        return QSize(max_w, hint.height());
      }
    }
    return hint;
  }

  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override {
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    // Force elision instead of word wrapping.
    opt.features &= ~QStyleOptionViewItem::WrapText;
    opt.textElideMode = Qt::ElideRight;

    // Check if this is the "Source" column or if the text looks like a link
    // We can just format any URL-looking text as a link color
    QString text = opt.text;
    if (text.startsWith("http://") || text.startsWith("https://") ||
        text.startsWith("magnet:") || text.startsWith("file://")) {
      // Change text color to standard link color (blue)
      opt.palette.setColor(QPalette::Text,
                           QApplication::palette().color(QPalette::Link));
    }

    QStyledItemDelegate::paint(painter, opt, index);
  }
};

#endif // MAXWIDTHDELEGATE_H
