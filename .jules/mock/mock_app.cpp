#include <QApplication>
#include <QTimer>
#include <QDir>
#include <QStandardPaths>
#include <iostream>
#include "core/StorageManager.h"
#include "ui/MainWindow.h"
#include "core/Item.h"

void populateMockData(StorageManager& storage) {
    auto now = QDateTime::currentDateTime();

    // Unprocessed item
    Item item1;
    item1.id = "mock_unprocessed_1";
    item1.state = ItemState::Unprocessed;
    item1.sourcePath = "magnet:?xt=urn:btih:mock_magnet_hash_1234567890abcdef&dn=Ubuntu+22.04+Desktop";
    item1.name = "Ubuntu 22.04 Desktop";
    item1.createdTime = now.addDays(-1);
    storage.saveItem(item1);

    Item item1b;
    item1b.id = "mock_unprocessed_2";
    item1b.state = ItemState::Unprocessed;
    item1b.sourcePath = "file:///home/user/Downloads/debian-12.torrent";
    item1b.name = "debian-12.torrent";
    item1b.createdTime = now.addSecs(-1000);
    storage.saveItem(item1b);

    // Queued item
    Item item2;
    item2.id = "mock_queue_1";
    item2.state = ItemState::Queued;
    item2.sourcePath = "file:///tmp/fedora-39.torrent";
    item2.name = "fedora-39.torrent";
    item2.connectorId = "qBittorrent";
    item2.createdTime = now.addSecs(-3600);
    storage.saveItem(item2);

    // Done item
    Item item3;
    item3.id = "mock_done_1";
    item3.state = ItemState::Done;
    item3.sourcePath = "magnet:?xt=urn:btih:mock_arch_linux_magnet&dn=archlinux-2023.iso";
    item3.name = "archlinux-2023.iso";
    item3.connectorId = "Put.io";
    item3.createdTime = now.addDays(-5);
    QJsonObject meta;
    meta["dispatchResult"] = "Successfully added to Put.io";
    meta["lastDispatchTime"] = now.addDays(-5).addSecs(300).toString(Qt::ISODate);
    item3.metadata = meta;
    storage.saveItem(item3);

    // Error item
    Item item4;
    item4.id = "mock_error_1";
    item4.state = ItemState::Failed;
    item4.sourcePath = "https://example.com/invalid-torrent.torrent";
    item4.name = "invalid-torrent.torrent";
    item4.connectorId = "TorBox";
    item4.createdTime = now.addSecs(-60);
    QJsonObject metaErr;
    metaErr["dispatchResult"] = "Invalid format or connection timeout";
    metaErr["error"] = "Connection refused";
    metaErr["lastDispatchTime"] = now.toString(Qt::ISODate);
    item4.metadata = metaErr;
    storage.saveItem(item4);

    // Archived item
    Item item5;
    item5.id = "mock_archive_1";
    item5.state = ItemState::Archived;
    item5.sourcePath = "magnet:?xt=urn:btih:mock_mint_magnet&dn=linuxmint-21.2-cinnamon-64bit.iso";
    item5.name = "linuxmint-21.2-cinnamon-64bit.iso";
    item5.createdTime = now.addDays(-30);
    storage.saveItem(item5);
}

int main(int argc, char *argv[]) {
    // Set a different organization/application name to not mess up real data
    QApplication::setOrganizationName("KMagMuxMock");
    QApplication::setApplicationName("KMagMuxMock");

    qputenv("KMAGMUX_MOCK_MODE", "1");

    QApplication app(argc, argv);

    StorageManager storage;

    // Clean mock dir first
    QString baseDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(baseDir);
    if (dir.exists()) {
        dir.removeRecursively();
    }

    storage.init();
    populateMockData(storage);

    MainWindow window(&storage);

    if (app.arguments().contains("--screenshot")) {
        // Delay to allow UI to render completely, then write a signal to stdout
        QTimer::singleShot(2000, [&]() {
            std::cout << "MOCK_READY" << std::endl;
        });
    }

    window.resize(1024, 768);
    window.show();

    return app.exec();
}
