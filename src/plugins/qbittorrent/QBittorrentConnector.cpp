#include "QBittorrentConnector.h"
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>

QBittorrentConnector::QBittorrentConnector() : QBittorrentConnector(nullptr) {}

QBittorrentConnector::QBittorrentConnector(QObject *parent)
    : QObject(parent), m_networkManager(new QNetworkAccessManager(this)),
      m_baseUrl("http://localhost:8080"), m_username("admin"),
      m_password("adminadmin"), m_pendingItem(), m_isPending(false) {
}

QString QBittorrentConnector::getId() const { return "qBittorrent"; }

QString QBittorrentConnector::getName() const { return "qBittorrent (WebAPI)"; }

void QBittorrentConnector::setBaseUrl(const QString &url) {
  m_baseUrl = url;
  if (m_baseUrl.endsWith("/")) {
    m_baseUrl.chop(1);
  }
}

void QBittorrentConnector::setCredentials(const QString &username,
                                          const QString &password) {
  m_username = username;
  m_password = password;
}

void QBittorrentConnector::dispatch(const Item &item) {
  // For now, assume we need to login first or check session.
  // A robust implementation would check if we have a valid cookie.
  // We'll just try to login every time for this MVP step to ensure it works,
  // or better, try to dispatch and if 403/401, login and retry.
  // Simplest reliable path: Login first, then dispatch.

  m_pendingItem = item;
  m_isPending = true;
  login();
}

void QBittorrentConnector::login() {
  QUrl url(m_baseUrl + "/api/v2/auth/login");
  QNetworkRequest request(url);
  request.setHeader(QNetworkRequest::ContentTypeHeader,
                    "application/x-www-form-urlencoded");

  QUrlQuery postData;
  postData.addQueryItem("username", m_username);
  postData.addQueryItem("password", m_password);

  QNetworkReply *reply = m_networkManager->post(
      request, postData.toString(QUrl::FullyEncoded).toUtf8());
  connect(reply, &QNetworkReply::finished, this,
          &QBittorrentConnector::onLoginReply);
}

void QBittorrentConnector::onLoginReply() {
  QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
  if (!reply)
    return;

  if (reply->error() == QNetworkReply::NoError) {
    // Login successful. The cookie jar in QNetworkAccessManager should handle
    // the session cookie automatically. Proceed to dispatch pending item.
    if (m_isPending) {
      performDispatch(m_pendingItem);
      m_isPending = false;
    }
  } else {
    qWarning() << "qBittorrent Login failed:" << reply->errorString();
    if (m_isPending) {
      emit dispatchFinished(m_pendingItem.id, false,
                            "Login failed: " + reply->errorString());
      m_isPending = false;
    }
  }
  reply->deleteLater();
}

void QBittorrentConnector::performDispatch(const Item &item) {
  QUrl url(m_baseUrl + "/api/v2/torrents/add");
  QNetworkRequest request(url);

  // We use QHttpMultiPart for both files and magnets because qBittorrent API
  // accepts multipart/form-data
  QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

  // 1. URLs (Magnet links)
  if (item.sourcePath.startsWith("magnet:")) {
    QHttpPart textPart;
    textPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QVariant("form-data; name=\"urls\""));
    textPart.setBody(item.sourcePath.toUtf8());
    multiPart->append(textPart);
  }
  // 2. Torrent Files
  else {
    QFile *file = new QFile(item.sourcePath);
    if (!file->open(QIODevice::ReadOnly)) {
      emit dispatchFinished(item.id, false,
                            "Could not open torrent file: " + item.sourcePath);
      delete multiPart;
      delete file;
      return;
    }

    QHttpPart filePart;
    // qBittorrent requires the filename in content-disposition
    QString filename = QFileInfo(item.sourcePath).fileName();
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QVariant("form-data; name=\"torrents\"; filename=\"" +
                                filename + "\""));
    filePart.setBodyDevice(file);
    file->setParent(
        multiPart); // Valid trick: file is deleted when multiPart is deleted
    multiPart->append(filePart);
  }

  // 3. Save Path (Destination)
  if (!item.destinationPath.isEmpty()) {
    QHttpPart savePathPart;
    savePathPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                           QVariant("form-data; name=\"savepath\""));
    savePathPart.setBody(item.destinationPath.toUtf8());
    multiPart->append(savePathPart);
  }

  // 4. Category (from labels if present? or metadata?)
  if (item.metadata.contains("category")) {
    QHttpPart categoryPart;
    categoryPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                           QVariant("form-data; name=\"category\""));
    categoryPart.setBody(item.metadata["category"].toString().toUtf8());
    multiPart->append(categoryPart);
  }

  // 5. Tags (Labels)
  if (item.metadata.contains("labels")) {
    QHttpPart tagsPart;
    tagsPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QVariant("form-data; name=\"tags\""));
    tagsPart.setBody(item.metadata["labels"].toString().toUtf8());
    multiPart->append(tagsPart);
  }

  // 6. Paused (optional, maybe from metadata)
  // "paused" value="true"|"false"

  QNetworkReply *reply = m_networkManager->post(request, multiPart);
  multiPart->setParent(reply); // Delete multiPart with reply

  // We need to pass the item ID to the reply handler, or store it in a map.
  // Since we are processing one at a time via `m_pendingItem` (for now), we can
  // use that, BUT since we might fire multiple requests if we improve the
  // engine, let's attach the ID to the reply.
  reply->setProperty("itemId", item.id);

  connect(reply, &QNetworkReply::finished, this,
          &QBittorrentConnector::onAddTorrentReply);
}

void QBittorrentConnector::onAddTorrentReply() {
  QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
  if (!reply)
    return;

  QString itemId = reply->property("itemId").toString();

  if (reply->error() == QNetworkReply::NoError) {
    QString response = reply->readAll();
    if (response.toLower().contains("fail")) {
      emit dispatchFinished(itemId, false,
                            "qBittorrent API returned failure: " + response);
    } else {
      emit dispatchFinished(itemId, true, "Dispatched successfully.");
    }
  } else {
    emit dispatchFinished(itemId, false,
                          "Network error: " + reply->errorString());
  }
  reply->deleteLater();
}
