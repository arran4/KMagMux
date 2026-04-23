#include "TorrentInfoDialog.h"
#include <QDateTime>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QVBoxLayout>

TorrentInfoDialog::TorrentInfoDialog(const QString &sourcePath,
                                     const Item *item, QWidget *parent)
    : QDialog(parent), m_sourcePath(sourcePath), m_item(item),
      m_isQuerying(false), m_currentTrackerIndex(0) {

  m_trackerClient = new TrackerClient(this);
  connect(m_trackerClient, &TrackerClient::scrapeFinished, this,
          &TorrentInfoDialog::onScrapeFinished);

  m_info = TorrentParser::parse(sourcePath);

  setupUi();

  if (!m_info.valid) {
    QMessageBox::warning(this, "Parse Error", m_info.errorString);
  }
}

TorrentInfoDialog::~TorrentInfoDialog() {
  if (m_trackerClient != nullptr) {
    m_trackerClient->cancel();
  }
}

void TorrentInfoDialog::setupUi() {
  setWindowTitle("Torrent Information");
  resize(700, 500);

  QVBoxLayout *mainLayout = new QVBoxLayout(this);

  setupBasicInfoSection(mainLayout);
  setupFilesSection(mainLayout);
  setupTrackerSection(mainLayout);
  setupHistorySection(mainLayout);
  setupLogSection(mainLayout);
  setupButtonsSection(mainLayout);
}

void TorrentInfoDialog::setupBasicInfoSection(QVBoxLayout *mainLayout) {
  QFormLayout *infoLayout = new QFormLayout();

  auto createReadOnlyField = [](const QString &text) {
    QLineEdit *field = new QLineEdit(text);
    field->setReadOnly(true);
    field->setFrame(false);
    field->setCursorPosition(0);
    return field;
  };

  infoLayout->addRow(new QLabel("<b>Name:</b>"),
                     createReadOnlyField(m_info.name));
  infoLayout->addRow(new QLabel("<b>Info Hash:</b>"),
                     createReadOnlyField(m_info.infoHash.toHex()));
  infoLayout->addRow(new QLabel("<b>Source:</b>"),
                     createReadOnlyField(m_sourcePath));

  if (!m_info.comment.isEmpty()) {
    infoLayout->addRow(new QLabel("<b>Comment:</b>"),
                       createReadOnlyField(m_info.comment));
  }
  if (!m_info.createdBy.isEmpty()) {
    infoLayout->addRow(new QLabel("<b>Created By:</b>"),
                       createReadOnlyField(m_info.createdBy));
  }
  if (m_info.creationDate.isValid()) {
    infoLayout->addRow(new QLabel("<b>Creation Date:</b>"),
                       createReadOnlyField(m_info.creationDate.toString()));
  }

  if (m_info.totalSize > 0) {
    const double sizeMb =
        static_cast<double>(m_info.totalSize) / (1024.0 * 1024.0);
    infoLayout->addRow(
        new QLabel("<b>Total Size:</b>"),
        createReadOnlyField(QString::number(sizeMb, 'f', 2) + " MB"));
  }

  mainLayout->addLayout(infoLayout);
}

void TorrentInfoDialog::setupFilesSection(QVBoxLayout *mainLayout) {
  if (!m_info.files.isEmpty()) {
    QLabel *filesLabel = new QLabel("<b>Files:</b>");
    mainLayout->addWidget(filesLabel);

    QTableWidget *filesTable = new QTableWidget(m_info.files.size(), 2, this);
    filesTable->setHorizontalHeaderLabels({"Path", "Size (MB)"});
    filesTable->horizontalHeader()->setSectionResizeMode(0,
                                                         QHeaderView::Stretch);
    filesTable->horizontalHeader()->setSectionResizeMode(
        1, QHeaderView::ResizeToContents);
    filesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    filesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    filesTable->setMaximumHeight(150);

    for (int i = 0; i < m_info.files.size(); ++i) {
      filesTable->setItem(i, 0, new QTableWidgetItem(m_info.files[i].path));
      const double fileMb =
          static_cast<double>(m_info.files[i].length) / (1024.0 * 1024.0);
      QTableWidgetItem *sizeItem =
          new QTableWidgetItem(QString::number(fileMb, 'f', 2));
      sizeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
      filesTable->setItem(i, 1, sizeItem);
    }
    mainLayout->addWidget(filesTable);
  }
}

void TorrentInfoDialog::setupTrackerSection(QVBoxLayout *mainLayout) {
  m_trackerTable = new QTableWidget(m_info.trackers.size(), 5, this);
  m_trackerTable->setHorizontalHeaderLabels(
      {"Tracker", "Status", "Seeders", "Leechers", "Downloaded"});
  m_trackerTable->horizontalHeader()->setSectionResizeMode(
      0, QHeaderView::Stretch);
  m_trackerTable->horizontalHeader()->setSectionResizeMode(
      1, QHeaderView::ResizeToContents);
  m_trackerTable->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_trackerTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

  for (int i = 0; i < m_info.trackers.size(); ++i) {
    QTableWidgetItem *trackerItem = new QTableWidgetItem(m_info.trackers[i]);
    trackerItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled |
                          Qt::ItemIsSelectable);
    trackerItem->setCheckState(Qt::Checked);

    m_trackerTable->setItem(i, 0, trackerItem);
    m_trackerTable->setItem(i, 1, new QTableWidgetItem("Idle"));
    m_trackerTable->setItem(i, 2, new QTableWidgetItem("-"));
    m_trackerTable->setItem(i, 3, new QTableWidgetItem("-"));
    m_trackerTable->setItem(i, 4, new QTableWidgetItem("-"));
  }

  mainLayout->addWidget(m_trackerTable);
}

void TorrentInfoDialog::setupHistorySection(QVBoxLayout *mainLayout) {
  if (m_item != nullptr && m_item->metadata.contains("history")) {
    QLabel *historyLabel = new QLabel("<b>History:</b>", this);
    mainLayout->addWidget(historyLabel);

    QTableWidget *historyTable = new QTableWidget(this);
    historyTable->setColumnCount(2);
    historyTable->setHorizontalHeaderLabels({"Time", "Event"});
    historyTable->horizontalHeader()->setStretchLastSection(true);
    historyTable->verticalHeader()->setVisible(false);
    historyTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    historyTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    historyTable->setShowGrid(false);

    QJsonArray history = m_item->metadata["history"].toArray();
    historyTable->setRowCount(history.size());
    for (int i = 0; i < history.size(); ++i) {
      QJsonObject entry = history[i].toObject();
      const QDateTime dateTime =
          QDateTime::fromString(entry["timestamp"].toString(), Qt::ISODate);
      const QString timeStr = dateTime.isValid()
                                  ? dateTime.toString(Qt::TextDate)
                                  : entry["timestamp"].toString();
      historyTable->setItem(i, 0, new QTableWidgetItem(timeStr));
      historyTable->setItem(i, 1,
                            new QTableWidgetItem(entry["message"].toString()));
    }
    historyTable->resizeColumnsToContents();
    mainLayout->addWidget(historyTable);
  }
}

void TorrentInfoDialog::setupLogSection(QVBoxLayout *mainLayout) {
  m_logView = new QTextEdit(this);
  m_logView->setReadOnly(true);
  mainLayout->addWidget(m_logView);
}

void TorrentInfoDialog::setupButtonsSection(QVBoxLayout *mainLayout) {
  QHBoxLayout *btnLayout = new QHBoxLayout();
  m_queryBtn = new QPushButton("Query Trackers", this);
  m_cancelBtn = new QPushButton("Cancel Query", this);
  m_closeBtn = new QPushButton("Close", this);

  m_cancelBtn->setEnabled(false);

  connect(m_queryBtn, &QPushButton::clicked, this,
          &TorrentInfoDialog::onQueryTrackers);
  connect(m_cancelBtn, &QPushButton::clicked, this,
          &TorrentInfoDialog::onCancelQuery);
  connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::accept);

  if (m_info.trackers.isEmpty()) {
    m_queryBtn->setEnabled(false);
  }

  btnLayout->addStretch();
  btnLayout->addWidget(m_queryBtn);
  btnLayout->addWidget(m_cancelBtn);
  btnLayout->addWidget(m_closeBtn);

  mainLayout->addLayout(btnLayout);
}

void TorrentInfoDialog::onQueryTrackers() {
  if (m_info.trackers.isEmpty()) {
    return;
  }

  m_isQuerying = true;
  m_queryBtn->setEnabled(false);
  m_cancelBtn->setEnabled(true);

  // Reset statuses
  for (int i = 0; i < m_trackerTable->rowCount(); ++i) {
    if (m_trackerTable->item(i, 0)->checkState() == Qt::Checked) {
      m_trackerTable->item(i, 1)->setText("Waiting...");
    } else {
      m_trackerTable->item(i, 1)->setText("Skipped");
    }
    m_trackerTable->item(i, 2)->setText("-");
    m_trackerTable->item(i, 3)->setText("-");
    m_trackerTable->item(i, 4)->setText("-");
  }

  m_logView->clear();
  m_logView->append("Starting query on selected trackers...");

  m_currentTrackerIndex = 0;
  processNextTracker();
}

void TorrentInfoDialog::onCancelQuery() {
  m_isQuerying = false;
  m_trackerClient->cancel();

  if (m_currentTrackerIndex < m_trackerTable->rowCount()) {
    m_trackerTable->item(m_currentTrackerIndex, 1)->setText("Cancelled");
    m_logView->append(QString("Cancelled query for %1")
                          .arg(m_info.trackers[m_currentTrackerIndex]));
  }

  // Mark remaining as cancelled
  for (int i = m_currentTrackerIndex + 1; i < m_trackerTable->rowCount(); ++i) {
    if (m_trackerTable->item(i, 0)->checkState() == Qt::Checked) {
      m_trackerTable->item(i, 1)->setText("Cancelled");
    }
  }

  m_logView->append("Query cancelled by user.");

  m_queryBtn->setEnabled(true);
  m_cancelBtn->setEnabled(false);
}

void TorrentInfoDialog::processNextTracker() {
  if (!m_isQuerying) {
    return;
  }

  while (m_currentTrackerIndex < m_trackerTable->rowCount() &&
         m_trackerTable->item(m_currentTrackerIndex, 0)->checkState() !=
             Qt::Checked) {
    m_currentTrackerIndex++;
  }

  if (m_currentTrackerIndex >= m_info.trackers.size()) {
    m_isQuerying = false;
    m_queryBtn->setEnabled(true);
    m_cancelBtn->setEnabled(false);
    m_logView->append("Query finished.");
    return;
  }

  const QString trackerUrl = m_info.trackers[m_currentTrackerIndex];
  m_trackerTable->item(m_currentTrackerIndex, 1)->setText("Querying...");
  m_logView->append(QString("Querying %1 ...").arg(trackerUrl));

  m_trackerClient->scrape(trackerUrl, m_info.infoHash);
}

void TorrentInfoDialog::onScrapeFinished(const TrackerStats &stats) {
  if (!m_isQuerying) {
    return;
  }

  // Find the row for this tracker (should be m_currentTrackerIndex)
  if (m_currentTrackerIndex < m_trackerTable->rowCount()) {
    updateTrackerRow(m_currentTrackerIndex, stats);
  }

  m_currentTrackerIndex++;
  processNextTracker();
}

void TorrentInfoDialog::updateTrackerRow(int row, const TrackerStats &stats) {
  if (stats.success) {
    m_trackerTable->item(row, 1)->setText("Success");
    m_trackerTable->item(row, 2)->setText(QString::number(stats.seeders));
    m_trackerTable->item(row, 3)->setText(QString::number(stats.leechers));
    m_trackerTable->item(row, 4)->setText(QString::number(stats.downloaded));
    m_logView->append(QString("Success for %1: Seeders=%2, Leechers=%3")
                          .arg(stats.trackerUrl)
                          .arg(stats.seeders)
                          .arg(stats.leechers));
  } else {
    m_trackerTable->item(row, 1)->setText("Error: " + stats.errorString);
    m_trackerTable->item(row, 1)->setToolTip(stats.errorString);
    m_trackerTable->item(row, 2)->setText("-");
    m_trackerTable->item(row, 3)->setText("-");
    m_trackerTable->item(row, 4)->setText("-");
    m_logView->append(QString("Error for %1: %2")
                          .arg(stats.trackerUrl)
                          .arg(stats.errorString));
  }
}
