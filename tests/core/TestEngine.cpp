#include "core/Engine.h"
#include "core/StorageManager.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QtTest>

class TestEngine : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() {
    // Initialize Qt test framework if needed
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
    } else if (QFile::exists(destPluginPath)) {
      qDebug() << "Mock plugin already exists at" << destPluginPath;
    } else {
      qWarning() << "Could not find mock plugin at" << sourcePluginPath;
      // Maybe the path is different depending on CMake generator/version
      QDir searchDir(appDir);
      searchDir.cdUp(); // tests
      searchDir.cdUp(); // build
      QString altPath =
          searchDir.absoluteFilePath("tests/plugins/" + mockPluginFilename);
      if (QFile::exists(altPath) && !QFile::exists(destPluginPath)) {
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
};

QTEST_MAIN(TestEngine)
#include "TestEngine.moc"
