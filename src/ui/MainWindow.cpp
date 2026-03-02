#include "MainWindow.h"
#include "AddItemDialog.h"
#include "PreferencesDialog.h"
#include <QApplication>
#include <QDateTime>
#include <QDebug>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QHeaderView>
#include <QInputDialog>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

MainWindow::MainWindow(StorageManager *storage, QWidget *parent)
    : QMainWindow(parent), m_storage(storage) {
  setupUi();
  loadData();

  m_engine = new Engine(m_storage, this);
  m_engine->start();

  connect(m_storage, &StorageManager::itemAdded, this,
          &MainWindow::onItemAdded);
  connect(m_storage, &StorageManager::itemUpdated, this,
          &MainWindow::onItemUpdated);
}

MainWindow::~MainWindow() {}

void MainWindow::setupUi() {
  setWindowTitle("KMagMux");
  resize(1000, 600);

  // Setup Menu Bar
  QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
  QAction *addFilesAction =
      fileMenu->addAction(QIcon::fromTheme("document-open"),
                          tr("&Add File(s)..."), this, &MainWindow::onAddFiles);
  addFilesAction->setShortcut(QKeySequence("Ctrl+O"));

  QAction *addLinksAction =
      fileMenu->addAction(QIcon::fromTheme("insert-link"),
                          tr("&Add Link(s)..."), this, &MainWindow::onAddLinks);
  addLinksAction->setShortcut(QKeySequence("Ctrl+L"));

  fileMenu->addSeparator();
  QAction *quitAction =
      fileMenu->addAction(QIcon::fromTheme("application-exit"), tr("&Quit"),
                          qApp, &QApplication::quit);
  quitAction->setShortcut(QKeySequence("Ctrl+Q"));

  QMenu *editMenu = menuBar()->addMenu(tr("&Edit"));
  QAction *prefAction =
      editMenu->addAction(QIcon::fromTheme("preferences-system"),
                          tr("&Preferences"), this, &MainWindow::onPreferences);
  prefAction->setShortcut(QKeySequence("Ctrl+,"));

  QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
  helpMenu->addAction(QIcon::fromTheme("help-about"), tr("&About KMagMux"),
                      this, &MainWindow::onAbout);

  // Setup Tool Bar
  QToolBar *mainToolBar = addToolBar(tr("Main Toolbar"));
  mainToolBar->addAction(addFilesAction);
  mainToolBar->addAction(addLinksAction);
  mainToolBar->addSeparator();
  mainToolBar->addAction(quitAction);

  // Setup Status Bar
  statusBar()->showMessage(tr("Ready"));

  QWidget *centralWidget = new QWidget(this);
  setCentralWidget(centralWidget);

  QVBoxLayout *layout = new QVBoxLayout(centralWidget);

  m_tabWidget = new QTabWidget(this);
  layout->addWidget(m_tabWidget);

  auto setupView = [this](QTableView *view, ItemModel *model,
                          const QString &title) {
    view->setModel(model);
    view->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    view->setSelectionBehavior(QAbstractItemView::SelectRows);
    view->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(view, &QTableView::customContextMenuRequested, this,
            &MainWindow::onCustomContextMenuRequested);
    m_tabWidget->addTab(view, title);
  };

  // Unprocessed Tab
  m_unprocessedView = new QTableView(this);
  m_unprocessedModel = new ItemModel(this);
  setupView(m_unprocessedView, m_unprocessedModel, "Unprocessed");
  connect(m_unprocessedView, &QTableView::doubleClicked, this,
          &MainWindow::onProcessItem);

  // Queue Tab
  m_queueView = new QTableView(this);
  m_queueModel = new ItemModel(this);
  setupView(m_queueView, m_queueModel, "Queue");

  // Archive Tab
  m_archiveView = new QTableView(this);
  m_archiveModel = new ItemModel(this);
  setupView(m_archiveView, m_archiveModel, "Archive");
}

void MainWindow::loadData() {
  auto items = m_storage->loadAllItems();

  std::vector<Item> unprocessedItems;
  std::vector<Item> queueItems;
  std::vector<Item> archiveItems;

  for (const auto &item : items) {
    switch (item.state) {
    case ItemState::Unprocessed:
      unprocessedItems.push_back(item);
      break;
    case ItemState::Queued:
    case ItemState::Scheduled:
    case ItemState::Held:
      queueItems.push_back(item);
      break;
    case ItemState::Archived:
    case ItemState::Dispatched:
    case ItemState::Failed:
      archiveItems.push_back(item);
      break;
    default:
      break;
    }
  }

  m_unprocessedModel->setItems(unprocessedItems);
  m_queueModel->setItems(queueItems);
  m_archiveModel->setItems(archiveItems);
}

QTableView *MainWindow::getCurrentView() const {
  return qobject_cast<QTableView *>(m_tabWidget->currentWidget());
}

ItemModel *MainWindow::getCurrentModel() const {
  QTableView *view = getCurrentView();
  if (view) {
    return qobject_cast<ItemModel *>(view->model());
  }
  return nullptr;
}

void MainWindow::onCustomContextMenuRequested(const QPoint &pos) {
  QTableView *view = getCurrentView();
  if (!view)
    return;

  QModelIndex index = view->indexAt(pos);
  if (!index.isValid())
    return;

  QMenu menu(this);

  // If we are in the Unprocessed view, offer a "Process..." action
  if (view == m_unprocessedView) {
    QAction *processAction = menu.addAction("Process...");
    connect(processAction, &QAction::triggered, this,
            &MainWindow::onProcessItem);
    menu.addSeparator();
  }

  QAction *queueAction = menu.addAction("Queue");
  QAction *holdAction = menu.addAction("Hold");
  QAction *archiveAction = menu.addAction("Archive");

  connect(queueAction, &QAction::triggered, this,
          [this]() { onItemAction(ItemState::Queued); });
  connect(holdAction, &QAction::triggered, this,
          [this]() { onItemAction(ItemState::Held); });
  connect(archiveAction, &QAction::triggered, this,
          [this]() { onItemAction(ItemState::Archived); });

  menu.exec(view->viewport()->mapToGlobal(pos));
}

void MainWindow::onItemAction(ItemState newState) {
  QTableView *view = getCurrentView();
  if (!view)
    return;

  const ItemModel *model = getCurrentModel();
  if (!model)
    return;

  QModelIndexList selection = view->selectionModel()->selectedRows();
  if (selection.isEmpty())
    return;

  int row = selection.first().row();
  Item item = model->getItem(row);

  qDebug() << "Changing item state for:" << item.id << "to" << (int)newState;

  item.state = newState;
  if (m_storage->saveItem(item)) {
    // Model refresh happens via itemUpdated signal
  } else {
    qWarning() << "Failed to save item state change";
  }
}

void MainWindow::onItemAdded(const Item &item) { loadData(); }

void MainWindow::onItemUpdated(const Item &item) { loadData(); }

void MainWindow::onAddFiles() {
  QStringList files = QFileDialog::getOpenFileNames(this, tr("Select Files"));
  if (files.isEmpty())
    return;

  std::vector<Item> items;
  qint64 now = QDateTime::currentMSecsSinceEpoch();
  int idx = 0;
  for (const QString &file : files) {
    Item newItem;
    newItem.id = QString::number(now) + "_" + QString::number(idx++) + "_file";
    newItem.state = ItemState::Unprocessed;
    newItem.sourcePath = file;
    newItem.createdTime = QDateTime::currentDateTime();
    items.push_back(newItem);
  }

  openProcessDialog(items);
}

void MainWindow::onAddLinks() {
  QDialog dialog(this);
  dialog.setWindowTitle(tr("Add Links"));
  dialog.resize(400, 300);

  QVBoxLayout *layout = new QVBoxLayout(&dialog);
  QPlainTextEdit *textEdit = new QPlainTextEdit(&dialog);
  textEdit->setPlaceholderText(tr("Paste links here (one per line)..."));
  layout->addWidget(textEdit);

  QDialogButtonBox *buttonBox = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
  layout->addWidget(buttonBox);

  connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

  if (dialog.exec() == QDialog::Accepted) {
    QStringList lines = textEdit->toPlainText().split('\n', Qt::SkipEmptyParts);
    std::vector<Item> items;
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    int idx = 0;
    for (QString line : lines) {
      line = line.trimmed();
      if (line.isEmpty())
        continue;

      Item newItem;
      newItem.id =
          QString::number(now) + "_" + QString::number(idx++) + "_link";
      newItem.state = ItemState::Unprocessed;
      newItem.sourcePath = line;
      newItem.createdTime = QDateTime::currentDateTime();
      items.push_back(newItem);
    }

    if (!items.empty()) {
      openProcessDialog(items);
    }
  }
}

void MainWindow::onProcessItem() {
  QTableView *view = getCurrentView();
  if (!view)
    return;

  const ItemModel *model = getCurrentModel();
  if (!model)
    return;

  QModelIndexList selection = view->selectionModel()->selectedRows();
  if (selection.isEmpty())
    return;

  std::vector<Item> selectedItems;
  for (const QModelIndex &index : selection) {
    selectedItems.push_back(model->getItem(index.row()));
  }

  openProcessDialog(selectedItems);
}

void MainWindow::openProcessDialog(std::vector<Item> &items) {
  if (items.empty())
    return;

  AddItemDialog dialog(items, m_engine->getAvailableConnectors(), this);
  if (dialog.exec() == QDialog::Accepted) {
    std::vector<Item> updatedItems = dialog.getItems();

    for (Item &updatedItem : updatedItems) {
      bool success = true;

      if (dialog.shouldDeleteOriginal()) {
        // Move logic
        if (!m_storage->moveToManaged(updatedItem, true)) {
          QMessageBox::warning(
              this, "Error",
              QString("Failed to move original file to managed storage: %1")
                  .arg(updatedItem.sourcePath));
          success = false; // Should we abort saving? Maybe just warn.
        }
      } else {
        // Just save. If it's a new item, we should save it anyway.
      }

      if (success) {
        if (m_storage->saveItem(updatedItem)) {
          qDebug() << "Item processed and saved:" << updatedItem.id;
        } else {
          QMessageBox::warning(this, "Error",
                               QString("Failed to save item metadata: %1")
                                   .arg(updatedItem.sourcePath));
        }
      }
    }
  }
}

void MainWindow::onPreferences() {
  PreferencesDialog dialog(this);
  dialog.exec();
}

void MainWindow::onAbout() {
  QMessageBox::about(
      this, tr("About KMagMux"),
      tr("KMagMux\nTorrent and Magnet file handler and router."));
}
