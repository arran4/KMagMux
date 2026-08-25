#include "ApplicationRequestDispatcher.h"
#include <QDebug>
#include <QTimer>

ApplicationRequestDispatcher::ApplicationRequestDispatcher(QObject *parent)
    : QObject(parent), m_isProcessing(false) {}

void ApplicationRequestDispatcher::addRecentRequest(const QString &id) {
  m_recentRequestIds.append(id);
  if (m_recentRequestIds.size() > 50) {
    m_recentRequestIds.removeFirst();
  }
}

bool ApplicationRequestDispatcher::isRecentRequest(const QString &id) const {
  return m_recentRequestIds.contains(id);
}

IpcProtocol::ResponseStatus
ApplicationRequestDispatcher::dispatch(const IpcProtocol::Request &request) {
  if (!request.isValid()) {
    return IpcProtocol::ResponseStatus::MalformedRequest;
  }

  if (isRecentRequest(request.requestId)) {
    // Idempotency: Already processed this, so just return Accepted
    return IpcProtocol::ResponseStatus::Accepted;
  }

  addRecentRequest(request.requestId);

  if (request.type == IpcProtocol::RequestType::ActivateWindow) {
    emit activateWindowRequested();
    return IpcProtocol::ResponseStatus::Accepted;
  } else if (request.type == IpcProtocol::RequestType::AddInputs) {
    emit activateWindowRequested();

    m_addInputsQueue.enqueue(request.payload);

    if (!m_isProcessing) {
      QTimer::singleShot(0, this, &ApplicationRequestDispatcher::processNext);
    }
    return IpcProtocol::ResponseStatus::Accepted;
  }

  return IpcProtocol::ResponseStatus::UnknownRequestType;
}

void ApplicationRequestDispatcher::processNext() {
  if (m_addInputsQueue.isEmpty()) {
    m_isProcessing = false;
    return;
  }

  m_isProcessing = true;
  QStringList nextPayload = m_addInputsQueue.dequeue();

  emit processAddedLinesRequested(nextPayload);

  // We rely on the fact that processAddedLines opens a modal dialog
  // (ProcessItemDialog/AddItemDialog) The modal dialog will block the event
  // loop, so the next queue item won't be processed until we get back here, but
  // we should make sure we only queue the next one after the UI has completed.
  m_isProcessing = false;
  if (!m_addInputsQueue.isEmpty()) {
    QTimer::singleShot(
        500, this,
        &ApplicationRequestDispatcher::processNext); // basic debounce
  }
}
