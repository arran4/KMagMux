#include "TorBoxConnector.h"
#include "core/SecureStorage.h"
#include <QCheckBox>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QLabel>
#include <QLineEdit>
#include <QSettings>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>
#include <numeric>

TorBoxConnector::TorBoxConnector() : TorBoxConnector(nullptr) {}

TorBoxConnector::TorBoxConnector(QObject *parent)
    : QObject(parent), m_networkManager(new QNetworkAccessManager(this)),
      m_apiToken(""), m_enabled(false) {
  QSettings settings;
  settings.beginGroup("Plugins/TorBox");
  m_enabled = settings.value("enabled", false).toBool();
  settings.endGroup();

  m_apiToken = SecureStorage::readPassword("Plugins/TorBox", "apiToken");
}

QString TorBoxConnector::getId() const { return "TorBox"; }

QString TorBoxConnector::getName() const { return "TorBox"; }

bool TorBoxConnector::isEnabled() const { return m_enabled; }

void TorBoxConnector::dispatch(const Item &item) {
  if (m_apiToken.isEmpty()) {
    emit dispatchFinished(item.id, false, "API Token is missing.");
    return;
  }

  // Simple stub for dispatch
  const QUrl url("https://api.torbox.app/v1/api/torrents/createtorrent");
  QNetworkRequest request(url);
  // Assuming Bearer token auth
  request.setRawHeader("Authorization", ("Bearer " + m_apiToken).toUtf8());

  QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
  if (item.sourcePath.startsWith("magnet:")) {
    QHttpPart textPart;
    textPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QVariant("form-data; name=\"magnet\""));
    textPart.setBody(item.sourcePath.toUtf8());
    multiPart->append(textPart);
  } else {
    QFile *file = new QFile(item.sourcePath);
    if (!file->open(QIODevice::ReadOnly)) {
      emit dispatchFinished(item.id, false,
                            "Could not open torrent file: " + item.sourcePath);
      multiPart->deleteLater();
      file->deleteLater();
      return;
    }
    QHttpPart filePart;
    const QString filename = QFileInfo(item.sourcePath).fileName();
    filePart.setHeader(
        QNetworkRequest::ContentDispositionHeader,
        QVariant("form-data; name=\"file\"; filename=\"" + filename + "\""));
    filePart.setBodyDevice(file);
    file->setParent(multiPart);
    multiPart->append(filePart);
  }

  const QString apiCallLog = Connector::buildApiCallLog("POST", request);

  QNetworkReply *reply = m_networkManager->post(request, multiPart);
  multiPart->setParent(reply);
  reply->setProperty("itemId", item.id);
  reply->setProperty("apiCallLog", apiCallLog);
  connect(reply, &QNetworkReply::finished, this,
          &TorBoxConnector::onAddTorrentReply);
}

void TorBoxConnector::onAddTorrentReply() {
  QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
  if (reply == nullptr) {
    return;
  }

  const QString itemId = reply->property("itemId").toString();
  const QString apiCallLog = reply->property("apiCallLog").toString();

  if (reply->error() == QNetworkReply::NoError) {
    QJsonObject extraMeta;
    extraMeta["raw_response"] = QString::fromUtf8(reply->readAll());
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
    rawHttp = std::accumulate(
        resHeaders.begin(), resHeaders.end(), rawHttp,
        [reply](const QString &str, const QByteArray &headerName) {
          return str + QString::fromUtf8(headerName) + ": " +
                 QString::fromUtf8(reply->rawHeader(headerName)) + "\n";
        });

    const QByteArray body = reply->readAll();
    rawHttp += "\nResponse Body:\n" + QString::fromUtf8(body) + "\n";

    extraMeta["raw_http"] = rawHttp;

#ifndef QT_NO_DEBUG
    QString shortBody = QString::fromUtf8(body).left(500);
    if (statusCode > 0 || !shortBody.isEmpty()) {
      errorMessage +=
          QString(" (Status: %1, Body: %2)").arg(statusCode).arg(shortBody);
    }
#endif
    emit dispatchFinished(itemId, false, errorMessage + apiCallLog);
  }

  reply->deleteLater();
}

bool TorBoxConnector::hasSettings() const { return true; }

QWidget *TorBoxConnector::createSettingsWidget(QWidget *parent) {
  QWidget *widget = new QWidget(parent);
  QVBoxLayout *mainLayout = new QVBoxLayout(widget);

  QSettings settings;
  settings.beginGroup("Plugins/TorBox");

  QCheckBox *const enabledCheck = new QCheckBox(tr("Enable TorBox"), widget);
  enabledCheck->setObjectName("enabledCheck");
  enabledCheck->setChecked(settings.value("enabled", false).toBool());
  mainLayout->addWidget(enabledCheck);

  QWidget *configWidget = new QWidget(widget);
  QFormLayout *configLayout = new QFormLayout(configWidget);

  QLineEdit *const tokenEdit = new QLineEdit(configWidget);
  tokenEdit->setObjectName("tokenEdit");
  tokenEdit->setEchoMode(QLineEdit::Password);
  tokenEdit->setText(SecureStorage::readPassword("Plugins/TorBox", "apiToken"));
  configLayout->addRow(tr("API Token:"), tokenEdit);

  mainLayout->addWidget(configWidget);
  settings.endGroup();

  configWidget->setVisible(enabledCheck->isChecked());
  connect(enabledCheck, &QCheckBox::toggled, configWidget,
          &QWidget::setVisible);

  return widget;
}

void TorBoxConnector::saveSettings(QWidget *settingsWidget) {
  if (settingsWidget == nullptr) {
    return;
  }

  QCheckBox *const enabledCheck =
      settingsWidget->findChild<QCheckBox *>("enabledCheck");
  QLineEdit *const tokenEdit =
      settingsWidget->findChild<QLineEdit *>("tokenEdit");

  QSettings settings;
  settings.beginGroup("Plugins/TorBox");

  if (enabledCheck != nullptr) {
    bool en = enabledCheck->isChecked();
    settings.setValue("enabled", en);
    m_enabled = en;
  }
  if (tokenEdit != nullptr) {
    SecureStorage::writePassword("Plugins/TorBox", "apiToken",
                                 tokenEdit->text());
    m_apiToken = tokenEdit->text();
  }

  settings.endGroup();
}

bool TorBoxConnector::hasDebugMenu() const { return true; }

QList<HttpApiEndpoint> TorBoxConnector::getHttpApiEndpoints() const {
  QList<HttpApiEndpoint> endpoints;

  HttpApiEndpoint userInfo;
  userInfo.name = "User Info";
  userInfo.description = "Retrieves user information";
  userInfo.method = "GET";
  userInfo.url = "https://api.torbox.app/v1/api/user/me";
  userInfo.headers.insert("Authorization", "Bearer ${API_TOKEN}");
  endpoints.append(userInfo);

  HttpApiEndpoint listTorrents;
  listTorrents.name = "List Torrents";
  listTorrents.description = "Lists all torrents";
  listTorrents.method = "GET";
  listTorrents.url = "https://api.torbox.app/v1/api/torrents/mylist";
  listTorrents.headers.insert("Authorization", "Bearer ${API_TOKEN}");
  endpoints.append(listTorrents);

  HttpApiEndpoint createTorrentMagnet;
  createTorrentMagnet.name = "Create Torrent (Magnet)";
  createTorrentMagnet.description =
      "Creates a new torrent transfer from a magnet link";
  createTorrentMagnet.method = "POST";
  createTorrentMagnet.url =
      "https://api.torbox.app/v1/api/torrents/createtorrent";
  createTorrentMagnet.headers.insert("Authorization", "Bearer ${API_TOKEN}");
  createTorrentMagnet.isMultipart = true;
  createTorrentMagnet.multipartParts.insert("magnet", "${MAGNET_LINK}");
  endpoints.append(createTorrentMagnet);

  HttpApiEndpoint createTorrentFile;
  createTorrentFile.name = "Create Torrent (File)";
  createTorrentFile.description =
      "Creates a new torrent transfer from a .torrent file";
  createTorrentFile.method = "POST";
  createTorrentFile.url =
      "https://api.torbox.app/v1/api/torrents/createtorrent";
  createTorrentFile.headers.insert("Authorization", "Bearer ${API_TOKEN}");
  createTorrentFile.isMultipart = true;
  createTorrentFile.multipartParts.insert("file",
                                          "file:///path/to/test.torrent");
  endpoints.append(createTorrentFile);

  HttpApiEndpoint createUsenet;
  createUsenet.name = "Create Usenet";
  createUsenet.description = "Creates a new usenet transfer";
  createUsenet.method = "POST";
  createUsenet.url = "https://api.torbox.app/v1/api/usenet/createusenet";
  createUsenet.headers.insert("Authorization", "Bearer ${API_TOKEN}");
  createUsenet.isMultipart = true;
  createUsenet.multipartParts.insert("link", "${USENET_LINK}");
  endpoints.append(createUsenet);

  HttpApiEndpoint createWebdl;
  createWebdl.name = "Create Web Download";
  createWebdl.description = "Creates a new web download";
  createWebdl.method = "POST";
  createWebdl.url = "https://api.torbox.app/v1/api/webdl/createwebdl";
  createWebdl.headers.insert("Authorization", "Bearer ${API_TOKEN}");
  createWebdl.isMultipart = true;
  createWebdl.multipartParts.insert("link", "${WEB_LINK}");
  endpoints.append(createWebdl);

  return endpoints;
}

QMap<QString, QString> TorBoxConnector::getApiSubstitutions() const {
  QMap<QString, QString> subs;
  subs.insert("API_TOKEN", m_apiToken);
  return subs;
}
