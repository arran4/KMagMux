#include "StatusCountsWidget.h"
#include <QAbstractItemModel>
#include <QHBoxLayout>
#include <QLabel>
#include <QString>
#include <QWidget>

StatusCountsWidget::StatusCountsWidget(QAbstractItemModel *inbox,
                                       QAbstractItemModel *queue,
                                       QAbstractItemModel *errors,
                                       QAbstractItemModel *done,
                                       QWidget *parent)
    : QWidget(parent) {
  setAccessibleName(tr("Current item totals"));
  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(4, 0, 4, 0);
  const auto bind = [this, layout](const QString &name,
                                   QAbstractItemModel *model) {
    auto *label = new QLabel(this);
    label->setObjectName(name);
    label->setToolTip(
        tr("%1: current total (independent of search)").arg(name));
    layout->addWidget(label);
    const auto refresh = [label, model, name] {
      label->setText(QStringLiteral("%1 %2").arg(name).arg(model->rowCount()));
      label->setAccessibleName(
          tr("%1: %2 items total").arg(name).arg(model->rowCount()));
    };
    connect(model, &QAbstractItemModel::modelReset, label, refresh);
    connect(model, &QAbstractItemModel::rowsInserted, label, refresh);
    connect(model, &QAbstractItemModel::rowsRemoved, label, refresh);
    refresh();
  };
  bind(tr("Inbox"), inbox);
  bind(tr("Queue"), queue);
  bind(tr("Errors"), errors);
  bind(tr("Done"), done);
}
