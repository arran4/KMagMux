#include <QApplication>
#include <QMessageBox>
#include <QDebug>
#include "core/StorageManager.h"
#include "ui/MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("KMagMux");
    app.setOrganizationName("KMagMux");

    // Initialize Core Storage
    StorageManager storage;
    if (!storage.init()) {
        QMessageBox::critical(nullptr, "Error", "Failed to initialize storage directories.");
        return 1;
    }

    // --- Persistence Test (Removed) ---
    // Item testItem;
    // ...

    // Handle CLI arguments (Files/URLs)
    QStringList args = app.arguments();
    // Skip the first argument (program name)
    for (int i = 1; i < args.size(); ++i) {
        QString arg = args[i];

        Item newItem;
        // Generate a simple ID based on timestamp and random/hash (for now just timestamp)
        newItem.id = QString::number(QDateTime::currentMSecsSinceEpoch()) + "_" + QString::number(i);
        newItem.state = ItemState::Unprocessed;
        newItem.sourcePath = arg;
        newItem.createdTime = QDateTime::currentDateTime();

        if (storage.saveItem(newItem)) {
            qDebug() << "Imported item from CLI:" << arg;
        } else {
            qWarning() << "Failed to import item from CLI:" << arg;
        }
    }

    MainWindow window(&storage);
    window.show();

    return app.exec();
}
