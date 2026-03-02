#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTabWidget>
#include <QTableView>
#include <QMenu>
#include <QAction>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include "../core/ItemModel.h"
#include "../core/StorageManager.h"
#include "../core/Engine.h"

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
    void onProcessItem();
    void onAddItem();
    void onPreferences();
    void onAbout();

private:
    StorageManager *m_storage;
    Engine *m_engine;
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
    void openProcessDialog(Item &item);
};

#endif // MAINWINDOW_H
