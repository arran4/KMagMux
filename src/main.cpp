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
#include <QPointer>
#include <QUrl>

namespace {

QString setupApplication(QApplication &app) {
  QApplication::setApplicationName("KMagMux");
  QApplication::setOrganizationName("KMagMux");
  QApplication::setWindowIcon(QIcon(":/icons/kmagmux.svg"));

  QString appName = "kmagmux";
#ifdef QT_DEBUG
  appName = "kmagmux-dev1";
#endif

  app.setApplicationName(
      appName); // This will affect the config directory as well
  return appName + "_SingleInstance";
}

bool sendArgsToExistingInstance(const QString &serverName,
                                const QStringList &args) {
  QLocalSocket socket;
  socket.connectToServer(serverName);
  const int connectTimeoutMs = 500;
  if (socket.waitForConnected(connectTimeoutMs)) {
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
    const int writeTimeoutMs = 1000;
    socket.waitForBytesWritten(writeTimeoutMs);
    return true; // Exit since the existing instance will handle it
  }
  return false;
}

void setupLocalServer(QLocalServer &server, const QString &serverName) {
  // Not running, clean up any stale socket
  QLocalServer::removeServer(serverName);
  server.setSocketOptions(QLocalServer::UserAccessOption);
  if (!server.listen(serverName)) {
    qWarning() << "Failed to start local server for single instance logic:"
               << server.errorString();
  }
}

void setupIpcHandler(QLocalServer &server, MainWindow *window) {
  // Handle incoming connections from new instances
  const QPointer<MainWindow> windowPtr(window);
  QObject::connect(
      &server, &QLocalServer::newConnection, [&server, windowPtr]() {
        QLocalSocket *client = server.nextPendingConnection();
        QObject::connect(client, &QLocalSocket::readyRead,
                         [client, windowPtr]() {
                           QDataStream dataStream(client);
                           dataStream.startTransaction();
                           QStringList passedArgs;
                           dataStream >> passedArgs;
                           if (!dataStream.commitTransaction()) {
                             return; // Wait for more data
                           }

                           // Delegate all argument parsing to processAddedLines
                           // which uses ItemParser as the single source of
                           // truth
                           QStringList validLines;

                           for (int i = 0; i < passedArgs.size(); ++i) {
                             validLines.append(passedArgs[i]);
                           }

                           // Bring window to front
                           if (windowPtr) {
                             windowPtr->show();
                             windowPtr->raise();
                             windowPtr->activateWindow();

                             if (!validLines.isEmpty()) {
                               windowPtr->processAddedLines(validLines);
                             }
                           }
                         });
        QObject::connect(client, &QLocalSocket::disconnected, client,
                         &QLocalSocket::deleteLater);
      });
}

void processCliArgs(const QStringList &args, MainWindow *windowPtr) {
  // Handle CLI arguments (Files/URLs) from the FIRST instance
  QStringList validLines;
  for (int i = 1; i < args.size(); ++i) {
    validLines.append(args[i]);
  }

  if (windowPtr) {
    if (!validLines.isEmpty()) {
      windowPtr->processAddedLines(validLines);
    }
  }
}

} // namespace

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);

  KLocalizedString::setApplicationDomain("kmagmux");
  const KAboutData aboutData("kmagmux", i18n("KMagMux"), "0.1");
  KAboutData::setApplicationData(aboutData);

  const QString serverName = setupApplication(app);
  const QStringList args = QApplication::arguments();

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

  MainWindow *window = new MainWindow(&storage);
  window->setObjectName("KMagMuxMainWindow");
  setupIpcHandler(server, window);
  processCliArgs(args, window);

  window->show();

  return QApplication::exec();
}
