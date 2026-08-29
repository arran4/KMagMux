#include "ApplicationRequestDispatcher.h"
#include <QCryptographicHash>
#include <QDebug>
#include <QIODevice>
#include <QUuid>

ApplicationRequestDispatcher::ApplicationRequestDispatcher(QObject *parent)
    : QObject(parent), m_isProcessing(false), m_currentTokenId() {}

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
  QByteArray canonical;
  QDataStream out(&canonical, QIODevice::WriteOnly);
  IpcProtocol::setupStream(out);
  out << request.version << static_cast<quint16>(request.type)
      << request.payload;
  hash.addData(canonical);
  return QString::fromLatin1(hash.result().toHex());
}

bool ApplicationRequestDispatcher::checkRecentResult(
    const QString &id, const QString &fingerprint,
    IpcProtocol::ResponseStatus &outStatus) const {
  auto it = std::find_if(
      m_recentResults.begin(), m_recentResults.end(),
      [&id](const CachedResult &cached) { return cached.requestId == id; });
  if (it != m_recentResults.end()) {
    if (it->fingerprint != fingerprint) {
      outStatus = IpcProtocol::ResponseStatus::MalformedRequest; // Collision
    } else {
      outStatus = it->status;
    }
    return true;
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
    return status; // Return the cached status exactly (e.g. if it was an error,
                   // return error)
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
    m_currentTokenId.clear();
    return;
  }
  QStringList nextPayload = m_addInputsQueue.dequeue();
  m_currentTokenId = QUuid::createUuid().toString();
  emit processAddedLinesRequested(nextPayload, m_currentTokenId);
}

void ApplicationRequestDispatcher::completeCurrentProcessing(
    const QString &tokenId) {
  if (tokenId != m_currentTokenId) {
    return; // Ignore unrelated local processing completion
  }

  if (!m_addInputsQueue.isEmpty()) {
    processNext();
  } else {
    m_isProcessing = false;
    m_currentTokenId.clear();
  }
}
