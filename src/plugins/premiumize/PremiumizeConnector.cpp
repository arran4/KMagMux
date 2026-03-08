#include "PremiumizeConnector.h"
#include "core/SecureStorage.h"
#include <QCheckBox>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QLineEdit>
#include <QSettings>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>
#include <QWidget>

PremiumizeConnector::PremiumizeConnector() : PremiumizeConnector(nullptr) {}

PremiumizeConnector::PremiumizeConnector(QObject *parent)
    : QObject(parent), m_networkManager(new QNetworkAccessManager(this)),
      m_apiKey(""), m_enabled(false) {
  QSettings settings;
  settings.beginGroup("Plugins/Premiumize");
  m_enabled = settings.value("enabled", false).toBool();
  settings.endGroup();

  m_apiKey = SecureStorage::readPassword("Plugins/Premiumize", "apiKey");
}

QString PremiumizeConnector::getId() const { return "Premiumize"; }

QString PremiumizeConnector::getName() const { return "Premiumize.me"; }

bool PremiumizeConnector::isEnabled() const { return m_enabled; }

void PremiumizeConnector::dispatch(const Item &item) {
  QUrl url("https://www.premiumize.me/api/transfer/create");

  if (item.sourcePath.startsWith("magnet:")) {
    // Premiumize uses form-data or x-www-form-urlencoded for magnet links
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      "application/x-www-form-urlencoded");

    QUrlQuery postData;
    postData.addQueryItem("apikey", m_apiKey);
    postData.addQueryItem("src", item.sourcePath);

    QNetworkReply *reply = m_networkManager->post(
        request, postData.toString(QUrl::FullyEncoded).toUtf8());
    reply->setProperty("itemId", item.id);
    connect(reply, &QNetworkReply::finished, this,
            &PremiumizeConnector::onAddTorrentReply);
  } else {
    // For files we need multipart
    QNetworkRequest request(url);
    // Note: Do not set content-type for multipart, QNetworkAccessManager
    // handles the boundary automatically

    QHttpMultiPart *multiPart =
        new QHttpMultiPart(QHttpMultiPart::FormDataType);

    QHttpPart apiKeyPart;
    apiKeyPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                         QVariant("form-data; name=\"apikey\""));
    apiKeyPart.setBody(m_apiKey.toUtf8());
    multiPart->append(apiKeyPart);

    QFile *file = new QFile(item.sourcePath);
    if (!file->open(QIODevice::ReadOnly)) {
      emit dispatchFinished(item.id, false,
                            "Could not open torrent file: " + item.sourcePath);
      delete multiPart;
      delete file;
      return;
    }
    QHttpPart filePart;
    QString filename = QFileInfo(item.sourcePath).fileName();
    filePart.setHeader(
        QNetworkRequest::ContentDispositionHeader,
        QVariant("form-data; name=\"src\"; filename=\"" + filename + "\""));
    filePart.setBodyDevice(file);
    file->setParent(multiPart);
    multiPart->append(filePart);

    QNetworkReply *reply = m_networkManager->post(request, multiPart);
    multiPart->setParent(reply);
    reply->setProperty("itemId", item.id);
    connect(reply, &QNetworkReply::finished, this,
            &PremiumizeConnector::onAddTorrentReply);
  }
}

void PremiumizeConnector::onAddTorrentReply() {
  QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
  if (!reply)
    return;

  QString itemId = reply->property("itemId").toString();
  if (reply->error() == QNetworkReply::NoError) {
    emit dispatchFinished(itemId, true, "Dispatched successfully.");
  } else {
    emit dispatchFinished(itemId, false,
                          "Network error: " + reply->errorString());
  }
  reply->deleteLater();
}

bool PremiumizeConnector::hasSettings() const { return true; }

QWidget *PremiumizeConnector::createSettingsWidget(QWidget *parent) {
  QWidget *widget = new QWidget(parent);
  QVBoxLayout *mainLayout = new QVBoxLayout(widget);

  QSettings settings;
  settings.beginGroup("Plugins/Premiumize");

  QCheckBox *enabledCheck = new QCheckBox(tr("Enable Premiumize.me"), widget);
  enabledCheck->setObjectName("enabledCheck");
  enabledCheck->setChecked(settings.value("enabled", false).toBool());
  mainLayout->addWidget(enabledCheck);

  QWidget *configWidget = new QWidget(widget);
  QFormLayout *configLayout = new QFormLayout(configWidget);

  QLineEdit *tokenEdit = new QLineEdit(configWidget);
  tokenEdit->setObjectName("tokenEdit");
  tokenEdit->setEchoMode(QLineEdit::Password);
  tokenEdit->setText(
      SecureStorage::readPassword("Plugins/Premiumize", "apiKey"));
  configLayout->addRow(tr("API Key:"), tokenEdit);

  mainLayout->addWidget(configWidget);
  settings.endGroup();

  configWidget->setVisible(enabledCheck->isChecked());
  connect(enabledCheck, &QCheckBox::toggled, configWidget,
          &QWidget::setVisible);

  return widget;
}

void PremiumizeConnector::saveSettings(QWidget *settingsWidget) {
  if (!settingsWidget)
    return;

  QCheckBox *enabledCheck =
      settingsWidget->findChild<QCheckBox *>("enabledCheck");
  QLineEdit *tokenEdit = settingsWidget->findChild<QLineEdit *>("tokenEdit");

  QSettings settings;
  settings.beginGroup("Plugins/Premiumize");

  if (enabledCheck) {
    bool en = enabledCheck->isChecked();
    settings.setValue("enabled", en);
    m_enabled = en;
  }
  if (tokenEdit) {
    SecureStorage::writePassword("Plugins/Premiumize", "apiKey",
                                 tokenEdit->text());
    m_apiKey = tokenEdit->text();
  }

  settings.endGroup();
}

bool PremiumizeConnector::hasDebugMenu() const { return true; }

QList<HttpApiEndpoint> PremiumizeConnector::getHttpApiEndpoints() const {
  QList<HttpApiEndpoint> endpoints;

  HttpApiEndpoint accountInfo;
  accountInfo.name = "Account Info";
  accountInfo.description = "Retrieves account information";
  accountInfo.method = "GET";
  accountInfo.url =
      "https://www.premiumize.me/api/account/info?apikey=${API_KEY}";
  endpoints.append(accountInfo);

  HttpApiEndpoint transferList;
  transferList.name = "Transfer List";
  transferList.description = "Lists all active transfers";
  transferList.method = "GET";
  transferList.url =
      "https://www.premiumize.me/api/transfer/list?apikey=${API_KEY}";
  endpoints.append(transferList);

  HttpApiEndpoint transferCreateMagnet;
  transferCreateMagnet.name = "Transfer Create (Magnet)";
  transferCreateMagnet.description =
      "Creates a new transfer from a magnet link";
  transferCreateMagnet.method = "POST";
  transferCreateMagnet.url = "https://www.premiumize.me/api/transfer/create";
  transferCreateMagnet.headers.insert("Content-Type",
                                      "application/x-www-form-urlencoded");
  transferCreateMagnet.body = "apikey=${API_KEY}&src=${MAGNET_LINK}";
  endpoints.append(transferCreateMagnet);

  HttpApiEndpoint transferCreateTorrent;
  transferCreateTorrent.name = "Transfer Create (Torrent File)";
  transferCreateTorrent.description =
      "Creates a new transfer from a .torrent file";
  transferCreateTorrent.method = "POST";
  transferCreateTorrent.url = "https://www.premiumize.me/api/transfer/create";
  transferCreateTorrent.isMultipart = true;
  transferCreateTorrent.multipartParts.insert("apikey", "${API_KEY}");
  transferCreateTorrent.multipartParts.insert("src",
                                              "file:///path/to/test.torrent");
  endpoints.append(transferCreateTorrent);

  return endpoints;
}

QMap<QString, QString> PremiumizeConnector::getApiSubstitutions() const {
  QMap<QString, QString> subs;
  subs.insert("API_KEY", m_apiKey);
  return subs;
}
