#include "ProcessItemDialog.h"
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

#include <QClipboard>
#include <QGuiApplication>
#include <QLabel>

ProcessItemDialog::ProcessItemDialog(const std::vector<Item> &items,
                                     const QStringList &connectors,
                                     QWidget *parent)
    : QDialog(parent), m_items(items), m_connectors(connectors) {
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

    bool isLocalFile = false;
    QString pathToCheck = m_items[i].sourcePath;
    if (pathToCheck.startsWith("file://")) {
      pathToCheck = QUrl(pathToCheck).toLocalFile();
      isLocalFile = QFileInfo(pathToCheck).exists();
    } else {
      isLocalFile = QFileInfo(pathToCheck).exists();
    }

    QTableWidgetItem *deleteItem = new QTableWidgetItem();
    deleteItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
    if (isLocalFile) {
      deleteItem->setCheckState(Qt::Unchecked);
    } else {
      deleteItem->setFlags(
          Qt::NoItemFlags); // Disable checkbox for non-local files
    }
    m_itemsTable->setItem(i, 1, deleteItem);

    QString displayName = m_items[i].getDisplayName();
    QTableWidgetItem *nameItem = new QTableWidgetItem(displayName);
    nameItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    m_itemsTable->setItem(i, 2, nameItem);

    QLabel *linkLabel =
        new QLabel(QString("<a href=\"%1\">%1</a>").arg(m_items[i].sourcePath));
    linkLabel->setOpenExternalLinks(
        false); // We want to show it, not necessarily open a browser directly
    linkLabel->setTextFormat(Qt::RichText);
    linkLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    m_itemsTable->setCellWidget(i, 3, linkLabel);

    // Keep the actual data in the item for easy retrieval
    QTableWidgetItem *linkItem = new QTableWidgetItem();
    linkItem->setData(Qt::UserRole, m_items[i].sourcePath);
    m_itemsTable->setItem(i, 3, linkItem);
  }

  // Check if any items are local files. If none are, hide the delete column.
  bool hasLocalFiles = false;
  for (int i = 0; i < m_itemsTable->rowCount(); ++i) {
    if (m_itemsTable->item(i, 1)->flags() & Qt::ItemIsUserCheckable) {
      hasLocalFiles = true;
      break;
    }
  }
  if (!hasLocalFiles) {
    m_itemsTable->hideColumn(1);
  }
}

void ProcessItemDialog::setupUi() {
  QVBoxLayout *mainLayout = new QVBoxLayout(this);

  // Table
  m_itemsTable = new QTableWidget(this);
  m_itemsTable->setColumnCount(4);
  m_itemsTable->setHorizontalHeaderLabels(
      {"Enable", "Delete file", "Name", "Link"});
  m_itemsTable->horizontalHeaderItem(1)->setToolTip("Delete file after import");
  m_itemsTable->horizontalHeader()->setSectionResizeMode(
      QHeaderView::ResizeToContents);
  m_itemsTable->horizontalHeader()->setStretchLastSection(true);
  m_itemsTable->setTextElideMode(Qt::ElideNone); // Do not truncate
  m_itemsTable->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
  m_itemsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_itemsTable->setAlternatingRowColors(true);
  m_itemsTable->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_itemsTable, &QTableWidget::customContextMenuRequested, this,
          &ProcessItemDialog::onCustomContextMenuRequested);
  mainLayout->addWidget(m_itemsTable);

  // Form layout for common properties
  QFormLayout *formLayout = new QFormLayout();

  m_stateCombo = new QComboBox(this);
  m_stateCombo->addItems({"Queue", "Hold", "Archive"});
  connect(m_stateCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &ProcessItemDialog::onStateChanged);
  formLayout->addRow("State:", m_stateCombo);

  m_holdTimeEdit =
      new QDateTimeEdit(QDateTime::currentDateTime().addSecs(3600), this);
  m_holdTimeEdit->setCalendarPopup(true);
  m_holdTimeEdit->setEnabled(
      false); // Initially disabled unless "Hold" is selected
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

std::vector<Item> ProcessItemDialog::getItems() const { return m_items; }

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

void ProcessItemDialog::onCustomContextMenuRequested(const QPoint &pos) {
  QTableWidgetItem *item = m_itemsTable->itemAt(pos);
  if (!item)
    return;

  int row = item->row();
  int col = item->column();
  if (row < 0 || static_cast<size_t>(row) >= m_items.size())
    return;

  QMenu menu(this);

  if (col == 1) {
    QAction *selectAllAction = menu.addAction("Select All");
    connect(selectAllAction, &QAction::triggered, this, [this]() {
      for (int i = 0; i < m_itemsTable->rowCount(); ++i) {
        QTableWidgetItem *deleteItem = m_itemsTable->item(i, 1);
        if (deleteItem && (deleteItem->flags() & Qt::ItemIsUserCheckable)) {
          deleteItem->setCheckState(Qt::Checked);
        }
      }
    });
    QAction *selectNoneAction = menu.addAction("Select None");
    connect(selectNoneAction, &QAction::triggered, this, [this]() {
      for (int i = 0; i < m_itemsTable->rowCount(); ++i) {
        QTableWidgetItem *deleteItem = m_itemsTable->item(i, 1);
        if (deleteItem && (deleteItem->flags() & Qt::ItemIsUserCheckable)) {
          deleteItem->setCheckState(Qt::Unchecked);
        }
      }
    });
    menu.addSeparator();
  }

  QAction *copyAction = menu.addAction("Copy cell");
  connect(copyAction, &QAction::triggered, this, [this, item, row, col]() {
    QString text;
    if (col == 3) {
      text = item->data(Qt::UserRole).toString();
      if (text.isEmpty()) {
          text = m_items[row].sourcePath;
      }
    } else {
      text = item->text();
    }
    QGuiApplication::clipboard()->setText(text);
  });

  QAction *infoAction = menu.addAction("Get Info");
  connect(infoAction, &QAction::triggered, this, [this, row]() {
    QMessageBox::information(
        this, "Item Information",
        QString("Source Path:\n%1").arg(m_items[row].sourcePath));
  });

  menu.exec(m_itemsTable->viewport()->mapToGlobal(pos));
}
