#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "../core/Engine.h"
#include "../core/ItemModel.h"
#include "../core/StorageManager.h"
#include <QAction>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableView>
#include <QToolBar>

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
  void onItemDeleted(const QString &id);
  void onProcessItem();
  void onDeleteItems();
  void onAddItems();
  void onPreferences();
  void onAbout();
  void onToggleProcessing(bool checked);
  void onOpenCacheDirectory();

private:
  StorageManager *m_storage;
  Engine *m_engine;
  QTabWidget *m_tabWidget;

  // Models
  ItemModel *m_unprocessedModel;
  ItemModel *m_queueModel;
  ItemModel *m_doneModel;
  ItemModel *m_archiveModel;
  ItemModel *m_errorModel;

  // Views
  QTableView *m_unprocessedView;
  QTableView *m_queueView;
  QTableView *m_doneView;
  QTableView *m_archiveView;
  QTableView *m_errorView;

  QAction *m_toggleProcessingAction;

  void setupUi();
  void loadData();
  QTableView *getCurrentView() const;
  ItemModel *getCurrentModel() const;
  void openAddItemsDialog(const std::vector<Item> &items);
  void openProcessItemDialog(const std::vector<Item> &items);
};

#endif // MAINWINDOW_H
