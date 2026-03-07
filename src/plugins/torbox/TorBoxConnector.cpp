#include "TorBoxConnector.h"
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
#include <QSslConfiguration>
#include <QSslSocket>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

TorBoxConnector::TorBoxConnector() : TorBoxConnector(nullptr) {}

TorBoxConnector::TorBoxConnector(QObject *parent)
    : QObject(parent), m_networkManager(new QNetworkAccessManager(this)),
      m_apiToken(""), m_enabled(false) {
  QSettings settings;
  settings.beginGroup("Plugins/TorBox");
  m_enabled = settings.value("enabled", false).toBool();
  settings.endGroup();

  m_apiToken = SecureStorage::readPassword("Plugins/TorBox", "apiToken");
}

QString TorBoxConnector::getId() const { return "TorBox"; }

QString TorBoxConnector::getName() const { return "TorBox"; }

bool TorBoxConnector::isEnabled() const { return m_enabled; }

void TorBoxConnector::dispatch(const Item &item) {
  // Simple stub for dispatch
  QUrl url("https://api.torbox.app/v1/api/torrents/createtorrent");
  QNetworkRequest request(url);
  QSslConfiguration sslConfig = request.sslConfiguration();
  sslConfig.setPeerVerifyMode(QSslSocket::VerifyPeer);
  request.setSslConfiguration(sslConfig);
  // Assuming Bearer token auth
  request.setRawHeader("Authorization", ("Bearer " + m_apiToken).toUtf8());

  QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
  if (item.sourcePath.startsWith("magnet:")) {
    QHttpPart textPart;
    textPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QVariant("form-data; name=\"magnet\""));
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
    filePart.setHeader(
        QNetworkRequest::ContentDispositionHeader,
        QVariant("form-data; name=\"file\"; filename=\"" + filename + "\""));
    filePart.setBodyDevice(file);
    file->setParent(multiPart);
    multiPart->append(filePart);
  }

  QNetworkReply *reply = m_networkManager->post(request, multiPart);
  multiPart->setParent(reply);
  reply->setProperty("itemId", item.id);
  connect(reply, &QNetworkReply::finished, this,
          &TorBoxConnector::onAddTorrentReply);
}

void TorBoxConnector::onAddTorrentReply() {
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

bool TorBoxConnector::hasSettings() const { return true; }

QWidget *TorBoxConnector::createSettingsWidget(QWidget *parent) {
  QWidget *widget = new QWidget(parent);
  QVBoxLayout *mainLayout = new QVBoxLayout(widget);

  QSettings settings;
  settings.beginGroup("Plugins/TorBox");

  QCheckBox *enabledCheck = new QCheckBox(tr("Enable TorBox"), widget);
  enabledCheck->setObjectName("enabledCheck");
  enabledCheck->setChecked(settings.value("enabled", false).toBool());
  mainLayout->addWidget(enabledCheck);

  QWidget *configWidget = new QWidget(widget);
  QFormLayout *configLayout = new QFormLayout(configWidget);

  QLineEdit *tokenEdit = new QLineEdit(configWidget);
  tokenEdit->setObjectName("tokenEdit");
  tokenEdit->setEchoMode(QLineEdit::Password);
  tokenEdit->setText(SecureStorage::readPassword("Plugins/TorBox", "apiToken"));
  configLayout->addRow(tr("API Token:"), tokenEdit);

  mainLayout->addWidget(configWidget);
  settings.endGroup();

  configWidget->setVisible(enabledCheck->isChecked());
  connect(enabledCheck, &QCheckBox::toggled, configWidget,
          &QWidget::setVisible);

  return widget;
}

void TorBoxConnector::saveSettings(QWidget *settingsWidget) {
  if (!settingsWidget)
    return;

  QCheckBox *enabledCheck =
      settingsWidget->findChild<QCheckBox *>("enabledCheck");
  QLineEdit *tokenEdit = settingsWidget->findChild<QLineEdit *>("tokenEdit");

  QSettings settings;
  settings.beginGroup("Plugins/TorBox");

  if (enabledCheck) {
    bool en = enabledCheck->isChecked();
    settings.setValue("enabled", en);
    m_enabled = en;
  }
  if (tokenEdit) {
    SecureStorage::writePassword("Plugins/TorBox", "apiToken",
                                 tokenEdit->text());
    m_apiToken = tokenEdit->text();
  }

  settings.endGroup();
}
