#include <QCoreApplication>
#include <QElapsedTimer>
#include <QDebug>
#include "src/core/Engine.h"
#include "src/core/StorageManager.h"
#include "src/core/Item.h"
#include <iostream>

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    StorageManager storage;
    storage.init();

    Engine engine(&storage);

    // clear data dir
    system("rm -rf /home/jules/.local/share/bench/data/*");

    // create 1000 items, most NOT queued or scheduled
    for (int i = 0; i < 1000; ++i) {
        Item item;
        item.id = QString("item_%1").arg(i);
        item.state = ItemState::Done;
        storage.saveItem(item);
    }
    // create a few queued/scheduled items
    for (int i = 1000; i < 1010; ++i) {
        Item item;
        item.id = QString("item_%1").arg(i);
        item.state = ItemState::Queued;
        storage.saveItem(item);
    }

    QElapsedTimer timer;
    timer.start();

    for (int i = 0; i < 100; ++i) {
        QMetaObject::invokeMethod(&engine, "processQueue");
    }

    qDebug() << "Baseline processQueue (100x 1000 items):" << timer.elapsed() << "ms";
    return 0;
}
