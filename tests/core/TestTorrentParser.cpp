#include <QTest>
#include <QByteArray>
#include <QFile>
#include <QTemporaryDir>
#include <QUrl>
#include "../../src/core/TorrentParser.h"

class TestTorrentParser : public QObject {
  Q_OBJECT

private slots:
  void testParseMagnetSimple() {
    QString magnet = "magnet:?xt=urn:btih:0123456789abcdef0123456789abcdef01234567&dn=Test%20Torrent";
    TorrentInfo info = TorrentParser::parse(magnet);

    QVERIFY(info.valid);
    QCOMPARE(info.name, QString("Test Torrent"));
    QCOMPARE(info.infoHash.toHex(), QByteArray("0123456789abcdef0123456789abcdef01234567"));
    QVERIFY(info.trackers.isEmpty());
  }

  void testParseMagnetWithTrackers() {
    QString magnet = "magnet:?xt=urn:btih:0123456789abcdef0123456789abcdef01234567&tr=udp%3A%2F%2Ftracker.test.org%3A80&tr=http%3A%2F%2Ftracker.test.org%2Fannounce";
    TorrentInfo info = TorrentParser::parse(magnet);

    QVERIFY(info.valid);
    QCOMPARE(info.name, QString("Unknown Name"));
    QCOMPARE(info.infoHash.toHex(), QByteArray("0123456789abcdef0123456789abcdef01234567"));
    QCOMPARE(info.trackers.size(), 2);
    QVERIFY(info.trackers.contains("udp://tracker.test.org:80"));
    QVERIFY(info.trackers.contains("http://tracker.test.org/announce"));
  }

  void testParseInvalidMagnet() {
    QString magnet = "magnet:?dn=Test%20Torrent"; // Missing xt
    TorrentInfo info = TorrentParser::parse(magnet);
    QVERIFY(!info.valid);
    QVERIFY(!info.errorString.isEmpty());
  }

  void testParseTorrentFile() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QString filePath = dir.path() + "/test.torrent";
    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly));

    // Create a simple valid bencoded torrent file content
    QByteArray data = "d8:announce28:http://tracker.test.org/test13:announce-listll29:http://tracker.test.org/test2ee4:infod4:name12:Test Torrent12:piece lengthi262144e6:pieces20:12345678901234567890ee";
    file.write(data);
    file.close();

    TorrentInfo info = TorrentParser::parse(filePath);

    QVERIFY(info.valid);
    QCOMPARE(info.name, QString("Test Torrent"));
    QVERIFY(!info.infoHash.isEmpty());
    QCOMPARE(info.trackers.size(), 2);
    QVERIFY(info.trackers.contains("http://tracker.test.org/test"));
    QVERIFY(info.trackers.contains("http://tracker.test.org/test2"));
  }
};

QTEST_MAIN(TestTorrentParser)
#include "TestTorrentParser.moc"
