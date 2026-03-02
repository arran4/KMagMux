#include "AddItemDialog.h"
#include <QDateTime>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>

AddItemDialog::AddItemDialog(Item &item, const QStringList &connectors,
                             QWidget *parent)
    : QDialog(parent), m_item(item) {
  setupUi();
  setWindowTitle("Process Item");
  resize(500, 300);

  // Populate fields
  m_sourceEdit->setText(m_item.sourcePath);
  m_destEdit->setText(m_item.destinationPath);

  // Dynamic connector handling
  m_connectorCombo->addItem("Default");
  for (const QString &connector : connectors) {
    m_connectorCombo->addItem(connector);
  }

  if (!m_item.connectorId.isEmpty()) {
    int index = m_connectorCombo->findText(m_item.connectorId);
    if (index >= 0)
      m_connectorCombo->setCurrentIndex(index);
  }

  if (m_item.metadata.contains("labels")) {
    m_labelsEdit->setText(m_item.metadata["labels"].toString());
  }

  // Schedule default time (now + 1 hour)
  if (m_item.scheduledTime.isValid()) {
    m_scheduleEdit->setDateTime(m_item.scheduledTime);
  } else {
    m_scheduleEdit->setDateTime(QDateTime::currentDateTime().addSecs(3600));
  }
}

void AddItemDialog::setupUi() {
  QVBoxLayout *mainLayout = new QVBoxLayout(this);

  QFormLayout *formLayout = new QFormLayout();

  m_sourceEdit = new QLineEdit(this);
  m_sourceEdit->setReadOnly(true);
  formLayout->addRow("Source:", m_sourceEdit);

  QHBoxLayout *destLayout = new QHBoxLayout();
  m_destEdit = new QLineEdit(this);
  QPushButton *browseBtn = new QPushButton("...", this);
  connect(browseBtn, &QPushButton::clicked, this, [this]() {
    QString dir = QFileDialog::getExistingDirectory(this, "Select Destination");
    if (!dir.isEmpty())
      m_destEdit->setText(dir);
  });
  destLayout->addWidget(m_destEdit);
  destLayout->addWidget(browseBtn);
  formLayout->addRow("Destination:", destLayout);

  m_connectorCombo = new QComboBox(this);
  formLayout->addRow("Connector:", m_connectorCombo);

  m_labelsEdit = new QLineEdit(this);
  m_labelsEdit->setPlaceholderText("comma, separated, labels");
  formLayout->addRow("Labels:", m_labelsEdit);

  m_deleteOriginalCheck =
      new QCheckBox("Delete original file after action", this);
  formLayout->addRow("", m_deleteOriginalCheck);

  m_scheduleEdit = new QDateTimeEdit(this);
  m_scheduleEdit->setDisplayFormat("yyyy-MM-dd HH:mm:ss");
  m_scheduleEdit->setCalendarPopup(true);
  formLayout->addRow("Schedule Time:", m_scheduleEdit);

  mainLayout->addLayout(formLayout);

  // Action Buttons
  QHBoxLayout *btnLayout = new QHBoxLayout();

  QPushButton *dispatchBtn = new QPushButton("Dispatch Now", this);
  connect(dispatchBtn, &QPushButton::clicked, this,
          &AddItemDialog::onDispatchClicked);

  QPushButton *queueBtn = new QPushButton("Queue", this);
  connect(queueBtn, &QPushButton::clicked, this,
          &AddItemDialog::onQueueClicked);

  QPushButton *scheduleBtn = new QPushButton("Schedule", this);
  connect(scheduleBtn, &QPushButton::clicked, this,
          &AddItemDialog::onScheduleClicked);

  QPushButton *holdBtn = new QPushButton("Save for Later", this);
  connect(holdBtn, &QPushButton::clicked, this, &AddItemDialog::onHoldClicked);

  QPushButton *cancelBtn = new QPushButton("Cancel", this);
  connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

  btnLayout->addWidget(dispatchBtn);
  btnLayout->addWidget(queueBtn);
  btnLayout->addWidget(scheduleBtn);
  btnLayout->addWidget(holdBtn);
  btnLayout->addStretch();
  btnLayout->addWidget(cancelBtn);

  mainLayout->addLayout(btnLayout);
}

Item AddItemDialog::getItem() const {
  Item item = m_item;
  item.destinationPath = m_destEdit->text();
  item.connectorId = m_connectorCombo->currentText();

  QJsonObject meta = item.metadata;
  meta["labels"] = m_labelsEdit->text();
  item.metadata = meta;

  return item;
}

bool AddItemDialog::shouldDeleteOriginal() const {
  return m_deleteOriginalCheck->isChecked();
}

void AddItemDialog::onDispatchClicked() {
  m_item = getItem();
  m_item.state = ItemState::Queued;
  accept();
}

void AddItemDialog::onQueueClicked() {
  m_item = getItem();
  m_item.state = ItemState::Queued;
  accept();
}

void AddItemDialog::onScheduleClicked() {
  m_item = getItem();
  m_item.state = ItemState::Scheduled;
  m_item.scheduledTime = m_scheduleEdit->dateTime();
  accept();
}

void AddItemDialog::onHoldClicked() {
  m_item = getItem();
  m_item.state = ItemState::Held;
  accept();
}
