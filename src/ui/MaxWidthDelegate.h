#ifndef MAXWIDTHDELEGATE_H
#define MAXWIDTHDELEGATE_H

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
};

#endif // MAXWIDTHDELEGATE_H
