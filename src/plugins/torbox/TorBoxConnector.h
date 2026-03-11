#ifndef TORBOXCONNECTOR_H
#define TORBOXCONNECTOR_H

#include "core/Connector.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QtPlugin>

class TorBoxConnector : public QObject, public Connector {
  Q_OBJECT
  Q_INTERFACES(Connector)
  Q_PLUGIN_METADATA(IID "com.kmagmux.Connector/1.0" FILE "torbox.json")

public:
  TorBoxConnector();
  explicit TorBoxConnector(QObject *parent);
  QString getId() const override;
  QString getName() const override;
  void dispatch(const Item &item) override;
  bool isEnabled() const override;

  bool hasSettings() const override;
  QWidget *createSettingsWidget(QWidget *parent) override;
  void saveSettings(QWidget *settingsWidget) override;

  bool hasDebugMenu() const override;
  QList<HttpApiEndpoint> getHttpApiEndpoints() const override;
  QMap<QString, QString> getApiSubstitutions() const override;

private slots:
  void onAddTorrentReply();

private:
  QNetworkAccessManager *m_networkManager;
  QString m_apiToken;
  bool m_enabled;

signals:
  void dispatchFinished(const QString &itemId, bool success,
                        const QString &message);
};

#endif // TORBOXCONNECTOR_H
