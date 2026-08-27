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

StartupCoordinator::StartupCoordinator(const QString &serverName)
    : m_serverName(serverName) {}

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
    auto *primaryLock = new QLockFile(primaryFilePath);
    primaryLock->setStaleLockTime(0);
    if (primaryLock->tryLock(0)) {
      return {CoordinatorAction::BecomePrimary, primaryLock};
    }
    delete primaryLock;
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
      if (!spawnCleanPrimary()) {
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

bool StartupCoordinator::spawnCleanPrimary() {
  QString program = QCoreApplication::applicationFilePath();
  qDebug() << "Spawning clean primary:" << program;
  return QProcess::startDetached(program, QStringList());
}

CoordinatorAction
StartupCoordinator::handleClientRequest(const IpcProtocol::Request &request) {
  SingleInstanceClient client(m_serverName);

  // We might have just spawned the primary, so give it some time to start the
  // server.
  int retries = 20; // Up to 4 seconds
  ClientResult result{ClientResultCode::RequestRejected, ""};

  while (retries > 0) {
    result = client.sendRequest(request, 1000, 5000);
    if (result.code == ClientResultCode::ConnectFailed ||
        result.code == ClientResultCode::ConnectionClosed) {
      QThread::msleep(200); // Wait and retry connecting
      retries--;
    } else {
      break;
    }
  }

  while (!result.isSuccess()) {
    qWarning() << "IPC Request Failed:" << result.diagnostic;

    // Requirement 21: "NON-RESPONSIVE PRIMARY RECOVERY UX"
    QMessageBox::StandardButton reply = QMessageBox::warning(
        nullptr, "KMagMux Error",
        QString("KMagMux appears to be running but is not responding.\n\n"
                "This request has NOT been confirmed as accepted.\n\n"
                "Error: %1")
            .arg(result.diagnostic),
        QMessageBox::Retry | QMessageBox::Cancel);

    if (reply == QMessageBox::Retry) {
      result = client.sendRequest(request, 1000, 5000);
    } else {
      return CoordinatorAction::UserCancelled;
    }
  }

  qDebug() << "IPC Request Success:" << result.diagnostic;

  return CoordinatorAction::RequestDelivered;
}
