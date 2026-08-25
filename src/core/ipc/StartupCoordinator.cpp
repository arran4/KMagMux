#include "StartupCoordinator.h"
#include "SingleInstanceClient.h"
#include "SingleInstanceServer.h"
#include <QCoreApplication>
#include <QDebug>
#include <QMessageBox>
#include <QProcess>
#include <QThread>
#include <QUuid>

StartupCoordinator::StartupCoordinator(const QString &serverName)
    : m_serverName(serverName) {}

bool StartupCoordinator::coordinate(const QStringList &args) {
  IpcProtocol::Request request;
  request.requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);

  QStringList inputs;
  for (int i = 1; i < args.size(); ++i) {
    inputs.append(args[i]);
  }

  if (inputs.isEmpty()) {
    request.type = IpcProtocol::RequestType::ActivateWindow;
  } else {
    request.type = IpcProtocol::RequestType::AddInputs;
    request.payload = inputs;
  }

  SingleInstanceServer tempServer(m_serverName, nullptr);
  bool isPrimary = tempServer.tryAcquire();

  if (isPrimary) {
    // We are the primary!
    if (inputs.isEmpty()) {
      return true; // Just run normally.
    } else {
      // We are the first instance, but we were launched WITH arguments.
      // Requirement 4: "WHEN NO PRIMARY EXISTS, START A CLEAN PRIMARY"
      // "The spawned process must be clean/argument-free"
      qDebug()
          << "We are first, but we have arguments. Spawning clean primary...";
      spawnCleanPrimary();

      // Now we act as a client and wait for the clean primary to become ready.
      return handleClientRequest(request);
    }
  } else {
    // Primary already exists. We are a client.
    return handleClientRequest(request);
  }
}

void StartupCoordinator::spawnCleanPrimary() {
  QString program = QCoreApplication::applicationFilePath();
  qDebug() << "Spawning clean primary:" << program;
  QProcess::startDetached(program, QStringList());
}

bool StartupCoordinator::handleClientRequest(
    const IpcProtocol::Request &request) {
  SingleInstanceClient client(m_serverName);

  // We might have just spawned the primary, so give it some time to start the
  // server.
  int retries = 10;
  ClientResult result{ClientResultCode::RequestRejected, ""};

  while (retries > 0) {
    result = client.sendRequest(request, 1000, 5000);
    if (result.code == ClientResultCode::ConnectFailed) {
      QThread::msleep(200); // Wait and retry connecting
      retries--;
    } else {
      break;
    }
  }

  if (!result.isSuccess()) {
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
      return handleClientRequest(
          request); // Recurse to retry with SAME request ID
    }
  } else {
    qDebug() << "IPC Request Success:" << result.diagnostic;
  }

  // Client job is done, whether successful or failed.
  return false;
}
