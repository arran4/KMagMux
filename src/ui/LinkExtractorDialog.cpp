#include "LinkExtractorDialog.h"

#include <QCloseEvent>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextStream>
#include <QUrl>
#include <QVBoxLayout>

LinkExtractorDialog::LinkExtractorDialog(const QStringList &lines, bool extractMagnets, bool extractTorrents, QWidget *parent)
    : QDialog(parent)
    , m_inputLines(lines)
    , m_currentIndex(0)
    , m_cancelled(false)
    , m_modified(false)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_currentReply(nullptr)
{
    setWindowTitle(tr("Extracting Links"));
    resize(500, 300);

    QVBoxLayout *layout = new QVBoxLayout(this);

    QHBoxLayout *optionsLayout = new QHBoxLayout();
    m_extractMagnetsCb = new QCheckBox(tr("Extract magnet links"), this);
    m_extractMagnetsCb->setChecked(extractMagnets);
    m_extractMagnetsCb->setEnabled(false); // Read-only

    m_extractTorrentsCb = new QCheckBox(tr("Extract torrent links"), this);
    m_extractTorrentsCb->setChecked(extractTorrents);
    m_extractTorrentsCb->setEnabled(false); // Read-only
    optionsLayout->addWidget(m_extractMagnetsCb);
    optionsLayout->addWidget(m_extractTorrentsCb);
    optionsLayout->addStretch();
    layout->addLayout(optionsLayout);

    m_logEdit = new QPlainTextEdit(this);
    m_logEdit->setReadOnly(true);
    layout->addWidget(m_logEdit);

    m_cancelBtn = new QPushButton(tr("Cancel"), this);
    layout->addWidget(m_cancelBtn);

    connect(m_cancelBtn, &QPushButton::clicked, this, &LinkExtractorDialog::onCancelClicked);

    // Start processing asynchronously so the dialog can show first
    QMetaObject::invokeMethod(this, "processNext", Qt::QueuedConnection);
}

void LinkExtractorDialog::appendLog(const QString &msg)
{
    m_logEdit->appendPlainText(msg);
}

void LinkExtractorDialog::closeEvent(QCloseEvent *event)
{
    if (!m_cancelled && m_currentIndex < m_inputLines.size()) {
        onCancelClicked();
        event->ignore();
    } else {
        event->accept();
    }
}

void LinkExtractorDialog::onCancelClicked()
{
    m_cancelled = true;
    if (m_currentReply) {
        m_currentReply->abort();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }
    appendLog(tr("Cancelled."));
    reject();
}

void LinkExtractorDialog::processNext()
{
    if (m_cancelled) {
    return;
}

    if (m_currentIndex >= m_inputLines.size()) {
        appendLog(tr("Finished processing."));
        m_cancelBtn->setText(tr("Dismiss"));
        m_cancelBtn->disconnect();
        connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::accept);
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
    } else if (url.isValid() && (url.scheme() == "http" || url.scheme() == "https")) {
        ext = QFileInfo(url.path()).suffix().toLower();
    }

    // Always try to expand if given via this dialog explicitly, regardless of
    // extension. We'll treat it as HTML if not explicitly txt.
    bool isTxt = (ext == "txt");

    if (isLocalFile) {
        appendLog(tr("Reading local file: %1").arg(pathToCheck));
        QFile file(pathToCheck);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString content = QTextStream(&file).readAll();
            if (isTxt) {
                extractFromTxt(content);
            } else {
                extractFromHtml(content, QUrl::fromLocalFile(pathToCheck));
            }
            m_modified = true;
        } else {
            appendLog(tr("Failed to open file: %1").arg(pathToCheck));
        }
        QMetaObject::invokeMethod(this, "processNext", Qt::QueuedConnection);
        return;
    }

    if (url.isValid() && (url.scheme() == "http" || url.scheme() == "https")) {
        appendLog(tr("Downloading remote file: %1").arg(line));
        m_currentReply = m_networkManager->get(QNetworkRequest(url));
        connect(m_currentReply, &QNetworkReply::finished, this, &LinkExtractorDialog::onReplyFinished);
        return;
    }

    appendLog(tr("Invalid or unsupported link provided: %1").arg(line));
    QMetaObject::invokeMethod(this, "processNext", Qt::QueuedConnection);
}

void LinkExtractorDialog::onReplyFinished()
{
    if (m_cancelled) {
    return;
}

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
        if (ext == "txt") {
            extractFromTxt(content);
        } else {
            extractFromHtml(content, url);
        }
        m_modified = true;
        appendLog(tr("Successfully processed remote file."));
    } else {
        appendLog(tr("Failed to download %1: %2").arg(url.toString(), reply->errorString()));
    }

    if (reply) {
        reply->deleteLater();
        reply = nullptr;
    }
    QMetaObject::invokeMethod(this, "processNext", Qt::QueuedConnection);
}

void LinkExtractorDialog::extractFromHtml(const QString &content, const QUrl &baseUrl)
{
    QRegularExpression re("href=[\"']([^\"']+)[\"']", QRegularExpression::CaseInsensitiveOption);
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
            bool isMagnet = resolvedStr.startsWith("magnet:");
            bool isTorrent = resolvedStr.endsWith(".torrent");

            if ((isMagnet && m_extractMagnetsCb->isChecked()) || (isTorrent && m_extractTorrentsCb->isChecked())) {
                m_expandedLines.append(resolvedStr);
                addedCount++;
            }
        }
    }
    appendLog(tr("Extracted %1 links from HTML.").arg(addedCount));
}

void LinkExtractorDialog::extractFromTxt(const QString &content)
{
    QStringList lines = content.split('\n');
    int addedCount = 0;
    for (const QString &line : lines) {
        QString trimmed = line.trimmed();
        if (!trimmed.isEmpty()) {
            bool isMagnet = trimmed.startsWith("magnet:");
            bool isTorrent = trimmed.endsWith(".torrent");

            // For TXT, we might want to extract all non-empty lines if they are URLs,
            // but to match the checkbox logic, we should filter magnets and torrents.
            // If it's neither, we probably still want to add it as it was directly in
            // a text file assuming the user knows what they're doing. Wait, user said
            // "all of the links provided... assumed to be either magnet links, or
            // direct paths to torrents". Let's filter TXT files the same way we do
            // HTML, but if it doesn't end with .torrent and isn't a magnet, we check
            // if it's a URL or file path. Actually, let's just apply the same
            // checkbox filters to be safe and consistent. But what if it's a generic
            // http link that the server resolves to a torrent? Let's just append it
            // if the respective checkboxes are checked or if it doesn't look like a
            // magnet/torrent explicitly. Actually, if they checked the boxes, they
            // explicitly want to filter. Let's filter based on checkboxes if it's a
            // magnet or ends with .torrent. If it's a generic link, keep it if
            // torrents are enabled.

            bool shouldAdd = false;
            if (isMagnet) {
                shouldAdd = m_extractMagnetsCb->isChecked();
            } else if (isTorrent) {
                shouldAdd = m_extractTorrentsCb->isChecked();
            } else {
                // If it's neither explicitly, we assume it's a link to a torrent/file
                shouldAdd = m_extractTorrentsCb->isChecked();
            }

            if (shouldAdd) {
                m_expandedLines.append(trimmed);
                addedCount++;
            }
        }
    }
    appendLog(tr("Extracted %1 links from TXT.").arg(addedCount));
}
