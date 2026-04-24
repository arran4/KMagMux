#include "ItemParser.h"
#include "core/Item.h"
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QHostAddress>
#include <QHostInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSslConfiguration>
#include <QTextStream>
#include <QUrl>
#include <qcontainerfwd.h>
#include <qjsonobject.h>
#include <qnamespace.h>
#include <qobject.h>
#include <qtypes.h>
#include <vector>

std::vector<Item> ItemParser::parseLines(const QStringList &lines) {
  std::vector<Item> parsedItems;
  const qint64 now = QDateTime::currentMSecsSinceEpoch();
  int idx = 0;

  QNetworkAccessManager manager; // Reused across lines for efficiency

  auto processLine = [&](QString line) {
    line = line.trimmed();
    if (line.isEmpty()) {
      return;
    }

    QString pathToCheck = line;
    if (line.startsWith("file://")) {
      const int filePrefixLen = 7;
      pathToCheck = line.mid(filePrefixLen);
    }

    Item newItem;
    newItem.id = QString::number(now) + "_" + QString::number(idx++) + "_item";
    newItem.state = ItemState::Unprocessed;

    if ((pathToCheck.startsWith("http://", Qt::CaseInsensitive) ||
         pathToCheck.startsWith("https://", Qt::CaseInsensitive)) &&
        pathToCheck.endsWith(".torrent", Qt::CaseInsensitive)) {

      QUrl url(pathToCheck);
      QString host = url.host();
      QHostInfo info = QHostInfo::fromName(host);

      bool isSafe = false;
      QHostAddress safeAddr;
      if (info.error() == QHostInfo::NoError && !info.addresses().isEmpty()) {
        isSafe = true;
        for (const QHostAddress &addr : info.addresses()) {
          if (addr.isLoopback() || addr.isMulticast() || addr.isBroadcast() ||
              addr.isLinkLocal() || addr.isSiteLocal() ||
              addr.isUniqueLocalUnicast()) {
            isSafe = false;
            break;
          }
          bool ok = false;
          quint32 ipv4 = addr.toIPv4Address(&ok);
          if (ok && (ipv4 & 0xFF000000) == 0x00000000) {
            isSafe = false;
            break;
          }
        }
        if (isSafe) {
          safeAddr = info.addresses().first();
        }
      }

      if (!isSafe) {
        qWarning() << "ItemParser: Blocked potentially unsafe torrent URL "
                      "(SSRF prevention):"
                   << pathToCheck;
        return; // Skip this line completely
      }

      // Reconstruct URL with IP to prevent DNS rebinding attacks
      QUrl safeUrl = url;
      if (safeAddr.protocol() == QAbstractSocket::IPv6Protocol) {
        safeUrl.setHost("[" + safeAddr.toString() + "]");
      } else {
        safeUrl.setHost(safeAddr.toString());
      }

      QNetworkRequest request(safeUrl);
      request.setRawHeader("Host", host.toUtf8());

      // Set PeerVerifyName for SNI/HTTPS when connecting by IP
      request.setPeerVerifyName(host);

      request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                           QNetworkRequest::NoLessSafeRedirectPolicy);

      QEventLoop loop;
      QObject::connect(&manager, &QNetworkAccessManager::finished, &loop,
                       &QEventLoop::quit);
      QNetworkReply *reply = manager.get(request);
      loop.exec();

      if (reply->error() == QNetworkReply::NoError) {
        QString const tempPath = QDir::tempPath() + "/" + newItem.id + "_" +
                                 QFileInfo(QUrl(pathToCheck).path()).fileName();
        QFile file(tempPath);
        if (file.open(QIODevice::WriteOnly)) {
          file.write(reply->readAll());
          file.close();
          QJsonObject meta = newItem.metadata;
          meta["original_url"] = pathToCheck;
          meta["delete_source_file"] = true;
          newItem.metadata = meta;
          pathToCheck = tempPath;
        }
      }
      reply->deleteLater();
    }

    newItem.sourcePath = pathToCheck;
    newItem.createdTime = QDateTime::currentDateTime();
    parsedItems.push_back(newItem);
  };

  for (const QString &line : lines) {
    processLine(line);
  }

  return parsedItems;
}
