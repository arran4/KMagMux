#ifndef TORRENTPARSER_H
#define TORRENTPARSER_H

#include <QByteArray>
#include <QString>
#include <QStringList>

struct TorrentInfo {
  QString name;
  QByteArray infoHash;
  QStringList trackers;
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
