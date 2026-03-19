cat << 'DIFF' > main.diff
<<<<<<< SEARCH
static void setupIpcHandler(QLocalServer &server, StorageManager &storage,
                            MainWindow &window) {
  // Handle incoming connections from new instances
  QObject::connect(
      &server, &QLocalServer::newConnection, [&storage, &server, &window]() {
        QLocalSocket *client = server.nextPendingConnection();
        QObject::connect(
            client, &QLocalSocket::readyRead, [&storage, client, &window]() {
=======
static void setupIpcHandler(QLocalServer &server, StorageManager &storage,
                            MainWindow *window) {
  // Handle incoming connections from new instances
  QObject::connect(
      &server, &QLocalServer::newConnection, [&storage, &server, window]() {
        QLocalSocket *client = server.nextPendingConnection();
        QObject::connect(
            client, &QLocalSocket::readyRead, [&storage, client, window]() {
>>>>>>> REPLACE
<<<<<<< SEARCH
              // Bring window to front
              window.show();
              window.raise();
              window.activateWindow();
            });
=======
              // Bring window to front
              window->show();
              window->raise();
              window->activateWindow();
            });
>>>>>>> REPLACE
<<<<<<< SEARCH
  QLocalServer server;
  setupLocalServer(server, serverName);

  MainWindow window(&storage);
  setupIpcHandler(server, storage, window);
  processCliArgs(args, storage);

  window.show();

  return app.exec();
}
=======
  QLocalServer server;
  setupLocalServer(server, serverName);

  MainWindow *window = new MainWindow(&storage);
  setupIpcHandler(server, storage, window);
  processCliArgs(args, storage);

  window->show();

  return app.exec();
}
>>>>>>> REPLACE
DIFF
