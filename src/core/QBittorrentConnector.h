#ifndef QBITTORRENTCONNECTOR_H
#define QBITTORRENTCONNECTOR_H

#include "Connector.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrl>

class QBittorrentConnector : public Connector {
    Q_OBJECT

public:
    explicit QBittorrentConnector(QObject *parent = nullptr);
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
};

#endif // QBITTORRENTCONNECTOR_H
