#ifndef SINGLEINSTANCESERVER_H
#define SINGLEINSTANCESERVER_H

#include "IpcProtocol.h"
#include <QLocalServer>
#include <QLockFile>
#include <QObject>

class ApplicationRequestDispatcher;

class SingleInstanceServer : public QObject {
  Q_OBJECT
public:
  explicit SingleInstanceServer(const QString &serverName,
                                ApplicationRequestDispatcher *dispatcher,
                                QObject *parent = nullptr);
  ~SingleInstanceServer();

  bool tryAcquire();

private slots:
  void handleNewConnection();

private:
  QString m_serverName;
  QLocalServer m_server;
  QLockFile *m_lockFile;
  ApplicationRequestDispatcher *m_dispatcher;
};

#endif // SINGLEINSTANCESERVER_H
