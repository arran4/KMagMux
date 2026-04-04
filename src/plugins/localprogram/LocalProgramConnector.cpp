#include "LocalProgramConnector.h"
#include <QCheckBox>
#include <QDebug>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QProcess>
#include <QSettings>
#include <QVBoxLayout>

LocalProgramConnector::LocalProgramConnector()
    : LocalProgramConnector(nullptr) {}

LocalProgramConnector::LocalProgramConnector(QObject *parent)
    : QObject(parent) {
  QSettings settings;
  settings.beginGroup("Plugins/LocalProgram");
  m_enabled = settings.value("enabled", false).toBool();
  m_programPath = settings.value("programPath", "ktorrent").toString();
  settings.endGroup();
}

QString LocalProgramConnector::getId() const { return "LocalProgram"; }

QString LocalProgramConnector::getName() const { return "Local Program"; }

bool LocalProgramConnector::isEnabled() const { return m_enabled; }

void LocalProgramConnector::dispatch(const Item &item) {
  if (m_programPath.isEmpty()) {
    emit dispatchFinished(item.id, false, "Program path is not configured.");
    return;
  }

  QStringList args;
  args << item.sourcePath;

  qDebug() << "Launching local program:" << m_programPath
           << "with arguments:" << args;

  bool success = QProcess::startDetached(m_programPath, args);
  if (success) {
    emit dispatchFinished(item.id, true,
                          "Dispatched successfully to local program.");
  } else {
    emit dispatchFinished(item.id, false,
                          "Failed to launch local program: " + m_programPath);
  }
}

bool LocalProgramConnector::hasSettings() const { return true; }

QWidget *LocalProgramConnector::createSettingsWidget(QWidget *parent) {
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

  QLineEdit *programEdit = new QLineEdit(configWidget);
  programEdit->setObjectName("programEdit");
  programEdit->setText(settings.value("programPath", "ktorrent").toString());
  programEdit->setToolTip(
      "Executable name (e.g. ktorrent, qbittorrent) or absolute path.");
  configLayout->addRow(tr("Program Executable:"), programEdit);

  mainLayout->addWidget(configWidget);
  settings.endGroup();

  configWidget->setVisible(enabledCheck->isChecked());
  connect(enabledCheck, &QCheckBox::toggled, configWidget,
          &QWidget::setVisible);

  return widget;
}

void LocalProgramConnector::saveSettings(QWidget *settingsWidget) {
  if (!settingsWidget)
    return;

  QCheckBox *enabledCheck =
      settingsWidget->findChild<QCheckBox *>("enabledCheck");
  QLineEdit *programEdit =
      settingsWidget->findChild<QLineEdit *>("programEdit");

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

  settings.endGroup();
}
