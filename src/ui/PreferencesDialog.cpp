#include "PreferencesDialog.h"

#include "../core/Connector.h"
#include "../core/Engine.h"
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTableWidget>
#include <QVBoxLayout>

PreferencesDialog::PreferencesDialog(Engine *engine, QWidget *parent)
    : QDialog(parent), m_engine(engine) {
  setWindowTitle(tr("Preferences"));
  resize(800, 600);

  m_categoriesList = new QListWidget(this);
  m_categoriesList->setViewMode(QListView::IconMode);
  m_categoriesList->setIconSize(QSize(48, 48));
  m_categoriesList->setMovement(QListView::Static);
  m_categoriesList->setMaximumWidth(160);
  m_categoriesList->setSpacing(12);

  m_pagesWidget = new QStackedWidget(this);

  createGeneralPage();
  createShortcutsPage();
  createPluginsPage();

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

  table->setItem(0, 0, new QTableWidgetItem(tr("Add Item(s)...")));
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

void PreferencesDialog::createPluginsPage() {
  QWidget *page = new QWidget(this);
  QVBoxLayout *layout = new QVBoxLayout(page);

  QScrollArea *scrollArea = new QScrollArea(page);
  scrollArea->setWidgetResizable(true);
  scrollArea->setFrameShape(QFrame::NoFrame);

  QWidget *scrollContent = new QWidget(scrollArea);
  QVBoxLayout *scrollLayout = new QVBoxLayout(scrollContent);

  if (m_engine) {
    QStringList connectors = m_engine->getAvailableConnectors();
    if (connectors.isEmpty()) {
      QLabel *noPluginsLabel = new QLabel(
          tr("No plugins found. Ensure plugins are compiled into the 'plugins' "
             "directory.\nLooking in: ") +
              QCoreApplication::applicationDirPath() + "/plugins and " +
              QCoreApplication::applicationDirPath() + "/../plugins",
          scrollContent);
      noPluginsLabel->setWordWrap(true);
      noPluginsLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
      scrollLayout->addWidget(noPluginsLabel);
    } else {
      for (const QString &id : connectors) {
        Connector *connector = m_engine->getConnector(id);
        if (connector) {
          QGroupBox *groupBox =
              new QGroupBox(connector->getName(), scrollContent);
          QVBoxLayout *groupLayout = new QVBoxLayout(groupBox);

          if (connector->hasSettings()) {
            QWidget *settingsWidget = connector->createSettingsWidget(groupBox);
            if (settingsWidget) {
              groupLayout->addWidget(settingsWidget);

              // We need to save settings when OK/Apply is clicked.
              // We can connect to the buttonBox accepted signal to call
              // saveSettings.
              connect(m_buttonBox, &QDialogButtonBox::accepted, this,
                      [connector, settingsWidget]() {
                        connector->saveSettings(settingsWidget);
                      });
            } else {
              groupLayout->addWidget(
                  new QLabel(tr("Settings widget creation failed.")));
            }
          } else {
            groupLayout->addWidget(new QLabel(tr("No configurable settings.")));
          }

          scrollLayout->addWidget(groupBox);
        }
      }
    }
  } else {
    QLabel *errorLabel = new QLabel(tr("Engine not available."), scrollContent);
    scrollLayout->addWidget(errorLabel);
  }

  scrollLayout->addStretch();
  scrollArea->setWidget(scrollContent);
  layout->addWidget(scrollArea);

  m_pagesWidget->addWidget(page);

  QListWidgetItem *item = new QListWidgetItem(m_categoriesList);
  item->setIcon(QIcon::fromTheme("preferences-plugin"));
  item->setText(tr("Plugins"));
  item->setTextAlignment(Qt::AlignHCenter);
  item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
}
