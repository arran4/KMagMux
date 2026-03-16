#include "QBittorrentConnector.h"
#include "core/SecureStorage.h"
#include <QCheckBox>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QSettings>
#include <QUrlQuery>
#include <QWidget>

QBittorrentConnector::QBittorrentConnector() : QBittorrentConnector(nullptr) {}

QBittorrentConnector::QBittorrentConnector(QObject *parent)
    : QObject(parent), m_networkManager(new QNetworkAccessManager(this)),
      m_baseUrl(""), m_username(""), m_password(""), m_enabled(false),
      m_isLoggingIn(false) {
  m_networkManager->setCookieJar(new QNetworkCookieJar(this));

  QSettings settings;
  settings.beginGroup("Plugins/qBittorrent");
  m_baseUrl = settings.value("baseUrl", "http://localhost:8080").toString();
  m_username = settings.value("username", "").toString();
  m_enabled = settings.value("enabled", false).toBool();
  settings.endGroup();

  m_password = SecureStorage::readPassword("Plugins/qBittorrent", "password");
}

QString QBittorrentConnector::getId() const { return "qBittorrent"; }

QString QBittorrentConnector::getName() const { return "qBittorrent (WebAPI)"; }

bool QBittorrentConnector::isEnabled() const { return m_enabled; }

void QBittorrentConnector::setBaseUrl(const QString &url) {
  m_baseUrl = url;
  if (m_baseUrl.endsWith("/")) {
    m_baseUrl.chop(1);
  }
}

void QBittorrentConnector::setCredentials(const QString &username,
                                          const QString &password) {
  m_username = username;
  m_password = password;
}

void QBittorrentConnector::dispatch(const Item &item) {
  if (m_isLoggingIn) {
    m_pendingItems.append(item);
  } else {
    performDispatch(item);
  }
}

void QBittorrentConnector::login() {
  QUrl url(m_baseUrl + "/api/v2/auth/login");
  QNetworkRequest request(url);
  request.setHeader(QNetworkRequest::ContentTypeHeader,
                    "application/x-www-form-urlencoded");

  QUrlQuery postData;
  postData.addQueryItem("username", m_username);
  postData.addQueryItem("password", m_password);

  QNetworkReply *reply = m_networkManager->post(
      request, postData.toString(QUrl::FullyEncoded).toUtf8());
  connect(reply, &QNetworkReply::finished, this,
          &QBittorrentConnector::onLoginReply);
}

void QBittorrentConnector::onLoginReply() {
  QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
  if (!reply)
    return;

  m_isLoggingIn = false;

  if (reply->error() == QNetworkReply::NoError) {
    QString response = reply->readAll();
    if (response.trimmed() == "Fails.") {
      qWarning() << "qBittorrent Login failed: Invalid username or password.";
      QList<Item> itemsToFail = m_pendingItems;
      m_pendingItems.clear();

      for (const Item &item : itemsToFail) {
        emit dispatchFinished(item.id, false,
                              "Login failed: Invalid username or password.");
      }
    } else {
      // Login successful. The cookie jar in QNetworkAccessManager handles
      // the session cookie automatically. Proceed to dispatch pending items.
      QList<Item> itemsToDispatch = m_pendingItems;
      m_pendingItems.clear();

      for (const Item &item : itemsToDispatch) {
        performDispatch(item);
      }
    }
  } else {
    qWarning() << "qBittorrent Login failed:" << reply->errorString();
    QList<Item> itemsToFail = m_pendingItems;
    m_pendingItems.clear();

    for (const Item &item : itemsToFail) {
      emit dispatchFinished(item.id, false,
                            "Login failed: " + reply->errorString());
    }
  }
  reply->deleteLater();
}

void QBittorrentConnector::performDispatch(const Item &item) {
  QUrl url(m_baseUrl + "/api/v2/torrents/add");
  QNetworkRequest request(url);

  // We use QHttpMultiPart for both files and magnets because qBittorrent API
  // accepts multipart/form-data
  QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

  // 1. URLs (Magnet links)
  if (item.sourcePath.startsWith("magnet:")) {
    QHttpPart textPart;
    textPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QVariant("form-data; name=\"urls\""));
    textPart.setBody(item.sourcePath.toUtf8());
    multiPart->append(textPart);
  }
  // 2. Torrent Files
  else {
    QFile *file = new QFile(item.sourcePath);
    if (!file->open(QIODevice::ReadOnly)) {
      emit dispatchFinished(item.id, false,
                            "Could not open torrent file: " + item.sourcePath);
      if (multiPart) multiPart->deleteLater();
      if (file) file->deleteLater();
      return;
    }

    QHttpPart filePart;
    // qBittorrent requires the filename in content-disposition
    QString filename = QFileInfo(item.sourcePath).fileName();
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QVariant("form-data; name=\"torrents\"; filename=\"" +
                                filename + "\""));
    filePart.setBodyDevice(file);
    file->setParent(
        multiPart); // Valid trick: file is deleted when multiPart is deleted
    multiPart->append(filePart);
  }

  // 3. Save Path (Destination)
  if (!item.destinationPath.isEmpty()) {
    QHttpPart savePathPart;
    savePathPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                           QVariant("form-data; name=\"savepath\""));
    savePathPart.setBody(item.destinationPath.toUtf8());
    multiPart->append(savePathPart);
  }

  // 4. Category (from labels if present? or metadata?)
  if (item.metadata.contains("category")) {
    QHttpPart categoryPart;
    categoryPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                           QVariant("form-data; name=\"category\""));
    categoryPart.setBody(item.metadata["category"].toString().toUtf8());
    multiPart->append(categoryPart);
  }

  // 5. Tags (Labels)
  if (item.metadata.contains("labels")) {
    QHttpPart tagsPart;
    tagsPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QVariant("form-data; name=\"tags\""));
    tagsPart.setBody(item.metadata["labels"].toString().toUtf8());
    multiPart->append(tagsPart);
  }

  // 6. Paused (optional, maybe from metadata)
  // "paused" value="true"|"false"

  QNetworkReply *reply = m_networkManager->post(request, multiPart);
  multiPart->setParent(reply); // Delete multiPart with reply

  // Store the full item so we can re-dispatch if authentication fails
  reply->setProperty("item", QVariant::fromValue(item));
  reply->setProperty("itemId", item.id);

  connect(reply, &QNetworkReply::finished, this,
          &QBittorrentConnector::onAddTorrentReply);
}

void QBittorrentConnector::onAddTorrentReply() {
  QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
  if (!reply)
    return;

  Item item = reply->property("item").value<Item>();
  QString itemId = reply->property("itemId").toString();
  int statusCode =
      reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

  if (statusCode == 401 || statusCode == 403) {
    // Authentication failed, queue the item and attempt to login
    m_pendingItems.append(item);
    if (!m_isLoggingIn) {
      m_isLoggingIn = true;
      login();
    }
  } else if (reply->error() == QNetworkReply::NoError) {
    QString response = reply->readAll();
    if (response.toLower().contains("fail")) {
      emit dispatchFinished(itemId, false,
                            "qBittorrent API returned failure: " + response);
    } else {
      emit dispatchFinished(itemId, true, "Dispatched successfully.");
    }
  } else {
    emit dispatchFinished(itemId, false,
                          "Network error: " + reply->errorString());
  }
  reply->deleteLater();
}

bool QBittorrentConnector::hasSettings() const { return true; }

QWidget *QBittorrentConnector::createSettingsWidget(QWidget *parent) {
  QWidget *widget = new QWidget(parent);
  QVBoxLayout *mainLayout = new QVBoxLayout(widget);

  QSettings settings;
  settings.beginGroup("Plugins/qBittorrent");

  QCheckBox *enabledCheck = new QCheckBox(tr("Enable qBittorrent"), widget);
  enabledCheck->setObjectName("enabledCheck");
  enabledCheck->setChecked(settings.value("enabled", false).toBool());
  mainLayout->addWidget(enabledCheck);

  QWidget *configWidget = new QWidget(widget);
  QFormLayout *configLayout = new QFormLayout(configWidget);

  QLineEdit *urlEdit = new QLineEdit(configWidget);
  urlEdit->setObjectName("urlEdit");
  urlEdit->setText(
      settings.value("baseUrl", "http://localhost:8080").toString());
  configLayout->addRow(tr("Base URL:"), urlEdit);

  QLineEdit *userEdit = new QLineEdit(configWidget);
  userEdit->setObjectName("userEdit");
  userEdit->setText(settings.value("username", "").toString());
  configLayout->addRow(tr("Username:"), userEdit);

  QLineEdit *passEdit = new QLineEdit(configWidget);
  passEdit->setObjectName("passEdit");
  passEdit->setEchoMode(QLineEdit::Password);
  passEdit->setText(
      SecureStorage::readPassword("Plugins/qBittorrent", "password"));
  configLayout->addRow(tr("Password:"), passEdit);

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

void QBittorrentConnector::saveSettings(QWidget *settingsWidget) {
  if (!settingsWidget)
    return;

  QCheckBox *enabledCheck =
      settingsWidget->findChild<QCheckBox *>("enabledCheck");
  QLineEdit *urlEdit = settingsWidget->findChild<QLineEdit *>("urlEdit");
  QLineEdit *userEdit = settingsWidget->findChild<QLineEdit *>("userEdit");
  QLineEdit *passEdit = settingsWidget->findChild<QLineEdit *>("passEdit");

  QSettings settings;
  settings.beginGroup("Plugins/qBittorrent");

  if (enabledCheck) {
    bool en = enabledCheck->isChecked();
    settings.setValue("enabled", en);
    m_enabled = en;
  }
  if (urlEdit) {
    settings.setValue("baseUrl", urlEdit->text());
    setBaseUrl(urlEdit->text());
  }
  if (userEdit && passEdit) {
    settings.setValue("username", userEdit->text());
    SecureStorage::writePassword("Plugins/qBittorrent", "password",
                                 passEdit->text());
    setCredentials(userEdit->text(), passEdit->text());
  }

  settings.endGroup();
}

bool QBittorrentConnector::hasDebugMenu() const { return true; }

QList<HttpApiEndpoint> QBittorrentConnector::getHttpApiEndpoints() const {
  QList<HttpApiEndpoint> endpoints;

  HttpApiEndpoint login;
  login.name = "Login";
  login.description = "Authenticate with qBittorrent";
  login.method = "POST";
  login.url = "${BASE_URL}/api/v2/auth/login";
  login.headers.insert("Content-Type", "application/x-www-form-urlencoded");
  login.body = "username=${USERNAME}&password=${PASSWORD}";
  endpoints.append(login);

  HttpApiEndpoint getTorrents;
  getTorrents.name = "Get Torrents";
  getTorrents.description = "List all torrents";
  getTorrents.method = "GET";
  getTorrents.url = "${BASE_URL}/api/v2/torrents/info";
  endpoints.append(getTorrents);

  HttpApiEndpoint addTorrentMagnet;
  addTorrentMagnet.name = "Add Torrent (Magnet)";
  addTorrentMagnet.description = "Add a new torrent from a magnet link";
  addTorrentMagnet.method = "POST";
  addTorrentMagnet.url = "${BASE_URL}/api/v2/torrents/add";
  addTorrentMagnet.isMultipart = true;
  addTorrentMagnet.multipartParts.insert("urls", "${MAGNET_LINK}");
  endpoints.append(addTorrentMagnet);

  HttpApiEndpoint addTorrentFile;
  addTorrentFile.name = "Add Torrent (File)";
  addTorrentFile.description = "Add a new torrent from a .torrent file";
  addTorrentFile.method = "POST";
  addTorrentFile.url = "${BASE_URL}/api/v2/torrents/add";
  addTorrentFile.isMultipart = true;
  addTorrentFile.multipartParts.insert("torrents",
                                       "file:///path/to/test.torrent");
  endpoints.append(addTorrentFile);

  return endpoints;
}

QMap<QString, QString> QBittorrentConnector::getApiSubstitutions() const {
  QMap<QString, QString> subs;
  subs.insert("BASE_URL", m_baseUrl);
  subs.insert("USERNAME", m_username);
  subs.insert("PASSWORD", m_password);
  return subs;
}
