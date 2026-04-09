#include "TrackerClient.h"
#include "BencodeParser.h"
#include <QDataStream>
#include <QDateTime>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QTimer>
#include <QUrlQuery>

// UDP protocol constants
const uint64_t UDP_MAGIC_CONNECTION_ID = 0x41727101980;
const uint32_t UDP_ACTION_CONNECT = 0;
const uint32_t UDP_ACTION_ANNOUNCE = 1;
const uint32_t UDP_ACTION_SCRAPE = 2;
const uint32_t UDP_ACTION_ERROR = 3;

TrackerClient::TrackerClient(QObject *parent)
    : QObject(parent), m_udpSocket(new QUdpSocket(this)),
      m_nam(new QNetworkAccessManager(this)), m_currentReply(nullptr),
      m_timer(new QTimer(this)), m_udpState(UdpState::Idle), m_isActive(false) {

  connect(m_udpSocket, &QUdpSocket::readyRead, this,
          &TrackerClient::onUdpReadyRead);
  connect(m_udpSocket, &QUdpSocket::errorOccurred, this,
          &TrackerClient::onUdpError);
  connect(m_nam, &QNetworkAccessManager::finished, this,
          &TrackerClient::onHttpFinished);
  connect(m_timer, &QTimer::timeout, this, &TrackerClient::onTimeout);

  m_timer->setSingleShot(true);
}

TrackerClient::~TrackerClient() { cancel(); }

void TrackerClient::scrape(const QString &trackerUrl,
                           const QByteArray & infoHash) {
  cancel(); // stop any ongoing request

  m_isActive = true;
  m_currentTrackerUrl = trackerUrl;
  m_currentInfoHash = infoHash;

  const int timeoutMs = 10000;
  m_timer->start(timeoutMs);

  const QUrl url(trackerUrl);
  if (url.scheme() == "udp") {
    startUdpScrape(trackerUrl, infoHash);
  } else if (url.scheme() == "http" || url.scheme() == "https") {
    startHttpScrape(trackerUrl, infoHash);
  } else {
    TrackerStats stats;
    stats.trackerUrl = trackerUrl;
    stats.errorString = "Unsupported tracker protocol";
    m_isActive = false;
    emit scrapeFinished(stats);
  }
}

void TrackerClient::cancel() {
  m_isActive = false;
  if (m_timer != nullptr) {
    m_timer->stop();
  }

  if (m_udpSocket->state() != QAbstractSocket::UnconnectedState) {
    m_udpSocket->abort();
  }
  m_udpState = UdpState::Idle;

  if (m_currentReply != nullptr) {
    m_currentReply->abort();
    m_currentReply->deleteLater();
    m_currentReply = nullptr;
  }
}

void TrackerClient::startUdpScrape(const QString &urlStr,
                                   const QByteArray & infoHash) {
  const QUrl url(urlStr);

  const int defaultPort = 80;
  m_udpSocket->connectToHost(url.host(), url.port(defaultPort));
  m_udpState = UdpState::Connecting;

  sendUdpConnect();
}

void TrackerClient::sendUdpConnect() {
  QByteArray packet;
  QDataStream stream(&packet, QIODevice::WriteOnly);
  stream.setByteOrder(QDataStream::BigEndian);

  m_transactionId = QRandomGenerator::global()->generate();

  stream << static_cast<quint64>(UDP_MAGIC_CONNECTION_ID);
  stream << UDP_ACTION_CONNECT;
  stream << m_transactionId;

  m_udpSocket->write(packet);
}

void TrackerClient::sendUdpScrape() {
  QByteArray packet;
  QDataStream stream(&packet, QIODevice::WriteOnly);
  stream.setByteOrder(QDataStream::BigEndian);

  m_transactionId = QRandomGenerator::global()->generate();

  stream << static_cast<quint64>(m_connectionId);
  stream << UDP_ACTION_SCRAPE;
  stream << m_transactionId;

  // write 20-byte infohash directly
  packet.append(m_currentInfoHash);

  m_udpSocket->write(packet);
}

void TrackerClient::onUdpReadyRead() {
  if (!m_isActive) {
    return;
  }

  while (m_udpSocket->hasPendingDatagrams()) {
    QByteArray datagram;
    datagram.resize(m_udpSocket->pendingDatagramSize());
    m_udpSocket->readDatagram(datagram.data(), datagram.size());

    QDataStream stream(datagram);
    stream.setByteOrder(QDataStream::BigEndian);

    uint32_t action;
    uint32_t transactionId;

    const int minResponseLen = 8;
    if (datagram.size() < minResponseLen) {
      continue;
    } // too small

    stream >> action >> transactionId;

    if (transactionId != m_transactionId) {
      continue;
    } // ignore mismatched transaction ID

    if (action == UDP_ACTION_CONNECT) {
      const int minConnectLen = 16;
      if (datagram.size() < minConnectLen) {
        continue;
      }
      qint64 connId;
      stream >> connId;
      m_connectionId = static_cast<uint64_t>(connId);
      m_udpState = UdpState::Scraping;
      sendUdpScrape();
    } else if (action == UDP_ACTION_SCRAPE) {
      const int minScrapeLen = 20;
      if (datagram.size() < minScrapeLen) {
        continue;
      }

      uint32_t seeders = 0;
      uint32_t completed = 0;
      uint32_t leechers = 0;
      stream >> seeders >> completed >> leechers;

      TrackerStats stats;
      stats.trackerUrl = m_currentTrackerUrl;
      stats.success = true;
      stats.seeders = static_cast<int>(seeders);
      stats.downloaded = static_cast<int>(completed);
      stats.leechers = static_cast<int>(leechers);

      m_isActive = false;
      if (m_timer != nullptr) {
        m_timer->stop();
      }
      emit scrapeFinished(stats);
    } else if (action == UDP_ACTION_ERROR) {
      const int errorStrOffset = 8;
      const QString errorStr =
          QString::fromLatin1(datagram.mid(errorStrOffset));
      TrackerStats stats;
      stats.trackerUrl = m_currentTrackerUrl;
      stats.errorString = errorStr;

      m_isActive = false;
      if (m_timer != nullptr) {
        m_timer->stop();
      }
      emit scrapeFinished(stats);
    }
  }
}

void TrackerClient::onUdpError(QAbstractSocket::SocketError /*error*/) {
  if (!m_isActive) {
    return;
  }

  TrackerStats stats;
  stats.trackerUrl = m_currentTrackerUrl;
  stats.errorString = m_udpSocket->errorString();

  m_isActive = false;
  if (m_timer != nullptr) {
    m_timer->stop();
  }
  emit scrapeFinished(stats);
}

void TrackerClient::startHttpScrape(const QString &urlStr,
                                    const QByteArray &infoHash) {
  QUrl url(urlStr);

  // Replace /announce with /scrape
  QString path = url.path();
  if (path.endsWith("announce")) {
    const int announceLen = 8;
    path.replace(path.length() - announceLen, announceLen, "scrape");
  } else if (path.endsWith("announce.php")) {
    const int announcePhpLen = 12;
    path.replace(path.length() - announcePhpLen, announcePhpLen, "scrape.php");
  }
  url.setPath(path);

  // Note: Standard HTTP trackers require info_hash to be url-encoded exactly
  // But QUrlQuery's addQueryItem will double encode or miss encoding binary
  // data. We need to encode the raw hash properly.

  const QByteArray encodedHash = infoHash.toPercentEncoding();

  QString queryStr = url.query(QUrl::FullyEncoded);
  if (!queryStr.isEmpty()) {
    queryStr += "&";
  }
  queryStr += "info_hash=" + encodedHash;

  url.setQuery(queryStr, QUrl::StrictMode);

  QNetworkRequest request(url);
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                       QNetworkRequest::NoLessSafeRedirectPolicy);

  m_currentReply = m_nam->get(request);
}

void TrackerClient::onHttpFinished(QNetworkReply *reply) {
  if (reply != m_currentReply || !m_isActive) {
    if (reply != nullptr) {
      reply->deleteLater();
      reply = nullptr;
    }
    return;
  }

  m_isActive = false;
  if (m_timer != nullptr) {
    m_timer->stop();
  }
  m_currentReply = nullptr;

  TrackerStats stats;
  stats.trackerUrl = m_currentTrackerUrl;

  if (reply->error() != QNetworkReply::NoError) {
    stats.errorString = reply->errorString();
    reply->deleteLater();
    emit scrapeFinished(stats);
    return;
  }

  const QByteArray data = reply->readAll();
  reply->deleteLater();

  BencodeParser parser;
  if (!parser.parse(data)) {
    stats.errorString = "Invalid bencode response";
    emit scrapeFinished(stats);
    return;
  }

  QVariantMap dict = parser.dictionary();
  if (dict.contains("failure reason")) {
    stats.errorString = QString::fromUtf8(dict["failure reason"].toByteArray());
    emit scrapeFinished(stats);
    return;
  }

  if (dict.contains("files")) {
    const QVariant filesVar = dict["files"];
    if (filesVar.typeId() == QMetaType::QVariantMap) {
      QVariantMap filesDict = filesVar.toMap();
      // filesDict keys are the info_hashes. Find ours, or just take the first
      // one since we only asked for one.

      // We asked for m_currentInfoHash. The key in dictionary is raw bytes.
      const QString hashKey = QString::fromUtf8(m_currentInfoHash);
      QVariantMap targetFile;

      if (filesDict.contains(hashKey)) {
        targetFile = filesDict[hashKey].toMap();
      } else if (!filesDict.isEmpty()) {
        targetFile = filesDict.first().toMap();
      }

      if (!targetFile.isEmpty()) {
        stats.success = true;
        stats.seeders = targetFile.value("complete", -1).toInt();
        stats.downloaded = targetFile.value("downloaded", -1).toInt();
        stats.leechers = targetFile.value("incomplete", -1).toInt();
      } else {
        stats.errorString = "No matching info_hash in scrape response";
      }
    } else {
      stats.errorString = "Invalid scrape response format";
    }
  } else {
    stats.errorString = "No files dict in scrape response";
  }

  emit scrapeFinished(stats);
}

void TrackerClient::onTimeout() {
  if (!m_isActive) {
    return;
  }

  TrackerStats stats;
  stats.trackerUrl = m_currentTrackerUrl;
  stats.errorString = "Request timed out";

  cancel(); // internally sets m_isActive = false
  emit scrapeFinished(stats);
}
