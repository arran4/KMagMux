#include "core/Item.h"
#include "core/StorageManager.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QObject>
#include <QThread>
#include <iostream>

class DummyReceiver : public QObject {
  Q_OBJECT
public:
  int count = 0;
public slots:
  void onItemUpdated(const Item &) {
    // simulate some UI work
    QThread::msleep(2);
    count++;
  }
  void onItemsUpdated() {
    QThread::msleep(2); // Only single refresh
    count++;
  }
};

int main(int argc, char **argv) {
  QCoreApplication app(argc, argv);
  StorageManager storage;
  storage.init();

  DummyReceiver receiver;
  QObject::connect(&storage, &StorageManager::itemsUpdated, &receiver,
                   &DummyReceiver::onItemsUpdated);

  std::vector<Item> items;
  for (int i = 0; i < 500; ++i) {
    Item item;
    item.id = QString("item_%1").arg(i);
    item.sourcePath = "/path/to/source";
    items.push_back(item);
  }

  QElapsedTimer timer;
  timer.start();
  storage.saveItems(items);

  // Process events to run the slots if they are queued or standard
  app.processEvents();

  qint64 elapsed = timer.elapsed();

  std::cout << "BENCHMARK_RESULT: " << elapsed
            << " ms, count: " << receiver.count << std::endl;

  return 0;
}
#include "benchmark.moc"
