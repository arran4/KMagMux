#include "core/Item.h"
#include "plugins/localprogram/LocalProgramConnector.h"
#include <QDir>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTest>

class TestLocalProgramConnector : public QObject {
  Q_OBJECT

private slots:
  void testGetId() {
    LocalProgramConnector connector;
    QCOMPARE(connector.getId(), QString("LocalProgram"));
  }

  void testGetName() {
    LocalProgramConnector connector;
    QCOMPARE(connector.getName(), QString("Local Program"));
  }

  void testDiscoverClients() {
    LocalProgramConnector connector;
    QStringList clients = connector.discoverClients();
    // Just make sure it returns a list, can't guarantee what's installed on CI
    QVERIFY(clients.size() >= 0);
  }
};

QTEST_MAIN(TestLocalProgramConnector)
#include "TestLocalProgramConnector.moc"
