#include "../../src/core/TorrentParser.h"
#include <QByteArray>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

class TestTorrentParser : public QObject {
  Q_OBJECT

private slots:
  void testParseMagnetSimple() {
    QString magnet =
        "magnet:?xt=urn:btih:0123456789abcdef0123456789abcdef01234567&dn=Test%"
        "20Torrent";
    TorrentInfo info = TorrentParser::parse(magnet);

    QVERIFY(info.valid);
    QCOMPARE(info.name, QString("Test Torrent"));
    QCOMPARE(info.infoHash.toHex(),
             QByteArray("0123456789abcdef0123456789abcdef01234567"));
    QVERIFY(info.trackers.isEmpty());
  }

  void testParseMagnetWithTrackers() {
    QString magnet =
        "magnet:?xt=urn:btih:0123456789abcdef0123456789abcdef01234567&tr=udp%"
        "3A%2F%2Ftracker.test.org%3A80&tr=http%3A%2F%2Ftracker.test.org%"
        "2Fannounce";
    TorrentInfo info = TorrentParser::parse(magnet);

    QVERIFY(info.valid);
    QCOMPARE(info.name, QString("Unknown Name"));
    QCOMPARE(info.infoHash.toHex(),
             QByteArray("0123456789abcdef0123456789abcdef01234567"));
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
    QByteArray data =
        "d8:announce28:http://tracker.test.org/test13:announce-listll29:http://"
        "tracker.test.org/test2ee4:infod4:name12:Test Torrent12:piece "
        "lengthi262144e6:pieces20:12345678901234567890ee";
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

  void testParseTorrentFileWithPathTraversal() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QString filePath = dir.path() + "/test_traversal.torrent";
    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly));

    // Create a bencoded torrent with multiple files, one containing directory
    // traversal payloads. info -> files -> [ { length: 100, path: ["..", "..",
    // "etc", "passwd"] }, { length: 200, path: ["normal", "path.txt"] }, {
    // length: 50, path: ["a/b", "c\\d.txt"] } ] d4:info d5:files l
    // d6:lengthi100e 4:path l2:.. 2:.. 3:etc 6:passwd e e d6:lengthi200e 4:path
    // l6:normal 8:path.txt e e d6:lengthi50e 4:path l3:a/b 7:c\\d.txte e e
    // 4:name 4:Test e e
    QByteArray data = "d4:infod5:filesld6:lengthi100e4:pathl2:..2:..3:etc6:"
                      "passwdeed6:lengthi200e4:pathl6:normal8:path.txteed6:"
                      "lengthi50e4:pathl3:a/b7:c\\d.txteee4:name4:Testee";
    file.write(data);
    file.close();

    TorrentInfo info = TorrentParser::parse(filePath);

    QVERIFY(info.valid);
    QCOMPARE(info.files.size(), 3);

    QCOMPARE(info.files[0].path, QString("etc/passwd"));
    QCOMPARE(info.files[1].path, QString("normal/path.txt"));
    QCOMPARE(info.files[2].path, QString("a_b/c_d.txt"));
  }
};

QTEST_MAIN(TestTorrentParser)
#include "TestTorrentParser.moc"
