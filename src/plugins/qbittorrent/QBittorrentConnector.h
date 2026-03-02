#ifndef QBITTORRENTCONNECTOR_H
#define QBITTORRENTCONNECTOR_H

#include "core/Connector.h"
#include <QNetworkAccessManager>
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

  // Configuration setters
  void setBaseUrl(const QString &url);
  void setCredentials(const QString &username, const QString &password);

private slots:
  void onLoginReply();
  void onAddTorrentReply();

private:
  QNetworkAccessManager *m_networkManager;
  QString m_baseUrl;
  QString m_username;
  QString m_password;

  // We store the pending item while waiting for login
  Item m_pendingItem;
  bool m_isPending;

  void login();
  void performDispatch(const Item &item);

signals:
  void dispatchFinished(const QString &itemId, bool success,
                        const QString &message);
};

#endif // QBITTORRENTCONNECTOR_H
