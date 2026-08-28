#include "StartupCoordinator.h"
#include "SingleInstanceClient.h"
#include "SingleInstanceServer.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QLockFile>
#include <QMessageBox>
#include <QProcess>
#include <QStandardPaths>
#include <QThread>
#include <QUuid>

bool DefaultStartupSystem::spawnDetached(const QString &program, const QStringList &args) {
  return QProcess::startDetached(program, args);
}

ClientResult DefaultStartupSystem::sendClientRequest(const QString &serverName, const IpcProtocol::Request &request, int connectTimeout, int responseTimeout) {
  SingleInstanceClient client(serverName);
  return client.sendRequest(request, connectTimeout, responseTimeout);
}

void DefaultStartupSystem::msleep(int ms) { QThread::msleep(ms); }

void DefaultStartupSystem::showErrorMessage(const QString &msg) {
  QMessageBox::critical(nullptr, "KMagMux Error", msg);
}

bool DefaultStartupSystem::showRecoveryPrompt(const QString &diagnostic) {
  QMessageBox::StandardButton reply = QMessageBox::warning(
      nullptr, "KMagMux Error",
      QString("KMagMux appears to be running but is not responding.\n\n"
              "This request has NOT been confirmed as accepted.\n\n"
              "Error: %1")
          .arg(diagnostic),
      QMessageBox::Retry | QMessageBox::Cancel);
  return reply == QMessageBox::Retry;
}

static DefaultStartupSystem g_defaultSystem;

StartupCoordinator::StartupCoordinator(const QString &serverName,
                                       StartupSystemInterface *sys,
                                       StartupRetryPolicy policy)
    : m_serverName(serverName), m_sys(sys ? sys : &g_defaultSystem), m_policy(policy) {}

CoordinatorResult StartupCoordinator::coordinate(const QStringList &args) {
  IpcProtocol::Request request;
  request.requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);

  QStringList inputs;
  for (int i = 1; i < args.size(); ++i) {
    if (!args[i].startsWith("-")) {
      inputs.append(args[i]);
    }
  }

  if (inputs.isEmpty()) {
    request.type = IpcProtocol::RequestType::ActivateWindow;
  } else {
    request.type = IpcProtocol::RequestType::AddInputs;
    request.payload = inputs;
  }

  QString runtimePath =
      QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
  if (runtimePath.isEmpty()) {
    runtimePath = QDir::tempPath();
  }

  // Election lock is used to serialize multiple concurrent launchers.
  QString electionFilePath =
      QDir(runtimePath).filePath(m_serverName + "_election.lock");
  QLockFile electionLock(electionFilePath);
  electionLock.setStaleLockTime(0);

  // Persistent primary ownership lock.
  QString primaryFilePath = QDir(runtimePath).filePath(m_serverName + ".lock");

  if (inputs.isEmpty()) {
    // We are a no-arg launcher. Try to become the primary immediately.
    auto primaryLock = std::make_unique<QLockFile>(primaryFilePath);
    primaryLock->setStaleLockTime(0);
    if (primaryLock->tryLock(0)) {
      return {CoordinatorAction::BecomePrimary, std::move(primaryLock)};
    }

    // Primary exists, act as a client to activate window.
    return {handleClientRequest(request), nullptr};
  }

  // We have arguments. We must ensure only ONE launcher spawns a primary.
  if (electionLock.tryLock(5000)) {
    // We hold the election lock. Check if a primary already exists.
    auto *tempPrimaryLock = new QLockFile(primaryFilePath);
    tempPrimaryLock->setStaleLockTime(0);
    bool primaryExists = !tempPrimaryLock->tryLock(0);

    if (!primaryExists) {
      // No primary exists. We held the lock momentarily to check.
      tempPrimaryLock->unlock();
      delete tempPrimaryLock;

      qDebug() << "We are an action-bearing launcher and no primary exists. "
                  "Spawning clean primary...";
      QString program = QCoreApplication::applicationFilePath();
      if (!m_sys->spawnDetached(program, QStringList())) {
        electionLock.unlock();
        return {CoordinatorAction::SpawnFailed, nullptr};
      }
    } else {
      delete tempPrimaryLock;
    }

    // We spawned it or it already existed.
    // Act as client. We keep the election lock until the client request
    // finishes or times out to ensure concurrent launchers wait for our newly
    // spawned primary to become ready.
    CoordinatorAction action = handleClientRequest(request);
    electionLock.unlock();
    return {action, nullptr};
  }

  // Could not get election lock. Another launcher is likely spawning the
  // primary. Just act as a client.
  return {handleClientRequest(request), nullptr};
}

CoordinatorAction
StartupCoordinator::handleClientRequest(const IpcProtocol::Request &request) {
  int retries = m_policy.maxRetries;
  ClientResult result{ClientResultCode::RequestRejected, ""};

  while (retries > 0) {
    result = m_sys->sendClientRequest(m_serverName, request, m_policy.connectTimeoutMs, m_policy.responseTimeoutMs);
    if (result.code == ClientResultCode::ConnectFailed ||
        result.code == ClientResultCode::ConnectionClosed) {
      m_sys->msleep(m_policy.retryDelayMs);
      retries--;
    } else {
      break;
    }
  }

  while (!result.isSuccess()) {
    qWarning() << "IPC Request Failed:" << result.diagnostic;

    // Explicitly reject deterministic failures rather than treating them as
    // transport timeout
    if (result.code == ClientResultCode::RequestRejected ||
        result.code == ClientResultCode::UnsupportedProtocol ||
        result.code == ClientResultCode::InvalidResponse ||
        result.code == ClientResultCode::NoPrimary) {
      m_sys->showErrorMessage("KMagMux could not process the request:\n\n" +
                              result.diagnostic);
      return CoordinatorAction::RequestFailed;
    }

    // Requirement 21: "NON-RESPONSIVE PRIMARY RECOVERY UX"
    // Apply only for actual transport connection loss or timeouts
    bool retry = m_sys->showRecoveryPrompt(result.diagnostic);

    if (retry) {
      result = m_sys->sendClientRequest(m_serverName, request, m_policy.connectTimeoutMs, m_policy.responseTimeoutMs);
    } else {
      return CoordinatorAction::UserCancelled;
    }
  }

  qDebug() << "IPC Request Success:" << result.diagnostic;

  return CoordinatorAction::RequestDelivered;
}
