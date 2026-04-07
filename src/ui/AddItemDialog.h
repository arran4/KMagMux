#ifndef ADDITEMDIALOG_H
#define ADDITEMDIALOG_H

#include <QCheckBox>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QObject>
#include <QPoint>
#include <QPushButton>
#include <QStringList>
#include <QTableWidget>
#include <QWidget>
#include <vector>

class AddItemDialog : public QDialog {
  Q_OBJECT

public:
  explicit AddItemDialog(const std::vector<Item> &items,
                         const QStringList &connectors,
                         QWidget *parent = nullptr);

  const std::vector<Item> &getItems() const;

private Q_SLOTS:
  void onProcessClicked();
  void onCustomContextMenuRequested(const QPoint &pos);

  std::vector<Item> m_items;

  QTableWidget *m_itemsTable;

  static void setupUi();
};

#endif // ADDITEMDIALOG_H
