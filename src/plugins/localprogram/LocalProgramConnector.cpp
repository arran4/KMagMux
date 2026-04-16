#include "LocalProgramConnector.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QProcess>
#include <QPushButton>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>
#include <QTableWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <functional>

LocalProgramConnector::LocalProgramConnector()
    : LocalProgramConnector(nullptr, "", "", false, true) {}

LocalProgramConnector::LocalProgramConnector(QObject *parent,
                                             const QString &path,
                                             const QString &name,
                                             bool useTerminal, bool isFactory)
    : QObject(parent), m_programPath(path), m_programName(name),
      m_useTerminal(useTerminal), m_isFactory(isFactory) {

  if (m_isFactory) {
    QSettings settings;
    settings.beginGroup("Plugins/LocalProgram");
    // Local plugin factory is enabled by default as requested
    m_enabled = settings.value("enabled", true).toBool();
    settings.endGroup();
  } else {
    m_enabled = true; // Auto-discovered sub-connectors are always enabled if
                      // they reach here
  }
}

QList<LocalClientConfig> LocalProgramConnector::loadClientConfigs() const {
  QList<LocalClientConfig> configs;
  QSettings settings;
  settings.beginGroup("Plugins/LocalProgram");

  int count = settings.beginReadArray("Clients");
  for (int i = 0; i < count; ++i) {
    settings.setArrayIndex(i);
    LocalClientConfig c;
    c.name = settings.value("name").toString();
    c.path = settings.value("path").toString();
    c.enabled = settings.value("enabled", true).toBool();
    c.useTerminal = settings.value("useTerminal", false).toBool();
    configs.append(c);
  }
  settings.endArray();
  settings.endGroup();

  if (configs.isEmpty()) {
    // First run or empty, auto-discover
    configs = discoverClients();
  }

  return configs;
}

QList<Connector *> LocalProgramConnector::getSubConnectors() {
  QList<Connector *> connectors;

  if (m_isFactory && m_enabled) {
    QList<LocalClientConfig> configs = loadClientConfigs();

    for (const LocalClientConfig &c : configs) {
      if (c.enabled && !c.path.isEmpty()) {
        connectors.append(new LocalProgramConnector(
            parent(), c.path, "Local: " + c.name, c.useTerminal, false));
      }
    }
  }
  return connectors;
}

QString LocalProgramConnector::getId() const {
  if (m_isFactory)
    return "LocalProgramFactory";
  return "LocalProgram_" + m_programName;
}

QString LocalProgramConnector::getName() const {
  if (m_isFactory)
    return "Local Program (Settings)";
  return m_programName;
}

bool LocalProgramConnector::isEnabled() const { return m_enabled; }

QString LocalProgramConnector::findExecutable(const QString &name) {
  QString path = QStandardPaths::findExecutable(name);
  if (!path.isEmpty()) {
    return path;
  }
  return QString();
}

QList<LocalClientConfig> LocalProgramConnector::discoverClients() {
  QStringList knownClients = {"qbittorrent",     "transmission-gtk",
                              "transmission-qt", "transmission-remote",
                              "deluge",          "deluge-gtk",
                              "ktorrent",        "fragments",
                              "biglybt",         "tribler",
                              "webtorrent",      "tixati",
                              "bitcomet",        "frostwire",
                              "halite",          "motrix",
                              "aria2c"};

  QList<LocalClientConfig> discovered;

  // Add system default option
  QString xdgOpenPath = findExecutable("xdg-open");
  if (!xdgOpenPath.isEmpty()) {
    LocalClientConfig xdg;
    xdg.name = "System Default (xdg-open)";
    xdg.path = xdgOpenPath;
    xdg.enabled = true;
    xdg.useTerminal = false;
    discovered.append(xdg);
  }

  for (const QString &client : knownClients) {
    QString path = findExecutable(client);
    if (!path.isEmpty()) {
      LocalClientConfig c;
      c.name = client;
      c.path = path;
      c.enabled = true;
      c.useTerminal = false;
      discovered.append(c);
    }
  }
  return discovered;
}

void LocalProgramConnector::dispatch(const Item &item) {
  if (m_programPath.isEmpty()) {
    emit dispatchFinished(item.id, false, "Program path is not configured.");
    return;
  }

  QStringList userCommand = QProcess::splitCommand(m_programPath);
  if (userCommand.isEmpty()) {
    emit dispatchFinished(item.id, false, "Invalid program path configured.");
    return;
  }

  QString program = userCommand.takeFirst();
  QStringList args = userCommand; // Any remaining args from the user config

  // Security enhancement: Validate executable and arguments
  QString baseName = QFileInfo(program).fileName().toLower();
  if (baseName.endsWith(".exe")) {
    baseName.chop(4);
  }

  static const QStringList allowedPrograms = {
      "qbittorrent",     "transmission-gtk",
      "transmission-qt", "transmission-remote",
      "deluge",          "deluge-gtk",
      "ktorrent",        "fragments",
      "biglybt",         "tribler",
      "webtorrent",      "tixati",
      "bitcomet",        "frostwire",
      "halite",          "motrix",
      "aria2c",          "xdg-open"};

  if (!allowedPrograms.contains(baseName)) {
    emit dispatchFinished(item.id, false,
                          "Security Alert: Program '" + baseName +
                              "' is not in the allowed list of applications.");
    return;
  }

  // Prevent argument injection (e.g., executing arbitrary scripts via client
  // flags)
  static const QStringList dangerousFlags = {"--torrent-done-script",
                                             "-S",
                                             "--on-download-complete",
                                             "--on-download-start",
                                             "--on-download-pause",
                                             "--on-download-stop",
                                             "--on-download-error",
                                             "--on-bt-download-complete",
                                             "--execute",
                                             "-x",
                                             "-e"};
  auto it = std::find_if(args.begin(), args.end(), [](const QString &arg) {
    return std::any_of(
        dangerousFlags.begin(), dangerousFlags.end(),
        [&arg](const QString &flag) { return arg.startsWith(flag); });
  });

  if (it != args.end()) {
    emit dispatchFinished(item.id, false,
                          "Security Alert: Dangerous argument '" + *it +
                              "' is not allowed.");
    return;
  }

  if (m_useTerminal) {
    // Launch via terminal (e.g. xterm -e program args)
    QString terminal = findExecutable("xterm");
    if (terminal.isEmpty())
      terminal = findExecutable("gnome-terminal");
    if (terminal.isEmpty())
      terminal = findExecutable("konsole");

    if (!terminal.isEmpty()) {
      QString termBaseName = QFileInfo(terminal).fileName().toLower();

      QStringList commandArgs;
      commandArgs << program;
      commandArgs << args;
      if (baseName != "xdg-open")
        commandArgs << "--";
      commandArgs << item.sourcePath;

      QString commandString;
      for (const QString &arg : commandArgs) {
        QString escaped = arg;
        escaped.replace("'", "'\\''");
        commandString += " '" + escaped + "'";
      }
      commandString = commandString.trimmed();

      args.clear();
      if (termBaseName == "gnome-terminal") {
        args << "--" << "sh" << "-c" << commandString;
      } else {
        args << "-e" << "sh" << "-c" << commandString;
      }
      program = terminal;
    } else {
      qWarning()
          << "Requested terminal execution, but no terminal emulator found.";
      if (baseName != "xdg-open")
        args.append("--");
      args << item.sourcePath;
    }
  } else {
    if (baseName != "xdg-open")
      args.append("--");
    args << item.sourcePath;
  }

  qDebug() << "Launching local program:" << program
           << "with arguments:" << args;

  bool success = QProcess::startDetached(program, args);
  if (success) {
    emit dispatchFinished(item.id, true,
                          "Dispatched successfully to local program (" +
                              program + ").");
  } else {
    emit dispatchFinished(item.id, false,
                          "Failed to launch local program: " + program);
  }
}

bool LocalProgramConnector::hasSettings() const { return m_isFactory; }

QWidget *LocalProgramConnector::createSettingsWidget(QWidget *parent) {
  if (!m_isFactory)
    return nullptr;
  QWidget *widget = new QWidget(parent);
  QVBoxLayout *mainLayout = new QVBoxLayout(widget);

  QSettings settings;
  settings.beginGroup("Plugins/LocalProgram");

  QCheckBox *enabledCheck =
      new QCheckBox(tr("Enable Local Program Routing"), widget);
  enabledCheck->setObjectName("enabledCheck");
  enabledCheck->setChecked(settings.value("enabled", true).toBool());
  mainLayout->addWidget(enabledCheck);

  QWidget *configWidget = new QWidget(widget);
  QVBoxLayout *configLayout = new QVBoxLayout(configWidget);

  QLabel *infoLabel = new QLabel(
      tr("Configure the local programs you want to be available as targets. "
         "Check 'Terminal' if the application is CLI-only."),
      configWidget);
  infoLabel->setWordWrap(true);
  configLayout->addWidget(infoLabel);

  QTableWidget *table = new QTableWidget(0, 4, configWidget);
  table->setObjectName("clientsTable");
  table->setHorizontalHeaderLabels(
      {tr("Enabled"), tr("Name"), tr("Executable / Path"), tr("Terminal")});
  table->horizontalHeader()->setSectionResizeMode(
      QHeaderView::ResizeToContents);
  table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
  table->setSelectionBehavior(QAbstractItemView::SelectRows);
  configLayout->addWidget(table);

  QHBoxLayout *btnLayout = new QHBoxLayout();
  QPushButton *addBtn = new QPushButton(tr("Add Custom"), configWidget);
  QPushButton *discoverBtn = new QPushButton(tr("Discover"), configWidget);
  QPushButton *removeBtn = new QPushButton(tr("Remove Selected"), configWidget);

  btnLayout->addWidget(addBtn);
  btnLayout->addWidget(discoverBtn);
  btnLayout->addWidget(removeBtn);
  btnLayout->addStretch();
  configLayout->addLayout(btnLayout);

  mainLayout->addWidget(configWidget);
  settings.endGroup();

  auto addRow = [table](const LocalClientConfig &c) {
    int row = table->rowCount();
    table->insertRow(row);

    QTableWidgetItem *enabledItem = new QTableWidgetItem();
    enabledItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
    enabledItem->setCheckState(c.enabled ? Qt::Checked : Qt::Unchecked);
    table->setItem(row, 0, enabledItem);

    QTableWidgetItem *nameItem = new QTableWidgetItem(c.name);
    table->setItem(row, 1, nameItem);

    QTableWidgetItem *pathItem = new QTableWidgetItem(c.path);
    table->setItem(row, 2, pathItem);

    QTableWidgetItem *termItem = new QTableWidgetItem();
    termItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
    termItem->setCheckState(c.useTerminal ? Qt::Checked : Qt::Unchecked);
    table->setItem(row, 3, termItem);
  };

  QList<LocalClientConfig> configs = loadClientConfigs();
  for (const LocalClientConfig &c : configs) {
    addRow(c);
  }

  connect(addBtn, &QPushButton::clicked, widget, [addRow]() {
    LocalClientConfig c;
    c.name = "New Program";
    c.path = "/path/to/binary";
    c.enabled = true;
    c.useTerminal = false;
    addRow(c);
  });

  connect(discoverBtn, &QPushButton::clicked, widget, [table, addRow]() {
    QList<LocalClientConfig> newlyDiscovered =
        LocalProgramConnector::discoverClients();
    for (const auto &newC : newlyDiscovered) {
      // Check if path already exists in table to avoid duplicates
      bool found = false;
      for (int i = 0; i < table->rowCount(); ++i) {
        if (table->item(i, 2)->text() == newC.path) {
          found = true;
          break;
        }
      }
      if (!found) {
        addRow(newC);
      }
    }
  });

  connect(removeBtn, &QPushButton::clicked, widget, [table]() {
    QList<QTableWidgetItem *> selected = table->selectedItems();
    if (!selected.isEmpty()) {
      // Get unique rows to delete
      QSet<int> uniqueRows;
      for (auto *item : selected) {
        uniqueRows.insert(item->row());
      }
      QList<int> rows = uniqueRows.values();
      // Sort descending to avoid index shifting issues
      std::sort(rows.begin(), rows.end(), std::greater<int>());
      for (int row : rows) {
        table->removeRow(row);
      }
    }
  });

  configWidget->setVisible(enabledCheck->isChecked());
  connect(enabledCheck, &QCheckBox::toggled, configWidget,
          &QWidget::setVisible);

  return widget;
}

void LocalProgramConnector::saveSettings(QWidget *settingsWidget) {
  if (!m_isFactory || !settingsWidget)
    return;

  QCheckBox *enabledCheck =
      settingsWidget->findChild<QCheckBox *>("enabledCheck");
  QTableWidget *table =
      settingsWidget->findChild<QTableWidget *>("clientsTable");

  QSettings settings;
  settings.beginGroup("Plugins/LocalProgram");

  if (enabledCheck) {
    bool en = enabledCheck->isChecked();
    settings.setValue("enabled", en);
    m_enabled = en;
  }

  if (table) {
    settings.beginWriteArray("Clients");
    for (int i = 0; i < table->rowCount(); ++i) {
      settings.setArrayIndex(i);

      bool itemEnabled = (table->item(i, 0)->checkState() == Qt::Checked);
      QString name = table->item(i, 1)->text();
      QString path = table->item(i, 2)->text();
      bool useTerm = (table->item(i, 3)->checkState() == Qt::Checked);

      settings.setValue("enabled", itemEnabled);
      settings.setValue("name", name);
      settings.setValue("path", path);
      settings.setValue("useTerminal", useTerm);
    }
    settings.endArray();
  }

  settings.endGroup();
}
