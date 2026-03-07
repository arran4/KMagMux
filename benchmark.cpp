#include <QCoreApplication>
#include <QElapsedTimer>
#include <QDebug>
#include "src/core/StorageManager.h"
#include "src/core/Item.h"
#include <iostream>

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    StorageManager storage;
    storage.init();

    // create 1000 items
    for (int i = 0; i < 1000; ++i) {
        Item item;
        item.id = QString("item_%1").arg(i);
        item.state = ItemState::Done;
        storage.saveItem(item);
    }

    QElapsedTimer timer;
    timer.start();

    for (int i = 0; i < 100; ++i) {
        auto items = storage.loadAllItems();
    }

    qDebug() << "Baseline loadAllItems (100x 1000 items):" << timer.elapsed() << "ms";
    return 0;
}
