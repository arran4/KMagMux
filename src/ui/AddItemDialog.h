#ifndef ADDITEMDIALOG_H
#define ADDITEMDIALOG_H

#include "../core/Item.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStringList>
#include <QTableWidget>
#include <vector>

class AddItemDialog : public QDialog {
  Q_OBJECT

public:
  explicit AddItemDialog(const std::vector<Item> &items,
                         const QStringList &connectors,
                         QWidget *parent = nullptr);

  static constexpr int Process = QDialog::Accepted + 1;

  const std::vector<Item> &getItems() const;

private slots:
  void onProcessClicked();
  void onCustomContextMenuRequested(const QPoint &pos);

private:
  std::vector<Item> m_items;

  QTableWidget *m_itemsTable;

  void setupUi();
  void finishSelection(int result);
};

#endif // ADDITEMDIALOG_H
