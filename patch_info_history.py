import sys

with open('src/ui/TorrentInfoDialog.cpp', 'r') as f:
    content = f.read()

# Add a history tab to TorrentInfoDialog
target_tabs = '''
  tabWidget->addTab(trackersTab, tr("Trackers"));
  tabWidget->addTab(peersTab, tr("Peers"));
'''
replacement_tabs = '''
  tabWidget->addTab(trackersTab, tr("Trackers"));
  tabWidget->addTab(peersTab, tr("Peers"));

  QWidget *historyTab = new QWidget(this);
  QVBoxLayout *historyLayout = new QVBoxLayout(historyTab);
  QTableWidget *historyTable = new QTableWidget(this);
  historyTable->setColumnCount(2);
  historyTable->setHorizontalHeaderLabels({tr("Time"), tr("Event")});
  historyTable->horizontalHeader()->setStretchLastSection(true);
  historyTable->verticalHeader()->setVisible(false);
  historyTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
  historyTable->setSelectionBehavior(QAbstractItemView::SelectRows);
  historyTable->setShowGrid(false);

  QJsonArray history = item.metadata["history"].toArray();
  historyTable->setRowCount(history.size());
  for (int i = 0; i < history.size(); ++i) {
    QJsonObject entry = history[i].toObject();
    QDateTime dt = QDateTime::fromString(entry["timestamp"].toString(), Qt::ISODate);
    QString timeStr = dt.isValid() ? dt.toString(Qt::DefaultLocaleShortDate) : entry["timestamp"].toString();
    historyTable->setItem(i, 0, new QTableWidgetItem(timeStr));
    historyTable->setItem(i, 1, new QTableWidgetItem(entry["message"].toString()));
  }
  historyTable->resizeColumnsToContents();
  historyLayout->addWidget(historyTable);
  tabWidget->addTab(historyTab, tr("History"));
'''

if 'tabWidget->addTab(historyTab, tr("History"));' not in content:
    content = content.replace(target_tabs, replacement_tabs)
    with open('src/ui/TorrentInfoDialog.cpp', 'w') as f:
        f.write(content)
