#ifndef STATUSCOUNTSWIDGET_H
#define STATUSCOUNTSWIDGET_H

#include <QWidget>

class QAbstractItemModel;

// Current source-model totals, independent of filters and unseen arrivals.
class StatusCountsWidget : public QWidget {
  Q_OBJECT
public:
  StatusCountsWidget(QAbstractItemModel *inbox, QAbstractItemModel *queue,
                     QAbstractItemModel *errors, QAbstractItemModel *done,
                     QWidget *parent = nullptr);
};

#endif
