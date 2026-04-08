#include "TorrentParser.h"
#include "BencodeParser.h"
#include <QFile>
#include <QUrl>
#include <QUrlQuery>
#include <QVariantList>

TorrentInfo TorrentParser::parse(const QString &sourcePath) {
  if (sourcePath.startsWith("magnet:?")) {
    return parseMagnet(sourcePath);
  }
  return parseTorrentFile(sourcePath);
}

TorrentInfo TorrentParser::parseMagnet(const QString &magnetUri) {
  TorrentInfo info;
  const QUrl url(magnetUri);
  if (!url.isValid() || url.scheme() != "magnet") {
    info.errorString = "Invalid magnet URI";
    return info;
  }

  const QUrlQuery query(url);

  if (query.hasQueryItem("dn")) {
    info.name = QUrl::fromPercentEncoding(query.queryItemValue("dn").toUtf8());
  }

  // Find info hash
  // xt=urn:btih:<hash>
  const QList<QPair<QString, QString>> items = query.queryItems();
  for (const auto &item : items) {
    if (item.first == "xt") {
      const QString val = item.second;
      if (val.startsWith("urn:btih:")) {
        const int hashHexPrefixLen = 9;
        const QString hashHex = val.mid(hashHexPrefixLen);
        if (hashHex.length() == 40) {
          info.infoHash = QByteArray::fromHex(hashHex.toUtf8());
        } else if (hashHex.length() == 32) {
          // base32 encoded, not fully supported here, simplified
          info.infoHash = hashHex.toUtf8(); // Need full base32 decoding if used
        }
      }
    } else if (item.first == "tr") {
      const QString trackerUrl =
          QUrl::fromPercentEncoding(item.second.toUtf8());
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

  const QByteArray data = file.readAll();
  file.close();

  BencodeParser parser;
  if (!parser.parse(data)) {
    info.errorString = "Failed to parse bencode data: " + parser.errorString();
    return info;
  }

  QVariantMap dict = parser.dictionary();

  // Extract announce
  if (dict.contains("announce")) {
    const QString announce = QString::fromUtf8(dict["announce"].toByteArray());
    if (!info.trackers.contains(announce)) {
      info.trackers.append(announce);
    }
  }

  // Extract announce-list
  if (dict.contains("announce-list")) {
    const QVariantList announceList = dict["announce-list"].toList();
    for (const QVariant &tierVar : announceList) {
      if (tierVar.typeId() == QMetaType::QVariantList) {
        const QVariantList tier = tierVar.toList();
        for (const QVariant &trackerVar : tier) {
          if (trackerVar.typeId() == QMetaType::QByteArray) {
            const QString trackerUrl =
                QString::fromUtf8(trackerVar.toByteArray());
            if (!info.trackers.contains(trackerUrl)) {
              info.trackers.append(trackerUrl);
            }
          }
        }
      }
    }
  }

  if (dict.contains("comment")) {
    info.comment = QString::fromUtf8(dict["comment"].toByteArray());
  }

  if (dict.contains("created by")) {
    info.createdBy = QString::fromUtf8(dict["created by"].toByteArray());
  }

  if (dict.contains("creation date")) {
    const qint64 creationTs = dict["creation date"].toLongLong();
    if (creationTs > 0) {
      info.creationDate = QDateTime::fromSecsSinceEpoch(creationTs);
    }
  }

  // Extract info dictionary values
  if (dict.contains("info")) {
    const QVariant infoVar = dict["info"];
    if (infoVar.typeId() == QMetaType::QVariantMap) {
      QVariantMap infoDict = infoVar.toMap();
      if (infoDict.contains("name")) {
        info.name = QString::fromUtf8(infoDict["name"].toByteArray());
      }

      if (infoDict.contains("length")) {
        // Single file mode
        TorrentFileInfo fileInfo;
        fileInfo.path = info.name;
        fileInfo.length = infoDict["length"].toLongLong();
        info.files.append(fileInfo);
        info.totalSize += fileInfo.length;
      } else if (infoDict.contains("files")) {
        // Multiple files mode
        const QVariantList filesList = infoDict["files"].toList();
        for (const QVariant &fileVar : filesList) {
          if (fileVar.typeId() == QMetaType::QVariantMap) {
            QVariantMap fileDict = fileVar.toMap();
            TorrentFileInfo fileInfo;
            fileInfo.length = fileDict["length"].toLongLong();

            if (fileDict.contains("path")) {
              const QVariantList pathList = fileDict["path"].toList();
              QStringList pathParts;
              for (const QVariant &part : pathList) {
                QString partStr = QString::fromUtf8(part.toByteArray());
                // Sanitize path component to prevent directory traversal
                if (partStr == ".." || partStr == ".") {
                  continue;
                }
                // Prevent embedded separators
                partStr.replace("/", "_");
                partStr.replace("\\", "_");
                if (!partStr.isEmpty()) {
                  pathParts.append(partStr);
                }
              }
              fileInfo.path = pathParts.join("/");
              if (fileInfo.path.isEmpty()) {
                fileInfo.path = "unnamed_file";
              }
            }

            info.files.append(fileInfo);
            info.totalSize += fileInfo.length;
          }
        }
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
