#include "core/Item.h"
#include "core/StorageManager.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QtTest>

class TestStorageManager : public QObject {
  Q_OBJECT

private slots:
  void testSaveItemEmptyId() {
    StorageManager storage;
    Item item;
    item.id = "";
    QVERIFY(!storage.saveItem(item));
  }

  void testSaveItemSuccess() {
    StorageManager storage;
    QVERIFY(storage.init());

    Item item;
    item.id = "test_item_success";
    item.sourcePath = "/path/to/source";

    QVERIFY(storage.saveItem(item));

    // Verify file exists
    QString dataDir = storage.getDataDir();
    QString filePath = dataDir + "/test_item_success.json";
    QVERIFY(QFile::exists(filePath));

    // Cleanup
    QFile::remove(filePath);
  }

  void testSaveItemWriteFailure() {
    StorageManager storage;
    QVERIFY(storage.init());

    QString dataDir = storage.getDataDir();
    QDir dir(dataDir);

    // Create a directory with the same name as the expected file
    // This will cause QFile::open(WriteOnly) to fail
    QString fakeDirName = "unwritable_item.json";
    QVERIFY(dir.mkdir(fakeDirName));

    Item item;
    item.id = "unwritable_item";
    item.sourcePath = "/path/to/source";

    // Should return false because it can't open the file for writing
    QVERIFY(!storage.saveItem(item));

    // Cleanup
    QVERIFY(dir.rmdir(fakeDirName));
  }
};

QTEST_MAIN(TestStorageManager)
#include "TestStorageManager.moc"
