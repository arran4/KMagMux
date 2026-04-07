#include "LocalProgramConnector.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QVBoxLayout>

LocalProgramConnector::LocalProgramConnector()
    : LocalProgramConnector(nullptr, "", "", true) {}

LocalProgramConnector::LocalProgramConnector(QObject *parent,
                                             const QString &path,
                                             const QString &name,
                                             bool isFactory)
    : QObject(parent), m_programPath(path), m_programName(name),
      m_isFactory(isFactory) {

  if (m_isFactory) {
    QSettings settings;
    settings.beginGroup("Plugins/LocalProgram");
    m_enabled = settings.value("enabled", true)
                    .toBool(); // Default true to allow auto-discovered to work
    m_programPath = settings.value("programPath", "").toString();
    m_useTerminal = settings.value("useTerminal", false).toBool();
    settings.endGroup();
  } else {
    m_enabled = true; // Auto-discovered sub-connectors are enabled by default
    m_useTerminal = false;
  }
}

QList<Connector *> LocalProgramConnector::getSubConnectors() {
  QList<Connector *> connectors;

  if (m_isFactory && m_enabled) {
    // Add the main configured/custom instance if a path is explicitly set in
    // settings
    if (!m_programPath.isEmpty()) {
      connectors.append(new LocalProgramConnector(parent(), m_programPath,
                                                  "Local Program (Custom)"));
    }

    // Discover and add an instance for every client found
    QStringList discoveredPaths = discoverClients();
    for (const QString &path : discoveredPaths) {
      QString baseName = QFileInfo(path).fileName();

      // Avoid duplicate if custom path is the same as discovered
      if (path != m_programPath) {
        connectors.append(new LocalProgramConnector(
            parent(), path, "Local: " + baseName, false));
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

QStringList LocalProgramConnector::discoverClients() {
  QStringList knownClients = {"qbittorrent",     "transmission-gtk",
                              "transmission-qt", "transmission-remote",
                              "deluge",          "deluge-gtk",
                              "ktorrent",        "fragments",
                              "biglybt",         "tribler",
                              "webtorrent",      "tixati",
                              "bitcomet",        "frostwire",
                              "halite",          "motrix",
                              "aria2c"};

  QStringList discovered;

  // Add system default option
  QString xdgOpenPath = findExecutable("xdg-open");
  if (!xdgOpenPath.isEmpty()) {
    discovered.append(xdgOpenPath);
  }

  for (const QString &client : knownClients) {
    QString path = findExecutable(client);
    if (!path.isEmpty()) {
      discovered.append(path);
    }
  }
  return discovered;
}

void LocalProgramConnector::dispatch(const Item &item) {
  if (m_programPath.isEmpty()) {
    emit dispatchFinished(item.id, false, "Program path is not configured.");
    return;
  }

  QString program = m_programPath;
  QStringList args;

  // Handle specific client variations if needed
  QString baseName = QFileInfo(program).fileName();

  if (m_useTerminal) {
    // Launch via terminal (e.g. xterm -e program args)
    QString terminal = findExecutable("xterm");
    if (terminal.isEmpty())
      terminal = findExecutable("gnome-terminal");
    if (terminal.isEmpty())
      terminal = findExecutable("konsole");

    if (!terminal.isEmpty()) {
      args.append("-e");
      args.append(program);
      if (baseName == "aria2c") {
        // aria2c might need specific args for torrent files/magnets
        // but standard passing often works
      }
      args.append(item.sourcePath);
      program = terminal;
    } else {
      qWarning()
          << "Requested terminal execution, but no terminal emulator found.";
      args << item.sourcePath;
    }
  } else {
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
  enabledCheck->setChecked(settings.value("enabled", false).toBool());
  mainLayout->addWidget(enabledCheck);

  QWidget *configWidget = new QWidget(widget);
  QFormLayout *configLayout = new QFormLayout(configWidget);

  QComboBox *discoveredCombo = new QComboBox(configWidget);
  discoveredCombo->setObjectName("discoveredCombo");

  QStringList discovered = discoverClients();
  discoveredCombo->addItem(tr("Custom/Manual Entry"), "");
  for (const QString &clientPath : discovered) {
    discoveredCombo->addItem(clientPath, clientPath);
  }

  QLineEdit *programEdit = new QLineEdit(configWidget);
  programEdit->setObjectName("programEdit");
  QString currentPath = settings.value("programPath", "").toString();
  programEdit->setText(currentPath);
  programEdit->setToolTip(
      "Executable name (e.g. ktorrent, qbittorrent) or absolute path.");

  // Sync combo box and line edit
  int index = discoveredCombo->findData(currentPath);
  if (index != -1) {
    discoveredCombo->setCurrentIndex(index);
  } else if (!currentPath.isEmpty()) {
    discoveredCombo->setCurrentIndex(0); // Custom
  }

  connect(discoveredCombo, &QComboBox::currentIndexChanged, configWidget,
          [programEdit, discoveredCombo](int idx) {
            if (idx > 0) {
              programEdit->setText(discoveredCombo->itemData(idx).toString());
            }
          });

  configLayout->addRow(tr("Discovered Clients:"), discoveredCombo);
  configLayout->addRow(tr("Program Executable:"), programEdit);

  QCheckBox *terminalCheck =
      new QCheckBox(tr("Run in Terminal Emulator"), configWidget);
  terminalCheck->setObjectName("terminalCheck");
  terminalCheck->setChecked(settings.value("useTerminal", false).toBool());
  configLayout->addRow("", terminalCheck);

  mainLayout->addWidget(configWidget);
  settings.endGroup();

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
  QLineEdit *programEdit =
      settingsWidget->findChild<QLineEdit *>("programEdit");
  QCheckBox *terminalCheck =
      settingsWidget->findChild<QCheckBox *>("terminalCheck");

  QSettings settings;
  settings.beginGroup("Plugins/LocalProgram");

  if (enabledCheck) {
    bool en = enabledCheck->isChecked();
    settings.setValue("enabled", en);
    m_enabled = en;
  }
  if (programEdit) {
    settings.setValue("programPath", programEdit->text());
    m_programPath = programEdit->text();
  }
  if (terminalCheck) {
    settings.setValue("useTerminal", terminalCheck->isChecked());
    m_useTerminal = terminalCheck->isChecked();
  }

  settings.endGroup();
}
