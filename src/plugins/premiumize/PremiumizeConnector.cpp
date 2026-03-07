#include "PremiumizeConnector.h"
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
#include <QWidget>

PremiumizeConnector::PremiumizeConnector() : PremiumizeConnector(nullptr) {}

PremiumizeConnector::PremiumizeConnector(QObject *parent)
    : QObject(parent), m_networkManager(new QNetworkAccessManager(this)),
      m_apiKey(""), m_enabled(true) {
  QSettings settings;
  settings.beginGroup("Plugins/Premiumize");
  m_apiKey = settings.value("apiKey", "").toString();
  m_enabled = settings.value("enabled", true).toBool();
  settings.endGroup();
}

QString PremiumizeConnector::getId() const { return "Premiumize"; }

QString PremiumizeConnector::getName() const { return "Premiumize.me"; }

bool PremiumizeConnector::isEnabled() const { return m_enabled; }

void PremiumizeConnector::dispatch(const Item &item) {
  QUrl url("https://www.premiumize.me/api/transfer/create");

  if (item.sourcePath.startsWith("magnet:")) {
    // Premiumize uses form-data or x-www-form-urlencoded for magnet links
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    QUrlQuery postData;
    postData.addQueryItem("apikey", m_apiKey);
    postData.addQueryItem("src", item.sourcePath);

    QNetworkReply *reply = m_networkManager->post(request, postData.toString(QUrl::FullyEncoded).toUtf8());
    reply->setProperty("itemId", item.id);
    connect(reply, &QNetworkReply::finished, this,
            &PremiumizeConnector::onAddTorrentReply);
  } else {
    // For files we need multipart
    QNetworkRequest request(url);
    // Note: Do not set content-type for multipart, QNetworkAccessManager handles the boundary automatically

    QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    QHttpPart apiKeyPart;
    apiKeyPart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"apikey\""));
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
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QVariant("form-data; name=\"src\"; filename=\"" +
                                filename + "\""));
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
  QFormLayout *layout = new QFormLayout(widget);

  QSettings settings;
  settings.beginGroup("Plugins/Premiumize");

  QCheckBox *enabledCheck = new QCheckBox(tr("Enable Premiumize.me"), widget);
  enabledCheck->setObjectName("enabledCheck");
  enabledCheck->setChecked(settings.value("enabled", true).toBool());
  layout->addRow(enabledCheck);

  QLineEdit *tokenEdit = new QLineEdit(widget);
  tokenEdit->setObjectName("tokenEdit");
  tokenEdit->setEchoMode(QLineEdit::Password);
  tokenEdit->setText(settings.value("apiKey", "").toString());
  layout->addRow(tr("API Key:"), tokenEdit);

  settings.endGroup();
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
    settings.setValue("apiKey", tokenEdit->text());
    m_apiKey = tokenEdit->text();
  }

  settings.endGroup();
}
