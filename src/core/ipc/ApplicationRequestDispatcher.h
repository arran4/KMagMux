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

public slots:
  void completeCurrentProcessing();

private slots:
  void processNext();

private:
  QQueue<QStringList> m_addInputsQueue;
  bool m_isProcessing;

  struct CachedResult {
    QString requestId;
    QString fingerprint;
    IpcProtocol::ResponseStatus status;
  };

  QList<CachedResult> m_recentResults;
  void addRecentResult(const QString &id, const QString &fingerprint,
                       IpcProtocol::ResponseStatus status);
  bool checkRecentResult(const QString &id, const QString &fingerprint,
                         IpcProtocol::ResponseStatus &outStatus) const;
  QString calculateFingerprint(const IpcProtocol::Request &request) const;

signals:
  void activateWindowRequested();
  void processAddedLinesRequested(const QStringList &lines);
};

#endif // APPLICATIONREQUESTDISPATCHER_H
