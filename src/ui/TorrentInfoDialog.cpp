#include "TorrentInfoDialog.h"
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>

TorrentInfoDialog::TorrentInfoDialog(const QString &sourcePath, QWidget *parent)
    : QDialog(parent), m_sourcePath(sourcePath), m_isQuerying(false),
      m_currentTrackerIndex(0) {

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
  if (m_trackerClient) {
    m_trackerClient->cancel();
  }
}

void TorrentInfoDialog::setupUi() {
  setWindowTitle("Torrent Information");
  resize(700, 500);

  QVBoxLayout *mainLayout = new QVBoxLayout(this);

  QFormLayout *infoLayout = new QFormLayout();
  infoLayout->addRow(new QLabel("<b>Name:</b>"), new QLabel(m_info.name));
  infoLayout->addRow(new QLabel("<b>Info Hash:</b>"), new QLabel(m_info.infoHash.toHex()));
  infoLayout->addRow(new QLabel("<b>Source:</b>"), new QLabel(m_sourcePath));

  mainLayout->addLayout(infoLayout);

  m_trackerTable = new QTableWidget(m_info.trackers.size(), 5, this);
  m_trackerTable->setHorizontalHeaderLabels({"Tracker", "Status", "Seeders", "Leechers", "Downloaded"});
  m_trackerTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
  m_trackerTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
  m_trackerTable->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_trackerTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

  for (int i = 0; i < m_info.trackers.size(); ++i) {
    m_trackerTable->setItem(i, 0, new QTableWidgetItem(m_info.trackers[i]));
    m_trackerTable->setItem(i, 1, new QTableWidgetItem("Idle"));
    m_trackerTable->setItem(i, 2, new QTableWidgetItem("-"));
    m_trackerTable->setItem(i, 3, new QTableWidgetItem("-"));
    m_trackerTable->setItem(i, 4, new QTableWidgetItem("-"));
  }

  mainLayout->addWidget(m_trackerTable);

  QHBoxLayout *btnLayout = new QHBoxLayout();
  m_queryBtn = new QPushButton("Query Trackers", this);
  m_cancelBtn = new QPushButton("Cancel Query", this);
  m_closeBtn = new QPushButton("Close", this);

  m_cancelBtn->setEnabled(false);

  connect(m_queryBtn, &QPushButton::clicked, this, &TorrentInfoDialog::onQueryTrackers);
  connect(m_cancelBtn, &QPushButton::clicked, this, &TorrentInfoDialog::onCancelQuery);
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
  if (m_info.trackers.isEmpty()) return;

  m_isQuerying = true;
  m_queryBtn->setEnabled(false);
  m_cancelBtn->setEnabled(true);

  // Reset statuses
  for (int i = 0; i < m_trackerTable->rowCount(); ++i) {
    m_trackerTable->item(i, 1)->setText("Waiting...");
    m_trackerTable->item(i, 2)->setText("-");
    m_trackerTable->item(i, 3)->setText("-");
    m_trackerTable->item(i, 4)->setText("-");
  }

  m_currentTrackerIndex = 0;
  processNextTracker();
}

void TorrentInfoDialog::onCancelQuery() {
  m_isQuerying = false;
  m_trackerClient->cancel();

  if (m_currentTrackerIndex < m_trackerTable->rowCount()) {
      m_trackerTable->item(m_currentTrackerIndex, 1)->setText("Cancelled");
  }

  // Mark remaining as cancelled
  for (int i = m_currentTrackerIndex + 1; i < m_trackerTable->rowCount(); ++i) {
    m_trackerTable->item(i, 1)->setText("Cancelled");
  }

  m_queryBtn->setEnabled(true);
  m_cancelBtn->setEnabled(false);
}

void TorrentInfoDialog::processNextTracker() {
  if (!m_isQuerying) return;

  if (m_currentTrackerIndex >= m_info.trackers.size()) {
    m_isQuerying = false;
    m_queryBtn->setEnabled(true);
    m_cancelBtn->setEnabled(false);
    return;
  }

  QString trackerUrl = m_info.trackers[m_currentTrackerIndex];
  m_trackerTable->item(m_currentTrackerIndex, 1)->setText("Querying...");

  m_trackerClient->scrape(trackerUrl, m_info.infoHash);
}

void TorrentInfoDialog::onScrapeFinished(const TrackerStats &stats) {
  if (!m_isQuerying) return;

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
  } else {
    m_trackerTable->item(row, 1)->setText("Error: " + stats.errorString);
    m_trackerTable->item(row, 1)->setToolTip(stats.errorString);
    m_trackerTable->item(row, 2)->setText("-");
    m_trackerTable->item(row, 3)->setText("-");
    m_trackerTable->item(row, 4)->setText("-");
  }
}
