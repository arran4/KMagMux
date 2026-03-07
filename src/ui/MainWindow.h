#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "../core/Engine.h"
#include "../core/ItemModel.h"
#include "../core/StorageManager.h"
#include <QAction>
#include <QCloseEvent>
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

private slots:
  void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
  void toggleShowHide();
  void minimizeToTray();
  void quitApplication();

  void onCustomContextMenuRequested(const QPoint &pos);
  void onItemAction(ItemState newState);
  void onItemAdded(const Item &item);
  void onItemUpdated(const Item &item);
  void onProcessItem();
  void onAddItems();
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
  ItemModel *m_errorModel;

  // Views
  QTableView *m_unprocessedView;
  QTableView *m_queueView;
  QTableView *m_archiveView;
  QTableView *m_errorView;

  void setupUi();
  void loadData();
  QTableView *getCurrentView() const;
  ItemModel *getCurrentModel() const;
  void openProcessDialog(const std::vector<Item> &items);

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
