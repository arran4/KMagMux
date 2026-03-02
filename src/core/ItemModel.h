#ifndef ITEMMODEL_H
#define ITEMMODEL_H

#include "Item.h"
#include <QAbstractTableModel>
#include <vector>

class ItemModel : public QAbstractTableModel {
  Q_OBJECT

public:
  enum Columns { ColId = 0, ColState, ColSource, ColCreated, ColumnCount };

  explicit ItemModel(QObject *parent = nullptr);

  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  int columnCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role = Qt::DisplayRole) const override;

  void addItem(const Item &item);
  void setItems(const std::vector<Item> &items);
  const Item &getItem(int row) const;

private:
  std::vector<Item> m_items;
};

#endif // ITEMMODEL_H
