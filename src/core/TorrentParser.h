#ifndef TORRENTPARSER_H
#define TORRENTPARSER_H

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <QDateTime>

struct TorrentFileInfo {
  QString path;
  qint64 length;
};

struct TorrentInfo {
  QString name;
  QByteArray infoHash;
  QStringList trackers;

  QString comment;
  QString createdBy;
  QDateTime creationDate;
  qint64 totalSize = 0;
  QList<TorrentFileInfo> files;

  bool valid = false;
  QString errorString;
};

class TorrentParser {
public:
  static TorrentInfo parse(const QString &sourcePath);

private:
  static TorrentInfo parseMagnet(const QString &magnetUri);
  static TorrentInfo parseTorrentFile(const QString &filePath);
};

#endif // TORRENTPARSER_H
