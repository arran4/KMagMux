#include "StorageManager.h"
#include <QStandardPaths>
#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDirIterator>
#include <QDateTime>

StorageManager::StorageManager(QObject *parent) : QObject(parent) {
    // Set base directory to ~/.local/share/KMagMux
    m_baseDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (m_baseDir.isEmpty()) {
        // Fallback for systems that might not return AppDataLocation correctly, though unlikely with Qt
        m_baseDir = QDir::homePath() + "/.local/share/KMagMux";
    }

    m_inboxDir = m_baseDir + "/inbox";
    m_queueDir = m_baseDir + "/queue";
    m_scheduledDir = m_baseDir + "/scheduled";
    m_holdDir = m_baseDir + "/hold";
    m_archiveDir = m_baseDir + "/archive";
    m_dataDir = m_baseDir + "/data";
    m_logsDir = m_baseDir + "/logs";

    m_watcher = new QFileSystemWatcher(this);
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, &StorageManager::onDirectoryChanged);
}

bool StorageManager::init() {
    bool success = true;
    success &= createDirIfNotExists(m_baseDir);
    success &= createDirIfNotExists(m_inboxDir);
    success &= createDirIfNotExists(m_queueDir);
    success &= createDirIfNotExists(m_scheduledDir);
    success &= createDirIfNotExists(m_holdDir);
    success &= createDirIfNotExists(m_archiveDir);
    success &= createDirIfNotExists(m_dataDir);
    success &= createDirIfNotExists(m_logsDir);

    if (success) {
        qDebug() << "Storage initialized at:" << m_baseDir;
        // Start watching inbox
        if (m_watcher->addPath(m_inboxDir)) {
             qDebug() << "Watching inbox:" << m_inboxDir;
             // Initial scan to populate known files and process existing ones
             m_knownFiles = scanInbox();
             for (const QString &file : m_knownFiles) {
                 // In a real scenario, we might want to process existing files on startup
                 // For now, we'll just log them to acknowledge existence.
                 // processNewFile(m_inboxDir + "/" + file);
                 qDebug() << "Found existing file in inbox:" << file;
             }
        } else {
             qWarning() << "Failed to watch inbox:" << m_inboxDir;
        }
    } else {
        qWarning() << "Failed to initialize storage at:" << m_baseDir;
    }
    return success;
}

bool StorageManager::createDirIfNotExists(const QString &path) {
    QDir dir(path);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            qWarning() << "Failed to create directory:" << path;
            return false;
        }
        qDebug() << "Created directory:" << path;
    }
    return true;
}

QString StorageManager::getBaseDir() const { return m_baseDir; }
QString StorageManager::getInboxDir() const { return m_inboxDir; }
QString StorageManager::getQueueDir() const { return m_queueDir; }
QString StorageManager::getDataDir() const { return m_dataDir; }

QString StorageManager::getItemPath(const QString &id) const {
    // Basic sanitization
    QString safeId = id;
    safeId.replace("/", "_").replace("\\", "_");
    return m_dataDir + "/" + safeId + ".json";
}

bool StorageManager::saveItem(const Item &item) {
    if (item.id.isEmpty()) {
        qWarning() << "Cannot save item with empty ID";
        return false;
    }

    QString path = getItemPath(item.id);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to open file for writing:" << path;
        return false;
    }

    QJsonDocument doc(item.toJson());
    if (file.write(doc.toJson()) == -1) {
        qWarning() << "Failed to write to file:" << path;
        return false;
    }

    // Notify updates
    emit itemUpdated(item);

    return true;
}

std::optional<Item> StorageManager::loadItem(const QString &id) {
    QString path = getItemPath(id);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open file for reading:" << path;
        return std::nullopt;
    }

    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) {
        qWarning() << "Failed to parse JSON from file:" << path;
        return std::nullopt;
    }

    return Item::fromJson(doc.object());
}

std::vector<Item> StorageManager::loadAllItems() {
    std::vector<Item> items;
    QDirIterator it(m_dataDir, QStringList() << "*.json", QDir::Files);
    while (it.hasNext()) {
        QString path = it.next();
        QFile file(path);
        if (file.open(QIODevice::ReadOnly)) {
            QByteArray data = file.readAll();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (!doc.isNull() && doc.isObject()) {
                items.push_back(Item::fromJson(doc.object()));
            }
        }
    }
    return items;
}

QStringList StorageManager::scanInbox() const {
    QDir dir(m_inboxDir);
    return dir.entryList(QDir::Files | QDir::NoDotAndDotDot);
}

void StorageManager::onDirectoryChanged(const QString &path) {
    if (path == m_inboxDir) {
        QStringList currentFiles = scanInbox();

        // Find new files
        for (const QString &file : currentFiles) {
            if (!m_knownFiles.contains(file)) {
                processNewFile(m_inboxDir + "/" + file);
            }
        }

        m_knownFiles = currentFiles;
    }
}

void StorageManager::processNewFile(const QString &filePath) {
    QFileInfo info(filePath);
    if (!info.exists() || !info.isFile()) return;

    qDebug() << "Processing new file in inbox:" << filePath;

    Item newItem;
    newItem.id = QString::number(QDateTime::currentMSecsSinceEpoch()) + "_" + info.fileName();
    newItem.state = ItemState::Unprocessed;
    newItem.sourcePath = filePath; // In a real app, we might copy this to a managed location immediately
    newItem.createdTime = QDateTime::currentDateTime();

    if (saveItem(newItem)) {
        emit itemAdded(newItem);
        qDebug() << "New item added from inbox watcher:" << newItem.id;
    }
}
