#include "RealDebridConnector.h"
#include "core/SecureStorage.h"
#include <QCheckBox>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QSettings>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>
#include <QWidget>
#include <numeric>

RealDebridConnector::RealDebridConnector() : RealDebridConnector(nullptr) {}

RealDebridConnector::RealDebridConnector(QObject *parent)
    : QObject(parent), m_networkManager(new QNetworkAccessManager(this)),
      m_apiToken(""), m_enabled(false) {
  QSettings settings;
  settings.beginGroup("Plugins/RealDebrid");
  m_enabled = settings.value("enabled", false).toBool();
  settings.endGroup();

  m_apiToken = SecureStorage::readPassword("Plugins/RealDebrid", "apiToken");
}

QString RealDebridConnector::getId() const { return "RealDebrid"; }

QString RealDebridConnector::getName() const { return "Real-Debrid"; }

bool RealDebridConnector::isEnabled() const { return m_enabled; }

void RealDebridConnector::dispatch(const Item &item) {
  if (m_apiToken.isEmpty()) {
    emit dispatchFinished(item.id, false, "API Token is missing.");
    return;
  }

  if (item.sourcePath.startsWith("magnet:")) {
    const QUrl url("https://api.real-debrid.com/rest/1.0/torrents/addMagnet");
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", ("Bearer " + m_apiToken).toUtf8());
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      "application/x-www-form-urlencoded");

    QUrlQuery postData;
    postData.addQueryItem("magnet", item.sourcePath);

    const QByteArray body = postData.toString(QUrl::FullyEncoded).toUtf8();
    const QString apiCallLog =
        Connector::buildApiCallLog("POST", request, body);

    QNetworkReply *reply = m_networkManager->post(request, body);
    reply->setProperty("itemId", item.id);
    reply->setProperty("apiCallLog", apiCallLog);
    connect(reply, &QNetworkReply::finished, this,
            &RealDebridConnector::onAddTorrentReply);

  } else {
    const QUrl url("https://api.real-debrid.com/rest/1.0/torrents/addTorrent");
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", ("Bearer " + m_apiToken).toUtf8());

    QFile file(item.sourcePath);
    if (!file.open(QIODevice::ReadOnly)) {
      emit dispatchFinished(item.id, false,
                            "Could not open torrent file: " + item.sourcePath);
      return;
    }

    const QByteArray body = file.readAll();
    file.close();

    // Since addTorrent accepts raw bytes in PUT body
    const QString apiCallLog = Connector::buildApiCallLog("PUT", request, body);

    QNetworkReply *reply = m_networkManager->put(request, body);
    reply->setProperty("itemId", item.id);
    reply->setProperty("apiCallLog", apiCallLog);
    connect(reply, &QNetworkReply::finished, this,
            &RealDebridConnector::onAddTorrentReply);
  }
}

void RealDebridConnector::onAddTorrentReply() {
  QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
  if (reply == nullptr) {
    return;
  }

  const QString itemId = reply->property("itemId").toString();
  const QString apiCallLog = reply->property("apiCallLog").toString();
  const QByteArray responseBody = reply->readAll();

  if (reply->error() == QNetworkReply::NoError) {
    QJsonParseError parseError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(responseBody, &parseError);

    if (parseError.error == QJsonParseError::NoError && jsonDoc.isObject()) {
      QJsonObject jsonObj = jsonDoc.object();
      QString torrentId = jsonObj.value("id").toString();

      // After adding, we need to select the files to start the download
      if (!torrentId.isEmpty()) {
        QJsonObject extraMeta;
        extraMeta["raw_response"] = QString::fromUtf8(responseBody);

        // Optionally call selectFiles immediately or let user do it
        // Real-Debrid requires selecting files to actually start.
        // For simplicity in dispatch, we just call selectFiles with "all"
        selectFiles(itemId, torrentId, apiCallLog, extraMeta);
        reply->deleteLater();
        return;
      }
    }

    QJsonObject extraMeta;
    extraMeta["raw_response"] = QString::fromUtf8(responseBody);
    emit dispatchFinished(itemId, true, "Dispatched successfully.", extraMeta);
  } else {
    QString errorMessage = "Network error: " + reply->errorString();
    QJsonObject extraMeta;

    QString rawHttp =
        "Request URL:\n" + reply->request().url().toString() + "\n\n";
    rawHttp += "Request Headers:\n";
    const auto reqHeaders = reply->request().rawHeaderList();
    for (const QByteArray &headerName : reqHeaders) {
      if (QString::fromUtf8(headerName).toLower() == "authorization") {
        rawHttp += QString::fromUtf8(headerName) + ": [REDACTED]\n";
      } else {
        rawHttp += QString::fromUtf8(headerName) + ": " +
                   QString::fromUtf8(reply->request().rawHeader(headerName)) +
                   "\n";
      }
    }

    const int statusCode =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    rawHttp +=
        "\nResponse Status Code: " + QString::number(statusCode) + "\n\n";

    rawHttp += "Response Headers:\n";
    const auto resHeaders = reply->rawHeaderList();
    std::for_each(resHeaders.begin(), resHeaders.end(),
                  [&rawHttp, &reply](const QByteArray &headerName) {
                    rawHttp += QString::fromUtf8(headerName) + ": " +
                               QString::fromUtf8(reply->rawHeader(headerName)) +
                               "\n";
                  });
    rawHttp += "\nResponse Body:\n" + QString::fromUtf8(responseBody) + "\n";

    extraMeta["raw_http"] = rawHttp;

#ifndef QT_NO_DEBUG
    QString shortBody = QString::fromUtf8(responseBody.left(500));
    if (statusCode > 0 || !shortBody.isEmpty()) {
      errorMessage +=
          QString(" (Status: %1, Body: %2)").arg(statusCode).arg(shortBody);
    }
#endif
    emit dispatchFinished(itemId, false, errorMessage + apiCallLog, extraMeta);
  }

  reply->deleteLater();
}

void RealDebridConnector::selectFiles(const QString &itemId,
                                      const QString &torrentId,
                                      const QString &apiCallLog,
                                      const QJsonObject &extraMeta) {
  const QUrl url("https://api.real-debrid.com/rest/1.0/torrents/selectFiles/" +
                 torrentId);
  QNetworkRequest request(url);
  request.setRawHeader("Authorization", ("Bearer " + m_apiToken).toUtf8());
  request.setHeader(QNetworkRequest::ContentTypeHeader,
                    "application/x-www-form-urlencoded");

  QUrlQuery postData;
  postData.addQueryItem("files", "all");
  const QByteArray body = postData.toString(QUrl::FullyEncoded).toUtf8();

  const QString selectApiCallLog =
      Connector::buildApiCallLog("POST", request, body);

  QNetworkReply *reply = m_networkManager->post(request, body);
  reply->setProperty("itemId", itemId);
  reply->setProperty("apiCallLog", apiCallLog + "\n" + selectApiCallLog);
  reply->setProperty("extraMeta", extraMeta);

  connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    const QString itemId = reply->property("itemId").toString();
    const QString fullApiCallLog = reply->property("apiCallLog").toString();
    QJsonObject extraMeta = reply->property("extraMeta").toJsonObject();

    if (reply->error() == QNetworkReply::NoError) {
      emit dispatchFinished(itemId, true,
                            "Dispatched and started successfully.", extraMeta);
    } else {
      const QByteArray responseBody = reply->readAll();
      QString errorMessage =
          "Network error on select files: " + reply->errorString();
      // Append to extraMeta["raw_http"]
      QString rawHttp = extraMeta["raw_http"].toString();
      rawHttp += "\n\n--- SELECT FILES REQUEST ---\nRequest URL:\n" +
                 reply->request().url().toString() + "\n\n";
      // Add simplified error details
      extraMeta["raw_http"] = rawHttp;
      emit dispatchFinished(itemId, false, errorMessage + fullApiCallLog,
                            extraMeta);
    }
    reply->deleteLater();
  });
}

bool RealDebridConnector::hasSettings() const { return true; }

QWidget *RealDebridConnector::createSettingsWidget(QWidget *parent) {
  QWidget *widget = new QWidget(parent);
  QVBoxLayout *mainLayout = new QVBoxLayout(widget);

  QSettings settings;
  settings.beginGroup("Plugins/RealDebrid");

  QCheckBox *const enabledCheck =
      new QCheckBox(tr("Enable Real-Debrid"), widget);
  enabledCheck->setObjectName("enabledCheck");
  enabledCheck->setChecked(settings.value("enabled", false).toBool());
  mainLayout->addWidget(enabledCheck);

  QWidget *configWidget = new QWidget(widget);
  QFormLayout *configLayout = new QFormLayout(configWidget);

  QLineEdit *const tokenEdit = new QLineEdit(configWidget);
  tokenEdit->setObjectName("tokenEdit");
  tokenEdit->setEchoMode(QLineEdit::Password);
  tokenEdit->setText(
      SecureStorage::readPassword("Plugins/RealDebrid", "apiToken"));
  configLayout->addRow(tr("API Token:"), tokenEdit);

  const QSettings mainSettings;
  if (mainSettings.value("allowPlaintextStorage", false).toBool()) {
    QLabel *warningLabel = new QLabel(
        tr("⚠️ Warning: Data may be stored unencrypted based on preferences."));
    warningLabel->setStyleSheet("color: #d9534f; font-size: 11px;");
    configLayout->addRow("", warningLabel);
  }

  mainLayout->addWidget(configWidget);
  settings.endGroup();

  configWidget->setVisible(enabledCheck->isChecked());
  connect(enabledCheck, &QCheckBox::toggled, configWidget,
          &QWidget::setVisible);

  return widget;
}

void RealDebridConnector::saveSettings(QWidget *settingsWidget) {
  if (settingsWidget == nullptr) {
    return;
  }

  QCheckBox *const enabledCheck =
      settingsWidget->findChild<QCheckBox *>("enabledCheck");
  QLineEdit *const tokenEdit =
      settingsWidget->findChild<QLineEdit *>("tokenEdit");

  QSettings settings;
  settings.beginGroup("Plugins/RealDebrid");

  if (enabledCheck != nullptr) {
    bool en = enabledCheck->isChecked();
    settings.setValue("enabled", en);
    m_enabled = en;
  }
  if (tokenEdit != nullptr) {
    SecureStorage::writePassword("Plugins/RealDebrid", "apiToken",
                                 tokenEdit->text());
    m_apiToken = tokenEdit->text();
  }

  settings.endGroup();
}

bool RealDebridConnector::hasDebugMenu() const { return true; }

QList<HttpApiEndpoint> RealDebridConnector::getHttpApiEndpoints() const {
  QList<HttpApiEndpoint> endpoints;

  HttpApiEndpoint userInfo;
  userInfo.name = "User Info";
  userInfo.description = "Retrieves user information";
  userInfo.method = "GET";
  userInfo.url = "https://api.real-debrid.com/rest/1.0/user";
  userInfo.headers.insert("Authorization", "Bearer ${API_TOKEN}");
  endpoints.append(userInfo);

  HttpApiEndpoint listTorrents;
  listTorrents.name = "List Torrents";
  listTorrents.description = "Lists all torrents";
  listTorrents.method = "GET";
  listTorrents.url = "https://api.real-debrid.com/rest/1.0/torrents";
  listTorrents.headers.insert("Authorization", "Bearer ${API_TOKEN}");
  endpoints.append(listTorrents);

  HttpApiEndpoint createTorrentMagnet;
  createTorrentMagnet.name = "Create Torrent (Magnet)";
  createTorrentMagnet.description =
      "Creates a new torrent transfer from a magnet link";
  createTorrentMagnet.method = "POST";
  createTorrentMagnet.url =
      "https://api.real-debrid.com/rest/1.0/torrents/addMagnet";
  createTorrentMagnet.headers.insert("Authorization", "Bearer ${API_TOKEN}");
  createTorrentMagnet.headers.insert("Content-Type",
                                     "application/x-www-form-urlencoded");
  createTorrentMagnet.body = "magnet=${MAGNET_LINK}";
  endpoints.append(createTorrentMagnet);

  HttpApiEndpoint createTorrentFile;
  createTorrentFile.name = "Create Torrent (File)";
  createTorrentFile.description =
      "Creates a new torrent transfer from a .torrent file";
  createTorrentFile.method = "PUT";
  createTorrentFile.url =
      "https://api.real-debrid.com/rest/1.0/torrents/addTorrent";
  createTorrentFile.headers.insert("Authorization", "Bearer ${API_TOKEN}");
  createTorrentFile.body =
      "file:///path/to/test.torrent"; // Requires raw binary data of the torrent
                                      // file, use body or a mechanism to PUT
                                      // file. In KMagMux, custom implementation
                                      // is generally used. For API Explorer we
                                      // might just set body to path for
                                      // preview.
  endpoints.append(createTorrentFile);

  return endpoints;
}

QMap<QString, QString> RealDebridConnector::getApiSubstitutions() const {
  QMap<QString, QString> subs;
  subs.insert("API_TOKEN", m_apiToken);
  return subs;
}
