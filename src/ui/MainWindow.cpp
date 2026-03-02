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
#include <algorithm>

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
  QAction *addItemsAction =
      fileMenu->addAction(QIcon::fromTheme("document-open"),
                          tr("&Add Item(s)..."), this, &MainWindow::onAddItems);
  addItemsAction->setShortcut(QKeySequence("Ctrl+O"));

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
  mainToolBar->addAction(addItemsAction);
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

void MainWindow::onAddItems() {
  QDialog dialog(this);
  dialog.setWindowTitle(tr("Add Items"));
  dialog.resize(500, 400);

  QVBoxLayout *layout = new QVBoxLayout(&dialog);

  QLabel *label = new QLabel(tr("Paste links, paths, or use 'Browse Files...' to add items:"), &dialog);
  layout->addWidget(label);

  QPlainTextEdit *textEdit = new QPlainTextEdit(&dialog);
  textEdit->setPlaceholderText(tr("file://...\n/path/to/file\nhttp://...\nhttps://...\nmagnet:..."));
  layout->addWidget(textEdit);

  QHBoxLayout *buttonLayout = new QHBoxLayout();
  QPushButton *browseBtn = new QPushButton(tr("Browse Files..."), &dialog);
  buttonLayout->addWidget(browseBtn);
  buttonLayout->addStretch();

  QDialogButtonBox *buttonBox = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
  buttonLayout->addWidget(buttonBox);

  layout->addLayout(buttonLayout);

  connect(browseBtn, &QPushButton::clicked, this, [textEdit, this]() {
    QStringList files = QFileDialog::getOpenFileNames(this, tr("Select Files"));
    if (!files.isEmpty()) {
      QString currentText = textEdit->toPlainText();
      if (!currentText.isEmpty() && !currentText.endsWith('\n')) {
        currentText += '\n';
      }
      for (const QString &file : files) {
        currentText += "file://" + file + "\n";
      }
      textEdit->setPlainText(currentText);
    }
  });

  connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

  if (dialog.exec() == QDialog::Accepted) {
    QStringList lines = textEdit->toPlainText().split('\n', Qt::SkipEmptyParts);
    std::vector<Item> items;
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    int idx = 0;

    auto processLine = [&](QString line) {
      line = line.trimmed();
      if (line.isEmpty())
        return;

      QString pathToCheck = line;
      if (line.startsWith("file://")) {
        pathToCheck = line.mid(7);
      }

      // Check if it's a local file that might contain links (.txt or .html)
      QFileInfo fileInfo(pathToCheck);
      if (fileInfo.exists() && fileInfo.isFile()) {
        QString ext = fileInfo.suffix().toLower();
        if (ext == "txt" || ext == "html" || ext == "htm") {
          QFile file(pathToCheck);
          if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            while (!in.atEnd()) {
              QString fileLine = in.readLine().trimmed();
              if (fileLine.isEmpty()) continue;

              // In a real HTML parser we'd extract hrefs, but here we
              // just naively assume it might contain lines that are links
              // or we just add it and let the processor figure it out.
              // To match "mixed mode", we just add each non-empty line as an Item.
              // Wait, the prompt says "text file of direct links to torrent files,
              // and multiple magnent or torrent files html file with links, but otherwise like text files a torrent file"
              // So we can extract href="..." from html, or just treat each line.
              // Actually, a simple regex or just adding the line:
              if (ext == "html" || ext == "htm") {
                // simple naive regex to extract href
                QRegularExpression re("href=[\"']([^\"']+)[\"']");
                QRegularExpressionMatchIterator i = re.globalMatch(fileLine);
                while (i.hasNext()) {
                  QRegularExpressionMatch match = i.next();
                  QString link = match.captured(1).trimmed();
                  if (!link.isEmpty()) {
                    Item newItem;
                    newItem.id = QString::number(now) + "_" + QString::number(idx++) + "_item";
                    newItem.state = ItemState::Unprocessed;
                    newItem.sourcePath = link;
                    newItem.createdTime = QDateTime::currentDateTime();
                    items.push_back(newItem);
                  }
                }
              } else {
                // txt file
                Item newItem;
                newItem.id = QString::number(now) + "_" + QString::number(idx++) + "_item";
                newItem.state = ItemState::Unprocessed;
                newItem.sourcePath = fileLine;
                newItem.createdTime = QDateTime::currentDateTime();
                items.push_back(newItem);
              }
            }
          }
          return; // Skip adding the .txt/.html file itself as an item
        }
      }

      // Not a text/html file (or doesn't exist locally), treat as regular item
      Item newItem;
      newItem.id = QString::number(now) + "_" + QString::number(idx++) + "_item";
      newItem.state = ItemState::Unprocessed;
      newItem.sourcePath = line;
      newItem.createdTime = QDateTime::currentDateTime();
      items.push_back(newItem);
    };

    for (const QString &line : lines) {
      processLine(line);
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
  selectedItems.reserve(selection.size());
  std::transform(selection.begin(), selection.end(),
                 std::back_inserter(selectedItems),
                 [model](const QModelIndex &index) {
                   return model->getItem(index.row());
                 });

  openProcessDialog(selectedItems);
}

void MainWindow::openProcessDialog(const std::vector<Item> &items) {
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
