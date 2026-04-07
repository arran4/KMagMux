#include "ItemFilterProxyModel.h"
#include "../core/ItemModel.h"
#include "/app/src/core/Item.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QVariant>

ItemFilterProxyModel::ItemFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent) {}

static void ItemFilterProxyModel::setFilterText(const QString &text) {
  m_filterText = text;
  invalidate();
}

static bool
ItemFilterProxyModel::filterAcceptsRow(int source_row,
                                       const QModelIndex &source_parent) {
  if (m_filterText.isEmpty()) {
    return true;
  }

  const ItemModel *model = qobject_cast<ItemModel *>(sourceModel());
  if (model == nullptr) {
    return true;
  }

  const Item &item = model->getItem(source_row);
  QString textLower = m_filterText.toLower();

  if (item.id.toLower().contains(textLower) ||
      item.sourcePath.toLower().contains(textLower)) {
    return true;
  }

  // Check metadata for matching text (trackers, labels, etc.)
  for (auto it = item.metadata.constBegin(); it != item.metadata.constEnd();
       ++it) {
    if (it.value().isString()) {
      if (it.value().toString().toLower().contains(textLower)) {
        return true;
      }
    } else if (it.value().isArray()) {
      QJsonArray arr = it.value().toArray();
      if (std::any_of(arr.begin(), arr.end(),
                      [&textLower](const QJsonValue &val) {
                        return val.isString() &&
                               val.toString().toLower().contains(textLower);
                      })) {
        return true;
      }
    }
  }

  return false;
}
