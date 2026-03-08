#include "TorrentParser.h"
#include "BencodeParser.h"
#include <QFile>
#include <QUrl>
#include <QUrlQuery>
#include <QVariantList>

TorrentInfo TorrentParser::parse(const QString &sourcePath) {
  if (sourcePath.startsWith("magnet:?")) {
    return parseMagnet(sourcePath);
  } else {
    return parseTorrentFile(sourcePath);
  }
}

TorrentInfo TorrentParser::parseMagnet(const QString &magnetUri) {
  TorrentInfo info;
  QUrl url(magnetUri);
  if (!url.isValid() || url.scheme() != "magnet") {
    info.errorString = "Invalid magnet URI";
    return info;
  }

  QUrlQuery query(url);

  if (query.hasQueryItem("dn")) {
    info.name = QUrl::fromPercentEncoding(query.queryItemValue("dn").toUtf8());
  }

  // Find info hash
  // xt=urn:btih:<hash>
  QList<QPair<QString, QString>> items = query.queryItems();
  for (const auto &item : items) {
    if (item.first == "xt") {
      QString val = item.second;
      if (val.startsWith("urn:btih:")) {
        QString hashHex = val.mid(9);
        if (hashHex.length() == 40) {
          info.infoHash = QByteArray::fromHex(hashHex.toUtf8());
        } else if (hashHex.length() == 32) {
          // base32 encoded, not fully supported here, simplified
          info.infoHash = hashHex.toUtf8(); // Need full base32 decoding if used
        }
      }
    } else if (item.first == "tr") {
      QString trackerUrl = QUrl::fromPercentEncoding(item.second.toUtf8());
      if (!info.trackers.contains(trackerUrl)) {
        info.trackers.append(trackerUrl);
      }
    }
  }

  if (info.infoHash.isEmpty()) {
    info.errorString = "No info hash found in magnet link";
    return info;
  }

  if (info.name.isEmpty()) {
    info.name = "Unknown Name";
  }

  info.valid = true;
  return info;
}

TorrentInfo TorrentParser::parseTorrentFile(const QString &filePath) {
  TorrentInfo info;

  QString path = filePath;
  if (path.startsWith("file://")) {
      path = QUrl(path).toLocalFile();
  }

  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    info.errorString = "Could not open file: " + file.errorString();
    return info;
  }

  QByteArray data = file.readAll();
  file.close();

  BencodeParser parser;
  if (!parser.parse(data)) {
    info.errorString = "Failed to parse bencode data: " + parser.errorString();
    return info;
  }

  QVariantMap dict = parser.dictionary();

  // Extract announce
  if (dict.contains("announce")) {
    QString announce = QString::fromUtf8(dict["announce"].toByteArray());
    if (!info.trackers.contains(announce)) {
      info.trackers.append(announce);
    }
  }

  // Extract announce-list
  if (dict.contains("announce-list")) {
    QVariantList announceList = dict["announce-list"].toList();
    for (const QVariant &tierVar : announceList) {
      if (tierVar.typeId() == QMetaType::QVariantList) {
        QVariantList tier = tierVar.toList();
        for (const QVariant &trackerVar : tier) {
          if (trackerVar.typeId() == QMetaType::QByteArray) {
            QString trackerUrl = QString::fromUtf8(trackerVar.toByteArray());
            if (!info.trackers.contains(trackerUrl)) {
              info.trackers.append(trackerUrl);
            }
          }
        }
      }
    }
  }

  // Extract name
  if (dict.contains("info")) {
    QVariant infoVar = dict["info"];
    if (infoVar.typeId() == QMetaType::QVariantMap) {
      QVariantMap infoDict = infoVar.toMap();
      if (infoDict.contains("name")) {
        info.name = QString::fromUtf8(infoDict["name"].toByteArray());
      }
    }
  }

  info.infoHash = parser.infoHash();

  if (info.infoHash.isEmpty()) {
    info.errorString = "No info dict found, cannot compute info hash";
    return info;
  }

  if (info.name.isEmpty()) {
    info.name = "Unknown Name";
  }

  info.valid = true;
  return info;
}
