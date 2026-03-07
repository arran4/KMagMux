#ifndef PREFERENCESDIALOG_H
#define PREFERENCESDIALOG_H

#include <QDialog>

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
};

#endif // PREFERENCESDIALOG_H
