#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTabWidget>
#include <QTableView>
#include <QMenu>
#include <QAction>
#include "../core/ItemModel.h"
#include "../core/StorageManager.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(StorageManager *storage, QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onCustomContextMenuRequested(const QPoint &pos);
    void onItemAction(ItemState newState);
    void onItemAdded(const Item &item);
    void onItemUpdated(const Item &item);

private:
    StorageManager *m_storage;
    QTabWidget *m_tabWidget;

    // Models
    ItemModel *m_unprocessedModel;
    ItemModel *m_queueModel;
    ItemModel *m_archiveModel;

    // Views
    QTableView *m_unprocessedView;
    QTableView *m_queueView;
    QTableView *m_archiveView;

    void setupUi();
    void loadData();
    QTableView* getCurrentView() const;
    ItemModel* getCurrentModel() const;
};

#endif // MAINWINDOW_H
