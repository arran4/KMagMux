#ifndef TORRENTINFODIALOG_H
#define TORRENTINFODIALOG_H

#include "../core/Item.h"
#include "../core/TorrentParser.h"
#include "../core/TrackerClient.h"
#include <QDateTime>
#include <QDialog>
#include <QPushButton>
#include <QString>
#include <QTableWidget>
#include <QTextEdit>

class TorrentInfoDialog : public QDialog {
  Q_OBJECT

public:
  explicit TorrentInfoDialog(const QString &sourcePath,
                             const Item *item = nullptr,
                             QWidget *parent = nullptr);
  ~TorrentInfoDialog();

private slots:
  void onQueryTrackers();
  void onCancelQuery();
  void onScrapeFinished(const TrackerStats &stats);

private:
  void setupUi();
  void updateTrackerRow(int row, const TrackerStats &stats);
  void processNextTracker();

  QString m_sourcePath;
  const Item *m_item;
  TorrentInfo m_info;
  QTableWidget *m_trackerTable;
  QTextEdit *m_logView;
  QPushButton *m_queryBtn;
  QPushButton *m_cancelBtn;
  QPushButton *m_closeBtn;

  TrackerClient *m_trackerClient;
  int m_currentTrackerIndex;
  bool m_isQuerying;
};

#endif // TORRENTINFODIALOG_H
