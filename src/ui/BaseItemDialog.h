#ifndef BASEITEMDIALOG_H
#define BASEITEMDIALOG_H

#include "../core/Item.h"
#include <QDialog>
#include <QTableWidget>
#include <vector>

class BaseItemDialog : public QDialog {
  Q_OBJECT

public:
  explicit BaseItemDialog(const std::vector<Item> &items, QWidget *parent = nullptr);

  std::vector<Item> getItems() const;

protected slots:
  virtual void onCustomContextMenuRequested(const QPoint &pos);

protected:
  std::vector<Item> m_items;
  QTableWidget *m_itemsTable;

  void setupTable();
  void populateTable();
};

#endif // BASEITEMDIALOG_H
