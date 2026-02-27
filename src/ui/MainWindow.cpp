#include "MainWindow.h"
#include "AddItemDialog.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QDebug>
#include <QPushButton>
#include <QInputDialog>
#include <QMessageBox>
#include <QDateTime>

MainWindow::MainWindow(StorageManager *storage, QWidget *parent)
    : QMainWindow(parent), m_storage(storage) {
    setupUi();
    loadData();

    m_engine = new Engine(m_storage, this);
    m_engine->start();

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

    QPushButton *addBtn = new QPushButton("Add Torrent/Magnet...", this);
    connect(addBtn, &QPushButton::clicked, this, &MainWindow::onAddItem);
    layout->addWidget(addBtn);

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
    connect(m_unprocessedView, &QTableView::doubleClicked, this, &MainWindow::onProcessItem);

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

    // If we are in the Unprocessed view, offer a "Process..." action
    if (view == m_unprocessedView) {
        QAction *processAction = menu.addAction("Process...");
        connect(processAction, &QAction::triggered, this, &MainWindow::onProcessItem);
        menu.addSeparator();
    }

    QAction *queueAction = menu.addAction("Queue");
    QAction *holdAction = menu.addAction("Hold");
    QAction *archiveAction = menu.addAction("Archive");

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
    loadData();
}

void MainWindow::onItemUpdated(const Item &item) {
    loadData();
}

void MainWindow::onAddItem() {
    bool ok;
    QString text = QInputDialog::getText(this, tr("Add Item"),
                                         tr("Enter Magnet Link or File Path:"), QLineEdit::Normal,
                                         "", &ok);
    if (ok && !text.isEmpty()) {
        Item newItem;
        newItem.id = QString::number(QDateTime::currentMSecsSinceEpoch()) + "_manual";
        newItem.state = ItemState::Unprocessed;
        newItem.sourcePath = text;
        newItem.createdTime = QDateTime::currentDateTime();

        // Open the dialog immediately for the new item
        openProcessDialog(newItem);
    }
}

void MainWindow::onProcessItem() {
    QTableView *view = getCurrentView();
    if (!view) return;

    ItemModel *model = getCurrentModel();
    if (!model) return;

    QModelIndexList selection = view->selectionModel()->selectedRows();
    if (selection.isEmpty()) return;

    int row = selection.first().row();
    Item item = model->getItem(row);

    openProcessDialog(item);
}

void MainWindow::openProcessDialog(Item &item) {
    AddItemDialog dialog(item, this);
    if (dialog.exec() == QDialog::Accepted) {
        Item updatedItem = dialog.getItem();

        bool success = true;

        if (dialog.shouldDeleteOriginal()) {
             // Move logic
             if (!m_storage->moveToManaged(updatedItem, true)) {
                 QMessageBox::warning(this, "Error", "Failed to move original file to managed storage.");
                 success = false; // Should we abort saving? Maybe just warn.
             }
        } else {
             // Just save. If it's a new item, we should save it anyway.
        }

        if (success) {
            if (m_storage->saveItem(updatedItem)) {
                 qDebug() << "Item processed and saved:" << updatedItem.id;
            } else {
                 QMessageBox::warning(this, "Error", "Failed to save item metadata.");
            }
        }
    }
}
