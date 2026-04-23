#include "ItemParser.h"
#include "core/Item.h"
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QHostAddress>
#include <QHostInfo>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
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
      if (info.error() == QHostInfo::NoError) {
        isSafe = true;
        for (const QHostAddress &addr : info.addresses()) {
          if (addr.isLoopback() || addr.isMulticast() || addr.isBroadcast() || addr.isLinkLocal() || addr.isSiteLocal() || addr.isUniqueLocalUnicast()) {
            isSafe = false;
            break;
          }
          bool hasIpv4 = false;
          quint32 ipv4 = 0;
          if (addr.protocol() == QAbstractSocket::IPv4Protocol) {
            ipv4 = addr.toIPv4Address();
            hasIpv4 = true;
          } else if (addr.protocol() == QAbstractSocket::IPv6Protocol) {
            bool ok = false;
            ipv4 = addr.toIPv4Address(&ok);
            if (ok) {
              hasIpv4 = true;
            }
          }

          if (hasIpv4) {
            // 10.0.0.0/8
            if ((ipv4 & 0xFF000000) == 0x0A000000) { isSafe = false; break; }
            // 172.16.0.0/12
            if ((ipv4 & 0xFFF00000) == 0xAC100000) { isSafe = false; break; }
            // 192.168.0.0/16
            if ((ipv4 & 0xFFFF0000) == 0xC0A80000) { isSafe = false; break; }
            // 169.254.0.0/16
            if ((ipv4 & 0xFFFF0000) == 0xA9FE0000) { isSafe = false; break; }
            // 0.0.0.0/8
            if ((ipv4 & 0xFF000000) == 0x00000000) { isSafe = false; break; }
          }
        }
      }

      if (!isSafe) {
        qWarning() << "ItemParser: Blocked potentially unsafe torrent URL (SSRF prevention):" << pathToCheck;
        return; // Skip this line completely
      }

      QNetworkAccessManager manager;
      QNetworkRequest request((QUrl(pathToCheck)));
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
