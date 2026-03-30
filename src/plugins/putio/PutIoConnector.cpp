#include "PutIoConnector.h"
#include "core/SecureStorage.h"
#include <QCheckBox>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QLabel>
#include <QLineEdit>
#include <QSettings>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

PutIoConnector::PutIoConnector() : PutIoConnector(nullptr) {}

PutIoConnector::PutIoConnector(QObject *parent)
    : QObject(parent), m_networkManager(new QNetworkAccessManager(this)),
      m_oauthToken(""), m_enabled(false) {
  QSettings settings;
  settings.beginGroup("Plugins/PutIO");
  m_enabled = settings.value("enabled", false).toBool();
  settings.endGroup();

  m_oauthToken = SecureStorage::readPassword("Plugins/PutIO", "oauthToken");
}

QString PutIoConnector::getId() const { return "PutIO"; }

QString PutIoConnector::getName() const { return "Put.io"; }

bool PutIoConnector::isEnabled() const { return m_enabled; }

void PutIoConnector::dispatch(const Item &item) {
  QUrl url("https://api.put.io/v2/transfers/add");
  QNetworkRequest request(url);
  request.setRawHeader("Authorization", ("Bearer " + m_oauthToken).toUtf8());

  QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
  if (item.sourcePath.startsWith("magnet:")) {
    QHttpPart textPart;
    textPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QVariant("form-data; name=\"url\""));
    textPart.setBody(item.sourcePath.toUtf8());
    multiPart->append(textPart);
  } else {
    QFile *file = new QFile(item.sourcePath);
    if (!file->open(QIODevice::ReadOnly)) {
      emit dispatchFinished(item.id, false,
                            "Could not open torrent file: " + item.sourcePath);
      if (multiPart) {
        multiPart->deleteLater();
        multiPart = nullptr;
      }
      if (file) {
        file->deleteLater();
        file = nullptr;
      }
      return;
    }
    QHttpPart filePart;
    QString filename = QFileInfo(item.sourcePath).fileName();
    filePart.setHeader(
        QNetworkRequest::ContentDispositionHeader,
        QVariant("form-data; name=\"file\"; filename=\"" + filename + "\""));
    filePart.setBodyDevice(file);
    file->setParent(multiPart);
    multiPart->append(filePart);
  }

  QString apiCallLog = Connector::buildApiCallLog("POST", request);

  QNetworkReply *reply = m_networkManager->post(request, multiPart);
  multiPart->setParent(reply);
  reply->setProperty("itemId", item.id);
  reply->setProperty("apiCallLog", apiCallLog);
  connect(reply, &QNetworkReply::finished, this,
          &PutIoConnector::onAddTorrentReply);
}

void PutIoConnector::onAddTorrentReply() {
  QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
  if (!reply)
    return;

  QString itemId = reply->property("itemId").toString();
  QString apiCallLog = reply->property("apiCallLog").toString();

  if (reply->error() == QNetworkReply::NoError) {
    emit dispatchFinished(itemId, true, "Dispatched successfully.");
  } else {
    emit dispatchFinished(itemId, false,
                          "Network error: " + reply->errorString() + apiCallLog);
  }
  if (reply) {
    reply->deleteLater();
    reply = nullptr;
  }
}

bool PutIoConnector::hasSettings() const { return true; }

QWidget *PutIoConnector::createSettingsWidget(QWidget *parent) {
  QWidget *widget = new QWidget(parent);
  QVBoxLayout *mainLayout = new QVBoxLayout(widget);

  QSettings settings;
  settings.beginGroup("Plugins/PutIO");

  QCheckBox *enabledCheck = new QCheckBox(tr("Enable Put.io"), widget);
  enabledCheck->setObjectName("enabledCheck");
  enabledCheck->setChecked(settings.value("enabled", false).toBool());
  mainLayout->addWidget(enabledCheck);

  QWidget *configWidget = new QWidget(widget);
  QFormLayout *configLayout = new QFormLayout(configWidget);

  QLineEdit *tokenEdit = new QLineEdit(configWidget);
  tokenEdit->setObjectName("tokenEdit");
  tokenEdit->setEchoMode(QLineEdit::Password);
  tokenEdit->setText(
      SecureStorage::readPassword("Plugins/PutIO", "oauthToken"));
  configLayout->addRow(tr("OAuth Token:"), tokenEdit);

  QSettings mainSettings;
  if (mainSettings.value("allowPlaintextStorage", false).toBool()) {
    QLabel *warningLabel = new QLabel(
        tr("⚠️ Warning: Data may be stored unencrypted based on preferences."));
    warningLabel->setStyleSheet("color: #d9534f; font-size: 11px;");
    configLayout->addRow("", warningLabel);
  }

  mainLayout->addWidget(configWidget);
  settings.endGroup();

  configWidget->setVisible(enabledCheck->isChecked());
  connect(enabledCheck, &QCheckBox::toggled, configWidget,
          &QWidget::setVisible);

  return widget;
}

void PutIoConnector::saveSettings(QWidget *settingsWidget) {
  if (!settingsWidget)
    return;

  QCheckBox *enabledCheck =
      settingsWidget->findChild<QCheckBox *>("enabledCheck");
  QLineEdit *tokenEdit = settingsWidget->findChild<QLineEdit *>("tokenEdit");

  QSettings settings;
  settings.beginGroup("Plugins/PutIO");

  if (enabledCheck) {
    bool en = enabledCheck->isChecked();
    settings.setValue("enabled", en);
    m_enabled = en;
  }
  if (tokenEdit) {
    SecureStorage::writePassword("Plugins/PutIO", "oauthToken",
                                 tokenEdit->text());
    m_oauthToken = tokenEdit->text();
  }

  settings.endGroup();
}

bool PutIoConnector::hasDebugMenu() const { return true; }

QList<HttpApiEndpoint> PutIoConnector::getHttpApiEndpoints() const {
  QList<HttpApiEndpoint> endpoints;

  HttpApiEndpoint accountInfo;
  accountInfo.name = "Account Info";
  accountInfo.description = "Retrieves account information";
  accountInfo.method = "GET";
  accountInfo.url = "https://api.put.io/v2/account/info";
  accountInfo.headers.insert("Authorization", "Bearer ${OAUTH_TOKEN}");
  endpoints.append(accountInfo);

  HttpApiEndpoint transferList;
  transferList.name = "Transfer List";
  transferList.description = "Lists all active transfers";
  transferList.method = "GET";
  transferList.url = "https://api.put.io/v2/transfers/list";
  transferList.headers.insert("Authorization", "Bearer ${OAUTH_TOKEN}");
  endpoints.append(transferList);

  HttpApiEndpoint addTransferMagnet;
  addTransferMagnet.name = "Add Transfer (Magnet)";
  addTransferMagnet.description = "Creates a new transfer from a magnet link";
  addTransferMagnet.method = "POST";
  addTransferMagnet.url = "https://api.put.io/v2/transfers/add";
  addTransferMagnet.headers.insert("Authorization", "Bearer ${OAUTH_TOKEN}");
  addTransferMagnet.headers.insert("Content-Type",
                                   "application/x-www-form-urlencoded");
  addTransferMagnet.body = "url=${MAGNET_LINK}";
  endpoints.append(addTransferMagnet);

  HttpApiEndpoint addTransferTorrent;
  addTransferTorrent.name = "Add Transfer (Torrent File)";
  addTransferTorrent.description =
      "Creates a new transfer from a .torrent file";
  addTransferTorrent.method = "POST";
  addTransferTorrent.url = "https://upload.put.io/v2/files/upload";
  addTransferTorrent.headers.insert("Authorization", "Bearer ${OAUTH_TOKEN}");
  addTransferTorrent.isMultipart = true;
  addTransferTorrent.multipartParts.insert("file",
                                           "file:///path/to/test.torrent");
  endpoints.append(addTransferTorrent);

  return endpoints;
}

QMap<QString, QString> PutIoConnector::getApiSubstitutions() const {
  QMap<QString, QString> subs;
  subs.insert("OAUTH_TOKEN", m_oauthToken);
  return subs;
}
