#ifndef LOCALPROGRAMCONNECTOR_H
#define LOCALPROGRAMCONNECTOR_H

#include "core/Connector.h"
#include <QObject>
#include <QtPlugin>

class LocalProgramConnector : public QObject, public Connector {
  Q_OBJECT
  Q_INTERFACES(Connector)
  Q_PLUGIN_METADATA(IID "com.kmagmux.Connector/1.0" FILE "localprogram.json")

public:
  LocalProgramConnector();
  explicit LocalProgramConnector(QObject *parent);

  QString getId() const override;
  QString getName() const override;
  void dispatch(const Item &item) override;
  bool isEnabled() const override;

  bool hasSettings() const override;
  QWidget *createSettingsWidget(QWidget *parent) override;
  void saveSettings(QWidget *settingsWidget) override;

private:
  bool m_enabled;
  QString m_programPath;

signals:
  void dispatchFinished(const QString &itemId, bool success,
                        const QString &message,
                        const QJsonObject &metadata = QJsonObject());
};

#endif // LOCALPROGRAMCONNECTOR_H
