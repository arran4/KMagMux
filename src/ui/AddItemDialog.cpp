#include "AddItemDialog.h"
#include "../core/Constants.h"
#include "MaxWidthDelegate.h"
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

#include "TorrentInfoDialog.h"
#include <QClipboard>
#include <QGuiApplication>
#include <QLabel>

AddItemDialog::AddItemDialog(const std::vector<Item> &items,
                             const QStringList &connectors, QWidget *parent)
    : QDialog(parent), m_items(items) {
  setupUi();
  setWindowTitle("Add Items");
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

void AddItemDialog::setupUi() {
  QVBoxLayout *mainLayout = new QVBoxLayout(this);

  // Table
  m_itemsTable = new QTableWidget(this);
  m_itemsTable->setColumnCount(4);
  m_itemsTable->setHorizontalHeaderLabels(
      {"Enable", "Delete file", "Name", "Link"});
  m_itemsTable->horizontalHeaderItem(1)->setToolTip("Delete file after import");
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
          &AddItemDialog::onCustomContextMenuRequested);
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

const std::vector<Item>& AddItemDialog::getItems() const { return m_items; }

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

void AddItemDialog::onCustomContextMenuRequested(const QPoint &pos) {
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
    QString sourcePath = m_items[row].sourcePath;
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
