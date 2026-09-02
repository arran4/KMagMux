#include "core/Constants.h"
#include "core/Engine.h"
#include "core/StorageManager.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QStandardPaths>
#include <QUuid>
#include <QtTest>

class FakeConnector : public QObject, public Connector {
  Q_OBJECT
  Q_INTERFACES(Connector)
public:
  FakeConnector(const QString &id, QObject *parent = nullptr)
      : QObject(parent), m_id(id) {}
  QString getId() const override { return m_id; }
  QString getName() const override { return m_name.isEmpty() ? m_id : m_name; }
  void setName(const QString &name) { m_name = name; }
  void dispatch(const Item &item) override {
    m_dispatchCount++;
    m_lastDispatchedItemId = item.id;
  }

  bool isEnabled() const override { return m_enabled; }
  void setEnabled(bool enabled) { m_enabled = enabled; }

  int dispatchCount() const { return m_dispatchCount; }
  QString lastDispatchedItemId() const { return m_lastDispatchedItemId; }

private:
  QString m_id;
  QString m_name;
  bool m_enabled = true;
  int m_dispatchCount = 0;
  QString m_lastDispatchedItemId;
};

class TestEngine : public QObject {
  Q_OBJECT
  QTemporaryDir m_configDir;
  QTemporaryDir m_dataDir;

private slots:
  void initTestCase() {
    // Initialize Qt test framework if needed
    QCoreApplication::setOrganizationName("KMagMuxTest");
    QCoreApplication::setApplicationName("TestEngine");
  }

  void init() {
    // Each test gets a clean XDG_DATA_HOME and XDG_CONFIG_HOME.
    // QTemporaryDir manages its own clean path automatically upon creation.
    // We use unique sub-directories under m_dataDir and m_configDir for each
    // test to ensure isolation, rather than recreating the root directory.

    // To ensure fresh paths for each test run without dealing with
    // QTemporaryDir scoping in a class, we set XDG_DATA_HOME to a sub-folder
    // unique to this run.
    QString testSpecificDataHome =
        m_dataDir.path() + "/data_" +
        QUuid::createUuid().toString(QUuid::WithoutBraces);
    QDir().mkpath(testSpecificDataHome + "/KMagMuxTest/TestEngine/data");
    qputenv("XDG_DATA_HOME", testSpecificDataHome.toLocal8Bit());

    QString testSpecificConfigHome =
        m_configDir.path() + "/config_" +
        QUuid::createUuid().toString(QUuid::WithoutBraces);
    QDir().mkpath(testSpecificConfigHome);
    qputenv("XDG_CONFIG_HOME", testSpecificConfigHome.toLocal8Bit());

    // Verify that the resolved AppDataLocation actually respects the XDG_DATA_HOME override
    // and is completely empty to prevent test contamination.
    QString appDataLocation = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QVERIFY2(appDataLocation.startsWith(testSpecificDataHome), "QStandardPaths did not respect XDG_DATA_HOME!");

    // Explicitly clean it just in case QStandardPaths somehow returned an old path or it isn't empty
    QDir appDataDir(appDataLocation);
    if (appDataDir.exists()) {
        appDataDir.removeRecursively();
    }
    appDataDir.mkpath(".");
  }

  void testEnginePluginLoading() {
    StorageManager storage;
    QVERIFY(storage.init());

    QString appDir = QCoreApplication::applicationDirPath();
    qDebug() << "App Dir:" << appDir;
    qDebug() << "Working Dir:" << QDir::currentPath();

    // Ensure the plugins directory exists where Engine looks for it
    QDir dir(appDir);
    if (!dir.exists("plugins")) {
      dir.mkdir("plugins");
      qDebug() << "Created plugins directory at:"
               << dir.absoluteFilePath("plugins");
    }

    // Get the mock plugin filename injected by CMake based on the current
    // platform
    QString mockPluginFilename = QStringLiteral(MOCK_PLUGIN_FILENAME);

    // We know the mock plugin is built at ${CMAKE_BINARY_DIR}/tests/plugins/
    // We can copy it to appDir/plugins where the engine will look for it
    QString sourcePluginPath =
        QDir::cleanPath(appDir + "/../plugins/" + mockPluginFilename);
    QString destPluginPath =
        QDir::cleanPath(appDir + "/plugins/" + mockPluginFilename);

    if (QFile::exists(sourcePluginPath) && !QFile::exists(destPluginPath)) {
      QFile::copy(sourcePluginPath, destPluginPath);
      qDebug() << "Copied mock plugin from" << sourcePluginPath << "to"
               << destPluginPath;
    } else if (!QFile::exists(destPluginPath)) {
      // Try alternate path if running from build dir
      QString altPath =
          QDir::cleanPath(appDir + "/../../plugins/" + mockPluginFilename);
      if (QFile::exists(altPath)) {
        QFile::copy(altPath, destPluginPath);
        qDebug() << "Copied mock plugin from" << altPath << "to"
                 << destPluginPath;
      }
    }

    Engine engine(&storage);

    QStringList connectors = engine.getAvailableConnectors();

    qDebug() << "Connectors found:" << connectors;

    // Ensure we loaded the MockConnector successfully
    QVERIFY(!connectors.isEmpty());
    QVERIFY(connectors.contains("MockConnector"));
  }

  void testEngineDispatchLegacyDefaultFallback() {
    StorageManager storage;
    QVERIFY(storage.init());
    Engine engine(&storage);

    FakeConnector *qbt = new FakeConnector(Constants::QBittorrentConnectorId);
    qbt->setEnabled(false);
    engine.setConnectorForTesting(qbt);

    Item item;
    item.id = "test-legacy";
    item.connectorId = Constants::DefaultActionName;
    item.state = ItemState::Queued;
    storage.saveItem(item);

    QMetaObject::invokeMethod(&engine, "processQueue", Qt::DirectConnection);

    auto opt = storage.loadItem("test-legacy");
    QVERIFY(opt.has_value());

    // Disabled qBittorrent -> Should fail explicitly with missing connector
    QCOMPARE(opt->state, ItemState::Failed);
    QCOMPARE(opt->metadata["dispatchResult"].toString(),
             QString("No suitable connector found."));

    // Try again with enabled qBittorrent to ensure legacy fallback actually
    // works when enabled
    qbt->setEnabled(true);
    item.state = ItemState::Queued;
    QSignalSpy spy(&engine, SIGNAL(actionMessage(QString)));

    storage.saveItem(item);
    QMetaObject::invokeMethod(&engine, "processQueue", Qt::DirectConnection);

    QCOMPARE(qbt->dispatchCount(), 1);
    QCOMPARE(qbt->lastDispatchedItemId(), QString("test-legacy"));

    // One dispatch signal with the explicit name, since fake connector's id is
    // qBittorrent It should have emitted "Dispatching item to qBittorrent:
    // test-legacy"
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(0).toString(),
             QString("Dispatching item to qBittorrent: test-legacy"));
  }

  void testEngineActionMessageExactCountAndDisplayName() {
    StorageManager storage;
    QVERIFY(storage.init());
    Engine engine(&storage);

    FakeConnector *myConn = new FakeConnector("MyConnId");
    myConn->setName("My Fancy Display Name");
    engine.setConnectorForTesting(myConn);

    Item item;
    item.id = "test-msg";
    item.connectorId = "MyConnId";
    item.state = ItemState::Queued;
    storage.saveItem(item);

    QSignalSpy spy(&engine, SIGNAL(actionMessage(QString)));

    QMetaObject::invokeMethod(&engine, "processQueue", Qt::DirectConnection);

    QCOMPARE(myConn->dispatchCount(), 1);

    // Should emit EXACTLY one "Dispatching..." message, and it MUST use the
    // display name
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(0).toString(),
             QString("Dispatching item to My Fancy Display Name: test-msg"));

    // Now simulate finished via queued signal since onDispatchFinished is
    // private
    QMetaObject::invokeMethod(&engine, "onDispatchFinished",
                              Qt::DirectConnection, Q_ARG(QString, "test-msg"),
                              Q_ARG(bool, true), Q_ARG(QString, "OK"),
                              Q_ARG(QJsonObject, QJsonObject()));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(0).toString(),
             QString("Sent to Connector My Fancy Display Name: successful"));
  }

  void testEngineDispatchExplicitFailsIfDisabled() {
    StorageManager storage;
    QVERIFY(storage.init());
    Engine engine(&storage);

    FakeConnector *testConn = new FakeConnector("TestExplicitConnector");
    testConn->setEnabled(false);
    engine.setConnectorForTesting(testConn);

    FakeConnector *qbt = new FakeConnector(Constants::QBittorrentConnectorId);
    qbt->setEnabled(true); // Should NOT fallback to qbittorrent
    engine.setConnectorForTesting(qbt);

    Item item;
    item.id = "test-explicit-disabled";
    item.connectorId = "TestExplicitConnector";
    item.state = ItemState::Queued;
    storage.saveItem(item);

    QMetaObject::invokeMethod(&engine, "processQueue", Qt::DirectConnection);

    auto opt = storage.loadItem("test-explicit-disabled");
    QVERIFY(opt.has_value());

    QCOMPARE(opt->state, ItemState::Failed); // Should explicitly fail, NOT
                                             // fallback to the enabled QBT
    QCOMPARE(opt->metadata["dispatchResult"].toString(),
             QString("No suitable connector found."));
  }

  void testEngineDispatchExplicitFailsIfMissing() {
    StorageManager storage;
    QVERIFY(storage.init());
    Engine engine(&storage);

    FakeConnector *qbt = new FakeConnector(Constants::QBittorrentConnectorId);
    qbt->setEnabled(true); // Should NOT fallback to qbittorrent
    engine.setConnectorForTesting(qbt);

    Item item;
    item.id = "test-explicit";
    item.connectorId = "MissingConnectorId";
    item.state = ItemState::Queued;
    storage.saveItem(item);

    QMetaObject::invokeMethod(&engine, "processQueue", Qt::DirectConnection);

    auto opt = storage.loadItem("test-explicit");
    QVERIFY(opt.has_value());
    QCOMPARE(opt->state,
             ItemState::Failed); // Should explicitly fail, NOT fallback
    QCOMPARE(opt->metadata["dispatchResult"].toString(),
             QString("No suitable connector found."));
  }

  void testEngineDispatchEmptyIDFallsBackToQBittorrent() {
    StorageManager storage;
    QVERIFY(storage.init());
    Engine engine(&storage);

    FakeConnector *qbt = new FakeConnector(Constants::QBittorrentConnectorId);
    qbt->setEnabled(false);
    engine.setConnectorForTesting(qbt);

    Item item;
    item.id = "test-empty";
    item.connectorId = "";
    item.state = ItemState::Queued;
    storage.saveItem(item);

    QMetaObject::invokeMethod(&engine, "processQueue", Qt::DirectConnection);

    auto opt = storage.loadItem("test-empty");
    QVERIFY(opt.has_value());

    QCOMPARE(opt->state, ItemState::Failed);
    QCOMPARE(opt->metadata["dispatchResult"].toString(),
             QString("No suitable connector found."));

    // Try again with enabled qBittorrent to ensure empty string fallback
    // actually works when enabled
    qbt->setEnabled(true);
    item.state = ItemState::Queued;
    storage.saveItem(item);
    QMetaObject::invokeMethod(&engine, "processQueue", Qt::DirectConnection);

    QCOMPARE(qbt->dispatchCount(), 1);
    QCOMPARE(qbt->lastDispatchedItemId(), QString("test-empty"));
  }
};

QTEST_MAIN(TestEngine)
#include "TestEngine.moc"
