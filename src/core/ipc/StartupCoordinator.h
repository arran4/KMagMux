#ifndef STARTUPCOORDINATOR_H
#define STARTUPCOORDINATOR_H

#include "IpcProtocol.h"
#include "SingleInstanceClient.h"
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

  // Expose the raw program arguments to allow test assertions
  virtual bool spawnDetached(const QString &program, const QStringList &args) = 0;

  // Inject client request delivery to avoid spinning up raw sockets when testing coordination logic
  virtual ClientResult sendClientRequest(const QString &serverName, const IpcProtocol::Request &request, int connectTimeout, int responseTimeout) = 0;

  virtual void msleep(int ms) = 0;
  virtual void showErrorMessage(const QString &msg) = 0;
  virtual bool showRecoveryPrompt(const QString &diagnostic) = 0;
};

struct StartupRetryPolicy {
    int maxRetries = 20;
    int connectTimeoutMs = 1000;
    int responseTimeoutMs = 5000;
    int retryDelayMs = 200;
};

class DefaultStartupSystem : public StartupSystemInterface {
public:
  bool spawnDetached(const QString &program, const QStringList &args) override;
  ClientResult sendClientRequest(const QString &serverName, const IpcProtocol::Request &request, int connectTimeout, int responseTimeout) override;
  void msleep(int ms) override;
  void showErrorMessage(const QString &msg) override;
  bool showRecoveryPrompt(const QString &diagnostic) override;
};

class StartupCoordinator {
public:
  explicit StartupCoordinator(const QString &serverName,
                              StartupSystemInterface *sys = nullptr,
                              StartupRetryPolicy policy = StartupRetryPolicy());

  CoordinatorResult coordinate(const QStringList &args);

private:
  QString m_serverName;
  StartupSystemInterface *m_sys;
  StartupRetryPolicy m_policy;

  CoordinatorAction handleClientRequest(const IpcProtocol::Request &request);
};

#endif // STARTUPCOORDINATOR_H