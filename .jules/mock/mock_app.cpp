#include <QApplication>
#include <QTimer>
#include <QDir>
#include "core/StorageManager.h"
#include "ui/MainWindow.h"
#include <QStandardPaths>
#include <iostream>

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

    // Queued item
    Item item2;
    item2.id = "mock_queue_1";
    item2.state = ItemState::Queued;
    item2.sourcePath = "file:///tmp/debian-11.torrent";
    item2.name = "debian-11.torrent";
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
    item5.sourcePath = "magnet:?xt=urn:btih:mock_fedora_magnet&dn=Fedora-Workstation-Live-x86_64-39-1.5.iso";
    item5.name = "Fedora-Workstation-Live-x86_64-39-1.5.iso";
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
        // Here we could automate the screenshots
        // For now just show and let python script handle or use QTimer to emit screenshot command
        QTimer::singleShot(1000, [&]() {
            std::cout << "MOCK_READY" << std::endl;
        });
    }

    window.show();

    return app.exec();
}
