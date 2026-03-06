#ifndef PREFERENCESDIALOG_H
#define PREFERENCESDIALOG_H

#include <QDialog>

class QCheckBox;
class QListWidget;
class QListWidgetItem;
class QStackedWidget;
class QDialogButtonBox;

class PreferencesDialog : public QDialog {
  Q_OBJECT

public:
  explicit PreferencesDialog(QWidget *parent = nullptr);
  ~PreferencesDialog();

private slots:
  void changePage(QListWidgetItem *current, QListWidgetItem *previous);

private:
  void createGeneralPage();
  void createShortcutsPage();

  QListWidget *m_categoriesList;
  QStackedWidget *m_pagesWidget;
  QDialogButtonBox *m_buttonBox;

  QCheckBox *m_closeToTrayCb;
  QCheckBox *m_autoStartCb;
};

#endif // PREFERENCESDIALOG_H
