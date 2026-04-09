#include "ProcessItemDialog.h"
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
#include <cstddef>

#include <QClipboard>
#include <QGuiApplication>
#include <QLabel>
#include <QUuid>

ProcessItemDialog::ProcessItemDialog(const std::vector<Item> &items,
                                     const QStringList &connectors,
                                     QWidget *parent)
    : QDialog(parent), m_items(items), m_connectors(connectors) {
  setupUi();
  setWindowTitle("Process Items");
  resize(850, 500);

  // Populate table
  m_itemsTable->setRowCount(m_items.size());

  QHash<QString, bool> localFileCache;

  bool hasLocalFiles = false;
  for (size_t i = 0; i < m_items.size(); ++i) {
    QTableWidgetItem *checkItem = new QTableWidgetItem();
    checkItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
    checkItem->setCheckState(Qt::Checked);
    m_itemsTable->setItem(i, 0, checkItem);

    bool isLocalFile = false;
    const QString &pathToCheck = m_items[i].sourcePath;

    if (pathToCheck.startsWith("magnet:", Qt::CaseInsensitive) ||
        pathToCheck.startsWith("http://", Qt::CaseInsensitive) ||
        pathToCheck.startsWith("https://", Qt::CaseInsensitive)) {
      isLocalFile = false;
    } else {
      QString actualPath = pathToCheck;
      if (pathToCheck.startsWith("file://", Qt::CaseInsensitive)) {
        actualPath = QUrl(pathToCheck).toLocalFile();
      }

      auto it = localFileCache.find(actualPath);
      if (it != localFileCache.end()) {
        isLocalFile = it.value();
      } else {
        isLocalFile = QFileInfo(actualPath).exists();
        localFileCache.insert(actualPath, isLocalFile);
      }
    }

    QTableWidgetItem *deleteItem = new QTableWidgetItem();
    deleteItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
    if (isLocalFile) {
      hasLocalFiles = true;
      deleteItem->setCheckState(Qt::Unchecked);
    } else {
      deleteItem->setFlags(
          Qt::NoItemFlags); // Disable checkbox for non-local files
    }
    m_itemsTable->setItem(i, 1, deleteItem);

    QTableWidgetItem *deleteWhenDoneItem = new QTableWidgetItem();
    deleteWhenDoneItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
    deleteWhenDoneItem->setCheckState(Qt::Unchecked);
    m_itemsTable->setItem(i, 2, deleteWhenDoneItem);

    QString displayName = m_items[i].getDisplayName();
    QTableWidgetItem *nameItem = new QTableWidgetItem(displayName);
    nameItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    m_itemsTable->setItem(i, 3, nameItem);

    QLabel *linkLabel =
        new QLabel(QString("<a href=\"%1\">%1</a>").arg(m_items[i].sourcePath));
    linkLabel->setOpenExternalLinks(
        false); // We want to show it, not necessarily open a browser directly
    linkLabel->setTextFormat(Qt::RichText);
    linkLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    m_itemsTable->setCellWidget(i, 4, linkLabel);

    // Keep the actual data in the item for easy retrieval
    QTableWidgetItem *linkItem = new QTableWidgetItem();
    linkItem->setData(Qt::UserRole, m_items[i].sourcePath);
    m_itemsTable->setItem(i, 4, linkItem);
  }

  // Check if any items are local files. If none are, hide the delete column.
  if (!hasLocalFiles) {
    m_itemsTable->hideColumn(1);
  }
}

void ProcessItemDialog::setupUi() {
  QVBoxLayout *mainLayout = new QVBoxLayout(this);

  // Table
  m_itemsTable = new QTableWidget(this);
  m_itemsTable->setColumnCount(5);
  m_itemsTable->setHorizontalHeaderLabels(
      {"Enable", "Delete file", "Delete when done", "Name", "Link"});
  m_itemsTable->horizontalHeaderItem(1)->setToolTip("Delete file after import");
  m_itemsTable->horizontalHeaderItem(2)->setToolTip(
      "Delete item automatically after successful dispatch");
  m_itemsTable->setItemDelegate(new MaxWidthDelegate(m_itemsTable));
  m_itemsTable->horizontalHeader()->setSectionResizeMode(
      QHeaderView::ResizeToContents);
  m_itemsTable->horizontalHeader()->setStretchLastSection(true);
  m_itemsTable->setTextElideMode(Qt::ElideRight);
  m_itemsTable->setWordWrap(false);
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

  m_connectorList = new QListWidget(this);
  for (const QString &connector : m_connectors) {
    QListWidgetItem *item = new QListWidgetItem(connector, m_connectorList);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    if (connector == Constants::DefaultActionName) {
      item->setCheckState(Qt::Checked);
    } else {
      item->setCheckState(Qt::Unchecked);
    }
  }

  // If Default is not in the list or nothing is checked, check the first one if
  // available
  bool anythingChecked = false;
  for (int i = 0; i < m_connectorList->count(); ++i) {
    if (m_connectorList->item(i)->checkState() == Qt::Checked) {
      anythingChecked = true;
      break;
    }
  }
  if (!anythingChecked && m_connectorList->count() > 0) {
    m_connectorList->item(0)->setCheckState(Qt::Checked);
  }

  formLayout->addRow("Connectors:", m_connectorList);

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

const std::vector<Item> &ProcessItemDialog::getItems() const { return m_items; }

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

  QStringList selectedConnectors;
  for (int i = 0; i < m_connectorList->count(); ++i) {
    if (m_connectorList->item(i)->checkState() == Qt::Checked) {
      selectedConnectors.append(m_connectorList->item(i)->text());
    }
  }

  if (selectedConnectors.isEmpty()) {
    QMessageBox::warning(this, "No Connector Selected",
                         "Please select at least one destination connector.");
    return;
  }

  for (int i = 0; i < m_itemsTable->rowCount(); ++i) {
    QTableWidgetItem *const checkItem = m_itemsTable->item(i, 0);
    if (checkItem != nullptr && checkItem->checkState() == Qt::Checked) {
      Item originalItem = m_items[i];

      for (const QString &connectorId : selectedConnectors) {
        Item item = originalItem; // Duplicate the item for each connector
        // For multiple connectors we need to generate unique IDs if it's more
        // than one If it's more than one connector, we should really assign a
        // new ID to the duplicates but since they all share the same source
        // file, they might conflict if deleted source file is requested.
        // Actually, let's keep the original item ID for the first one, and
        // generate a new ID for the others
        if (connectorId != selectedConnectors.first()) {
          item.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        }

        QString oldState = item.stateToString();
        item.state = selectedState;
        item.addHistory(
            QString("Processed and state set to %1").arg(item.stateToString()));
        item.connectorId = connectorId;
        if (selectedState == ItemState::Held) {
          item.scheduledTime = m_holdTimeEdit->dateTime();
        } else {
          item.scheduledTime =
              QDateTime(); // Clear scheduled time if not holding
        }

        QTableWidgetItem *const deleteItem = m_itemsTable->item(i, 1);
        if (deleteItem != nullptr &&
            (deleteItem->flags() & Qt::ItemIsUserCheckable) != 0u) {
          if (deleteItem->checkState() == Qt::Checked) {
            QJsonObject meta = item.metadata;
            meta["delete_source_file"] = true;
            item.metadata = meta;
          }
        }

        QTableWidgetItem *deleteWhenDoneItem = m_itemsTable->item(i, 2);
        if (deleteWhenDoneItem &&
            deleteWhenDoneItem->flags() & Qt::ItemIsUserCheckable) {
          if (deleteWhenDoneItem->checkState() == Qt::Checked) {
            QJsonObject meta = item.metadata;
            meta["delete_once_submitted"] = true;
            item.metadata = meta;
          }
        }

        processedItems.push_back(item);
      }
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
  QTableWidgetItem *const item = m_itemsTable->itemAt(pos);
  if (item == nullptr) {
    return;
  }

  const int row = item->row();
  const int col = item->column();
  if (row < 0 || static_cast<size_t>(row) >= m_items.size()) {
    return;
  }

  QMenu menu(this);

  if (col == 1) {
    QAction *const selectAllAction = menu.addAction("Select All");
    connect(selectAllAction, &QAction::triggered, this, [this]() {
      for (int i = 0; i < m_itemsTable->rowCount(); ++i) {
        QTableWidgetItem *const deleteItem = m_itemsTable->item(i, 1);
        if (deleteItem && (deleteItem->flags() & Qt::ItemIsUserCheckable)) {
          deleteItem->setCheckState(Qt::Checked);
        }
      }
    });
    QAction *const selectNoneAction = menu.addAction("Select None");
    connect(selectNoneAction, &QAction::triggered, this, [this]() {
      for (int i = 0; i < m_itemsTable->rowCount(); ++i) {
        QTableWidgetItem *const deleteItem = m_itemsTable->item(i, 1);
        if (deleteItem && (deleteItem->flags() & Qt::ItemIsUserCheckable)) {
          deleteItem->setCheckState(Qt::Unchecked);
        }
      }
    });
    menu.addSeparator();
  }

  QAction *const copyAction = menu.addAction("Copy cell");
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

  QAction *const infoAction = menu.addAction("Get Info");
  connect(infoAction, &QAction::triggered, this, [this, row]() {
    const QString sourcePath = m_items[row].sourcePath;
    if (sourcePath.startsWith("magnet:") || sourcePath.endsWith(".torrent")) {
      TorrentInfoDialog dialog(sourcePath, &m_items[row], this);
      dialog.exec();
    } else {
      QMessageBox::information(this, "Item Information",
                               QString("Source Path:\n%1").arg(sourcePath));
    }
  });

  menu.exec(m_itemsTable->viewport()->mapToGlobal(pos));
}
