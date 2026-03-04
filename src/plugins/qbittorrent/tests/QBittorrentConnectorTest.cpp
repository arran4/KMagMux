#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QtTest>

// Since we compiled the source into this binary, we can just include the
// header.
#include "../QBittorrentConnector.h"

class MockQBitServer : public QTcpServer {
  Q_OBJECT
public:
  explicit MockQBitServer(QObject *parent = nullptr)
      : QTcpServer(parent), m_loginSuccess(true), m_addSuccess(true),
        m_lastAddedUrl(""), m_lastAddedPath("") {}

  bool m_loginSuccess;
  bool m_addSuccess;
  QString m_lastAddedUrl;
  QString m_lastAddedPath;

protected:
  void incomingConnection(qintptr socketDescriptor) override {
    QTcpSocket *client = new QTcpSocket(this);
    client->setSocketDescriptor(socketDescriptor);
    connect(client, &QTcpSocket::readyRead, this, [this, client]() {
      QByteArray requestData = client->readAll();
      QString requestStr(requestData);

      // Very simple HTTP routing
      if (requestStr.startsWith("POST /api/v2/auth/login")) {
        if (m_loginSuccess) {
          client->write("HTTP/1.1 200 OK\r\n"
                        "Set-Cookie: SID=test_cookie; path=/; HttpOnly\r\n"
                        "\r\nOk.");
        } else {
          client->write("HTTP/1.1 403 Forbidden\r\n\r\nFails.");
        }
      } else if (requestStr.startsWith("POST /api/v2/torrents/add")) {
        // Extract urls and savepath for testing
        int urlIdx = requestStr.indexOf("name=\"urls\"\r\n\r\n");
        if (urlIdx != -1) {
          int endIdx = requestStr.indexOf("\r\n", urlIdx + 15);
          m_lastAddedUrl = requestStr.mid(urlIdx + 15, endIdx - (urlIdx + 15));
        }

        int pathIdx = requestStr.indexOf("name=\"savepath\"\r\n\r\n");
        if (pathIdx != -1) {
          int endIdx = requestStr.indexOf("\r\n", pathIdx + 19);
          m_lastAddedPath =
              requestStr.mid(pathIdx + 19, endIdx - (pathIdx + 19));
        }

        if (m_addSuccess) {
          client->write("HTTP/1.1 200 OK\r\n\r\nOk.");
        } else {
          client->write("HTTP/1.1 200 OK\r\n\r\nFails.");
        }
      } else {
        client->write("HTTP/1.1 404 Not Found\r\n\r\n");
      }
      client->disconnectFromHost();
    });
  }
};

class QBittorrentConnectorTest : public QObject {
  Q_OBJECT
private slots:
  void initTestCase() {}
  void cleanupTestCase() {}

  void testSuccessfulDispatch() {
    MockQBitServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));

    QBittorrentConnector connector;
    connector.setBaseUrl(
        QString("http://127.0.0.1:%1").arg(server.serverPort()));
    connector.setCredentials("admin", "adminadmin");

    Item testItem;
    testItem.id = "test_item_1";
    testItem.sourcePath = "magnet:?xt=urn:btih:test";
    testItem.destinationPath = "/downloads/test";

    QSignalSpy spy(&connector, &QBittorrentConnector::dispatchFinished);

    connector.dispatch(testItem);

    QVERIFY(spy.wait(1000));
    QCOMPARE(spy.count(), 1);
    QList<QVariant> args = spy.takeFirst();
    QCOMPARE(args.at(0).toString(), QString("test_item_1"));
    QCOMPARE(args.at(1).toBool(), true);

    QCOMPARE(server.m_lastAddedUrl, QString("magnet:?xt=urn:btih:test"));
    QCOMPARE(server.m_lastAddedPath, QString("/downloads/test"));
  }

  void testFailedLogin() {
    MockQBitServer server;
    server.m_loginSuccess = false;
    QVERIFY(server.listen(QHostAddress::LocalHost));

    QBittorrentConnector connector;
    connector.setBaseUrl(
        QString("http://127.0.0.1:%1").arg(server.serverPort()));

    Item testItem;
    testItem.id = "test_item_fail_login";
    testItem.sourcePath = "magnet:?xt=urn:btih:fail";

    QSignalSpy spy(&connector, &QBittorrentConnector::dispatchFinished);

    connector.dispatch(testItem);

    QVERIFY(spy.wait(1000));
    QCOMPARE(spy.count(), 1);
    QList<QVariant> args = spy.takeFirst();
    QCOMPARE(args.at(0).toString(), QString("test_item_fail_login"));
    QCOMPARE(args.at(1).toBool(), false);
    // The message should mention login failed.
    QVERIFY(args.at(2).toString().contains("Login failed"));
  }

  void testFailedAdd() {
    MockQBitServer server;
    server.m_addSuccess = false; // Add returns "Fails."
    QVERIFY(server.listen(QHostAddress::LocalHost));

    QBittorrentConnector connector;
    connector.setBaseUrl(
        QString("http://127.0.0.1:%1").arg(server.serverPort()));

    Item testItem;
    testItem.id = "test_item_fail_add";
    testItem.sourcePath = "magnet:?xt=urn:btih:fail_add";

    QSignalSpy spy(&connector, &QBittorrentConnector::dispatchFinished);

    connector.dispatch(testItem);

    QVERIFY(spy.wait(1000));
    QCOMPARE(spy.count(), 1);
    QList<QVariant> args = spy.takeFirst();
    QCOMPARE(args.at(0).toString(), QString("test_item_fail_add"));
    QCOMPARE(args.at(1).toBool(), false);
    QVERIFY(args.at(2).toString().contains("failure"));
  }
};

QTEST_MAIN(QBittorrentConnectorTest)
#include "QBittorrentConnectorTest.moc"
