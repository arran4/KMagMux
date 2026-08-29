#include "core/Constants.h"
#include "core/Engine.h"
#include "core/StorageManager.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QtTest>

class FakeConnector : public QObject, public Connector {
  Q_OBJECT
  Q_INTERFACES(Connector)
public:
  FakeConnector(const QString &id, QObject *parent = nullptr) : QObject(parent), m_id(id) {}
  QString getId() const override { return m_id; }
  QString getName() const override { return m_id; }
  void dispatch(const Item &item) override { Q_UNUSED(item); }

  bool isEnabled() const override { return m_enabled; }
  void setEnabled(bool enabled) { m_enabled = enabled; }

private:
  QString m_id;
  bool m_enabled = true;
};

class TestEngine : public QObject {
  Q_OBJECT
  QTemporaryDir m_configDir;

private slots:
  void initTestCase() {
    // Initialize Qt test framework if needed
    QCoreApplication::setOrganizationName("KMagMuxTest");
    QCoreApplication::setApplicationName("TestEngine");

    // Ensure we can write to storage locally without test paths that fail creation
    QString dataHome = QDir::currentPath() + "/xdg_data";
    qputenv("XDG_DATA_HOME", dataHome.toLocal8Bit());
    QDir().mkpath(dataHome + "/KMagMuxTest/TestEngine/data");

    qputenv("XDG_CONFIG_HOME", m_configDir.path().toLocal8Bit());
  }

  void testEnginePluginLoading() {
    StorageManager storage;

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
    QCOMPARE(opt->metadata["dispatchResult"].toString(), QString("No suitable connector found."));

    // Try again with enabled qBittorrent to ensure legacy fallback actually works when enabled
    qbt->setEnabled(true);
    item.state = ItemState::Queued;
    storage.saveItem(item);
    QMetaObject::invokeMethod(&engine, "processQueue", Qt::DirectConnection);

    opt = storage.loadItem("test-legacy");
    QVERIFY(opt->metadata["dispatchResult"].toString() != "No suitable connector found.");
  }

  void testEngineDispatchExplicitFailsIfDisabled() {
    StorageManager storage;
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

    QCOMPARE(opt->state, ItemState::Failed); // Should explicitly fail, NOT fallback to the enabled QBT
    QCOMPARE(opt->metadata["dispatchResult"].toString(), QString("No suitable connector found."));
  }

  void testEngineDispatchExplicitFailsIfMissing() {
    StorageManager storage;
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
    QCOMPARE(opt->state, ItemState::Failed); // Should explicitly fail, NOT fallback
    QCOMPARE(opt->metadata["dispatchResult"].toString(), QString("No suitable connector found."));
  }

  void testEngineDispatchEmptyIDFallsBackToQBittorrent() {
    StorageManager storage;
    storage.deleteItem("test-legacy"); storage.deleteItem("test-explicit-disabled"); storage.deleteItem("test-explicit"); storage.deleteItem("test-empty");
    storage.deleteItem("test-legacy"); storage.deleteItem("test-explicit-disabled"); storage.deleteItem("test-explicit"); storage.deleteItem("test-empty");
    storage.deleteItem("test-legacy"); storage.deleteItem("test-explicit-disabled"); storage.deleteItem("test-explicit"); storage.deleteItem("test-empty");
    storage.deleteItem("test-legacy"); storage.deleteItem("test-explicit-disabled"); storage.deleteItem("test-explicit"); storage.deleteItem("test-empty");
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
    QCOMPARE(opt->metadata["dispatchResult"].toString(), QString("No suitable connector found."));
  }
};

QTEST_MAIN(TestEngine)
#include "TestEngine.moc"
