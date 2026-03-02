#include "PreferencesDialog.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QStackedWidget>
#include <QTableWidget>
#include <QVBoxLayout>

PreferencesDialog::PreferencesDialog(QWidget *parent) : QDialog(parent) {
  setWindowTitle(tr("Preferences"));
  resize(600, 400);

  m_categoriesList = new QListWidget(this);
  m_categoriesList->setViewMode(QListView::IconMode);
  m_categoriesList->setIconSize(QSize(48, 48));
  m_categoriesList->setMovement(QListView::Static);
  m_categoriesList->setMaximumWidth(120);
  m_categoriesList->setSpacing(12);

  m_pagesWidget = new QStackedWidget(this);

  createGeneralPage();
  createShortcutsPage();

  m_categoriesList->setCurrentRow(0);

  connect(m_categoriesList, &QListWidget::currentItemChanged, this,
          &PreferencesDialog::changePage);

  QHBoxLayout *horizontalLayout = new QHBoxLayout;
  horizontalLayout->addWidget(m_categoriesList);
  horizontalLayout->addWidget(m_pagesWidget, 1);

  m_buttonBox = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Apply | QDialogButtonBox::Cancel,
      this);
  connect(m_buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  mainLayout->addLayout(horizontalLayout);
  mainLayout->addWidget(m_buttonBox);
}

PreferencesDialog::~PreferencesDialog() {}

void PreferencesDialog::changePage(QListWidgetItem *current,
                                   QListWidgetItem *previous) {
  if (!current)
    current = previous;

  m_pagesWidget->setCurrentIndex(m_categoriesList->row(current));
}

void PreferencesDialog::createGeneralPage() {
  QWidget *page = new QWidget(this);
  QVBoxLayout *layout = new QVBoxLayout(page);
  QLabel *label = new QLabel(tr("General Settings"), page);
  layout->addWidget(label);
  layout->addStretch();
  m_pagesWidget->addWidget(page);

  QListWidgetItem *item = new QListWidgetItem(m_categoriesList);
  item->setIcon(QIcon::fromTheme("preferences-system"));
  item->setText(tr("General"));
  item->setTextAlignment(Qt::AlignHCenter);
  item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
}

void PreferencesDialog::createShortcutsPage() {
  QWidget *page = new QWidget(this);
  QVBoxLayout *layout = new QVBoxLayout(page);

  QTableWidget *table = new QTableWidget(3, 2, page);
  table->setHorizontalHeaderLabels({tr("Action"), tr("Shortcut")});
  table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
  table->horizontalHeader()->setSectionResizeMode(
      1, QHeaderView::ResizeToContents);
  table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  table->setSelectionBehavior(QAbstractItemView::SelectRows);
  table->verticalHeader()->setVisible(false);

  table->setItem(0, 0, new QTableWidgetItem(tr("Add Torrent/Magnet...")));
  table->setItem(0, 1, new QTableWidgetItem(tr("Ctrl+O")));

  table->setItem(1, 0, new QTableWidgetItem(tr("Preferences")));
  table->setItem(1, 1, new QTableWidgetItem(tr("Ctrl+,")));

  table->setItem(2, 0, new QTableWidgetItem(tr("Quit")));
  table->setItem(2, 1, new QTableWidgetItem(tr("Ctrl+Q")));

  layout->addWidget(table);
  m_pagesWidget->addWidget(page);

  QListWidgetItem *item = new QListWidgetItem(m_categoriesList);
  item->setIcon(QIcon::fromTheme("preferences-desktop-keyboard"));
  item->setText(tr("Shortcuts"));
  item->setTextAlignment(Qt::AlignHCenter);
  item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
}
