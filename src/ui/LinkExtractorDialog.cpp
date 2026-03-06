#include "LinkExtractorDialog.h"

#include <QCloseEvent>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextStream>
#include <QUrl>
#include <QVBoxLayout>

LinkExtractorDialog::LinkExtractorDialog(const QStringList &lines,
                                         QWidget *parent)
    : QDialog(parent), m_inputLines(lines), m_currentIndex(0),
      m_cancelled(false), m_modified(false),
      m_networkManager(new QNetworkAccessManager(this)),
      m_currentReply(nullptr) {
  setWindowTitle(tr("Extracting Links"));
  resize(500, 300);

  QVBoxLayout *layout = new QVBoxLayout(this);

  m_logEdit = new QPlainTextEdit(this);
  m_logEdit->setReadOnly(true);
  layout->addWidget(m_logEdit);

  m_cancelBtn = new QPushButton(tr("Cancel"), this);
  layout->addWidget(m_cancelBtn);

  connect(m_cancelBtn, &QPushButton::clicked, this,
          &LinkExtractorDialog::onCancelClicked);

  // Start processing asynchronously so the dialog can show first
  QMetaObject::invokeMethod(this, "processNext", Qt::QueuedConnection);
}

void LinkExtractorDialog::appendLog(const QString &msg) {
  m_logEdit->appendPlainText(msg);
}

void LinkExtractorDialog::closeEvent(QCloseEvent *event) {
  if (!m_cancelled && m_currentIndex < m_inputLines.size()) {
    onCancelClicked();
    event->ignore();
  } else {
    event->accept();
  }
}

void LinkExtractorDialog::onCancelClicked() {
  m_cancelled = true;
  if (m_currentReply) {
    m_currentReply->abort();
    m_currentReply->deleteLater();
    m_currentReply = nullptr;
  }
  appendLog(tr("Cancelled."));
  reject();
}

void LinkExtractorDialog::processNext() {
  if (m_cancelled)
    return;

  if (m_currentIndex >= m_inputLines.size()) {
    appendLog(tr("Finished processing."));
    accept();
    return;
  }

  QString line = m_inputLines[m_currentIndex].trimmed();
  m_currentIndex++;

  if (line.isEmpty()) {
    QMetaObject::invokeMethod(this, "processNext", Qt::QueuedConnection);
    return;
  }

  QUrl url(line);
  QString pathToCheck = line;
  if (line.startsWith("file://")) {
    pathToCheck = line.mid(7);
  }

  QFileInfo fileInfo(pathToCheck);
  bool isLocalFile = fileInfo.exists() && fileInfo.isFile();

  QString ext;
  if (isLocalFile) {
    ext = fileInfo.suffix().toLower();
  } else if (url.isValid() &&
             (url.scheme() == "http" || url.scheme() == "https")) {
    ext = QFileInfo(url.path()).suffix().toLower();
  }

  bool shouldExpand = (ext == "txt" || ext == "html" || ext == "htm");

  if (isLocalFile && shouldExpand) {
    appendLog(tr("Reading local file: %1").arg(pathToCheck));
    QFile file(pathToCheck);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
      QString content = QTextStream(&file).readAll();
      if (ext == "html" || ext == "htm") {
        extractFromHtml(content, QUrl::fromLocalFile(pathToCheck));
      } else {
        extractFromTxt(content);
      }
      m_modified = true;
    } else {
      appendLog(tr("Failed to open file: %1").arg(pathToCheck));
      m_expandedLines.append(line);
    }
    QMetaObject::invokeMethod(this, "processNext", Qt::QueuedConnection);
    return;
  }

  if (!isLocalFile && shouldExpand && url.isValid() &&
      (url.scheme() == "http" || url.scheme() == "https")) {
    appendLog(tr("Downloading remote file: %1").arg(line));
    m_currentReply = m_networkManager->get(QNetworkRequest(url));
    connect(m_currentReply, &QNetworkReply::finished, this,
            &LinkExtractorDialog::onReplyFinished);
    return;
  }

  // Keep the line as is
  m_expandedLines.append(line);
  QMetaObject::invokeMethod(this, "processNext", Qt::QueuedConnection);
}

void LinkExtractorDialog::onReplyFinished() {
  if (m_cancelled)
    return;

  if (!m_currentReply) {
    QMetaObject::invokeMethod(this, "processNext", Qt::QueuedConnection);
    return;
  }

  QNetworkReply *reply = m_currentReply;
  m_currentReply = nullptr; // Clear it so cancel doesn't touch it

  QUrl url = reply->url();
  QString ext = QFileInfo(url.path()).suffix().toLower();

  if (reply->error() == QNetworkReply::NoError) {
    QString content = QString::fromUtf8(reply->readAll());
    if (ext == "html" || ext == "htm") {
      extractFromHtml(content, url);
    } else {
      extractFromTxt(content);
    }
    m_modified = true;
    appendLog(tr("Successfully processed remote file."));
  } else {
    appendLog(tr("Failed to download %1: %2")
                  .arg(url.toString(), reply->errorString()));
    // Fallback to original
    m_expandedLines.append(url.toString());
  }

  reply->deleteLater();
  QMetaObject::invokeMethod(this, "processNext", Qt::QueuedConnection);
}

void LinkExtractorDialog::extractFromHtml(const QString &content,
                                          const QUrl &baseUrl) {
  QRegularExpression re("href=[\"']([^\"']+)[\"']",
                        QRegularExpression::CaseInsensitiveOption);
  QRegularExpressionMatchIterator i = re.globalMatch(content);
  int addedCount = 0;
  while (i.hasNext()) {
    QRegularExpressionMatch match = i.next();
    QString linkStr = match.captured(1).trimmed();
    if (!linkStr.isEmpty()) {
      QUrl linkUrl(linkStr);
      if (linkUrl.isRelative()) {
        linkUrl = baseUrl.resolved(linkUrl);
      }
      QString resolvedStr = linkUrl.toString();

      // Only add magnet or torrent links
      if (resolvedStr.startsWith("magnet:") ||
          resolvedStr.endsWith(".torrent")) {
        m_expandedLines.append(resolvedStr);
        addedCount++;
      }
    }
  }
  appendLog(tr("Extracted %1 links from HTML.").arg(addedCount));
}

void LinkExtractorDialog::extractFromTxt(const QString &content) {
  QStringList lines = content.split('\n');
  int addedCount = 0;
  for (const QString &line : lines) {
    QString trimmed = line.trimmed();
    if (!trimmed.isEmpty()) {
      m_expandedLines.append(trimmed);
      addedCount++;
    }
  }
  appendLog(tr("Extracted %1 links from TXT.").arg(addedCount));
}
