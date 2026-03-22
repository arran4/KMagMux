import sys

with open('src/ui/TorrentInfoDialog.h', 'r') as f:
    content = f.read()

# I need to pass the Item into TorrentInfoDialog.
# TorrentInfoDialog(const QString &sourcePath, QWidget *parent = nullptr);
# Wait! Instead of changing the signature, I can just pass an optional Item!
# Wait, no, `Item` is defined in `../core/Item.h`.
# TorrentInfoDialog already includes `<QString>`.

if '#include "../core/Item.h"' not in content:
    content = content.replace('#include <QDialog>', '#include <QDialog>\n#include "../core/Item.h"')

    # Add a new constructor or change the existing one
    content = content.replace('explicit TorrentInfoDialog(const QString &sourcePath, QWidget *parent = nullptr);', 'explicit TorrentInfoDialog(const QString &sourcePath, const Item *item = nullptr, QWidget *parent = nullptr);')

    with open('src/ui/TorrentInfoDialog.h', 'w') as f:
        f.write(content)

with open('src/ui/TorrentInfoDialog.cpp', 'r') as f:
    content = f.read()

if 'const Item *item' not in content:
    content = content.replace('TorrentInfoDialog::TorrentInfoDialog(const QString &sourcePath, QWidget *parent)', 'TorrentInfoDialog::TorrentInfoDialog(const QString &sourcePath, const Item *item, QWidget *parent)')

    # Now inside setupUi, we can add a tab widget. The original TorrentInfoDialog doesn't have a tab widget. It just has a mainLayout.
    # Oh! My previous script `patch_info_history.py` tried to add a tab to `tabWidget`, which didn't exist!
    # Let me check the actual code.

    setup_target = 'mainLayout->addWidget(m_trackerTable);'

    setup_replace = '''mainLayout->addWidget(m_trackerTable);

  if (item && item->metadata.contains("history")) {
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

    QJsonArray history = item->metadata["history"].toArray();
    historyTable->setRowCount(history.size());
    for (int i = 0; i < history.size(); ++i) {
      QJsonObject entry = history[i].toObject();
      QDateTime dt = QDateTime::fromString(entry["timestamp"].toString(), Qt::ISODate);
      QString timeStr = dt.isValid() ? dt.toString(Qt::DefaultLocaleShortDate) : entry["timestamp"].toString();
      historyTable->setItem(i, 0, new QTableWidgetItem(timeStr));
      historyTable->setItem(i, 1, new QTableWidgetItem(entry["message"].toString()));
    }
    historyTable->resizeColumnsToContents();
    mainLayout->addWidget(historyTable);
  }
'''
    if 'historyTable' not in content:
        content = content.replace(setup_target, setup_replace)

    with open('src/ui/TorrentInfoDialog.cpp', 'w') as f:
        f.write(content)

with open('src/ui/MainWindow.cpp', 'r') as f:
    content = f.read()

# Update MainWindow's usage of TorrentInfoDialog
if 'TorrentInfoDialog dialog(sourcePath, &item, this);' not in content:
    content = content.replace('TorrentInfoDialog dialog(sourcePath, this);', 'TorrentInfoDialog dialog(sourcePath, &item, this);')
    with open('src/ui/MainWindow.cpp', 'w') as f:
        f.write(content)

with open('src/ui/ProcessItemDialog.cpp', 'r') as f:
    content = f.read()
if 'TorrentInfoDialog dialog(sourcePath, &m_items[row], this);' not in content:
    content = content.replace('TorrentInfoDialog dialog(sourcePath, this);', 'TorrentInfoDialog dialog(sourcePath, &m_items[row], this);')
    with open('src/ui/ProcessItemDialog.cpp', 'w') as f:
        f.write(content)
