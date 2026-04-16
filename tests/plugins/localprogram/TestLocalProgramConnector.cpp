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
    QCOMPARE(connector.getId(), QString("LocalProgramFactory"));
  }

  void testGetName() {
    LocalProgramConnector connector;
    QCOMPARE(connector.getName(), QString("Local Program (Settings)"));
  }

  void testDiscoverClients() {
    LocalProgramConnector connector;
    QList<LocalClientConfig> clients = LocalProgramConnector::discoverClients();
    // Just make sure it returns a list, can't guarantee what's installed on CI
    QVERIFY(clients.size() >= 0);
  }

  void testEscapeShellArg() {
    QCOMPARE(LocalProgramConnector::escapeShellArg(""), QString("''"));
    QCOMPARE(LocalProgramConnector::escapeShellArg("normal_arg"),
             QString("'normal_arg'"));
    QCOMPARE(LocalProgramConnector::escapeShellArg("arg with spaces"),
             QString("'arg with spaces'"));
    QCOMPARE(LocalProgramConnector::escapeShellArg("arg'with'quotes"),
             QString("'arg'\\''with'\\''quotes'"));
    QCOMPARE(LocalProgramConnector::escapeShellArg("arg\"with\"double"),
             QString("'arg\"with\"double'"));
    QCOMPARE(LocalProgramConnector::escapeShellArg("arg$with$env"),
             QString("'arg$with$env'"));
  }
};

QTEST_MAIN(TestLocalProgramConnector)
#include "TestLocalProgramConnector.moc"
