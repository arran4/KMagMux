#include "ItemModel.h"

ItemModel::ItemModel(QObject *parent) : QAbstractTableModel(parent) {}

int ItemModel::rowCount(const QModelIndex &parent) const {
  if (parent.isValid())
    return 0;
  return static_cast<int>(m_items.size());
}

int ItemModel::columnCount(const QModelIndex &parent) const {
  if (parent.isValid())
    return 0;
  return ColumnCount;
}

QVariant ItemModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.row() >= static_cast<int>(m_items.size()))
    return QVariant();

  const Item &item = m_items[index.row()];

  if (role == Qt::DisplayRole) {
    switch (index.column()) {
    case ColId:
      return item.id;
    case ColState:
      return item.stateToString();
    case ColSource:
      return item.sourcePath;
    case ColCreated:
      return item.createdTime.toString(Qt::ISODate);
    default:
      return QVariant();
    }
  }
  return QVariant();
}

QVariant ItemModel::headerData(int section, Qt::Orientation orientation,
                               int role) const {
  if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
    return QVariant();

  switch (section) {
  case ColId:
    return "ID";
  case ColState:
    return "State";
  case ColSource:
    return "Source";
  case ColCreated:
    return "Created";
  default:
    return QVariant();
  }
}

void ItemModel::addItem(const Item &item) {
  beginInsertRows(QModelIndex(), rowCount(), rowCount());
  m_items.push_back(item);
  endInsertRows();
}

void ItemModel::setItems(const std::vector<Item> &items) {
  beginResetModel();
  m_items = items;
  endResetModel();
}

const Item &ItemModel::getItem(int row) const { return m_items[row]; }
