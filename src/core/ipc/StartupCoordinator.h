#ifndef STARTUPCOORDINATOR_H
#define STARTUPCOORDINATOR_H

#include "IpcProtocol.h"
#include <QLockFile>
#include <QString>
#include <QStringList>

enum class CoordinatorAction {
  BecomePrimary,
  RequestDelivered,
  RequestFailed,
  UserCancelled,
  SpawnFailed
};

struct CoordinatorResult {
  CoordinatorAction action;
  QLockFile *primaryLock = nullptr;
};

class StartupCoordinator {
public:
  explicit StartupCoordinator(const QString &serverName);

  CoordinatorResult coordinate(const QStringList &args);

private:
  QString m_serverName;

  CoordinatorAction handleClientRequest(const IpcProtocol::Request &request);
  static bool spawnCleanPrimary();
};

#endif // STARTUPCOORDINATOR_H