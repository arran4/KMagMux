#ifndef STARTUPCOORDINATOR_H
#define STARTUPCOORDINATOR_H

#include "IpcProtocol.h"
#include <QString>
#include <QStringList>

class StartupCoordinator {
public:
  StartupCoordinator(const QString &serverName);

  // Returns true if this process should run as the primary application,
  // or false if this process was a client/launcher and has completed its job
  // (or failed).
  bool coordinate(const QStringList &args);

private:
  QString m_serverName;

  bool handleClientRequest(const IpcProtocol::Request &request);
  void spawnCleanPrimary();
};

#endif // STARTUPCOORDINATOR_H
