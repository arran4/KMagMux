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

  std::vector<Item> getItems() const;
  bool shouldDeleteOriginal() const;

private slots:
  void onProcessClicked();

private:
  std::vector<Item> m_items;

  QTableWidget *m_itemsTable;
  QComboBox *m_connectorCombo;
  QLineEdit *m_labelsEdit;
  QCheckBox *m_deleteOriginalCheck;
  QCheckBox *m_enableScheduleCheck;
  QDateTimeEdit *m_scheduleEdit;

  void setupUi();
};

#endif // ADDITEMDIALOG_H
