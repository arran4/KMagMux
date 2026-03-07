#include "ProcessItemDialog.h"
#include "../core/Constants.h"
#include <QDateTime>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>

ProcessItemDialog::ProcessItemDialog(const std::vector<Item> &items,
                                     const QStringList &connectors,
                                     QWidget *parent)
    : BaseItemDialog(items, parent), m_connectors(connectors) {
  setupUi();
  setWindowTitle("Process Items");
  resize(850, 500);
  populateTable();
}

void ProcessItemDialog::setupUi() {
  QVBoxLayout *mainLayout = new QVBoxLayout(this);

  // Table
  setupTable();
  mainLayout->addWidget(m_itemsTable);

  // Form layout for common properties
  QFormLayout *formLayout = new QFormLayout();

  m_stateCombo = new QComboBox(this);
  m_stateCombo->addItems({"Queue", "Hold", "Archive"});
  connect(m_stateCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &ProcessItemDialog::onStateChanged);
  formLayout->addRow("State:", m_stateCombo);

  m_holdTimeEdit = new QDateTimeEdit(QDateTime::currentDateTime().addSecs(3600), this);
  m_holdTimeEdit->setCalendarPopup(true);
  m_holdTimeEdit->setEnabled(false); // Initially disabled unless "Hold" is selected
  formLayout->addRow("Hold Until:", m_holdTimeEdit);

  m_connectorCombo = new QComboBox(this);
  m_connectorCombo->addItems(m_connectors);
  if (m_connectors.contains(Constants::DefaultActionName)) {
      m_connectorCombo->setCurrentText(Constants::DefaultActionName);
  }
  formLayout->addRow("Connector:", m_connectorCombo);

  mainLayout->addLayout(formLayout);

  // Action Buttons
  QHBoxLayout *btnLayout = new QHBoxLayout();

  QPushButton *processBtn = new QPushButton("Process", this);
  connect(processBtn, &QPushButton::clicked, this,
          &ProcessItemDialog::onProcessClicked);

  QPushButton *cancelBtn = new QPushButton("Cancel", this);
  connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

  btnLayout->addStretch();
  btnLayout->addWidget(processBtn);
  btnLayout->addWidget(cancelBtn);

  mainLayout->addLayout(btnLayout);
}

void ProcessItemDialog::onProcessClicked() {
  std::vector<Item> processedItems;

  ItemState selectedState = ItemState::Queued;
  QString stateStr = m_stateCombo->currentText();
  if (stateStr == "Queue") {
      selectedState = ItemState::Queued;
  } else if (stateStr == "Hold") {
      selectedState = ItemState::Held;
  } else if (stateStr == "Archive") {
      selectedState = ItemState::Archived;
  }

  for (int i = 0; i < m_itemsTable->rowCount(); ++i) {
    QTableWidgetItem *checkItem = m_itemsTable->item(i, 0);
    if (checkItem && checkItem->checkState() == Qt::Checked) {
      Item item = m_items[i];
      item.state = selectedState;
      item.connectorId = m_connectorCombo->currentText();
      if (selectedState == ItemState::Held) {
          item.scheduledTime = m_holdTimeEdit->dateTime();
      } else {
          item.scheduledTime = QDateTime(); // Clear scheduled time if not holding
      }

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

void ProcessItemDialog::onStateChanged(int index) {
  // Index 1 is "Hold"
  m_holdTimeEdit->setEnabled(index == 1);
}
