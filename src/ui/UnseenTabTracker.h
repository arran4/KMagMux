#ifndef UNSEENTABTRACKER_H
#define UNSEENTABTRACKER_H

#include <QObject>
#include <QPaintEvent>
#include <QPointer>
#include <QSet>
#include <QSize>
#include <QString>
#include <QTabBar>
#include <QTabWidget>
#include <QWidget>
#include <QtCore/qtmetamacros.h>
#include <vector>

class ItemModel;

// Native tab painting with per-tab font emphasis.
class UnseenTabBar : public QTabBar {
public:
  using QTabBar::QTabBar;
  void setEmphasized(int index, bool emphasized);
  bool isEmphasized(int index) const;

protected:
  void paintEvent(QPaintEvent *event) override;
  QSize tabSizeHint(int index) const override;

private:
  QSet<int> m_emphasized;
};

class UnseenTabWidget : public QTabWidget {
public:
  explicit UnseenTabWidget(QWidget *parent = nullptr);
};

class UnseenTabTracker : public QObject {
  Q_OBJECT
public:
  explicit UnseenTabTracker(QTabWidget *tabs);
  // Call after initial population to establish the startup baseline.
  void track(int index, ItemModel *model);
  int unseenCount(int index) const;

private:
  struct Entry {
    QPointer<QWidget> page;
    QPointer<ItemModel> model;
    QString title;
    QSet<QString> previous;
    QSet<QString> unseen;
  };
  void refresh(Entry &entry);
  void present(const Entry &entry);
  QTabWidget *m_tabs;
  std::vector<Entry> m_entries;
};

#endif
