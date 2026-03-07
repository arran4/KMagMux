#ifndef PROCESSITEMDIALOG_H
#define PROCESSITEMDIALOG_H

#include "BaseItemDialog.h"
#include <QComboBox>
#include <QDateTimeEdit>

class ProcessItemDialog : public BaseItemDialog {
  Q_OBJECT

public:
  explicit ProcessItemDialog(const std::vector<Item> &items,
                             const QStringList &connectors,
                             QWidget *parent = nullptr);

private slots:
  void onProcessClicked();
  void onStateChanged(int index);

private:
  QStringList m_connectors;

  QComboBox *m_stateCombo;
  QDateTimeEdit *m_holdTimeEdit;
  QComboBox *m_connectorCombo;

  void setupUi();
};

#endif // PROCESSITEMDIALOG_H
