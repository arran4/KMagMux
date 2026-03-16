#include "core/StorageManager.h"
#include "ui/MainWindow.h"
#include <KAboutData>
#include <KLocalizedString>
#include <QApplication>
#include <QDataStream>
#include <QDebug>
#include <QFileInfo>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMessageBox>
#include <QUrl>

static bool isValidInput(const QString &arg) {
  if (arg.startsWith("magnet:?")) {
    return true;
  }
  if (arg.startsWith("magnet:") && arg.length() > 7) {
    return true;
  }
  QUrl url(arg);
  if (url.isValid() && (url.scheme() == "http" || url.scheme() == "https")) {
    return true;
  }

  QString pathToCheck = arg;
  if (arg.startsWith("file://")) {
    pathToCheck = QUrl(arg).toLocalFile();
  }
  QFileInfo fi(pathToCheck);
  return fi.exists() && fi.isFile();
}

static QString setupApplication(QApplication &app) {
  app.setApplicationName("KMagMux");
  app.setOrganizationName("KMagMux");
  app.setWindowIcon(QIcon(":/icons/kmagmux.svg"));

  QString appName = "kmagmux";
#ifdef QT_DEBUG
  appName = "kmagmux-dev1";
#endif

  app.setApplicationName(
      appName); // This will affect the config directory as well
  return appName + "_SingleInstance";
}

static bool sendArgsToExistingInstance(const QString &serverName,
                                       const QStringList &args) {
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
    return true; // Exit since the existing instance will handle it
  }
  return false;
}

static void setupLocalServer(QLocalServer &server, const QString &serverName) {
  // Not running, clean up any stale socket
  QLocalServer::removeServer(serverName);
  server.setSocketOptions(QLocalServer::UserAccessOption);
  if (!server.listen(serverName)) {
    qWarning() << "Failed to start local server for single instance logic:"
               << server.errorString();
  }
}

static void setupIpcHandler(QLocalServer &server, StorageManager &storage,
                            MainWindow &window) {
  // Handle incoming connections from new instances
  QObject::connect(
      &server, &QLocalServer::newConnection, [&storage, &server, &window]() {
        QLocalSocket *client = server.nextPendingConnection();
        QObject::connect(
            client, &QLocalSocket::readyRead, [&storage, client, &window]() {
              QDataStream in(client);
              in.startTransaction();
              QStringList passedArgs;
              in >> passedArgs;
              if (!in.commitTransaction()) {
                return; // Wait for more data
              }

              for (int i = 0; i < passedArgs.size(); ++i) {
                QString arg = passedArgs[i];
                if (!isValidInput(arg)) {
                  qWarning()
                      << "Invalid input received from IPC, ignoring:" << arg;
                  continue;
                }

                Item newItem;
                newItem.id =
                    QString::number(QDateTime::currentMSecsSinceEpoch()) + "_" +
                    QString::number(i) + "_remote"; // To avoid collision
                newItem.state = ItemState::Unprocessed;
                newItem.sourcePath = arg;
                newItem.createdTime = QDateTime::currentDateTime();

                if (storage.saveItem(newItem)) {
                  qDebug() << "Imported item from new instance:" << arg;
                } else {
                  qWarning()
                      << "Failed to import item from new instance:" << arg;
                }
              }

              // Bring window to front
              window.show();
              window.raise();
              window.activateWindow();
            });
        QObject::connect(client, &QLocalSocket::disconnected, client,
                         &QLocalSocket::deleteLater);
      });
}

static void processCliArgs(const QStringList &args, StorageManager &storage) {
  // Handle CLI arguments (Files/URLs) from the FIRST instance
  for (int i = 1; i < args.size(); ++i) {
    QString arg = args[i];

    if (!isValidInput(arg)) {
      qWarning() << "Invalid input received from CLI, ignoring:" << arg;
      continue;
    }

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
}

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);

  KLocalizedString::setApplicationDomain("kmagmux");
  KAboutData aboutData("kmagmux", i18n("KMagMux"), "0.1");
  KAboutData::setApplicationData(aboutData);

  const QString serverName = setupApplication(app);
  QStringList args = app.arguments();

  if (sendArgsToExistingInstance(serverName, args)) {
    return 0;
  }

  // Initialize Core Storage
  StorageManager storage;
  if (!storage.init()) {
    QMessageBox::critical(nullptr, "Error",
                          "Failed to initialize storage directories.");
    return 1;
  }

  QLocalServer server;
  setupLocalServer(server, serverName);

  MainWindow window(&storage);
  setupIpcHandler(server, storage, window);
  processCliArgs(args, storage);

  window.show();

  return app.exec();
}
