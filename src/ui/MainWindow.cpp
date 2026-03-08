#include "MainWindow.h"
#include "../core/ItemParser.h"
#include "AddItemDialog.h"
#include "LinkExtractorDialog.h"
#include "MaxWidthDelegate.h"
#include "PreferencesDialog.h"
#include "ProcessItemDialog.h"
#include <QApplication>
#include <QCloseEvent>
#include <QDateTime>
#include <QDebug>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFutureWatcher>
#include <QHeaderView>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QSystemTrayIcon>
#include <QTextStream>
#include <QUrl>
#include <QVBoxLayout>
#include <QtConcurrent>
#include <algorithm>

MainWindow::MainWindow(StorageManager *storage, QWidget *parent)
    : QMainWindow(parent), m_storage(storage), m_closeToTray(false),
      m_minimizeToTray(false), m_autoStart(false) {
  qApp->setQuitOnLastWindowClosed(false);

  applySettings();

  setupUi();
  loadData();

  m_engine = new Engine(m_storage, this);
  m_engine->start();

  connect(m_storage, &StorageManager::itemAdded, this,
          &MainWindow::onItemAdded);
  connect(m_storage, &StorageManager::itemUpdated, this,
          &MainWindow::onItemUpdated);
  connect(m_storage, &StorageManager::itemDeleted, this,
          &MainWindow::onItemDeleted);
}

MainWindow::~MainWindow() {}

void MainWindow::closeEvent(QCloseEvent *event) {
  if (m_closeToTray && m_trayIcon->isVisible()) {
    hide();
    event->ignore();
  } else {
    event->accept();
    qApp->quit();
  }
}

void MainWindow::quitApplication() {
  m_forceQuit = true;
  close();
  qApp->quit();
}

void MainWindow::changeEvent(QEvent *event) {
  QMainWindow::changeEvent(event);
  if (event->type() == QEvent::WindowStateChange) {
    if (isMinimized() && m_minimizeToTray && m_trayIcon->isVisible()) {
      hide();
    }
  }
}

void MainWindow::setupUi() {
  setWindowTitle("KMagMux");
  resize(1000, 600);

  // Setup Menu Bar
  QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
  QAction *addItemsAction = fileMenu->addAction(
      QIcon::fromTheme("document-open"), tr("&Find Item(s)..."), this,
      &MainWindow::onAddItems);
  addItemsAction->setShortcut(QKeySequence("Ctrl+O"));

  fileMenu->addSeparator();
  m_minimizeAction =
      fileMenu->addAction(QIcon::fromTheme("go-down"), tr("Minimize to Tray"),
                          this, &MainWindow::minimizeToTray);

  QAction *quitAction =
      fileMenu->addAction(QIcon::fromTheme("application-exit"), tr("&Quit"),
                          this, &MainWindow::quitApplication);
  quitAction->setShortcut(QKeySequence("Ctrl+Q"));

  QMenu *editMenu = menuBar()->addMenu(tr("&Edit"));

  // Since processing is ON by default (checked=true), the action should display
  // the Pause icon to indicate what happens when you click it, or you can just
  // use Pause to represent "Stop processing". Let's initialize it with Pause
  // since the default state is "Playing".
  m_toggleProcessingAction = new QAction(
      QIcon::fromTheme("media-playback-pause"), tr("&Play / Pause"), this);
  m_toggleProcessingAction->setCheckable(true);
  m_toggleProcessingAction->setChecked(true);
  m_toggleProcessingAction->setToolTip(
      tr("Play / Pause processing items in the list"));
  m_toggleProcessingAction->setStatusTip(
      tr("Play / Pause processing items in the list"));
  connect(m_toggleProcessingAction, &QAction::toggled, this,
          &MainWindow::onToggleProcessing);
  editMenu->addAction(m_toggleProcessingAction);
  editMenu->addSeparator();

  QAction *prefAction =
      editMenu->addAction(QIcon::fromTheme("preferences-system"),
                          tr("&Preferences"), this, &MainWindow::onPreferences);
  prefAction->setShortcut(QKeySequence("Ctrl+,"));

  QMenu *actionsMenu = menuBar()->addMenu(tr("A&ctions"));
  QMenu *debugMenu = actionsMenu->addMenu(tr("&Debug"));
  debugMenu->addAction(tr("Open &Cache directory"), this,
                       &MainWindow::onOpenCacheDirectory);

  QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
  helpMenu->addAction(QIcon::fromTheme("help-about"), tr("&About KMagMux"),
                      this, &MainWindow::onAbout);

  // Setup Tool Bar
  QToolBar *mainToolBar = addToolBar(tr("Main Toolbar"));
  mainToolBar->addAction(m_toggleProcessingAction);
  mainToolBar->addSeparator();
  mainToolBar->addAction(addItemsAction);
  mainToolBar->addSeparator();
  mainToolBar->addAction(m_minimizeAction);
  mainToolBar->addAction(quitAction);

  // Setup Status Bar
  statusBar()->showMessage(tr("Ready"));

  QWidget *centralWidget = new QWidget(this);
  setCentralWidget(centralWidget);

  QVBoxLayout *layout = new QVBoxLayout(centralWidget);

  m_tabWidget = new QTabWidget(this);
  layout->addWidget(m_tabWidget);

  auto setupView = [this](QTableView *view, ItemModel *model,
                          ItemFilterProxyModel *proxy, const QString &title) {
    QWidget *tab = new QWidget(this);
    QVBoxLayout *tabLayout = new QVBoxLayout(tab);

    QLineEdit *searchBox = new QLineEdit(tab);
    searchBox->setPlaceholderText(tr("Search by name, tracker, or label..."));
    searchBox->setClearButtonEnabled(true);
    tabLayout->addWidget(searchBox);

    proxy->setSourceModel(model);
    view->setModel(proxy);

    connect(searchBox, &QLineEdit::textChanged, proxy,
            &ItemFilterProxyModel::setFilterText);

    view->horizontalHeader()->setSectionResizeMode(
        QHeaderView::ResizeToContents);
    view->setModel(model);
    view->setItemDelegate(new MaxWidthDelegate(view));
    view->horizontalHeader()->setSectionResizeMode(
        QHeaderView::ResizeToContents);
    view->horizontalHeader()->setStretchLastSection(true);
    view->setTextElideMode(Qt::ElideRight);
    view->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    view->setSelectionBehavior(QAbstractItemView::SelectRows);
    view->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(view, &QTableView::customContextMenuRequested, this,
            &MainWindow::onCustomContextMenuRequested);

    tabLayout->addWidget(view);
    m_tabWidget->addTab(tab, title);
  };

  // Inbox Tab
  m_unprocessedView = new QTableView(this);
  m_unprocessedModel = new ItemModel(this);
  m_unprocessedProxy = new ItemFilterProxyModel(this);
  setupView(m_unprocessedView, m_unprocessedModel, m_unprocessedProxy, "Inbox");
  m_unprocessedView->hideColumn(ItemModel::ColState);
  m_unprocessedView->hideColumn(ItemModel::ColDispatchTime);
  m_unprocessedView->hideColumn(ItemModel::ColError);
  connect(m_unprocessedView, &QTableView::doubleClicked, this,
          &MainWindow::onProcessItem);

  // Queue Tab
  m_queueView = new QTableView(this);
  m_queueModel = new ItemModel(this);
  m_queueProxy = new ItemFilterProxyModel(this);
  setupView(m_queueView, m_queueModel, m_queueProxy, "Queue");
  m_queueView->hideColumn(ItemModel::ColError);

  // Done Tab
  m_doneView = new QTableView(this);
  m_doneModel = new ItemModel(this);
  m_doneProxy = new ItemFilterProxyModel(this);
  setupView(m_doneView, m_doneModel, m_doneProxy, "Done");
  m_doneView->hideColumn(ItemModel::ColState);
  m_doneView->hideColumn(ItemModel::ColError);

  // Archive Tab
  m_archiveView = new QTableView(this);
  m_archiveModel = new ItemModel(this);
  m_archiveProxy = new ItemFilterProxyModel(this);
  setupView(m_archiveView, m_archiveModel, m_archiveProxy, "Archive");
  m_archiveView->hideColumn(ItemModel::ColError);

  // Errors Tab
  m_errorView = new QTableView(this);
  m_errorModel = new ItemModel(this);
  m_errorProxy = new ItemFilterProxyModel(this);
  setupView(m_errorView, m_errorModel, m_errorProxy, "Errors");

  // System Tray Setup
  m_trayIcon = new QSystemTrayIcon(this);
  m_trayIcon->setIcon(QIcon(":/icons/kmagmux.svg"));
  if (m_trayIcon->icon().isNull()) {
    m_trayIcon->setIcon(QIcon::fromTheme("kmagmux"));
  }

  m_trayIconMenu = new QMenu(this);

  m_showHideAction = new QAction(tr("Show/Hide"), this);
  connect(m_showHideAction, &QAction::triggered, this,
          &MainWindow::toggleShowHide);
  m_trayIconMenu->addAction(m_showHideAction);

  m_trayIconMenu->addSeparator();

  m_quitAction = new QAction(tr("&Quit"), this);
  connect(m_quitAction, &QAction::triggered, qApp, &QApplication::quit);
  m_trayIconMenu->addAction(m_quitAction);

  m_trayIcon->setContextMenu(m_trayIconMenu);
  m_trayIcon->show();

  connect(m_trayIcon, &QSystemTrayIcon::activated, this,
          &MainWindow::onTrayIconActivated);
}

void MainWindow::onTrayIconActivated(QSystemTrayIcon::ActivationReason reason) {
  if (reason == QSystemTrayIcon::DoubleClick ||
      reason == QSystemTrayIcon::Trigger) {
    toggleShowHide();
  }
}

void MainWindow::toggleShowHide() {
  if (isVisible()) {
    hide();
  } else {
    showNormal();
    activateWindow();
  }
}

void MainWindow::minimizeToTray() { hide(); }

void MainWindow::applySettings() {
  QSettings settings;
  m_closeToTray = settings.value("closeToTray", false).toBool();
  m_minimizeToTray = settings.value("minimizeToTray", false).toBool();
  m_autoStart = settings.value("autoStart", false).toBool();

#ifdef Q_OS_WIN
  QSettings bootUpSettings(
      "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run",
      QSettings::NativeFormat);
  if (m_autoStart) {
    QString appPath =
        QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    bootUpSettings.setValue("KMagMux", "\"" + appPath + "\"");
  } else {
    bootUpSettings.remove("KMagMux");
  }
#elif defined(Q_OS_LINUX)
  QString autostartPath = QDir::homePath() + "/.config/autostart";
  QDir dir(autostartPath);
  if (!dir.exists()) {
    dir.mkpath(".");
  }

  QString desktopFilePath = autostartPath + "/kmagmux.desktop";
  QFile file(desktopFilePath);

  if (m_autoStart) {
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
      QTextStream out(&file);
      out << "[Desktop Entry]\n"
          << "Type=Application\n"
          << "Exec=\"" << QCoreApplication::applicationFilePath() << "\"\n"
          << "Hidden=false\n"
          << "NoDisplay=false\n"
          << "X-GNOME-Autostart-enabled=true\n"
          << "Name=KMagMux\n"
          << "Comment=Start KMagMux\n";
      file.close();
    }
  } else {
    if (file.exists()) {
      file.remove();
    }
  }
#endif
}

void MainWindow::loadData() {
  auto items = m_storage->loadAllItems();

  std::vector<Item> unprocessedItems;
  std::vector<Item> queueItems;
  std::vector<Item> doneItems;
  std::vector<Item> archiveItems;
  std::vector<Item> errorItems;

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
    case ItemState::Done:
      doneItems.push_back(item);
      break;
    case ItemState::Archived:
      archiveItems.push_back(item);
      break;
    case ItemState::Failed:
      errorItems.push_back(item);
      break;
    default:
      break;
    }
  }

  m_unprocessedModel->setItems(unprocessedItems);
  m_queueModel->setItems(queueItems);
  m_doneModel->setItems(doneItems);
  m_archiveModel->setItems(archiveItems);
  m_errorModel->setItems(errorItems);
}

QTableView *MainWindow::getCurrentView() const {
  QWidget *currentTab = m_tabWidget->currentWidget();
  if (currentTab) {
    return currentTab->findChild<QTableView *>();
  }
  return nullptr;
}

ItemModel *MainWindow::getCurrentModel() const {
  QTableView *view = getCurrentView();
  if (view) {
    ItemFilterProxyModel *proxy =
        qobject_cast<ItemFilterProxyModel *>(view->model());
    if (proxy) {
      return qobject_cast<ItemModel *>(proxy->sourceModel());
    }
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

  // If we are in the Inbox view, offer a "Process..." action
  if (view == m_unprocessedView) {
    QAction *processAction = menu.addAction("Process...");
    connect(processAction, &QAction::triggered, this,
            &MainWindow::onProcessItem);
    menu.addSeparator();
  }

  if (view == m_errorView) {
    QAction *reprocessAction = menu.addAction("Reprocess");
    connect(reprocessAction, &QAction::triggered, this,
            [this]() { onItemAction(ItemState::Queued); });

    QAction *dismissAction = menu.addAction("Dismiss");
    connect(dismissAction, &QAction::triggered, this,
            [this]() { onItemAction(ItemState::Archived); });

    menu.addSeparator();
  }

  if (view != m_unprocessedView) {
    QAction *queueAction = menu.addAction("Queue");
    QAction *holdAction = menu.addAction("Hold");

    connect(queueAction, &QAction::triggered, this,
            [this]() { onItemAction(ItemState::Queued); });
    connect(holdAction, &QAction::triggered, this,
            [this]() { onItemAction(ItemState::Held); });
  }

  QAction *archiveAction = menu.addAction("Archive");
  connect(archiveAction, &QAction::triggered, this,
          [this]() { onItemAction(ItemState::Archived); });

  menu.addSeparator();

  QAction *deleteAction =
      menu.addAction(QIcon::fromTheme("edit-delete"), "Delete");
  connect(deleteAction, &QAction::triggered, this, &MainWindow::onDeleteItems);

  menu.exec(view->viewport()->mapToGlobal(pos));
}

void MainWindow::onItemAction(ItemState newState) {
  QTableView *view = getCurrentView();
  if (!view)
    return;

  const ItemModel *model = getCurrentModel();
  if (!model)
    return;

  ItemFilterProxyModel *proxy =
      qobject_cast<ItemFilterProxyModel *>(view->model());
  if (!proxy)
    return;

  QModelIndexList selection = view->selectionModel()->selectedRows();
  if (selection.isEmpty())
    return;

  QModelIndex sourceIndex = proxy->mapToSource(selection.first());
  int row = sourceIndex.row();
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

void MainWindow::onItemDeleted(const QString &id) { loadData(); }

void MainWindow::onDeleteItems() {
  QTableView *view = getCurrentView();
  if (!view)
    return;

  const ItemModel *model = getCurrentModel();
  if (!model)
    return;

  QModelIndexList selection = view->selectionModel()->selectedRows();
  if (selection.isEmpty())
    return;

  int count = selection.size();
  QMessageBox::StandardButton reply;
  reply =
      QMessageBox::question(this, tr("Delete Items"),
                            tr("Are you sure you want to delete the selected "
                               "%n item(s)?\n\nThis will remove the item from "
                               "tracking and delete any managed payload files.",
                               "", count),
                            QMessageBox::Yes | QMessageBox::No);
  if (reply == QMessageBox::Yes) {
    std::vector<QString> idsToDelete;
    idsToDelete.reserve(selection.size());
    for (const QModelIndex &index : selection) {
      idsToDelete.push_back(model->getItem(index.row()).id);
    }

    for (const QString &id : idsToDelete) {
      qDebug() << "Deleting item:" << id;
      if (!m_storage->deleteItem(id)) {
        qWarning() << "Failed to delete item:" << id;
      }
    }
  }
}

void MainWindow::onAddItems() {
  QDialog dialog(this);
  dialog.setWindowTitle(tr("Find Items"));
  dialog.resize(500, 400);

  QVBoxLayout *layout = new QVBoxLayout(&dialog);

  QLabel *label = new QLabel(
      tr("Paste links, paths, or use 'Browse Files...' to add items:"),
      &dialog);
  layout->addWidget(label);

  QPlainTextEdit *textEdit = new QPlainTextEdit(&dialog);
  textEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
  textEdit->setPlaceholderText(
      tr("file://...\n/path/to/file\nhttp://...\nhttps://...\nmagnet:..."));
  layout->addWidget(textEdit);

  QHBoxLayout *buttonLayout = new QHBoxLayout();
  QPushButton *browseBtn = new QPushButton(tr("Browse Files..."), &dialog);
  QPushButton *openListBtn = new QPushButton(tr("Open List/HTML"), &dialog);
  buttonLayout->addWidget(browseBtn);
  buttonLayout->addWidget(openListBtn);
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
      currentText = std::accumulate(files.begin(), files.end(), currentText,
                                    [](const QString &a, const QString &b) {
                                      return a + "file://" + b + "\n";
                                    });
      textEdit->setPlainText(currentText);
    }
  });

  connect(
      openListBtn, &QPushButton::clicked, this, [&dialog, textEdit, this]() {
        QDialog openDialog(&dialog);
        openDialog.setWindowTitle(tr("Open List/HTML"));
        QVBoxLayout *olLayout = new QVBoxLayout(&openDialog);

        QLabel *olLabel = new QLabel(
            tr("Enter URLs or local file paths to text lists or HTML files:"),
            &openDialog);
        olLayout->addWidget(olLabel);

        QPlainTextEdit *olTextEdit = new QPlainTextEdit(&openDialog);
        olTextEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
        olLayout->addWidget(olTextEdit);

        QCheckBox *extractMagnetsCb =
            new QCheckBox(tr("Extract magnet links"), &openDialog);
        extractMagnetsCb->setChecked(true);
        olLayout->addWidget(extractMagnetsCb);

        QCheckBox *extractTorrentsCb =
            new QCheckBox(tr("Extract torrent links"), &openDialog);
        extractTorrentsCb->setChecked(true);
        olLayout->addWidget(extractTorrentsCb);

        QDialogButtonBox *olButtonBox = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &openDialog);
        olLayout->addWidget(olButtonBox);

        connect(olButtonBox, &QDialogButtonBox::accepted, &openDialog,
                &QDialog::accept);
        connect(olButtonBox, &QDialogButtonBox::rejected, &openDialog,
                &QDialog::reject);

        if (openDialog.exec() == QDialog::Accepted) {
          QStringList lines =
              olTextEdit->toPlainText().split('\n', Qt::SkipEmptyParts);
          if (!lines.isEmpty()) {
            LinkExtractorDialog extractor(lines, extractMagnetsCb->isChecked(),
                                          extractTorrentsCb->isChecked(),
                                          &dialog);
            if (extractor.exec() == QDialog::Accepted) {
              if (extractor.wasModified()) {
                QString expanded = extractor.getExpandedLines().join('\n');
                if (!expanded.isEmpty()) {
                  QString currentText = textEdit->toPlainText();
                  if (!currentText.isEmpty() && !currentText.endsWith('\n')) {
                    currentText += '\n';
                  }
                  currentText += expanded + '\n';
                  textEdit->setPlainText(currentText);
                }
              }
            }
          }
        }
      });

  connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

  if (dialog.exec() == QDialog::Accepted) {
    QStringList lines = textEdit->toPlainText().split('\n', Qt::SkipEmptyParts);

    // Offload parsing to a background thread
    auto *watcher = new QFutureWatcher<std::vector<Item>>(this);

    connect(watcher, &QFutureWatcher<std::vector<Item>>::finished, this,
            [this, watcher]() {
              std::vector<Item> items = watcher->result();
              if (!items.empty()) {
                openAddItemsDialog(items);
              }
              watcher->deleteLater();
            });

    QFuture<std::vector<Item>> future =
        QtConcurrent::run([lines]() -> std::vector<Item> {
          return ItemParser::parseLines(lines);
        });

    watcher->setFuture(future);
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

  ItemFilterProxyModel *proxy =
      qobject_cast<ItemFilterProxyModel *>(view->model());
  if (!proxy)
    return;

  std::vector<Item> selectedItems;
  selectedItems.reserve(selection.size());
  std::transform(selection.begin(), selection.end(),
                 std::back_inserter(selectedItems),
                 [model, proxy](const QModelIndex &index) {
                   QModelIndex sourceIndex = proxy->mapToSource(index);
                   return model->getItem(sourceIndex.row());
                 });

  openProcessItemDialog(selectedItems);
}

void MainWindow::saveItemsFromDialog(std::vector<Item> updatedItems) {
  std::vector<Item> itemsToSave;
  itemsToSave.reserve(updatedItems.size());

  for (Item &updatedItem : updatedItems) {
    bool success = true;

    if (updatedItem.metadata.value("delete_source_file").toBool(false)) {
      // Move logic
      if (!m_storage->moveToManaged(updatedItem, true, true)) {
        QMessageBox::warning(
            this, "Error",
            QString("Failed to move original file to managed storage: %1")
                .arg(updatedItem.sourcePath));
        success = false; // Should we abort saving? Maybe just warn.
      }

      // Remove the temporary internal flag before saving
      QJsonObject meta = updatedItem.metadata;
      meta.remove("delete_source_file");
      updatedItem.metadata = meta;
    }

    if (success) {
      itemsToSave.push_back(updatedItem);
      qDebug() << "Item queued for saving:" << updatedItem.id;
    }
  }

  if (!itemsToSave.empty()) {
    m_storage->saveItems(itemsToSave);
  }
}

void MainWindow::openAddItemsDialog(const std::vector<Item> &items) {
  if (items.empty())
    return;

  AddItemDialog dialog(items, m_engine->getAvailableConnectors(), this);
  if (dialog.exec() == QDialog::Accepted) {
    saveItemsFromDialog(dialog.getItems());
  }
}

void MainWindow::openProcessItemDialog(const std::vector<Item> &items) {
  if (items.empty())
    return;

  ProcessItemDialog dialog(items, m_engine->getAvailableConnectors(), this);
  if (dialog.exec() == QDialog::Accepted) {
    saveItemsFromDialog(dialog.getItems());
  }
}

void MainWindow::onPreferences() {
  PreferencesDialog dialog(m_engine, this);
  if (dialog.exec() == QDialog::Accepted) {
    applySettings();
  }
}

void MainWindow::onAbout() {
  QMessageBox::about(
      this, tr("About KMagMux"),
      tr("KMagMux\nTorrent and Magnet file handler and router."));
}

void MainWindow::onToggleProcessing(bool checked) {
  if (checked) {
    m_engine->setPaused(false);
    m_toggleProcessingAction->setIcon(QIcon::fromTheme("media-playback-pause"));
  } else {
    m_engine->setPaused(true);
    m_toggleProcessingAction->setIcon(QIcon::fromTheme("media-playback-start"));
  }
}

void MainWindow::onOpenCacheDirectory() {
  QDesktopServices::openUrl(QUrl::fromLocalFile(m_storage->getBaseDir()));
}
