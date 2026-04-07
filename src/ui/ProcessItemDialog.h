#ifndef PROCESSITEMDIALOG_H
#define PROCESSITEMDIALOG_H

#include "../core/Item.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QObject>
#include <QPoint>
#include <QPushButton>
#include <QStringList>
#include <QTableWidget>
#include <QWidget>
#include <vector>

class ProcessItemDialog : public QDialog {
  Q_OBJECT

public:
  explicit ProcessItemDialog(const std::vector<Item> &items,
                             const QStringList &connectors,
                             QWidget *parent = nullptr);

  const std::vector<Item> &getItems() const;

private Q_SLOTS:
  void onProcessClicked();
  void onCustomContextMenuRequested(const QPoint &pos);
  void onStateChanged(int index);

  std::vector<Item> m_items;
  QStringList m_connectors;

  QTableWidget *m_itemsTable;
  QComboBox *m_stateCombo;
  QDateTimeEdit *m_holdTimeEdit;
  QListWidget *m_connectorList;

  void setupUi();
};

#endif // PROCESSITEMDIALOG_H
