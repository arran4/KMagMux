#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "../core/Engine.h"
#include "../core/ItemModel.h"
#include "../core/StorageManager.h"
#include "ItemFilterProxyModel.h"
#include <QAction>
#include <QCloseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEvent>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QStatusBar>
#include <QSystemTrayIcon>
#include <QTabWidget>
#include <QTableView>
#include <QToolBar>

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  explicit MainWindow(StorageManager *storage, QWidget *parent = nullptr);
  ~MainWindow();

protected:
  void closeEvent(QCloseEvent *event) override;
  void changeEvent(QEvent *event) override;
  void dragEnterEvent(QDragEnterEvent *event) override;
  void dropEvent(QDropEvent *event) override;

private slots:
  void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
  void toggleShowHide();
  void minimizeToTray();
  void quitApplication();

  void onCustomContextMenuRequested(const QPoint &pos);
  void onItemAction(ItemState newState);
  void onItemAdded(const Item &item);
  void onItemUpdated(const Item &item);
  void onItemsUpdated();
  void onItemDeleted(const QString &id);
  void onProcessItem();
  void onDeleteItems();
  void onAddItems();
  void onPreferences();
  void onAbout();
  void onToggleProcessing(bool checked);
  void onOpenCacheDirectory();
  void updateActionsState();

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

  // Proxy Models
  ItemFilterProxyModel *m_unprocessedProxy;
  ItemFilterProxyModel *m_queueProxy;
  ItemFilterProxyModel *m_doneProxy;
  ItemFilterProxyModel *m_archiveProxy;
  ItemFilterProxyModel *m_errorProxy;

  // Views
  QTableView *m_unprocessedView;
  QTableView *m_queueView;
  QTableView *m_doneView;
  QTableView *m_archiveView;
  QTableView *m_errorView;

  QAction *m_toggleProcessingAction;

  // List View and Item Actions
  QAction *m_selectAllAction;
  QAction *m_processAction;
  QAction *m_reprocessAction;
  QAction *m_dismissAction;
  QAction *m_queueAction;
  QAction *m_holdAction;
  QAction *m_archiveAction;
  QAction *m_deleteAction;

  void setupUi();
  void loadData();
  QTableView *getCurrentView() const;
  ItemModel *getCurrentModel() const;
  void openAddItemsDialog(const std::vector<Item> &items);
  void openProcessItemDialog(const std::vector<Item> &items);
  void saveItemsFromDialog(std::vector<Item> updatedItems);

  QSystemTrayIcon *m_trayIcon;
  QMenu *m_trayIconMenu;

  QAction *m_minimizeAction;
  QAction *m_showHideAction;
  QAction *m_quitAction;

  bool m_closeToTray;
  bool m_minimizeToTray;
  bool m_autoStart;
  bool m_forceQuit;

  void applySettings();
};

#endif // MAINWINDOW_H
