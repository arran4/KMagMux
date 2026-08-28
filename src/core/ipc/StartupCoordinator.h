#ifndef STARTUPCOORDINATOR_H
#define STARTUPCOORDINATOR_H

#include "IpcProtocol.h"
#include <QLockFile>
#include <QString>
#include <QStringList>
#include <memory>

enum class CoordinatorAction {
  BecomePrimary,
  RequestDelivered,
  RequestFailed,
  UserCancelled,
  SpawnFailed
};

struct CoordinatorResult {
  CoordinatorAction action;
  std::unique_ptr<QLockFile> primaryLock = nullptr;
};

class StartupSystemInterface {
public:
  virtual ~StartupSystemInterface() = default;
  virtual bool spawnCleanPrimary() = 0;
  virtual void msleep(int ms) = 0;
  virtual void showErrorMessage(const QString &msg) = 0;
  virtual bool showRecoveryPrompt(const QString &diagnostic) = 0;
};

class DefaultStartupSystem : public StartupSystemInterface {
public:
  bool spawnCleanPrimary() override;
  void msleep(int ms) override;
  void showErrorMessage(const QString &msg) override;
  bool showRecoveryPrompt(const QString &diagnostic) override;
};

class StartupCoordinator {
public:
  explicit StartupCoordinator(const QString &serverName,
                              StartupSystemInterface *sys = nullptr);

  CoordinatorResult coordinate(const QStringList &args);

private:
  QString m_serverName;
  StartupSystemInterface *m_sys;

  CoordinatorAction handleClientRequest(const IpcProtocol::Request &request);
};

#endif // STARTUPCOORDINATOR_H