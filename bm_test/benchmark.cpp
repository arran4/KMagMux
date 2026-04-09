#include <QCoreApplication>
#include <QList>
#include <QSet>
#include <QElapsedTimer>
#include <QDebug>
#include <vector>
#include <algorithm>

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    // Simulate table items being selected, e.g., 5000 rows, 4 columns
    // Each row has 4 items selected.
    QList<int> itemRows;
    for (int i = 0; i < 5000; ++i) {
        for (int j = 0; j < 4; ++j) {
            itemRows.append(i);
        }
    }

    // Test QList::contains
    QElapsedTimer timer1;
    timer1.start();
    QList<int> rows1;
    for (int row : itemRows) {
        if (!rows1.contains(row)) {
            rows1.append(row);
        }
    }
    std::sort(rows1.begin(), rows1.end(), std::greater<int>());
    qint64 time1 = timer1.nsecsElapsed();

    // Test QSet::insert
    QElapsedTimer timer2;
    timer2.start();
    QSet<int> rowsSet;
    for (int row : itemRows) {
        rowsSet.insert(row);
    }
    QList<int> rows2(rowsSet.begin(), rowsSet.end());
    std::sort(rows2.begin(), rows2.end(), std::greater<int>());
    qint64 time2 = timer2.nsecsElapsed();

    qDebug() << "QList::contains time (ms):" << time1 / 1000000.0;
    qDebug() << "QSet::insert time (ms):" << time2 / 1000000.0;
    qDebug() << "Improvement:" << (double)time1 / time2 << "x";

    return 0;
}
