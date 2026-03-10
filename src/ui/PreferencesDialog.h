#ifndef PREFERENCESDIALOG_H
#define PREFERENCESDIALOG_H

#include <QDialog>

class QCheckBox;
class QComboBox;
class QListWidget;
class QListWidgetItem;
class QStackedWidget;
class QDialogButtonBox;

class Engine;

class PreferencesDialog : public QDialog {
  Q_OBJECT

public:
  explicit PreferencesDialog(Engine *engine, QWidget *parent = nullptr);
  ~PreferencesDialog();

private slots:
  void changePage(QListWidgetItem *current, QListWidgetItem *previous);

private:
  void createGeneralPage();
  void createShortcutsPage();
  void createPluginsPage();

  Engine *m_engine;

  QListWidget *m_categoriesList;
  QStackedWidget *m_pagesWidget;
  QDialogButtonBox *m_buttonBox;

  QCheckBox *m_closeToTrayCb;
  QCheckBox *m_minimizeToTrayCb;
  QCheckBox *m_autoStartCb;
  QCheckBox *m_autoMoveInboxCb;
  QComboBox *m_autoMoveInboxCombo;
  QCheckBox *m_allowPlaintextStorageCb;
};

#endif // PREFERENCESDIALOG_H
