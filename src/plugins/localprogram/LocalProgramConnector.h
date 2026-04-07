#ifndef LOCALPROGRAMCONNECTOR_H
#define LOCALPROGRAMCONNECTOR_H

#include "core/Connector.h"
#include <QList>
#include <QObject>
#include <QtPlugin>

struct LocalClientConfig {
  QString name;
  QString path;
  bool enabled;
  bool useTerminal;
};

class LocalProgramConnector : public QObject, public Connector {
  Q_OBJECT
  Q_INTERFACES(Connector)
  Q_PLUGIN_METADATA(IID "com.kmagmux.Connector/1.0" FILE "localprogram.json")

public:
  LocalProgramConnector();
  explicit LocalProgramConnector(QObject *parent,
                                 const QString &path = QString(),
                                 const QString &name = QString(),
                                 bool useTerminal = false,
                                 bool isFactory = true);

  QString getId() const override;
  QString getName() const override;
  void dispatch(const Item &item) override;
  bool isEnabled() const override;

  QList<Connector *> getSubConnectors() override;

  bool hasSettings() const override;
  QWidget *createSettingsWidget(QWidget *parent) override;
  void saveSettings(QWidget *settingsWidget) override;

private:
  bool m_enabled;
  QString m_programPath;
  QString m_programName;
  bool m_useTerminal;
  bool m_isFactory;

  QList<LocalClientConfig> loadClientConfigs() const;

public:
  static QList<LocalClientConfig> discoverClients();
  static QString findExecutable(const QString &name);

signals:
  void dispatchFinished(const QString &itemId, bool success,
                        const QString &message,
                        const QJsonObject &metadata = QJsonObject());
};

#endif // LOCALPROGRAMCONNECTOR_H
