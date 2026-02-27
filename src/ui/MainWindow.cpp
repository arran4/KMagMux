#include "MainWindow.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QDebug>

MainWindow::MainWindow(StorageManager *storage, QWidget *parent)
    : QMainWindow(parent), m_storage(storage) {
    setupUi();
    loadData();

    connect(m_storage, &StorageManager::itemAdded, this, &MainWindow::onItemAdded);
    connect(m_storage, &StorageManager::itemUpdated, this, &MainWindow::onItemUpdated);
}

MainWindow::~MainWindow() {}

void MainWindow::setupUi() {
    setWindowTitle("KMagMux");
    resize(1000, 600);

    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout *layout = new QVBoxLayout(centralWidget);

    m_tabWidget = new QTabWidget(this);
    layout->addWidget(m_tabWidget);

    auto setupView = [this](QTableView *view, ItemModel *model, const QString &title) {
        view->setModel(model);
        view->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        view->setSelectionBehavior(QAbstractItemView::SelectRows);
        view->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(view, &QTableView::customContextMenuRequested, this, &MainWindow::onCustomContextMenuRequested);
        m_tabWidget->addTab(view, title);
    };

    // Unprocessed Tab
    m_unprocessedView = new QTableView(this);
    m_unprocessedModel = new ItemModel(this);
    setupView(m_unprocessedView, m_unprocessedModel, "Unprocessed");

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
                queueItems.push_back(item);
                break;
            case ItemState::Archived:
            case ItemState::Dispatched:
            case ItemState::Failed:
                archiveItems.push_back(item);
                break;
            default:
                break; // Should probably handle hold
        }
    }

    m_unprocessedModel->setItems(unprocessedItems);
    m_queueModel->setItems(queueItems);
    m_archiveModel->setItems(archiveItems);
}

QTableView* MainWindow::getCurrentView() const {
    return qobject_cast<QTableView*>(m_tabWidget->currentWidget());
}

ItemModel* MainWindow::getCurrentModel() const {
    QTableView *view = getCurrentView();
    if (view) {
        return qobject_cast<ItemModel*>(view->model());
    }
    return nullptr;
}

void MainWindow::onCustomContextMenuRequested(const QPoint &pos) {
    QTableView *view = getCurrentView();
    if (!view) return;

    QModelIndex index = view->indexAt(pos);
    if (!index.isValid()) return;

    QMenu menu(this);
    QAction *queueAction = menu.addAction("Queue");
    QAction *holdAction = menu.addAction("Hold");
    QAction *archiveAction = menu.addAction("Archive");

    // Check which tab we are on to disable redundant actions
    // (This logic can be more sophisticated)

    connect(queueAction, &QAction::triggered, this, [this](){ onItemAction(ItemState::Queued); });
    connect(holdAction, &QAction::triggered, this, [this](){ onItemAction(ItemState::Held); });
    connect(archiveAction, &QAction::triggered, this, [this](){ onItemAction(ItemState::Archived); });

    menu.exec(view->viewport()->mapToGlobal(pos));
}

void MainWindow::onItemAction(ItemState newState) {
    QTableView *view = getCurrentView();
    if (!view) return;

    ItemModel *model = getCurrentModel();
    if (!model) return;

    QModelIndexList selection = view->selectionModel()->selectedRows();
    if (selection.isEmpty()) return;

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

void MainWindow::onItemAdded(const Item &item) {
    // Determine which model gets the new item
    // In a real app, we might optimize by just inserting into the model directly
    // instead of reloading everything, but loadData() is safer for consistency for now
    loadData();
}

void MainWindow::onItemUpdated(const Item &item) {
    loadData();
}
