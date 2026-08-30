#include "core/Constants.h"
#include "core/DefaultConnectors.h"
#include <QCoreApplication>
#include <QSettings>
#include <QtTest>

class TestDefaultConnectors : public QObject {
  Q_OBJECT
  QTemporaryDir m_configDir;

private slots:
  void initTestCase() {
    QCoreApplication::setOrganizationName("KMagMuxTest");
    QCoreApplication::setApplicationName("TestDefaultConnectors");

    // Ensure we can write to storage locally without test paths that fail creation
    QString configHome = QDir::currentPath() + "/xdg_config";
    qputenv("XDG_CONFIG_HOME", configHome.toLocal8Bit());
  }

  void init() {
    QSettings settings;
    settings.clear();
  }

  void testAbsentSettingQBittorrentAvailable() {
    QStringList available = {Constants::QBittorrentConnectorId, "Premiumize"};
    QStringList defaults = DefaultConnectors::get(available);
    QCOMPARE(defaults.size(), 1);
    QCOMPARE(defaults.first(), QString(Constants::QBittorrentConnectorId));
  }

  void testAbsentSettingQBittorrentUnavailable() {
    QStringList available = {"Premiumize", "RealDebrid"};
    QStringList defaults = DefaultConnectors::get(available);
    QVERIFY(defaults.isEmpty());
  }

  void testExplicitlyEmpty() {
    DefaultConnectors::set({}); // explicitly set to empty
    QStringList available = {Constants::QBittorrentConnectorId, "Premiumize"};
    QStringList defaults = DefaultConnectors::get(available);
    QVERIFY(defaults.isEmpty());
  }

  void testOneValidConnector() {
    DefaultConnectors::set({"Premiumize"});
    QStringList available = {"qBittorrent", "Premiumize", "RealDebrid"};
    QStringList defaults = DefaultConnectors::get(available);
    QCOMPARE(defaults.size(), 1);
    QCOMPARE(defaults.first(), QString("Premiumize"));
  }

  void testMultipleValidConnectors() {
    DefaultConnectors::set({"Premiumize", "RealDebrid"});
    QStringList available = {"qBittorrent", "Premiumize", "RealDebrid"};
    QStringList defaults = DefaultConnectors::get(available);
    QCOMPARE(defaults.size(), 2);
    QCOMPARE(defaults.at(0), QString("Premiumize"));
    QCOMPARE(defaults.at(1), QString("RealDebrid"));
  }

  void testStaleOnly() {
    DefaultConnectors::set({"Stale1", "Stale2"});
    QStringList available = {"qBittorrent", "Premiumize"};
    QStringList defaults = DefaultConnectors::get(available);
    QVERIFY(defaults.isEmpty());
  }

  void testDisabledUnavailableOnly() {
    DefaultConnectors::set(
        {"Premiumize"}); // configured but not in available list
    QStringList available = {"qBittorrent"};
    QStringList defaults = DefaultConnectors::get(available);
    QVERIFY(defaults.isEmpty());
  }

  void testStaleAndValid() {
    DefaultConnectors::set({"Stale", "Premiumize", "Stale2"});
    QStringList available = {"Premiumize", "RealDebrid"};
    QStringList defaults = DefaultConnectors::get(available);
    QCOMPARE(defaults.size(), 1);
    QCOMPARE(defaults.first(), QString("Premiumize"));
  }

  void testDefaultOnly() {
    DefaultConnectors::set({Constants::DefaultActionName});
    QStringList available = {Constants::QBittorrentConnectorId, "Premiumize"};
    QStringList defaults = DefaultConnectors::get(available);
    QCOMPARE(defaults.size(), 1);
    QCOMPARE(defaults.first(), QString(Constants::QBittorrentConnectorId));
  }

  void testDefaultAndAnotherValid() {
    DefaultConnectors::set({Constants::DefaultActionName, "Premiumize"});
    QStringList available = {Constants::QBittorrentConnectorId, "Premiumize"};
    QStringList defaults = DefaultConnectors::get(available);
    QCOMPARE(defaults.size(), 2);
    QCOMPARE(defaults.at(0), QString(Constants::QBittorrentConnectorId));
    QCOMPARE(defaults.at(1), QString("Premiumize"));
  }

  void testDefaultAndQBittorrentDeduplication() {
    DefaultConnectors::set(
        {Constants::DefaultActionName, Constants::QBittorrentConnectorId});
    QStringList available = {Constants::QBittorrentConnectorId, "Premiumize"};
    QStringList defaults = DefaultConnectors::get(available);
    QCOMPARE(defaults.size(), 1);
    QCOMPARE(defaults.first(), QString(Constants::QBittorrentConnectorId));
  }

  void testDefaultWhileQBittorrentUnavailable() {
    DefaultConnectors::set({Constants::DefaultActionName});
    QStringList available = {"Premiumize", "RealDebrid"};
    QStringList defaults = DefaultConnectors::get(available);
    QVERIFY(defaults.isEmpty());
  }

  void testDuplicateConfiguredIDs() {
    DefaultConnectors::set(
        {"Premiumize", "Premiumize", "RealDebrid", "Premiumize"});
    QStringList available = {"Premiumize", "RealDebrid"};
    QStringList defaults = DefaultConnectors::get(available);
    QCOMPARE(defaults.size(), 2);
    QCOMPARE(defaults.at(0), QString("Premiumize"));
    QCOMPARE(defaults.at(1), QString("RealDebrid"));
  }

  void testConfiguredOrderingPreserved() {
    DefaultConnectors::set({"RealDebrid", "Premiumize"});
    QStringList available = {"qBittorrent", "Premiumize", "RealDebrid"};
    QStringList defaults = DefaultConnectors::get(available);
    QCOMPARE(defaults.size(), 2);
    QCOMPARE(defaults.at(0), QString("RealDebrid"));
    QCOMPARE(defaults.at(1), QString("Premiumize"));
  }

  void testAvailableListOrderingDoesNotAlterResult() {
    DefaultConnectors::set({"Premiumize", "RealDebrid"});
    QStringList available = {"RealDebrid", "Premiumize", "qBittorrent"};
    QStringList defaults = DefaultConnectors::get(available);
    QCOMPARE(defaults.size(), 2);
    QCOMPARE(defaults.at(0), QString("Premiumize"));
    QCOMPARE(defaults.at(1), QString("RealDebrid"));
  }

  void testConfiguredValuesAllUnavailable() {
    DefaultConnectors::set({"Premiumize", "RealDebrid"});
    QStringList available = {"qBittorrent"};
    QStringList defaults = DefaultConnectors::get(available);
    QVERIFY(defaults.isEmpty()); // Should not fallback to qBittorrent
  }

  void testSaveEmptyRemainsExplicitlyEmpty() {
    DefaultConnectors::set({});
    QSettings settings;
    QVERIFY(settings.contains("defaultConnectors"));
    QStringList available = {"qBittorrent"};
    QStringList defaults = DefaultConnectors::get(available);
    QVERIFY(defaults.isEmpty());
  }
};

QTEST_MAIN(TestDefaultConnectors)
#include "TestDefaultConnectors.moc"
