#ifndef QBITTORRENTCONNECTOR_H
#define QBITTORRENTCONNECTOR_H

#include "core/Connector.h"
#include <QList>
#include <QNetworkAccessManager>
#include <QNetworkCookieJar>
#include <QNetworkReply>
#include <QObject>
#include <QUrl>
#include <QtPlugin>

class QBittorrentConnector : public QObject, public Connector {
  Q_OBJECT
  Q_INTERFACES(Connector)
  Q_PLUGIN_METADATA(IID "com.kmagmux.Connector/1.0" FILE "qbittorrent.json")

public:
  QBittorrentConnector();
  explicit QBittorrentConnector(QObject *parent);
  QString getId() const override;
  QString getName() const override;
  void dispatch(const Item &item) override;
  bool isEnabled() const override;

  // Configuration setters
  void setBaseUrl(const QString &url);
  void setCredentials(const QString &username, const QString &password);

  bool hasSettings() const override;
  QWidget *createSettingsWidget(QWidget *parent) override;
  void saveSettings(QWidget *settingsWidget) override;

  bool hasDebugMenu() const override;
  QList<HttpApiEndpoint> getHttpApiEndpoints() const override;
  QMap<QString, QString> getApiSubstitutions() const override;

private slots:
  void onLoginReply();
  void onAddTorrentReply();

private:
  QNetworkAccessManager *m_networkManager;
  QString m_baseUrl;
  QString m_username;
  QString m_password;
  bool m_enabled;

  // We store pending items while waiting for login
  QList<Item> m_pendingItems;
  bool m_isLoggingIn;

  void login();
  void performDispatch(const Item &item);

signals:
  void dispatchFinished(const QString &itemId, bool success,
                        const QString &message, const QJsonObject &metadata = QJsonObject());
};

#endif // QBITTORRENTCONNECTOR_H
