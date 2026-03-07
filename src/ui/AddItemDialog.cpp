#include "AddItemDialog.h"
#include "../core/Constants.h"
#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>

AddItemDialog::AddItemDialog(const std::vector<Item> &items,
                             const QStringList &connectors, QWidget *parent)
    : BaseItemDialog(items, parent) {
  setupUi();
  setWindowTitle("Add Items");
  resize(850, 500);
  populateTable();
}

void AddItemDialog::setupUi() {
  QVBoxLayout *mainLayout = new QVBoxLayout(this);

  // Table
  setupTable();
  mainLayout->addWidget(m_itemsTable);

  // Action Buttons
  QHBoxLayout *btnLayout = new QHBoxLayout();

  QPushButton *processBtn = new QPushButton("Add to Inbox", this);
  connect(processBtn, &QPushButton::clicked, this,
          &AddItemDialog::onProcessClicked);

  QPushButton *cancelBtn = new QPushButton("Cancel", this);
  connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

  btnLayout->addStretch();
  btnLayout->addWidget(processBtn);
  btnLayout->addWidget(cancelBtn);

  mainLayout->addLayout(btnLayout);
}

void AddItemDialog::onProcessClicked() {
  std::vector<Item> processedItems;

  for (int i = 0; i < m_itemsTable->rowCount(); ++i) {
    QTableWidgetItem *checkItem = m_itemsTable->item(i, 0);
    if (checkItem && checkItem->checkState() == Qt::Checked) {
      Item item = m_items[i];
      item.state = ItemState::Unprocessed;

      QTableWidgetItem *deleteItem = m_itemsTable->item(i, 1);
      if (deleteItem && deleteItem->flags() & Qt::ItemIsUserCheckable) {
        if (deleteItem->checkState() == Qt::Checked) {
          QJsonObject meta = item.metadata;
          meta["delete_source_file"] = true;
          item.metadata = meta;
        }
      }

      processedItems.push_back(item);
    }
  }

  m_items = processedItems;
  accept();
}
