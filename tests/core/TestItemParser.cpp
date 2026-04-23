#include <QTest>
#include <QStringList>
#include <QSignalSpy>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include "core/Item.h"
#include "core/ItemParser.h"

class TestItemParser : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void testSafeUrl() {
        // Use an IP address instead of domain to avoid flaky DNS resolution in offline CI environments
        // This IP belongs to Cloudflare (1.1.1.1) and should be considered a safe global IP
        QStringList lines = {"http://1.1.1.1/test.torrent"};
        // We just check if it parses and creates an item
        std::vector<Item> items = ItemParser::parseLines(lines);
        QCOMPARE(items.size(), 1);
        QVERIFY(items[0].sourcePath.contains("test.torrent") || items[0].sourcePath.contains("1.1.1.1")); // Might be tempPath if it downloaded something, or original url if it failed to download
    }

    void testSsrfUrls() {
        QStringList lines = {
            "http://localhost/test.torrent",
            "http://127.0.0.1/test.torrent",
            "http://192.168.1.1/test.torrent",
            "http://10.0.0.1/test.torrent",
            "http://172.16.0.1/test.torrent",
            "http://169.254.169.254/test.torrent",
            "http://[::ffff:192.168.1.1]/test.torrent"
        };
        std::vector<Item> items = ItemParser::parseLines(lines);

        // It should skip the lines completely
        QCOMPARE(items.size(), 0);
    }

    void testMagnetLink() {
        QStringList lines = {"magnet:?xt=urn:btih:12345"};
        std::vector<Item> items = ItemParser::parseLines(lines);
        QCOMPARE(items.size(), 1);
        QCOMPARE(items[0].sourcePath, QString("magnet:?xt=urn:btih:12345"));
    }
};

QTEST_MAIN(TestItemParser)
#include "TestItemParser.moc"
