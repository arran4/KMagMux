#ifndef SINGLEINSTANCESERVER_H
#define SINGLEINSTANCESERVER_H

#include <memory>

#include <QLocalServer>
#include <QLockFile>
#include <QObject>
#include <QString>

class ApplicationRequestDispatcher;

class SingleInstanceServer : public QObject {
  Q_OBJECT

public:
  SingleInstanceServer(const QString &serverName,
                       ApplicationRequestDispatcher *dispatcher,
                       QObject *parent = nullptr);
  ~SingleInstanceServer();

  bool tryAcquire(std::unique_ptr<QLockFile> existingLock = nullptr);

private slots:
  void handleNewConnection();
  static void disconnectClient(QLocalSocket *client);

private:
  QString m_serverName;
  QLocalServer m_server;
  ApplicationRequestDispatcher *m_dispatcher;
  std::unique_ptr<QLockFile> m_lockFile;
};

#endif // SINGLEINSTANCESERVER_H