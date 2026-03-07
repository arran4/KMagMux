#ifndef ADDITEMDIALOG_H
#define ADDITEMDIALOG_H

#include "BaseItemDialog.h"

class AddItemDialog : public BaseItemDialog {
  Q_OBJECT

public:
  explicit AddItemDialog(const std::vector<Item> &items,
                         const QStringList &connectors,
                         QWidget *parent = nullptr);

private slots:
  void onProcessClicked();

private:
  void setupUi();
};

#endif // ADDITEMDIALOG_H
