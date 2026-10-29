#include "UnseenTabTracker.h"
#include "../core/ItemModel.h"
#include <QAbstractItemModel>
#include <QFont>
#include <QFontMetrics>
#include <QObject>
#include <QPaintEvent>
#include <QSet>
#include <QSize>
#include <QStyle>
#include <QStyleOptionTab>
#include <QStylePainter>
#include <QTabBar>
#include <QTabWidget>
#include <QWidget>
#include <QtGlobal>
#include <algorithm>

void UnseenTabBar::setEmphasized(int index, bool emphasized) {
  if (emphasized) {
    m_emphasized.insert(index);
  } else {
    m_emphasized.remove(index);
  }
  updateGeometry();
  update();
}

bool UnseenTabBar::isEmphasized(int index) const {
  return m_emphasized.contains(index);
}

QSize UnseenTabBar::tabSizeHint(int index) const {
  QSize size = QTabBar::tabSizeHint(index);
  QFont bold = font();
  bold.setBold(true);
  size.rwidth() +=
      std::max(0, QFontMetrics(bold).horizontalAdvance(tabText(index)) -
                      fontMetrics().horizontalAdvance(tabText(index)));
  return size;
}

void UnseenTabBar::paintEvent(QPaintEvent * /*event*/) {
  QStylePainter painter(this);
  for (int i = 0; i < count(); ++i) {
    QStyleOptionTab option;
    initStyleOption(&option, i);
    QFont tabFont = font();
    tabFont.setBold(isEmphasized(i));
    painter.setFont(tabFont);
    option.fontMetrics = QFontMetrics(tabFont);
    painter.drawControl(QStyle::CE_TabBarTab, option);
  }
}

UnseenTabWidget::UnseenTabWidget(QWidget *parent) : QTabWidget(parent) {
  setTabBar(new UnseenTabBar(this));
}

UnseenTabTracker::UnseenTabTracker(QTabWidget *tabs)
    : QObject(tabs), m_tabs(tabs) {
  connect(tabs, &QTabWidget::currentChanged, this, [this](int index) {
    for (auto &entry : m_entries) {
      if (m_tabs->indexOf(entry.page) == index) {
        entry.unseen.clear();
        present(entry);
      }
    }
  });
}

void UnseenTabTracker::track(int index, ItemModel *model) {
  Entry entry;
  entry.page = m_tabs->widget(index);
  entry.model = model;
  entry.title = m_tabs->tabText(index);
  for (int row = 0; row < model->rowCount(); ++row) {
    entry.previous.insert(model->getItem(row).id);
  }
  m_entries.push_back(entry);
  auto changed = [this, model]() {
    for (auto &tracked : m_entries) {
      if (tracked.model == model) {
        refresh(tracked);
      }
    }
  };
  connect(model, &QAbstractItemModel::modelReset, this, changed);
  connect(model, &QAbstractItemModel::rowsInserted, this, changed);
  connect(model, &QAbstractItemModel::rowsRemoved, this, changed);
  connect(model, &QAbstractItemModel::dataChanged, this, changed);
  present(m_entries.back());
}

void UnseenTabTracker::refresh(Entry &entry) {
  QSet<QString> current;
  for (int row = 0; row < entry.model->rowCount(); ++row) {
    current.insert(entry.model->getItem(row).id);
  }
  // Reset notifications describe whole snapshots, not arrivals. Diff
  // identities.
  entry.unseen.intersect(current);
  if (m_tabs->currentWidget() == entry.page) {
    entry.unseen.clear();
  } else {
    entry.unseen.unite(current - entry.previous);
  }
  entry.previous = current;
  present(entry);
}

void UnseenTabTracker::present(const Entry &entry) {
  const int index = m_tabs->indexOf(entry.page);
  if (index < 0) {
    return;
  }
  const auto count = entry.unseen.size();
  m_tabs->setTabText(index, count > 0
                                ? tr("%1 (%2 new)").arg(entry.title).arg(count)
                                : entry.title);
  m_tabs->setTabToolTip(index,
                        tr("%1: %2 unseen items").arg(entry.title).arg(count));
  if (auto *bar = dynamic_cast<UnseenTabBar *>(m_tabs->tabBar())) {
    bar->setEmphasized(index, count > 0);
  }
}

int UnseenTabTracker::unseenCount(int index) const {
  const auto entry =
      std::find_if(m_entries.begin(), m_entries.end(),
                   [this, index](const Entry &candidate) {
                     return m_tabs->indexOf(candidate.page) == index;
                   });
  return entry == m_entries.end() ? 0 : static_cast<int>(entry->unseen.size());
}
