#include "ApiExplorerDialog.h"

#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QHttpMultiPart>
#include <QLabel>
#include <QMessageBox>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QSplitter>
#include <QSslConfiguration>
#include <QVBoxLayout>

ApiExplorerDialog::ApiExplorerDialog(Connector *connector, QWidget *parent)
    : QDialog(parent), m_connector(connector), m_currentReply(nullptr) {
  m_networkManager = new QNetworkAccessManager(this);
  connect(m_networkManager, &QNetworkAccessManager::finished, this,
          &ApiExplorerDialog::onNetworkReplyFinished);
  // Note: handling SSL errors generally shouldn't be done silently for
  // "Security by Default", but we can connect it to show an error.
  connect(m_networkManager, &QNetworkAccessManager::sslErrors, this,
          &ApiExplorerDialog::onSslErrors);

  if (m_connector) {
    m_endpoints = m_connector->getHttpApiEndpoints();
    m_substitutions = m_connector->getApiSubstitutions();
  }

  setupUi();
  loadEndpoints();
}

ApiExplorerDialog::~ApiExplorerDialog() {
  if (m_currentReply) {
    m_currentReply->abort();
    m_currentReply->deleteLater();
  }
}

void ApiExplorerDialog::setupUi() {
  setWindowTitle(QString("API Explorer - %1")
                     .arg(m_connector ? m_connector->getName() : "Unknown"));
  resize(900, 600);

  QHBoxLayout *mainLayout = new QHBoxLayout(this);
  QSplitter *splitter = new QSplitter(Qt::Horizontal, this);
  mainLayout->addWidget(splitter);

  // LHS: Endpoints List
  QWidget *leftWidget = new QWidget(splitter);
  QVBoxLayout *leftLayout = new QVBoxLayout(leftWidget);
  leftLayout->addWidget(new QLabel("Endpoints"));

  m_endpointList = new QListWidget(leftWidget);
  leftLayout->addWidget(m_endpointList);
  connect(m_endpointList, &QListWidget::currentRowChanged, this,
          &ApiExplorerDialog::onEndpointSelected);
  splitter->addWidget(leftWidget);

  // RHS: Form and Response
  QSplitter *rightSplitter = new QSplitter(Qt::Vertical, splitter);

  // Request Group
  QGroupBox *requestGroup = new QGroupBox("Request", rightSplitter);
  QVBoxLayout *requestLayout = new QVBoxLayout(requestGroup);

  QHBoxLayout *urlLayout = new QHBoxLayout();
  m_methodCombo = new QComboBox();
  m_methodCombo->addItems({"GET", "POST", "PUT", "DELETE", "PATCH"});
  urlLayout->addWidget(m_methodCombo);

  m_urlEdit = new QLineEdit();
  urlLayout->addWidget(m_urlEdit);

  QPushButton *sendButton = new QPushButton("Send");
  connect(sendButton, &QPushButton::clicked, this,
          &ApiExplorerDialog::onSendRequest);
  urlLayout->addWidget(sendButton);
  requestLayout->addLayout(urlLayout);

  // Headers Table
  requestLayout->addWidget(new QLabel("Headers:"));
  m_headersTable = new QTableWidget(0, 2);
  m_headersTable->setHorizontalHeaderLabels({"Key", "Value"});
  m_headersTable->horizontalHeader()->setStretchLastSection(true);
  m_headersTable->verticalHeader()->setVisible(false);
  requestLayout->addWidget(m_headersTable);

  QHBoxLayout *headerButtonsLayout = new QHBoxLayout();
  QPushButton *addHeaderBtn = new QPushButton("Add Header");
  QPushButton *removeHeaderBtn = new QPushButton("Remove Header");
  connect(addHeaderBtn, &QPushButton::clicked, this,
          &ApiExplorerDialog::onAddHeader);
  connect(removeHeaderBtn, &QPushButton::clicked, this,
          &ApiExplorerDialog::onRemoveHeader);
  headerButtonsLayout->addWidget(addHeaderBtn);
  headerButtonsLayout->addWidget(removeHeaderBtn);
  headerButtonsLayout->addStretch();
  requestLayout->addLayout(headerButtonsLayout);

  // Request Body
  requestLayout->addWidget(new QLabel("Body:"));

  m_rawBodyWidget = new QWidget();
  QVBoxLayout *rawBodyLayout = new QVBoxLayout(m_rawBodyWidget);
  rawBodyLayout->setContentsMargins(0, 0, 0, 0);
  m_bodyEdit = new QPlainTextEdit();
  rawBodyLayout->addWidget(m_bodyEdit);
  requestLayout->addWidget(m_rawBodyWidget);

  m_multipartWidget = new QWidget();
  QVBoxLayout *multipartLayout = new QVBoxLayout(m_multipartWidget);
  multipartLayout->setContentsMargins(0, 0, 0, 0);
  m_multipartTable = new QTableWidget(0, 2);
  m_multipartTable->setHorizontalHeaderLabels({"Name", "Value / FilePath"});
  m_multipartTable->horizontalHeader()->setStretchLastSection(true);
  m_multipartTable->verticalHeader()->setVisible(false);
  multipartLayout->addWidget(m_multipartTable);

  QHBoxLayout *multipartButtonsLayout = new QHBoxLayout();
  QPushButton *addMultiBtn = new QPushButton("Add Part");
  QPushButton *removeMultiBtn = new QPushButton("Remove Part");
  QPushButton *browseMultiBtn = new QPushButton("Browse File...");
  connect(addMultiBtn, &QPushButton::clicked, this,
          &ApiExplorerDialog::onAddMultipart);
  connect(removeMultiBtn, &QPushButton::clicked, this,
          &ApiExplorerDialog::onRemoveMultipart);
  connect(browseMultiBtn, &QPushButton::clicked, this,
          &ApiExplorerDialog::onBrowseMultipartFile);
  multipartButtonsLayout->addWidget(addMultiBtn);
  multipartButtonsLayout->addWidget(removeMultiBtn);
  multipartButtonsLayout->addWidget(browseMultiBtn);
  multipartButtonsLayout->addStretch();
  multipartLayout->addLayout(multipartButtonsLayout);

  requestLayout->addWidget(m_multipartWidget);
  m_multipartWidget->setVisible(false);
  m_isMultipartCurrent = false;

  rightSplitter->addWidget(requestGroup);

  // Response Group
  QGroupBox *responseGroup = new QGroupBox("Response", rightSplitter);
  QVBoxLayout *responseLayout = new QVBoxLayout(responseGroup);

  m_statusLabel = new QLabel("Status: ");
  responseLayout->addWidget(m_statusLabel);

  m_responseEdit = new QPlainTextEdit();
  m_responseEdit->setReadOnly(true);
  responseLayout->addWidget(m_responseEdit);
  rightSplitter->addWidget(responseGroup);

  splitter->addWidget(rightSplitter);
  splitter->setStretchFactor(0, 1);
  splitter->setStretchFactor(1, 3);
}

void ApiExplorerDialog::loadEndpoints() {
  m_endpointList->clear();
  m_endpointList->addItem("New");
  for (const auto &ep : m_endpoints) {
    m_endpointList->addItem(ep.name);
  }
  m_endpointList->setCurrentRow(0); // Select "New"
}

void ApiExplorerDialog::populateForm(const HttpApiEndpoint &endpoint) {
  // Set method
  int methodIndex = m_methodCombo->findText(
      endpoint.method, Qt::MatchExactly | Qt::MatchCaseSensitive);
  if (methodIndex != -1)
    m_methodCombo->setCurrentIndex(methodIndex);
  else
    m_methodCombo->setEditText(endpoint.method);

  // Set URL
  m_urlEdit->setText(applySubstitutions(endpoint.url));

  // Set headers
  m_headersTable->setRowCount(0);
  auto keys = endpoint.headers.keys();
  for (const QString &key : keys) {
    QString val = endpoint.headers.value(key);
    int row = m_headersTable->rowCount();
    m_headersTable->insertRow(row);
    m_headersTable->setItem(row, 0,
                            new QTableWidgetItem(applySubstitutions(key)));
    m_headersTable->setItem(row, 1,
                            new QTableWidgetItem(applySubstitutions(val)));
  }

  m_isMultipartCurrent = endpoint.isMultipart;
  if (m_isMultipartCurrent) {
    m_rawBodyWidget->setVisible(false);
    m_multipartWidget->setVisible(true);

    m_multipartTable->setRowCount(0);
    auto multiKeys = endpoint.multipartParts.keys();
    for (const QString &key : multiKeys) {
      QString val = endpoint.multipartParts.value(key);
      int row = m_multipartTable->rowCount();
      m_multipartTable->insertRow(row);
      m_multipartTable->setItem(row, 0,
                                new QTableWidgetItem(applySubstitutions(key)));
      m_multipartTable->setItem(row, 1,
                                new QTableWidgetItem(applySubstitutions(val)));
    }
  } else {
    m_rawBodyWidget->setVisible(true);
    m_multipartWidget->setVisible(false);

    // Set body
    m_bodyEdit->setPlainText(
        QString::fromUtf8(applySubstitutions(endpoint.body)));
  }

  m_responseEdit->clear();
  m_statusLabel->setText("Status: ");
}

QString ApiExplorerDialog::applySubstitutions(QString text) const {
  auto subKeys = m_substitutions.keys();
  for (const QString &key : subKeys) {
    QString val = m_substitutions.value(key);
    text.replace(QString("${%1}").arg(key), val);
  }
  return text;
}

QByteArray ApiExplorerDialog::applySubstitutions(QByteArray data) const {
  QString text = QString::fromUtf8(data);
  text = applySubstitutions(text);
  return text.toUtf8();
}

void ApiExplorerDialog::onEndpointSelected(int currentRow) {
  if (currentRow == 0) { // New
    m_methodCombo->setCurrentIndex(0);
    m_urlEdit->clear();
    m_headersTable->setRowCount(0);
    m_bodyEdit->clear();
    m_multipartTable->setRowCount(0);
    m_isMultipartCurrent = false;
    m_multipartWidget->setVisible(false);
    m_rawBodyWidget->setVisible(true);
    m_responseEdit->clear();
    m_statusLabel->setText("Status: ");
  } else if (currentRow > 0 && currentRow <= m_endpoints.size()) {
    populateForm(m_endpoints[currentRow - 1]);
  }
}

void ApiExplorerDialog::onAddHeader() {
  int row = m_headersTable->rowCount();
  m_headersTable->insertRow(row);
  m_headersTable->setItem(row, 0, new QTableWidgetItem(""));
  m_headersTable->setItem(row, 1, new QTableWidgetItem(""));
}

void ApiExplorerDialog::onRemoveHeader() {
  int row = m_headersTable->currentRow();
  if (row >= 0) {
    m_headersTable->removeRow(row);
  }
}

void ApiExplorerDialog::onAddMultipart() {
  int row = m_multipartTable->rowCount();
  m_multipartTable->insertRow(row);
  m_multipartTable->setItem(row, 0, new QTableWidgetItem(""));
  m_multipartTable->setItem(row, 1, new QTableWidgetItem(""));
}

void ApiExplorerDialog::onRemoveMultipart() {
  int row = m_multipartTable->currentRow();
  if (row >= 0) {
    m_multipartTable->removeRow(row);
  }
}

#include <QFileDialog>
void ApiExplorerDialog::onBrowseMultipartFile() {
  int row = m_multipartTable->currentRow();
  if (row < 0) {
    QMessageBox::information(this, "Select Row",
                             "Please select a row first to set its file path.");
    return;
  }
  QString file = QFileDialog::getOpenFileName(this, "Select File");
  if (!file.isEmpty()) {
    m_multipartTable->setItem(row, 1, new QTableWidgetItem("file://" + file));
  }
}

void ApiExplorerDialog::onSendRequest() {
  if (m_currentReply) {
    m_currentReply->abort();
    m_currentReply->deleteLater();
    m_currentReply = nullptr;
  }

  QString urlString = m_urlEdit->text();
  QUrl url(urlString);

  if (!url.isValid()) {
    QMessageBox::warning(this, "Error", "Invalid URL");
    return;
  }

  if (url.scheme() == "http") {
    qWarning()
        << "Security warning: Requesting an insecure HTTP URL in API Explorer.";
  }

  QNetworkRequest request(url);

  // Explicitly enforce SSL peer verification
  QSslConfiguration sslConfig = request.sslConfiguration();
  sslConfig.setPeerVerifyMode(QSslSocket::VerifyPeer);
  request.setSslConfiguration(sslConfig);

  // Apply headers
  for (int i = 0; i < m_headersTable->rowCount(); ++i) {
    QTableWidgetItem *keyItem = m_headersTable->item(i, 0);
    QTableWidgetItem *valItem = m_headersTable->item(i, 1);
    if (keyItem && valItem && !keyItem->text().isEmpty()) {
      request.setRawHeader(keyItem->text().toUtf8(), valItem->text().toUtf8());
    }
  }

  QString method = m_methodCombo->currentText();

  m_statusLabel->setText("Status: Sending...");
  m_responseEdit->clear();

  m_urlEdit->setEnabled(false);
  m_methodCombo->setEnabled(false);
  m_bodyEdit->setEnabled(false);
  m_multipartTable->setEnabled(false);
  m_headersTable->setEnabled(false);

  if (m_isMultipartCurrent) {
    QHttpMultiPart *multiPart =
        new QHttpMultiPart(QHttpMultiPart::FormDataType);

    for (int i = 0; i < m_multipartTable->rowCount(); ++i) {
      QTableWidgetItem *keyItem = m_multipartTable->item(i, 0);
      QTableWidgetItem *valItem = m_multipartTable->item(i, 1);
      if (keyItem && valItem && !keyItem->text().isEmpty()) {
        QHttpPart part;
        QString key = keyItem->text();
        QString val = valItem->text();

        if (val.startsWith("file://")) {
          QString filePath = val.mid(7); // strip file://
          QFile *file = new QFile(filePath);
          if (file->open(QIODevice::ReadOnly)) {
            QString filename = QFileInfo(filePath).fileName();
            part.setHeader(QNetworkRequest::ContentDispositionHeader,
                           QVariant("form-data; name=\"" + key +
                                    "\"; filename=\"" + filename + "\""));
            part.setBodyDevice(file);
            file->setParent(multiPart); // File will be deleted with multipart
          } else {
            QMessageBox::warning(this, "File Error",
                                 "Failed to open file: " + filePath);
            delete file;
            delete multiPart;
            return;
          }
        } else {
          part.setHeader(QNetworkRequest::ContentDispositionHeader,
                         QVariant("form-data; name=\"" + key + "\""));
          part.setBody(val.toUtf8());
        }
        multiPart->append(part);
      }
    }

    if (method == "POST") {
      m_currentReply = m_networkManager->post(request, multiPart);
    } else if (method == "PUT") {
      m_currentReply = m_networkManager->put(request, multiPart);
    } else {
      m_currentReply = m_networkManager->sendCustomRequest(
          request, method.toUtf8(), multiPart);
    }
    multiPart->setParent(m_currentReply);

  } else {
    QByteArray body = m_bodyEdit->toPlainText().toUtf8();
    if (method == "GET") {
      m_currentReply = m_networkManager->get(request);
    } else if (method == "POST") {
      m_currentReply = m_networkManager->post(request, body);
    } else if (method == "PUT") {
      m_currentReply = m_networkManager->put(request, body);
    } else if (method == "DELETE") {
      m_currentReply = m_networkManager->deleteResource(request);
    } else {
      m_currentReply =
          m_networkManager->sendCustomRequest(request, method.toUtf8(), body);
    }
  }
}

void ApiExplorerDialog::onNetworkReplyFinished() {
  QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
  if (!reply)
    return;

  m_urlEdit->setEnabled(true);
  m_methodCombo->setEnabled(true);
  m_bodyEdit->setEnabled(true);
  m_multipartTable->setEnabled(true);
  m_headersTable->setEnabled(true);

  if (reply == m_currentReply) {
    m_currentReply = nullptr;
  }

  int statusCode =
      reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  m_statusLabel->setText(QString("Status: %1 %2")
                             .arg(statusCode)
                             .arg(reply->error() != QNetworkReply::NoError
                                      ? reply->errorString()
                                      : "OK"));

  QString responseText;

  // Response Headers
  responseText += "--- Headers ---\n";
  const auto rawHeaders = reply->rawHeaderList();
  for (const QByteArray &headerName : rawHeaders) {
    responseText += headerName + ": " + reply->rawHeader(headerName) + "\n";
  }
  responseText += "\n--- Body ---\n";

  // Response Body
  responseText += reply->readAll();

  m_responseEdit->setPlainText(responseText);

  reply->deleteLater();
}

void ApiExplorerDialog::onSslErrors(QNetworkReply *reply,
                                    const QList<QSslError> &errors) {
  QString errorString;
  for (const QSslError &error : errors) {
    if (!errorString.isEmpty()) {
      errorString += "\n";
    }
    errorString += error.errorString();
  }
  QMessageBox::critical(
      this, "SSL Error",
      QString("SSL Verification Failed:\n%1").arg(errorString));
  // reply->ignoreSslErrors(); // DO NOT ignore ssl errors as per Security By
  // Default However, wait, in security by default we should NOT ignore. So we
  // DO NOT ignore, and the request will fail.
}
