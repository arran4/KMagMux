#include "core/StorageManager.h"
#include "ui/MainWindow.h"
#include <QApplication>
#include <QDebug>
#include <QMessageBox>
#include <QLocalSocket>
#include <QLocalServer>
#include <QDataStream>

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);
  app.setApplicationName("KMagMux");
  app.setOrganizationName("KMagMux");
  app.setWindowIcon(QIcon(":/icons/kmagmux.svg"));

  const QString serverName = "KMagMux_SingleInstance";
  QStringList args = app.arguments();

  // Try to connect to existing instance
  QLocalSocket socket;
  socket.connectToServer(serverName);
  if (socket.waitForConnected(500)) {
    qDebug() << "Sending arguments to existing instance...";
    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
    // Remove the program name from arguments
    QStringList passedArgs;
    if (args.size() > 1) {
      passedArgs = args.mid(1);
    }
    out << passedArgs;
    socket.write(block);
    socket.waitForBytesWritten(1000);
    return 0; // Exit since the existing instance will handle it
  }

  // Initialize Core Storage
  StorageManager storage;
  if (!storage.init()) {
    QMessageBox::critical(nullptr, "Error",
                          "Failed to initialize storage directories.");
    return 1;
  }

  // Not running, clean up any stale socket
  QLocalServer::removeServer(serverName);
  QLocalServer server;
  if (!server.listen(serverName)) {
    qWarning() << "Failed to start local server for single instance logic:" << server.errorString();
  }

  MainWindow window(&storage);

  // Handle incoming connections from new instances
  QObject::connect(&server, &QLocalServer::newConnection, [&storage, &server, &window]() {
    QLocalSocket *client = server.nextPendingConnection();
    QObject::connect(client, &QLocalSocket::readyRead, [&storage, client, &window]() {
      QDataStream in(client);
      in.startTransaction();
      QStringList passedArgs;
      in >> passedArgs;
      if (!in.commitTransaction()) {
        return; // Wait for more data
      }

      for (int i = 0; i < passedArgs.size(); ++i) {
        QString arg = passedArgs[i];
        Item newItem;
        newItem.id = QString::number(QDateTime::currentMSecsSinceEpoch()) + "_" +
                     QString::number(i) + "_remote"; // To avoid collision
        newItem.state = ItemState::Unprocessed;
        newItem.sourcePath = arg;
        newItem.createdTime = QDateTime::currentDateTime();

        if (storage.saveItem(newItem)) {
          qDebug() << "Imported item from new instance:" << arg;
        } else {
          qWarning() << "Failed to import item from new instance:" << arg;
        }
      }

      // Bring window to front
      window.show();
      window.raise();
      window.activateWindow();
    });
    QObject::connect(client, &QLocalSocket::disconnected, client, &QLocalSocket::deleteLater);
  });

  // Handle CLI arguments (Files/URLs) from the FIRST instance
  for (int i = 1; i < args.size(); ++i) {
    QString arg = args[i];

    Item newItem;
    newItem.id = QString::number(QDateTime::currentMSecsSinceEpoch()) + "_" +
                 QString::number(i);
    newItem.state = ItemState::Unprocessed;
    newItem.sourcePath = arg;
    newItem.createdTime = QDateTime::currentDateTime();

    if (storage.saveItem(newItem)) {
      qDebug() << "Imported item from CLI:" << arg;
    } else {
      qWarning() << "Failed to import item from CLI:" << arg;
    }
  }

  window.show();

  return app.exec();
}
