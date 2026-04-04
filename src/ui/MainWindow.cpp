#include "MainWindow.h"
#include "../core/ItemParser.h"
#include "AddItemDialog.h"
#include "ApiExplorerDialog.h"
#include "LinkExtractorDialog.h"
#include "MaxWidthDelegate.h"
#include "PreferencesDialog.h"
#include "ProcessItemDialog.h"
#include "TorrentInfoDialog.h"
#include <QApplication>
#include <QCloseEvent>
#include <QDateTime>
#include <QDebug>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFutureWatcher>
#include <QHeaderView>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QMimeData>
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
    : KXmlGuiWindow(parent), m_storage(storage), m_engine(nullptr),
      m_tabWidget(nullptr), m_unprocessedModel(nullptr), m_queueModel(nullptr),
      m_doneModel(nullptr), m_archiveModel(nullptr), m_errorModel(nullptr),
      m_unprocessedProxy(nullptr), m_queueProxy(nullptr), m_doneProxy(nullptr),
      m_archiveProxy(nullptr), m_errorProxy(nullptr),
      m_unprocessedView(nullptr), m_queueView(nullptr), m_doneView(nullptr),
      m_archiveView(nullptr), m_errorView(nullptr),
      m_toggleProcessingAction(nullptr), m_selectAllAction(nullptr),
      m_processAction(nullptr), m_processAllAction(nullptr),
      m_unprocessAction(nullptr), m_dismissAction(nullptr),
      m_archiveAction(nullptr), m_archiveAllAction(nullptr),
      m_deleteAction(nullptr), m_infoAction(nullptr),
      m_rawResultsAction(nullptr), m_trayIcon(nullptr), m_trayIconMenu(nullptr),
      m_minimizeAction(nullptr), m_showHideAction(nullptr),
      m_quitAction(nullptr), m_closeToTray(false), m_minimizeToTray(false),
      m_autoStart(false), m_forceQuit(false) {
  qApp->setQuitOnLastWindowClosed(false);

  applySettings();

  m_engine = new Engine(m_storage, this);
  m_engine->start();

  setupUi();
  loadData();

  connect(m_storage, &StorageManager::itemAdded, this,
          &MainWindow::onItemAdded);
  connect(m_storage, &StorageManager::itemUpdated, this,
          &MainWindow::onItemUpdated);
  connect(m_storage, &StorageManager::itemsUpdated, this,
          &MainWindow::onItemsUpdated);
  connect(m_storage, &StorageManager::itemDeleted, this,
          &MainWindow::onItemDeleted);
  connect(m_storage, &StorageManager::itemsDeleted, this,
          &MainWindow::onItemsDeleted);
}

MainWindow::~MainWindow() {
  if (m_trayIcon) {
    m_trayIcon->hide();
    m_trayIcon->setContextMenu(nullptr); // detach the menu before destruction
  }
  if (m_engine) {
    m_engine->stop();
    m_engine->deleteLater();
    m_engine = nullptr;
  }
}

void MainWindow::closeEvent(QCloseEvent *event) {
  if (m_closeToTray && m_trayIcon->isVisible() && !m_forceQuit) {
    hide();
    event->ignore();
  } else {
    KXmlGuiWindow::closeEvent(event);
    qApp->quit();
  }
}

void MainWindow::quitApplication() {
  m_forceQuit = true;
  close();
  qApp->quit();
}

void MainWindow::changeEvent(QEvent *event) {
  KXmlGuiWindow::changeEvent(event);
  if (event->type() == QEvent::WindowStateChange) {
    if (isMinimized() && m_minimizeToTray && m_trayIcon->isVisible()) {
      hide();
    }
  }
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event) {
  if (event->mimeData()->hasUrls() || event->mimeData()->hasText()) {
    event->acceptProposedAction();
  }
}

void MainWindow::dropEvent(QDropEvent *event) {
  QStringList lines;
  QStringList filesToRead;

  if (event->mimeData()->hasUrls()) {
    QList<QUrl> urls = event->mimeData()->urls();
    for (const QUrl &url : urls) {
      if (url.isLocalFile()) {
        QString localPath = url.toLocalFile();
        QFileInfo fileInfo(localPath);
        if (fileInfo.suffix().toLower() == "torrent") {
          lines.append("file://" + localPath);
        } else if (fileInfo.suffix().toLower() == "txt" ||
                   fileInfo.suffix().isEmpty()) {
          filesToRead.append(localPath);
        } else {
          lines.append("file://" + localPath);
        }
      } else {
        lines.append(url.toString());
      }
    }
  } else if (event->mimeData()->hasText()) {
    QString text = event->mimeData()->text();
    lines = text.split('\n', Qt::SkipEmptyParts);
  }

  if (!lines.isEmpty() || !filesToRead.isEmpty()) {
    auto *watcher = new QFutureWatcher<std::vector<Item>>(this);

    connect(watcher, &QFutureWatcher<std::vector<Item>>::finished, this,
            [this, watcher]() {
              std::vector<Item> parsedItems = watcher->result();
              if (!parsedItems.empty()) {
                m_storage->saveItems(parsedItems);
              }
              // Switch to Inbox tab
              m_tabWidget->setCurrentIndex(0);
              watcher->deleteLater();
            });

    QFuture<std::vector<Item>> future =
        QtConcurrent::run([lines, filesToRead]() -> std::vector<Item> {
          QStringList finalLines = lines;
          for (const QString &localPath : filesToRead) {
            QFile file(localPath);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
              QTextStream in(&file);
              while (!in.atEnd()) {
                QString line = in.readLine().trimmed();
                if (!line.isEmpty()) {
                  finalLines.append(line);
                }
              }
            } else {
              finalLines.append("file://" + localPath); // Fallback
            }
          }
          return ItemParser::parseLines(finalLines);
        });

    watcher->setFuture(future);
  }
  event->acceptProposedAction();
}

void MainWindow::setupUi() {
  setWindowTitle("KMagMux");
  resize(1000, 600);

  setupTabs();
  setupActionsAndMenus();
  setupSystemTray();

  // Setup Status Bar
  statusBar()->showMessage(tr("Ready"));
}

void MainWindow::setupActionsAndMenus() {
  QAction *addItemsAction =
      new QAction(QIcon::fromTheme("document-open"), tr("&Add..."), this);
  actionCollection()->setDefaultShortcut(addItemsAction,
                                         QKeySequence("Ctrl+O"));
  connect(addItemsAction, &QAction::triggered, this, &MainWindow::onAddItems);
  actionCollection()->addAction("add_items", addItemsAction);

  m_minimizeAction =
      new QAction(QIcon::fromTheme("go-down"), tr("Minimize to Tray"), this);
  connect(m_minimizeAction, &QAction::triggered, this,
          &MainWindow::minimizeToTray);
  actionCollection()->addAction("minimize_to_tray", m_minimizeAction);

  KStandardAction::quit(this, SLOT(quitApplication()), actionCollection());

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
  actionCollection()->addAction("toggle_processing", m_toggleProcessingAction);

  KStandardAction::preferences(this, SLOT(onPreferences()), actionCollection());

  m_selectAllAction = new QAction(tr("Select &All"), this);
  actionCollection()->setDefaultShortcut(m_selectAllAction,
                                         QKeySequence::SelectAll);
  connect(m_selectAllAction, &QAction::triggered, this, [this]() {
    QTableView *view = getCurrentView();
    if (view) {
      view->selectAll();
    }
  });
  actionCollection()->addAction("select_all", m_selectAllAction);

  m_processAction = new QAction(tr("&Process..."), this);
  connect(m_processAction, &QAction::triggered, this,
          &MainWindow::onProcessItem);
  actionCollection()->addAction("process_item", m_processAction);

  m_processAllAction = new QAction(QIcon::fromTheme("media-playback-start"),
                                   tr("Process &All"), this);
  connect(m_processAllAction, &QAction::triggered, this, [this]() {
    QTableView *view = getCurrentView();
    if (view) {
      view->selectAll();
      onProcessItem();
    }
  });
  actionCollection()->addAction("process_all_items", m_processAllAction);

  m_unprocessAction = new QAction(tr("&Send back to Inbox"), this);
  connect(m_unprocessAction, &QAction::triggered, this,
          [this]() { onItemAction(ItemState::Unprocessed); });
  actionCollection()->addAction("unprocess_item", m_unprocessAction);

  m_dismissAction = new QAction(tr("&Dismiss"), this);
  connect(m_dismissAction, &QAction::triggered, this,
          [this]() { onItemAction(ItemState::Archived); });
  actionCollection()->addAction("dismiss_item", m_dismissAction);

  m_archiveAction = new QAction(tr("&Archive"), this);
  m_archiveAllAction = new QAction(tr("Archive &All"), this);
  connect(m_archiveAllAction, &QAction::triggered, this, [this]() {
    const QTableView *view = getCurrentView();
    if (!view)
      return;
    const ItemModel *model = getCurrentModel();
    if (!model)
      return;

    std::vector<Item> itemsToSave;
    itemsToSave.reserve(model->rowCount());
    for (int i = 0; i < model->rowCount(); ++i) {
      Item item = model->getItem(i);
      QString oldState = item.stateToString();
      item.state = ItemState::Archived;
      item.addHistory(
          QString("State changed from %1 to Archived via Bulk Archive.")
              .arg(oldState));
      itemsToSave.push_back(item);
    }
    if (!itemsToSave.empty()) {
      m_storage->saveItems(itemsToSave);
    }
  });
  actionCollection()->addAction("archive_all_items", m_archiveAllAction);

  connect(m_archiveAction, &QAction::triggered, this,
          [this]() { onItemAction(ItemState::Archived); });
  actionCollection()->addAction("archive_item", m_archiveAction);

  m_deleteAction =
      new QAction(QIcon::fromTheme("edit-delete"), tr("&Delete"), this);
  m_infoAction = new QAction(QIcon::fromTheme("document-properties"),
                             tr("&Get Info / History"), this);
  m_rawResultsAction = new QAction(tr("View raw processing results"), this);
  connect(m_rawResultsAction, &QAction::triggered, this,
          &MainWindow::onViewRawProcessingResults);
  actionCollection()->addAction("raw_results_item", m_rawResultsAction);
  connect(m_infoAction, &QAction::triggered, this, [this]() {
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
    Item item = model->getItem(sourceIndex.row());
    QString sourcePath = item.sourcePath;
    if (sourcePath.startsWith("magnet:") || sourcePath.endsWith(".torrent")) {
      TorrentInfoDialog dialog(sourcePath, &item, this);
      dialog.exec();
    } else {
      QMessageBox::information(this, "Item Information",
                               QString("Source Path:\n%1").arg(sourcePath));
    }
  });
  actionCollection()->addAction("info_item", m_infoAction);

  connect(m_deleteAction, &QAction::triggered, this,
          &MainWindow::onDeleteItems);
  actionCollection()->addAction("delete_items", m_deleteAction);

  QAction *openCacheAction = new QAction(QIcon::fromTheme("folder-open"),
                                         tr("Open &Cache directory"), this);
  connect(openCacheAction, &QAction::triggered, this,
          &MainWindow::onOpenCacheDirectory);
  actionCollection()->addAction("open_cache", openCacheAction);

  KStandardAction::aboutApp(this, SLOT(onAbout()), actionCollection());

  setupGUI(Default, ":/kmagmuxui.rc");

  // We need to fetch the debug menu after setupGUI since it's defined in the
  // .rc file
  QMenu *debugMenu = nullptr;
  QList<QMenu *> menus = menuBar()->findChildren<QMenu *>();
  auto it = std::find_if(menus.begin(), menus.end(), [](QMenu *menu) {
    return menu->objectName() == "debug_menu";
  });
  if (it != menus.end()) {
    debugMenu = *it;
  }

  if (debugMenu) {
    setupPluginMenus(debugMenu);
  }
}

void MainWindow::setupTabs() {
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
    view->setItemDelegate(new MaxWidthDelegate(view));
    view->horizontalHeader()->setSectionResizeMode(
        QHeaderView::ResizeToContents);
    view->horizontalHeader()->setStretchLastSection(true);
    view->setTextElideMode(Qt::ElideRight);
    view->setWordWrap(false);
    view->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    view->setSelectionBehavior(QAbstractItemView::SelectRows);
    view->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(view, &QTableView::customContextMenuRequested, this,
            &MainWindow::onCustomContextMenuRequested);

    connect(view->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &MainWindow::updateActionsState);

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
  setAcceptDrops(true); // Allow drops on the main window

  // Queue Tab
  m_queueView = new QTableView(this);
  m_queueModel = new ItemModel(this);
  m_queueProxy = new ItemFilterProxyModel(this);
  setupView(m_queueView, m_queueModel, m_queueProxy, "Queue");
  m_queueView->hideColumn(ItemModel::ColError);
  connect(m_queueView, &QTableView::doubleClicked, this,
          &MainWindow::onProcessItem);

  // Done Tab
  m_doneView = new QTableView(this);
  m_doneModel = new ItemModel(this);
  m_doneProxy = new ItemFilterProxyModel(this);
  setupView(m_doneView, m_doneModel, m_doneProxy, "Done");
  m_doneView->hideColumn(ItemModel::ColState);
  m_doneView->hideColumn(ItemModel::ColError);
  connect(m_doneView, &QTableView::doubleClicked, this,
          &MainWindow::onProcessItem);

  // Archive Tab
  m_archiveView = new QTableView(this);
  m_archiveModel = new ItemModel(this);
  m_archiveProxy = new ItemFilterProxyModel(this);
  setupView(m_archiveView, m_archiveModel, m_archiveProxy, "Archive");
  m_archiveView->hideColumn(ItemModel::ColError);
  connect(m_archiveView, &QTableView::doubleClicked, this,
          &MainWindow::onProcessItem);

  // Errors Tab
  m_errorView = new QTableView(this);
  m_errorModel = new ItemModel(this);
  m_errorProxy = new ItemFilterProxyModel(this);
  setupView(m_errorView, m_errorModel, m_errorProxy, "Errors");
  connect(m_errorView, &QTableView::doubleClicked, this,
          &MainWindow::onProcessItem);

  connect(m_tabWidget, &QTabWidget::currentChanged, this,
          &MainWindow::updateActionsState);
  updateActionsState();
}

void MainWindow::setupSystemTray() {
  // System Tray Setup
  m_trayIcon = new QSystemTrayIcon(this);
  m_trayIcon->setIcon(QIcon(":/icons/kmagmux.svg"));
  if (m_trayIcon->icon().isNull()) {
    m_trayIcon->setIcon(QIcon::fromTheme("kmagmux"));
  }

  m_trayIconMenu = new QMenu(this);

  QAction *trayAddAction = new QAction(tr("&Add..."), this);
  connect(trayAddAction, &QAction::triggered, this, &MainWindow::onAddItems);
  m_trayIconMenu->addAction(trayAddAction);

  m_trayIconMenu->addSeparator();

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

  QString desktopFilePath = autostartPath + "/org.kde.kmagmux.desktop";
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
  const QTableView *view = getCurrentView();
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
  QMenu menu(this);

  // If we clicked on an empty space in the Inbox view, offer an "Add..." action
  if (!index.isValid()) {
    if (view == m_unprocessedView) {
      QAction *addAction = menu.addAction("Add...");
      connect(addAction, &QAction::triggered, this, &MainWindow::onAddItems);
      menu.exec(view->viewport()->mapToGlobal(pos));
    }
    return;
  }

  // Ensure the clicked row is selected if it wasn't already
  if (index.isValid() && !view->selectionModel()->isSelected(index)) {
    view->selectionModel()->clearSelection();
    view->selectionModel()->select(index, QItemSelectionModel::Select |
                                              QItemSelectionModel::Rows);
  }

  if (view == m_unprocessedView) {
    menu.addAction(actionCollection()->action("add_items"));
    menu.addSeparator();
  }

  menu.addAction(m_processAction);
  menu.addAction(m_processAllAction);
  menu.addSeparator();

  if (view == m_errorView) {
    ItemFilterProxyModel *proxy =
        qobject_cast<ItemFilterProxyModel *>(view->model());
    if (proxy && index.isValid()) {
      QModelIndex sourceIndex = proxy->mapToSource(index);
      Item item = m_errorModel->getItem(sourceIndex.row());
      if (item.metadata.contains("raw_http")) {
        QAction *rawHttpAction = menu.addAction(tr("View raw http"));
        connect(rawHttpAction, &QAction::triggered, this,
                &MainWindow::onViewRawHttp);
      }
    }
    menu.addAction(m_dismissAction);
    menu.addSeparator();
  }

  if (view != m_unprocessedView) {
    menu.addAction(m_unprocessAction);
  }

  menu.addAction(m_archiveAction);
  if (view == m_doneView || view == m_errorView) {
    menu.addAction(m_archiveAllAction);
  }
  menu.addSeparator();
  menu.addAction(m_infoAction);

  if (index.isValid()) {
    ItemFilterProxyModel *proxy =
        qobject_cast<ItemFilterProxyModel *>(view->model());
    if (proxy) {
      QModelIndex sourceIndex = proxy->mapToSource(index);
      const ItemModel *model = getCurrentModel();
      if (model) {
        Item item = model->getItem(sourceIndex.row());
        if (item.metadata.contains("raw_response")) {
          menu.addAction(m_rawResultsAction);
        }
      }
    }
  }

  menu.addAction(m_deleteAction);

  menu.exec(view->viewport()->mapToGlobal(pos));
}

void MainWindow::onViewRawProcessingResults() {
  QTableView *view = getCurrentView();
  if (!view)
    return;

  ItemFilterProxyModel *proxy =
      qobject_cast<ItemFilterProxyModel *>(view->model());
  if (!proxy)
    return;

  QModelIndexList selection = view->selectionModel()->selectedRows();
  if (selection.isEmpty())
    return;

  QModelIndex sourceIndex = proxy->mapToSource(selection.first());
  const ItemModel *model = getCurrentModel();
  if (!model)
    return;

  Item item = model->getItem(sourceIndex.row());

  if (item.metadata.contains("raw_response")) {
    QString rawResponse = item.metadata["raw_response"].toString();

    // Try to parse it as JSON to format it
    QJsonParseError parseError;
    QJsonDocument doc =
        QJsonDocument::fromJson(rawResponse.toUtf8(), &parseError);
    if (parseError.error == QJsonParseError::NoError) {
      rawResponse = doc.toJson(QJsonDocument::Indented);
    }

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Raw Processing Results"));
    dialog.resize(600, 400);
    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    QPlainTextEdit *textEdit = new QPlainTextEdit(&dialog);
    textEdit->setReadOnly(true);
    textEdit->setPlainText(rawResponse);
    layout->addWidget(textEdit);

    QDialogButtonBox *buttonBox =
        new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttonBox);

    dialog.exec();
  }
}

void MainWindow::onViewRawHttp() {
  QTableView *view = getCurrentView();
  if (!view || view != m_errorView)
    return;

  ItemFilterProxyModel *proxy =
      qobject_cast<ItemFilterProxyModel *>(view->model());
  if (!proxy)
    return;

  QModelIndexList selection = view->selectionModel()->selectedRows();
  if (selection.isEmpty())
    return;

  QModelIndex sourceIndex = proxy->mapToSource(selection.first());
  Item item = m_errorModel->getItem(sourceIndex.row());

  if (item.metadata.contains("raw_http")) {
    QString rawHttp = item.metadata["raw_http"].toString();
    QMessageBox::information(this, tr("Raw HTTP"), rawHttp);
  }
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

  std::vector<Item> itemsToSave;
  itemsToSave.reserve(selection.size());

  for (const QModelIndex &index : selection) {
    QModelIndex sourceIndex = proxy->mapToSource(index);
    int row = sourceIndex.row();
    Item item = model->getItem(row);

    qDebug() << "Changing item state for:" << item.id << "to" << (int)newState;

    QString oldState = item.stateToString();
    item.state = newState;
    item.addHistory(QString("State changed from %1 to %2 by user action.")
                        .arg(oldState, item.stateToString()));

    itemsToSave.push_back(item);
  }

  if (!itemsToSave.empty()) {
    m_storage->saveItems(itemsToSave);
  }
}

void MainWindow::onItemAdded(const Item &item) { loadData(); }

void MainWindow::onItemUpdated(const Item &item) { loadData(); }

void MainWindow::onItemsUpdated() { loadData(); }

void MainWindow::onItemDeleted(const QString &id) { loadData(); }

void MainWindow::onItemsDeleted(const std::vector<QString> &ids) { loadData(); }

void MainWindow::onDeleteItems() {
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
      QModelIndex sourceIndex = proxy->mapToSource(index);
      idsToDelete.push_back(model->getItem(sourceIndex.row()).id);
    }

    qDebug() << "Deleting" << idsToDelete.size() << "items";
    m_storage->deleteItems(idsToDelete);
  }
}

void MainWindow::onAddItems() {
  QDialog dialog(this);
  dialog.setWindowTitle(tr("Add Items"));
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
    processAddedLines(lines);
  }
}

void MainWindow::processAddedLines(const QStringList &lines) {
  if (lines.isEmpty())
    return;

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

  QFuture<std::vector<Item>> future = QtConcurrent::run(
      [lines]() -> std::vector<Item> { return ItemParser::parseLines(lines); });

  watcher->setFuture(future);
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

void MainWindow::updateActionsState() {
  QTableView *view = getCurrentView();
  if (!view) {
    if (m_selectAllAction)
      m_selectAllAction->setEnabled(false);
    if (m_processAction)
      m_processAction->setEnabled(false);
    if (m_unprocessAction)
      m_unprocessAction->setEnabled(false);
    if (m_dismissAction)
      m_dismissAction->setEnabled(false);
    if (m_archiveAction)
      m_archiveAction->setEnabled(false);
    if (m_archiveAllAction)
      m_archiveAllAction->setEnabled(false);
    if (m_deleteAction)
      m_deleteAction->setEnabled(false);
    if (m_infoAction)
      m_infoAction->setEnabled(false);
    if (m_rawResultsAction)
      m_rawResultsAction->setEnabled(false);
    return;
  }

  bool hasSelection =
      view->selectionModel() && view->selectionModel()->hasSelection();

  if (m_selectAllAction)
    m_selectAllAction->setEnabled(true);

  if (m_processAction) {
    m_processAction->setVisible(true);
    m_processAction->setEnabled(hasSelection);
  }

  if (m_processAllAction) {
    const ItemModel *model = getCurrentModel();
    m_processAllAction->setVisible(view == m_unprocessedView);
    m_processAllAction->setEnabled(model && model->rowCount() > 0 &&
                                   view == m_unprocessedView);
  }

  if (m_unprocessAction) {
    m_unprocessAction->setVisible(view != m_unprocessedView);
    m_unprocessAction->setEnabled(hasSelection && view != m_unprocessedView);
  }

  if (m_dismissAction) {
    m_dismissAction->setVisible(view == m_errorView);
    m_dismissAction->setEnabled(hasSelection && view == m_errorView);
  }

  if (m_archiveAction)
    m_archiveAction->setEnabled(hasSelection);
  if (m_archiveAllAction) {
    const ItemModel *model = getCurrentModel();
    m_archiveAllAction->setVisible(view == m_doneView || view == m_errorView);
    m_archiveAllAction->setEnabled(model && model->rowCount() > 0 &&
                                   (view == m_doneView || view == m_errorView));
  }
  if (m_deleteAction)
    m_deleteAction->setEnabled(hasSelection);
  if (m_infoAction) {
    QModelIndexList selection = view->selectionModel()->selectedRows();
    m_infoAction->setEnabled(hasSelection && selection.size() == 1);
  }
  if (m_rawResultsAction) {
    QModelIndexList selection = view->selectionModel()->selectedRows();
    bool rawEnabled = false;
    if (hasSelection && selection.size() == 1) {
      ItemFilterProxyModel *proxy =
          qobject_cast<ItemFilterProxyModel *>(view->model());
      if (proxy) {
        QModelIndex sourceIndex = proxy->mapToSource(selection.first());
        const ItemModel *model = getCurrentModel();
        if (model) {
          Item item = model->getItem(sourceIndex.row());
          if (item.metadata.contains("raw_response")) {
            rawEnabled = true;
          }
        }
      }
    }
    m_rawResultsAction->setEnabled(rawEnabled);
  }
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

void MainWindow::setupPluginMenus(QMenu *helpMenu) {
  if (!m_engine)
    return;

  for (const QString &connectorId : m_engine->getAvailableConnectors()) {
    Connector *connector = m_engine->getConnector(connectorId);
    if (connector && connector->hasDebugMenu()) {
      QMenu *pluginMenu =
          helpMenu->addMenu(QString("%1 Debug").arg(connector->getName()));

      QList<HttpApiEndpoint> endpoints = connector->getHttpApiEndpoints();
      if (!endpoints.isEmpty()) {
        QAction *apiExplorerAction = pluginMenu->addAction("API Explorer");
        connect(apiExplorerAction, &QAction::triggered, this,
                [this, connector]() { onOpenApiExplorer(connector); });
      }
    }
  }
}

void MainWindow::onOpenApiExplorer(Connector *connector) {
  if (!connector)
    return;

  ApiExplorerDialog dialog(connector, this);
  dialog.exec();
}
