#include "ItemParser.h"
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QTextStream>
#include <QUrl>

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
        QString tempPath = QDir::tempPath() + "/" + newItem.id + "_" +
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
