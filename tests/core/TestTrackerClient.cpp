#include <QtTest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QSignalSpy>
#include "core/TrackerClient.h"

Q_DECLARE_METATYPE(TrackerStats)

class TestTrackerClient : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        qRegisterMetaType<TrackerStats>("TrackerStats");
    }

    void testMissingFilesDict() {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));

        TrackerClient client;
        QSignalSpy spy(&client, &TrackerClient::scrapeFinished);

        QString url = QString("http://127.0.0.1:%1/announce").arg(server.serverPort());
        QByteArray infoHash = QByteArray::fromHex("1234567890123456789012345678901234567890");

        client.scrape(url, infoHash);

        QVERIFY(server.waitForNewConnection(5000));
        QTcpSocket* socket = server.nextPendingConnection();
        QVERIFY(socket->waitForReadyRead(5000));

        // Read the request
        socket->readAll();

        // Send the HTTP response
        QByteArray responseBody = "d8:some_key10:some_valuee"; // Valid bencode, no "files" dict
        QByteArray response = "HTTP/1.1 200 OK\r\n"
                              "Content-Type: text/plain\r\n"
                              "Content-Length: " + QByteArray::number(responseBody.size()) + "\r\n"
                              "Connection: close\r\n\r\n" + responseBody;

        socket->write(response);
        socket->flush();
        socket->disconnectFromHost();

        QVERIFY(spy.wait(5000));

        QCOMPARE(spy.count(), 1);
        QList<QVariant> arguments = spy.takeFirst();
        TrackerStats stats = arguments.at(0).value<TrackerStats>();

        QCOMPARE(stats.success, false);
        QCOMPARE(stats.errorString, QString("No files dict in scrape response"));

        socket->deleteLater();
    }
};

QTEST_MAIN(TestTrackerClient)
#include "TestTrackerClient.moc"
