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

class AddItemDialog : public QDialog {
  Q_OBJECT

public:
  explicit AddItemDialog(Item &item, QWidget *parent = nullptr);

  Item getItem() const;
  bool shouldDeleteOriginal() const;

private slots:
  void onDispatchClicked();
  void onQueueClicked();
  void onScheduleClicked();
  void onHoldClicked();

private:
  Item m_item;

  QLineEdit *m_sourceEdit;
  QLineEdit *m_destEdit;
  QComboBox *m_connectorCombo;
  QLineEdit *m_labelsEdit;
  QCheckBox *m_deleteOriginalCheck;
  QDateTimeEdit *m_scheduleEdit;

  void setupUi();
};

#endif // ADDITEMDIALOG_H
