#ifndef TRACKERCLIENT_H
#define TRACKERCLIENT_H

#include <QByteArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QUdpSocket>

struct TrackerStats {
  QString trackerUrl;
  bool success = false;
  int seeders = -1;
  int leechers = -1;
  int downloaded = -1;
  QString errorString;
};

class TrackerClient : public QObject {
  Q_OBJECT
public:
  explicit TrackerClient(QObject *parent = nullptr);
  ~TrackerClient();

  void scrape(const QString &trackerUrl, const QByteArray &infoHash);
  void cancel();

signals:
  void scrapeFinished(const TrackerStats &stats);

private Q_SLOTS:
  void onUdpReadyRead();
  void onUdpError(QAbstractSocket::SocketError error);
  void onHttpFinished(QNetworkReply *reply);
  void onTimeout();

private:
  void startUdpScrape(const QString &url, const QByteArray &infoHash);
  void startHttpScrape(const QString &url, const QByteArray &infoHash);

  void sendUdpConnect();
  void sendUdpScrape();

  QString m_currentTrackerUrl;
  QByteArray m_currentInfoHash;

  QUdpSocket *m_udpSocket;
  QNetworkAccessManager *m_nam;
  QNetworkReply *m_currentReply;
  QTimer *m_timer;

  // UDP protocol state
  enum class UdpState : std::uint8_t { Idle, Connecting, Scraping };
  UdpState m_udpState;
  uint32_t m_transactionId;
  uint64_t m_connectionId;

  bool m_isActive;
};

#endif // TRACKERCLIENT_H
