#include "core/ItemModel.h"
#include "ui/ItemFilterProxyModel.h"
#include "ui/StatusCountsWidget.h"
#include "ui/UnseenTabTracker.h"
#include <QLabel>
#include <QSignalSpy>
#include <QTest>

class TestStateIndicators : public QObject {
  Q_OBJECT
private:
  static Item item(const QString &id) {
    Item result;
    result.id = id;
    return result;
  }

private slots:
  void totals() {
    ItemModel inbox, queue, errors, done;
    StatusCountsWidget counts(&inbox, &queue, &errors, &done);
    auto text = [&counts](const QString &name) {
      return counts.findChild<QLabel *>(name)->text();
    };
    QCOMPARE(counts.findChildren<QLabel *>().size(), 4);
    QCOMPARE(text("Inbox"), "Inbox 0");
    QCOMPARE(text("Queue"), "Queue 0");
    QCOMPARE(text("Errors"), "Errors 0");
    QCOMPARE(text("Done"), "Done 0");
    inbox.addItem(item("one"));
    QCOMPARE(text("Inbox"), "Inbox 1");
    queue.setItems({item("queued"), item("scheduled"), item("held")});
    QCOMPARE(text("Queue"), "Queue 3");
    ItemFilterProxyModel filtered;
    filtered.setSourceModel(&queue);
    filtered.setFilterText("does not match");
    QCOMPARE(filtered.rowCount(), 0);
    QCOMPARE(text("Queue"), "Queue 3");
    inbox.setItems({});
    errors.addItem(item("error"));
    done.setItems({item("done"), item("done2")});
    QCOMPARE(text("Inbox"), "Inbox 0");
    QCOMPARE(text("Errors"), "Errors 1");
    QCOMPARE(text("Done"), "Done 2");
    done.setItems({});
    QCOMPARE(text("Done"), "Done 0");
    QVERIFY(!counts.findChild<QLabel *>("Queue")->toolTip().isEmpty());
    QVERIFY(
        counts.findChild<QLabel *>("Queue")->accessibleName().contains("3"));
  }

  void unseenSnapshots() {
    UnseenTabWidget tabs;
    ItemModel models[5];
    const QStringList names = {"Inbox", "Queue", "Done", "Archive", "Errors"};
    for (int i = 0; i < 5; ++i) {
      tabs.addTab(new QWidget(&tabs), names[i]);
      models[i].setItems({item("existing")});
    }
    UnseenTabTracker tracker(&tabs);
    auto *bar = dynamic_cast<UnseenTabBar *>(tabs.tabBar());
    QVERIFY(bar);
    for (int i = 0; i < 5; ++i) {
      tracker.track(i, &models[i]);
      QCOMPARE(tracker.unseenCount(i), 0);
      QCOMPARE(tabs.tabText(i), names[i]);
    }
    models[0].addItem(item("seen while active"));
    QCOMPARE(tracker.unseenCount(0), 0);
    tabs.setCurrentIndex(1);
    models[0].setItems({item("one"), item("two")});
    QCOMPARE(tracker.unseenCount(0), 2);
    QCOMPARE(tabs.tabText(0), "Inbox (2 new)");
    QVERIFY(bar->isEmphasized(0));
    // Reordering, repeated metadata changes and duplicate IDs are not arrivals.
    Item changed = item("one");
    changed.metadata["name"] = "changed";
    models[0].setItems({item("two"), changed, changed});
    QCOMPARE(tracker.unseenCount(0), 2);
    models[0].setItems({changed});
    QCOMPARE(tracker.unseenCount(0), 1);
    tabs.setCurrentIndex(0);
    QCOMPARE(tracker.unseenCount(0), 0);
    QCOMPARE(tabs.tabText(0), "Inbox");
    QVERIFY(!bar->isEmphasized(0));
    tabs.setCurrentIndex(1);
    tabs.setCurrentIndex(0);
    QCOMPARE(tracker.unseenCount(0), 0);
    for (int i = 1; i < 5; ++i) {
      models[i].setItems({item("existing"), item("incoming")});
      QCOMPARE(tracker.unseenCount(i), 1);
      models[i].setItems({item("incoming"), item("existing")});
      QCOMPARE(tracker.unseenCount(i), 1);
      QVERIFY(bar->isEmphasized(i));
      tabs.setCurrentIndex(i);
      QCOMPARE(tracker.unseenCount(i), 0);
      QVERIFY(!bar->isEmphasized(i));
      QCOMPARE(tabs.tabText(i), names[i]);
      tabs.setCurrentIndex(0);
    }
    ItemFilterProxyModel proxy;
    proxy.setSourceModel(&models[0]);
    proxy.setFilterText("nothing");
    QCOMPARE(tracker.unseenCount(0), 0);
    QCOMPARE(proxy.rowCount(), 0);
  }
};

QTEST_MAIN(TestStateIndicators)
#include "TestStateIndicators.moc"
