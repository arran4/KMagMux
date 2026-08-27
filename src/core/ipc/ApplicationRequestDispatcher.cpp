#include "ApplicationRequestDispatcher.h"
#include <QCryptographicHash>
#include <QDebug>

ApplicationRequestDispatcher::ApplicationRequestDispatcher(QObject *parent)
    : QObject(parent), m_isProcessing(false) {}

void ApplicationRequestDispatcher::addRecentResult(
    const QString &id, const QString &fingerprint,
    IpcProtocol::ResponseStatus status) {
  m_recentResults.append({id, fingerprint, status});
  if (m_recentResults.size() > 50) {
    m_recentResults.removeFirst();
  }
}

QString ApplicationRequestDispatcher::calculateFingerprint(
    const IpcProtocol::Request &request) const {
  QCryptographicHash hash(QCryptographicHash::Sha256);
  hash.addData(QByteArray::number(request.version));
  hash.addData(QByteArray::number(static_cast<int>(request.type)));
  for (const QString &s : request.payload) {
    hash.addData(s.toUtf8());
  }
  return QString::fromLatin1(hash.result().toHex());
}

bool ApplicationRequestDispatcher::checkRecentResult(
    const QString &id, const QString &fingerprint,
    IpcProtocol::ResponseStatus &outStatus) const {
  for (const auto &cached : m_recentResults) {
    if (cached.requestId == id) {
      if (cached.fingerprint != fingerprint) {
        outStatus = IpcProtocol::ResponseStatus::MalformedRequest; // Collision
      } else {
        outStatus = cached.status;
      }
      return true;
    }
  }
  return false;
}

IpcProtocol::ResponseStatus
ApplicationRequestDispatcher::dispatch(const IpcProtocol::Request &request) {
  if (!request.isValid()) {
    return IpcProtocol::ResponseStatus::MalformedRequest;
  }

  QString fingerprint = calculateFingerprint(request);
  IpcProtocol::ResponseStatus status;
  if (checkRecentResult(request.requestId, fingerprint, status)) {
    return status; // Return the cached status exactly (e.g. if it was an error, return error)
  }

  if (request.type == IpcProtocol::RequestType::ActivateWindow) {
    emit activateWindowRequested();
    status = IpcProtocol::ResponseStatus::Accepted;
  } else if (request.type == IpcProtocol::RequestType::AddInputs) {
    emit activateWindowRequested();

    m_addInputsQueue.enqueue(request.payload);

    if (!m_isProcessing) {
      m_isProcessing = true;
      processNext();
    }
    status = IpcProtocol::ResponseStatus::Accepted;
  } else {
    status = IpcProtocol::ResponseStatus::UnknownRequestType;
  }

  addRecentResult(request.requestId, fingerprint, status);
  return status;
}

void ApplicationRequestDispatcher::processNext() {
  if (m_addInputsQueue.isEmpty()) {
    m_isProcessing = false;
    return;
  }
  QStringList nextPayload = m_addInputsQueue.dequeue();
  emit processAddedLinesRequested(nextPayload);
}

void ApplicationRequestDispatcher::completeCurrentProcessing() {
  if (!m_addInputsQueue.isEmpty()) {
    processNext();
  } else {
    m_isProcessing = false;
  }
}
