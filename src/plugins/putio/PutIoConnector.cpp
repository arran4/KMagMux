#include "PutIoConnector.h"
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
#include <QWidget>

PutIoConnector::PutIoConnector() : PutIoConnector(nullptr) {}

PutIoConnector::PutIoConnector(QObject *parent)
    : QObject(parent), m_networkManager(new QNetworkAccessManager(this)),
      m_oauthToken(""), m_enabled(true) {
  QSettings settings;
  settings.beginGroup("Plugins/PutIO");
  m_oauthToken = settings.value("oauthToken", "").toString();
  m_enabled = settings.value("enabled", true).toBool();
  settings.endGroup();
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
      delete multiPart;
      delete file;
      return;
    }
    QHttpPart filePart;
    QString filename = QFileInfo(item.sourcePath).fileName();
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QVariant("form-data; name=\"file\"; filename=\"" +
                                filename + "\""));
    filePart.setBodyDevice(file);
    file->setParent(multiPart);
    multiPart->append(filePart);
  }

  QNetworkReply *reply = m_networkManager->post(request, multiPart);
  multiPart->setParent(reply);
  reply->setProperty("itemId", item.id);
  connect(reply, &QNetworkReply::finished, this,
          &PutIoConnector::onAddTorrentReply);
}

void PutIoConnector::onAddTorrentReply() {
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

bool PutIoConnector::hasSettings() const { return true; }

QWidget *PutIoConnector::createSettingsWidget(QWidget *parent) {
  QWidget *widget = new QWidget(parent);
  QFormLayout *layout = new QFormLayout(widget);

  QSettings settings;
  settings.beginGroup("Plugins/PutIO");

  QCheckBox *enabledCheck = new QCheckBox(tr("Enable Put.io"), widget);
  enabledCheck->setObjectName("enabledCheck");
  enabledCheck->setChecked(settings.value("enabled", true).toBool());
  layout->addRow(enabledCheck);

  QLineEdit *tokenEdit = new QLineEdit(widget);
  tokenEdit->setObjectName("tokenEdit");
  tokenEdit->setEchoMode(QLineEdit::Password);
  tokenEdit->setText(settings.value("oauthToken", "").toString());
  layout->addRow(tr("OAuth Token:"), tokenEdit);

  settings.endGroup();
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
    settings.setValue("oauthToken", tokenEdit->text());
    m_oauthToken = tokenEdit->text();
  }

  settings.endGroup();
}
