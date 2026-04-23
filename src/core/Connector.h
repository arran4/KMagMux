#ifndef CONNECTOR_H
#define CONNECTOR_H

#include "HttpApiEndpoint.h"
#include "Item.h"
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QString>

class Connector {
public:
  virtual ~Connector() = default;

  virtual QString getId() const = 0;
  virtual QString getName() const = 0;
  virtual void dispatch(const Item &item) = 0;

  virtual bool isEnabled() const { return true; }

  virtual QList<Connector *> getSubConnectors() { return {}; }

  virtual bool hasSettings() const { return false; }
  virtual class QWidget *createSettingsWidget(class QWidget * /*parent*/) {
    return nullptr;
  }
  virtual void saveSettings(class QWidget *settingsWidget) {}

  virtual bool hasDebugMenu() const { return false; }
  virtual QList<HttpApiEndpoint> getHttpApiEndpoints() const {
    return QList<HttpApiEndpoint>();
  }
  virtual QMap<QString, QString> getApiSubstitutions() const {
    return QMap<QString, QString>();
  }

  static QString buildApiCallLog(const QString &method,
                                 const QNetworkRequest &request,
                                 const QByteArray &body = QByteArray()) {
    QUrl url = request.url();
    if (!url.userInfo().isEmpty()) {
      url.setUserInfo("***:***");
    }
    url.setQuery(QString());

    QString log = QString("\n--- API Call ---\nMethod: %1\nURL: %2\nHeaders:\n")
                      .arg(method, url.toString());

    for (const QByteArray &header : request.rawHeaderList()) {
      const QString headerName = QString::fromUtf8(header).toLower();
      if (headerName == "authorization" || headerName == "cookie") {
        log += QString("  %1: ***\n").arg(QString::fromUtf8(header));
      } else {
        log += QString("  %1: %2\n")
                   .arg(QString::fromUtf8(header),
                        QString::fromUtf8(request.rawHeader(header)));
      }
    }

    if (!body.isEmpty()) {
      const QString contentType =
          request.header(QNetworkRequest::ContentTypeHeader).toString();
      if (contentType.contains("application/x-www-form-urlencoded")) {
        log += "Body: <application/x-www-form-urlencoded omitted>\n";
      } else if (!contentType.contains("multipart/form-data")) {
        log += QString("Body:\n%1\n").arg(QString::fromUtf8(body));
      } else {
        log += "Body: <multipart/form-data omitted>\n";
      }
    }
    log += "----------------\n";
    return log;
  }

  template <typename Func>
  static void handleBasicAddTorrentReply(QNetworkReply *reply,
                                         Func emitDispatchFinished) {
    if (reply == nullptr) {
      return;
    }

    const QString itemId = reply->property("itemId").toString();
    const QString apiCallLog = reply->property("apiCallLog").toString();

    if (reply->error() == QNetworkReply::NoError) {
      QJsonObject extraMeta;
      extraMeta["raw_response"] = QString::fromUtf8(reply->readAll());
      emitDispatchFinished(itemId, true, "Dispatched successfully.", extraMeta);
    } else {
      emitDispatchFinished(itemId, false,
                           "Network error: " + reply->errorString() +
                               apiCallLog,
                           QJsonObject());
    }

    reply->deleteLater();
  }
};

#define Connector_iid "com.kmagmux.Connector/1.0"
Q_DECLARE_INTERFACE(Connector, Connector_iid)

#endif // CONNECTOR_H
