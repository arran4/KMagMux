#ifndef APPLICATIONREQUESTDISPATCHER_H
#define APPLICATIONREQUESTDISPATCHER_H

#include "IpcProtocol.h"
#include <QObject>
#include <QQueue>
#include <QStringList>

class ApplicationRequestDispatcher : public QObject {
  Q_OBJECT
public:
  explicit ApplicationRequestDispatcher(QObject *parent = nullptr);

  IpcProtocol::ResponseStatus dispatch(const IpcProtocol::Request &request);

private slots:
  void processNext();

private:
  QQueue<QStringList> m_addInputsQueue;
  bool m_isProcessing;

  // Simplistic duplicate cache
  QStringList m_recentRequestIds;
  void addRecentRequest(const QString &id);
  bool isRecentRequest(const QString &id) const;

signals:
  void activateWindowRequested();
  void processAddedLinesRequested(const QStringList &lines);
};

#endif // APPLICATIONREQUESTDISPATCHER_H
