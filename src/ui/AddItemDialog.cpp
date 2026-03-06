#include "AddItemDialog.h"
#include "../core/Constants.h"
#include <QDateTime>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMenu>
#include <QMessageBox>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>

namespace {
QString getDisplayName(const QString &sourcePath) {
  if (sourcePath.startsWith("magnet:?")) {
    QUrl url(sourcePath);
    QUrlQuery query(url);
    if (query.hasQueryItem("dn")) {
      return QUrl::fromPercentEncoding(query.queryItemValue("dn").toUtf8());
    } else if (query.hasQueryItem("tr")) {
      return "Magnet Link (No Name)";
    } else {
      // maybe xt infohash?
      QString fullQuery = url.query();
      int xtIdx = fullQuery.indexOf("xt=");
      if (xtIdx != -1) {
        int endIdx = fullQuery.indexOf('&', xtIdx);
        if (endIdx == -1) endIdx = fullQuery.length();
        return fullQuery.mid(xtIdx + 3, endIdx - xtIdx - 3);
      }
    }
    return "Magnet Link";
  }

  QFileInfo fi(sourcePath);
  QString name = fi.fileName();
  if (!name.isEmpty()) {
    return QUrl::fromPercentEncoding(name.toUtf8());
  }
  return sourcePath;
}
} // namespace

AddItemDialog::AddItemDialog(const std::vector<Item> &items,
                             const QStringList &connectors, QWidget *parent)
    : QDialog(parent), m_items(items) {
  setupUi();
  setWindowTitle("Process Items");
  resize(850, 500);

  // Populate table
  m_itemsTable->setRowCount(m_items.size());
  for (size_t i = 0; i < m_items.size(); ++i) {
    QTableWidgetItem *checkItem = new QTableWidgetItem();
    checkItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
    checkItem->setCheckState(Qt::Checked);
    m_itemsTable->setItem(i, 0, checkItem);

    QString displayName = getDisplayName(m_items[i].sourcePath);
    QTableWidgetItem *nameItem = new QTableWidgetItem(displayName);
    nameItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    m_itemsTable->setItem(i, 1, nameItem);

    QTableWidgetItem *linkItem = new QTableWidgetItem(m_items[i].sourcePath);
    linkItem->setToolTip(m_items[i].sourcePath);
    linkItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    m_itemsTable->setItem(i, 2, linkItem);
  }

  // Dynamic connector handling
  m_connectorCombo->addItem(Constants::DefaultActionName);
  for (const QString &connector : connectors) {
    m_connectorCombo->addItem(connector);
  }

  // Common metadata? We just default to empty for now.
  // If there's only 1 item and it has metadata, load it.
  if (m_items.size() == 1) {
    if (!m_items[0].connectorId.isEmpty()) {
      int index = m_connectorCombo->findText(m_items[0].connectorId);
      if (index >= 0)
        m_connectorCombo->setCurrentIndex(index);
    }
    if (m_items[0].metadata.contains("labels")) {
      m_labelsEdit->setText(m_items[0].metadata["labels"].toString());
    }
    if (m_items[0].scheduledTime.isValid()) {
      m_scheduleEdit->setDateTime(m_items[0].scheduledTime);
      m_enableScheduleCheck->setChecked(true);
      m_scheduleEdit->setEnabled(true);
    }
  }
}

void AddItemDialog::setupUi() {
  QVBoxLayout *mainLayout = new QVBoxLayout(this);

  // Table
  m_itemsTable = new QTableWidget(this);
  m_itemsTable->setColumnCount(3);
  m_itemsTable->setHorizontalHeaderLabels({"Enable", "Name", "Link"});
  m_itemsTable->horizontalHeader()->setSectionResizeMode(
      0, QHeaderView::ResizeToContents);
  m_itemsTable->horizontalHeader()->setSectionResizeMode(
      1, QHeaderView::Interactive);
  m_itemsTable->horizontalHeader()->setSectionResizeMode(
      2, QHeaderView::Stretch);
  m_itemsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_itemsTable->setAlternatingRowColors(true);
  m_itemsTable->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_itemsTable, &QTableWidget::customContextMenuRequested, this,
          &AddItemDialog::onCustomContextMenuRequested);
  mainLayout->addWidget(m_itemsTable);

  QFormLayout *formLayout = new QFormLayout();

  m_connectorCombo = new QComboBox(this);
  formLayout->addRow("Action:", m_connectorCombo);

  m_labelsEdit = new QLineEdit(this);
  m_labelsEdit->setPlaceholderText("comma, separated, labels");
  formLayout->addRow("Labels:", m_labelsEdit);

  m_deleteOriginalCheck = new QCheckBox("Delete source list?", this);
  formLayout->addRow("", m_deleteOriginalCheck);

  QHBoxLayout *scheduleLayout = new QHBoxLayout();
  m_enableScheduleCheck = new QCheckBox(this);
  m_scheduleEdit =
      new QDateTimeEdit(QDateTime::currentDateTime().addSecs(3600), this);
  m_scheduleEdit->setDisplayFormat("yyyy-MM-dd HH:mm:ss");
  m_scheduleEdit->setCalendarPopup(true);
  m_scheduleEdit->setEnabled(false);

  connect(m_enableScheduleCheck, &QCheckBox::toggled, m_scheduleEdit,
          &QWidget::setEnabled);

  scheduleLayout->addWidget(m_enableScheduleCheck);
  scheduleLayout->addWidget(m_scheduleEdit);

  formLayout->addRow("Proceed to next action after:", scheduleLayout);

  mainLayout->addLayout(formLayout);

  // Action Buttons
  QHBoxLayout *btnLayout = new QHBoxLayout();

  QPushButton *processBtn = new QPushButton("Process", this);
  connect(processBtn, &QPushButton::clicked, this,
          &AddItemDialog::onProcessClicked);

  QPushButton *cancelBtn = new QPushButton("Cancel", this);
  connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

  btnLayout->addStretch();
  btnLayout->addWidget(processBtn);
  btnLayout->addWidget(cancelBtn);

  mainLayout->addLayout(btnLayout);
}

std::vector<Item> AddItemDialog::getItems() const { return m_items; }

bool AddItemDialog::shouldDeleteOriginal() const {
  return m_deleteOriginalCheck->isChecked();
}

void AddItemDialog::onProcessClicked() {
  QString action = m_connectorCombo->currentText();
  QString labels = m_labelsEdit->text();
  bool isScheduled = m_enableScheduleCheck->isChecked();
  QDateTime scheduledTime = m_scheduleEdit->dateTime();

  std::vector<Item> processedItems;

  for (int i = 0; i < m_itemsTable->rowCount(); ++i) {
    QTableWidgetItem *checkItem = m_itemsTable->item(i, 0);
    if (checkItem && checkItem->checkState() == Qt::Checked) {
      Item item = m_items[i];
      item.connectorId = action;

      QJsonObject meta = item.metadata;
      meta["labels"] = labels;
      item.metadata = meta;

      if (isScheduled) {
        item.state = ItemState::Scheduled;
        item.scheduledTime = scheduledTime;
      } else if (action == Constants::DefaultActionName) {
        item.state = ItemState::Held;
      } else {
        item.state = ItemState::Queued;
      }

      processedItems.push_back(item);
    }
  }

  m_items = processedItems;
  accept();
}

void AddItemDialog::onCustomContextMenuRequested(const QPoint &pos) {
  QTableWidgetItem *item = m_itemsTable->itemAt(pos);
  if (!item)
    return;

  int row = item->row();
  if (row < 0 || static_cast<size_t>(row) >= m_items.size())
    return;

  QMenu menu(this);
  QAction *infoAction = menu.addAction("Get Info");
  connect(infoAction, &QAction::triggered, this, [this, row]() {
    QMessageBox::information(
        this, "Item Information",
        QString("Source Path:\n%1").arg(m_items[row].sourcePath));
  });

  menu.exec(m_itemsTable->viewport()->mapToGlobal(pos));
}
