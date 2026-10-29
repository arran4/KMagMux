#include "core/ipc/ApplicationRequestDispatcher.h"
#include "core/ipc/SingleInstanceServer.h"
#include "core/ipc/StartupCoordinator.h"
#include "ui/ActivityLogWindow.h"
#include "ui/AddItemDialog.h"
#include "ui/LinkExtractorDialog.h"
#include "ui/MainWindow.h"
#include "ui/ProcessItemDialog.h"
#include "ui/StatusCountsWidget.h"
#include "ui/UnseenTabTracker.h"
#include <KLocalizedString>
#include <QApplication>
#include <QClipboard>
#include <QDateTimeEdit>
#include <QFile>
#include <QFileDialog>
#include <QFutureWatcher>
#include <QJsonArray>
#include <QLineEdit>
#include <QListWidget>
#include <QLocalSocket>
#include <QMessageBox>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QThreadPool>
#include <QTimer>
#include <QUuid>
#include <functional>
#include <memory>

// Observe real modal dialogs without adding a production test API. Queue the
// response so it runs inside exec(); the watchdog prevents assertion failures
// inside a handler from hanging the entire test process.
class DialogObserver : public QObject {
  Q_OBJECT
public:
  std::function<void(QDialog *)> respond;
  DialogObserver() { qApp->installEventFilter(this); }
  ~DialogObserver() override { qApp->removeEventFilter(this); }

signals:
  void shown(QDialog *dialog);

protected:
  bool eventFilter(QObject *object, QEvent *event) override {
    auto *dialog = qobject_cast<QDialog *>(object);
    if (dialog && event->type() == QEvent::Show) {
      emit shown(dialog);
      QTimer::singleShot(5000, dialog, &QDialog::reject);
      QTimer::singleShot(0, dialog, [this, dialog]() {
        if (respond) {
          respond(dialog);
        } else {
          QTest::qFail("Unexpected modal dialog", __FILE__, __LINE__);
          dialog->reject();
        }
      });
    }
    return QObject::eventFilter(object, event);
  }
};

class TestConnector : public Connector {
public:
  explicit TestConnector(QString id = "test-connector") : id(std::move(id)) {}
  QString getId() const override { return id; }
  QString getName() const override { return "Test Connector"; }
  void dispatch(const Item &) override { ++dispatches; }
  int dispatches = 0;
  QString id;
};

// Feed real command-line coordination through its delivery boundary without
// starting another GUI process or contacting an external service.
class CliDelivery : public StartupSystemInterface {
public:
  ApplicationRequestDispatcher *dispatcher = nullptr;
  QStringList delivered;
  bool spawnDetached(const QString &, const QStringList &) override {
    return false;
  }
  ClientResult sendClientRequest(const QString &, const IpcProtocol::Request &r,
                                 int, int) override {
    delivered = r.payload;
    dispatcher->dispatch(r);
    return {ClientResultCode::Accepted, {}};
  }
  void msleep(int) override {}
  void showErrorMessage(const QString &) override {}
  bool showRecoveryPrompt(const QString &) override { return false; }
};

class TestMainWindow : public QObject {
  Q_OBJECT
  std::unique_ptr<StorageManager> storage;
  std::unique_ptr<MainWindow> window;
  std::unique_ptr<QTemporaryDir> files;
  std::unique_ptr<QMimeData> clipboard;
  TestConnector connector;

  QString file(const QString &name,
               const QByteArray &contents = "test payload") {
    QString path = files->filePath(name);
    QFile output(path);
    if (!output.open(QIODevice::WriteOnly) || output.write(contents) < 0) {
      return {};
    }
    return path;
  }
  static void click(QDialog *dialog, const QString &text) {
    for (auto *button : dialog->findChildren<QPushButton *>()) {
      if (button->text() == text) {
        QTest::mouseClick(button, Qt::LeftButton);
        return;
      }
    }
    QFAIL(qPrintable("Button not found: " + text));
  }
  QTabWidget *tabs() const { return window->findChild<QTabWidget *>(); }
  void connectDispatcher(ApplicationRequestDispatcher &dispatcher) {
    connect(&dispatcher,
            &ApplicationRequestDispatcher::processAddedLinesRequested,
            window.get(), &MainWindow::processAddedLines);
    connect(window.get(), &MainWindow::processingCompleted, &dispatcher,
            &ApplicationRequestDispatcher::completeCurrentProcessing);
  }
  static void screenshot(QWidget *widget, const QString &name) {
    const QString dir = qEnvironmentVariable("KMAGMUX_SCREENSHOT_DIR");
    if (!dir.isEmpty()) {
      QDir().mkpath(dir);
      if (qobject_cast<MainWindow *>(widget)) {
        widget->resize(1000, 600);
        QApplication::processEvents();
      }
      widget->grab().save(dir + "/" + name + ".png");
    }
  }

private slots:
  void init() {
    QCoreApplication::setApplicationName(
        "KMagMuxUiTest-" + QString::fromLatin1(QTest::currentTestFunction()) +
        "-" + QUuid::createUuid().toString(QUuid::WithoutBraces));
    QSettings().clear();
    files = std::make_unique<QTemporaryDir>("/tmp/kmagmux-inputs-XXXXXX");
    QVERIFY(files->isValid());
    const QMimeData *original = QApplication::clipboard()->mimeData();
    clipboard = std::make_unique<QMimeData>();
    if (original) {
      for (const auto &format : original->formats()) {
        clipboard->setData(format, original->data(format));
      }
    }
    storage = std::make_unique<StorageManager>();
    QVERIFY(storage->init());
    window = std::make_unique<MainWindow>(storage.get());
    auto *engine = window->findChild<Engine *>();
    QVERIFY(engine);
    engine->setPaused(true);
    engine->setConnectorForTesting(&connector);
    window->show();
    window->activateWindow();
    QVERIFY(QTest::qWaitForWindowExposed(window.get()));
  }
  void cleanup() {
    QCOMPARE(connector.dispatches, 0);
    QThreadPool::globalInstance()->waitForDone();
    window.reset();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    storage.reset();
    files.reset();
    QApplication::clipboard()->setMimeData(clipboard.release());
  }

  void singleFileDirectAndDuplicate() {
    DialogObserver dialogs;
    QSignalSpy shown(&dialogs, &DialogObserver::shown);
    QSignalSpy completed(window.get(), &MainWindow::processingCompleted);
    const QString path = file("one file.torrent");
    window->processAddedLines({QUrl::fromLocalFile(path).toString()}, "single");
    QTRY_COMPARE(completed.count(), 1);
    QCOMPARE(shown.count(), 0);
    QCOMPARE(completed.first().first().toString(), "single");
    auto items = storage->loadAllItems();
    QCOMPARE(items.size(), 1U);
    QCOMPARE(items[0].state, ItemState::Unprocessed);
    QCOMPARE(items[0].sourcePath, path);
    QVERIFY(QFile::exists(path));
    QVERIFY(!items[0].metadata.value("delete_source_file").toBool());
    // Existing policy permits explicit repeated additions as separate records.
    window->processAddedLines({path});
    QTRY_COMPARE(completed.count(), 2);
    items = storage->loadAllItems();
    QCOMPARE(items.size(), 2U);
    QVERIFY(items[0].id != items[1].id);
  }

  void parsedSingleWithRejectedInput() {
    DialogObserver dialogs;
    QSignalSpy completed(window.get(), &MainWindow::processingCompleted);
    int warnings = 0;
    dialogs.respond = [&](QDialog *dialog) {
      auto *warning = qobject_cast<QMessageBox *>(dialog);
      QVERIFY(warning);
      QVERIFY(warning->text().contains("invalid input"));
      ++warnings;
      warning->accept();
    };
    window->processAddedLines({file("one.torrent"), "invalid input"});
    QTRY_COMPARE(completed.count(), 1);
    QCOMPARE(warnings, 1);
    QCOMPARE(storage->loadAllItems().size(), 1U);
  }

  void pasteTableFocus() {
    DialogObserver dialogs;
    QSignalSpy shown(&dialogs, &DialogObserver::shown);
    QSignalSpy completed(window.get(), &MainWindow::processingCompleted);
    const QString first = file("first #%.torrent");
    const QString second = file("second.torrent");
    dialogs.respond = [&](QDialog *dialog) {
      auto *review = qobject_cast<AddItemDialog *>(dialog);
      QVERIFY(
          review); // A manual QPlainTextEdit entry dialog must never appear.
      QVERIFY(!dialog->findChild<QPlainTextEdit *>());
      QCOMPARE(review->getItems().size(), 2U);
      QCOMPARE(review->getItems()[0].sourcePath, first);
      QCOMPARE(review->getItems()[1].sourcePath, second);
      screenshot(review, "batch-review");
      review->reject();
    };
    QApplication::clipboard()->setText(first + "\n\n  \n" + second + "\n");
    auto *view = tabs()->currentWidget()->findChild<QTableView *>();
    view->setFocus();
    QTRY_VERIFY(view->hasFocus());
    QTest::keySequence(view, QKeySequence::Paste);
    QTRY_COMPARE(completed.count(), 1);
    QCOMPARE(shown.count(), 1);
    QVERIFY(storage->loadAllItems().empty());
  }

  void pasteSearchFocus() {
    DialogObserver dialogs;
    QSignalSpy shown(&dialogs, &DialogObserver::shown);
    QSignalSpy completed(window.get(), &MainWindow::processingCompleted);
    auto *search = tabs()->currentWidget()->findChild<QLineEdit *>();
    search->setFocus();
    QTRY_VERIFY(search->hasFocus());
    QApplication::clipboard()->setText("ordinary search text");
    QTest::keySequence(search, QKeySequence::Paste);
    QCOMPARE(search->text(), "ordinary search text");
    // Parsing starts by creating a watcher synchronously. Check it as well
    // as completion, so a slow accidental parse cannot evade this assertion.
    QVERIFY(window->findChildren<QFutureWatcherBase *>().isEmpty());
    QCoreApplication::processEvents();
    QVERIFY(window->findChildren<QFutureWatcherBase *>().isEmpty());
    QCOMPARE(shown.count(), 0);
    QCOMPARE(completed.count(), 0);
    QVERIFY(storage->loadAllItems().empty());
  }

  void filePicker_data() {
    QTest::addColumn<int>("number");
    QTest::newRow("one-file") << 1;
    QTest::newRow("two-files") << 2;
  }
  void filePicker() {
    QFETCH(int, number);
    DialogObserver dialogs;
    QSignalSpy completed(window.get(), &MainWindow::processingCompleted);
    const QString first = file("first #%.torrent");
    const QString second = file("second.torrent");
    int reviews = 0;
    dialogs.respond = [&](QDialog *dialog) {
      if (auto *picker = qobject_cast<QFileDialog *>(dialog)) {
        auto *name = picker->findChild<QLineEdit *>("fileNameEdit");
        QVERIFY(name);
        name->setText(number == 1 ? first
                                  : "\"" + first + "\" \"" + second + "\"");
        QVERIFY(QMetaObject::invokeMethod(picker, "accept"));
      } else if (auto *review = qobject_cast<AddItemDialog *>(dialog)) {
        ++reviews;
        QCOMPARE(review->getItems().size(), 2U);
        review->reject();
      } else {
        QCOMPARE(dialog->windowTitle(), "Add Items");
        click(dialog, "Browse Files...");
        QVERIFY(
            !dialog->findChild<QPlainTextEdit *>()->toPlainText().isEmpty());
        dialog->accept();
      }
    };
    QVERIFY(QMetaObject::invokeMethod(window.get(), "onAddItems"));
    QTRY_COMPARE(completed.count(), 1);
    QCOMPARE(reviews, number == 1 ? 0 : 1);
    QCOMPARE(storage->loadAllItems().size(), number == 1 ? 1U : 0U);
    QVERIFY(QFile::exists(first));
    QVERIFY(QFile::exists(second));
  }

  void batchSelectedInbox() {
    DialogObserver dialogs;
    QSignalSpy completed(window.get(), &MainWindow::processingCompleted);
    const QString first = file("first #%.torrent");
    const QString second = file("second.torrent");
    dialogs.respond = [&](QDialog *dialog) {
      QVERIFY(qobject_cast<AddItemDialog *>(dialog));
      auto *table = dialog->findChild<QTableWidget *>();
      QCOMPARE(table->rowCount(), 2);
      QCOMPARE(table->item(0, 1)->checkState(), Qt::Unchecked);
      table->item(1, 0)->setCheckState(Qt::Unchecked);
      click(dialog, "Add to Inbox");
    };
    window->processAddedLines({first, second});
    QTRY_COMPARE(completed.count(), 1);
    auto items = storage->loadAllItems();
    QCOMPARE(items.size(), 1U);
    QCOMPARE(items[0].sourcePath, first);
    QVERIFY(QFile::exists(first));
    QVERIFY(QFile::exists(second));
  }

  void batchProcess_data() {
    QTest::addColumn<int>("state");
    QTest::addColumn<bool>("cancel");
    QTest::addColumn<bool>("keepSource");
    QTest::newRow("queue") << 0 << false << false;
    QTest::newRow("hold") << 1 << false << false;
    QTest::newRow("archive") << 2 << false << false;
    QTest::newRow("queue-keep-source") << 0 << false << true;
    QTest::newRow("cancel-processing") << 0 << true << false;
  }
  void batchProcess() {
    QFETCH(int, state);
    QFETCH(bool, cancel);
    QFETCH(bool, keepSource);
    DialogObserver dialogs;
    QSignalSpy completed(window.get(), &MainWindow::processingCompleted);
    const QString first = file("first #%.torrent");
    const QString second = file("second.torrent");
    int reviewCount = 0, processCount = 0;
    const QDateTime hold = QDateTime::currentDateTime().addDays(1);
    dialogs.respond = [&](QDialog *dialog) {
      if (qobject_cast<AddItemDialog *>(dialog)) {
        ++reviewCount;
        auto *table = dialog->findChild<QTableWidget *>();
        table->item(1, 0)->setCheckState(Qt::Unchecked);
        table->item(0, 1)->setCheckState(Qt::Checked);
        click(dialog, "Process...");
      } else {
        auto *process = qobject_cast<ProcessItemDialog *>(dialog);
        QVERIFY(process);
        ++processCount;
        QVERIFY(storage->loadAllItems().empty());
        QCOMPARE(process->getItems().size(), 1U);
        auto *table = process->findChild<QTableWidget *>();
        QCOMPARE(table->item(0, 1)->checkState(), Qt::Checked);
        if (cancel) {
          process->reject();
          return;
        }
        if (keepSource)
          table->item(0, 1)->setCheckState(Qt::Unchecked);
        process->findChild<QComboBox *>()->setCurrentIndex(state);
        process->findChild<QDateTimeEdit *>()->setDateTime(hold);
        auto *connectors = process->findChild<QListWidget *>();
        for (int i = 0; i < connectors->count(); ++i) {
          connectors->item(i)->setCheckState(
              connectors->item(i)->text() == connector.getId() ? Qt::Checked
                                                               : Qt::Unchecked);
        }
        click(process, "Process");
      }
    };
    window->processAddedLines({first, second}, "batch-process");
    QTRY_COMPARE(completed.count(), 1);
    QCOMPARE(reviewCount, 1);
    QCOMPARE(processCount, 1);
    auto items = storage->loadAllItems();
    if (cancel) {
      QVERIFY(items.empty());
      QVERIFY(QFile::exists(first));
      QVERIFY(QDir(storage->getManagedDir()).isEmpty());
    } else {
      QCOMPARE(items.size(), 1U);
      QCOMPARE(items[0].state, state == 0   ? ItemState::Queued
                               : state == 1 ? ItemState::Held
                                            : ItemState::Archived);
      QCOMPARE(items[0].connectorId, connector.getId());
      QCOMPARE(QFile::exists(first), keepSource);
      QVERIFY(QFile::exists(items[0].sourcePath));
      QVERIFY(!items[0].metadata.contains("delete_source_file"));
      QCOMPARE(items[0].scheduledTime.isValid(), state == 1);
      if (state == 1) {
        QCOMPARE(items[0].scheduledTime.toSecsSinceEpoch(),
                 hold.toSecsSinceEpoch());
      }
    }
    QVERIFY(QFile::exists(second));
  }

  void multipleConnectorsKeepMetadataAndOwnFiles() {
    TestConnector another("other-test-connector");
    window->findChild<Engine *>()->setConnectorForTesting(&another);
    DialogObserver dialogs;
    QSignalSpy completed(window.get(), &MainWindow::processingCompleted);
    const QString first = file("shared.torrent");
    const QString second = file("unused.torrent");
    QString originalId;
    dialogs.respond = [&](QDialog *dialog) {
      if (auto *review = qobject_cast<AddItemDialog *>(dialog)) {
        originalId = review->getItems()[0].id;
        auto *table = review->findChild<QTableWidget *>();
        table->item(1, 0)->setCheckState(Qt::Unchecked);
        table->item(0, 1)->setCheckState(Qt::Checked);
        click(review, "Process...");
      } else {
        auto *process = qobject_cast<ProcessItemDialog *>(dialog);
        QVERIFY(process);
        auto *list = process->findChild<QListWidget *>();
        for (int i = 0; i < list->count(); ++i) {
          const QString id = list->item(i)->text();
          list->item(i)->setCheckState(
              id == connector.getId() || id == another.getId() ? Qt::Checked
                                                               : Qt::Unchecked);
        }
        process->findChild<QTableWidget *>()->item(0, 2)->setCheckState(
            Qt::Checked);
        click(process, "Process");
      }
    };
    window->processAddedLines({first, second});
    QTRY_COMPARE(completed.count(), 1);
    auto items = storage->loadAllItems();
    QCOMPARE(items.size(), 2U);
    QVERIFY(items[0].id != items[1].id);
    QVERIFY(items[0].id == originalId || items[1].id == originalId);
    QVERIFY(items[0].sourcePath != items[1].sourcePath);
    QVERIFY(QFile::exists(items[0].sourcePath));
    QVERIFY(QFile::exists(items[1].sourcePath));
    QVERIFY(!QFile::exists(first));
    for (const Item &item : items) {
      QVERIFY(item.metadata.value("delete_once_submitted").toBool());
      QCOMPARE(item.state, ItemState::Queued);
      QVERIFY(!item.metadata.value("history").toArray().isEmpty());
    }
    // Deleting a connector's record must not remove another's payload.
    QThreadPool::globalInstance()->waitForDone();
    QVERIFY(storage->deleteItem(items[0].id));
    QVERIFY(QFile::exists(items[1].sourcePath));
    QVERIFY(QFile::exists(second));
    QCOMPARE(another.dispatches, 0);
  }

  void processingRequiresConnector() {
    DialogObserver dialogs;
    QSignalSpy completed(window.get(), &MainWindow::processingCompleted);
    int warnings = 0;
    dialogs.respond = [&](QDialog *dialog) {
      if (qobject_cast<AddItemDialog *>(dialog)) {
        click(dialog, "Process...");
      } else if (auto *process = qobject_cast<ProcessItemDialog *>(dialog)) {
        auto *list = process->findChild<QListWidget *>();
        for (int i = 0; i < list->count(); ++i)
          list->item(i)->setCheckState(Qt::Unchecked);
        click(process, "Process");
        QVERIFY(process->isVisible());
        process->reject();
      } else {
        auto *warning = qobject_cast<QMessageBox *>(dialog);
        QVERIFY(warning);
        QCOMPARE(warning->windowTitle(), "No Connector Selected");
        ++warnings;
        warning->accept();
      }
    };
    window->processAddedLines({file("one.torrent"), file("two.torrent")});
    QTRY_COMPARE(completed.count(), 1);
    QCOMPARE(warnings, 1);
    QVERIFY(storage->loadAllItems().empty());
  }

  void extractedResultsKeepReview_data() {
    QTest::addColumn<int>("results");
    QTest::newRow("single-extracted-result") << 1;
    QTest::newRow("multiple-extracted-results") << 2;
  }
  void extractedResultsKeepReview() {
    QFETCH(int, results);
    DialogObserver dialogs;
    QSignalSpy completed(window.get(), &MainWindow::processingCompleted);
    QByteArray html("<a href=\"magnet:?xt=urn:btih:one\">one</a>");
    if (results == 2)
      html += "<a href=\"magnet:?xt=urn:btih:two\">two</a>";
    const QString page = file("page.html", html);
    int reviews = 0;
    dialogs.respond = [&](QDialog *dialog) {
      if (auto *review = qobject_cast<AddItemDialog *>(dialog)) {
        ++reviews;
        QCOMPARE(review->getItems().size(), static_cast<size_t>(results));
        review->reject();
      } else if (auto *extractor =
                     qobject_cast<LinkExtractorDialog *>(dialog)) {
        auto *log = extractor->findChild<QPlainTextEdit *>();
        connect(log, &QPlainTextEdit::textChanged, extractor,
                [log, extractor]() {
                  if (log->toPlainText().contains("Finished processing.")) {
                    QTimer::singleShot(0, extractor, [extractor]() {
                      click(extractor, "Dismiss");
                    });
                  }
                });
      } else if (dialog->windowTitle() == "Open List/HTML") {
        dialog->findChild<QPlainTextEdit *>()->setPlainText(page);
        dialog->accept();
      } else {
        QCOMPARE(dialog->windowTitle(), "Add Items");
        click(dialog, "Open List/HTML");
        QVERIFY(
            !dialog->findChild<QPlainTextEdit *>()->toPlainText().isEmpty());
        dialog->accept();
      }
    };
    QVERIFY(QMetaObject::invokeMethod(window.get(), "onAddItems"));
    QTRY_COMPARE(completed.count(), 1);
    QCOMPARE(reviews, 1);
    QVERIFY(storage->loadAllItems().empty());
  }

  void startupBaseline() {
    window.reset();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    for (ItemState state :
         {ItemState::Unprocessed, ItemState::Held, ItemState::Done,
          ItemState::Archived, ItemState::Failed}) {
      Item item;
      item.id = QString::number(static_cast<int>(state));
      item.state = state;
      QVERIFY(storage->saveItem(item));
    }
    window = std::make_unique<MainWindow>(storage.get());
    window->findChild<Engine *>()->setPaused(true);
    auto *tracker = window->findChild<UnseenTabTracker *>();
    for (int i = 0; i < 5; ++i)
      QCOMPARE(tracker->unseenCount(i), 0);
    auto *counts = window->findChild<StatusCountsWidget *>();
    for (const QString &name : {"Inbox", "Queue", "Errors", "Done"}) {
      QCOMPARE(counts->findChild<QLabel *>(name)->text(), name + " 1");
    }
  }

  void webpageAndInvalidInputs() {
    DialogObserver dialogs;
    QSignalSpy completed(window.get(), &MainWindow::processingCompleted);
    int reviews = 0, errors = 0;
    dialogs.respond = [&](QDialog *dialog) {
      if (auto *review = qobject_cast<AddItemDialog *>(dialog)) {
        ++reviews;
        QCOMPARE(review->getItems().size(), 1U);
        review->reject();
      } else {
        auto *warning = qobject_cast<QMessageBox *>(dialog);
        QVERIFY(warning);
        QVERIFY(warning->text().contains("invalid input"));
        ++errors;
        warning->accept();
      }
    };
    window->processAddedLines({"https://example.invalid/page"});
    QTRY_COMPARE(completed.count(), 1);
    QCOMPARE(reviews, 1);
    window->processAddedLines({"invalid input"});
    QTRY_COMPARE(completed.count(), 2);
    QCOMPARE(errors, 1);
    QVERIFY(storage->loadAllItems().empty());
  }

  void dropInputs_data() {
    QTest::addColumn<int>("kind");
    QTest::newRow("single-file") << 0;
    QTest::newRow("multiple-files") << 1;
    QTest::newRow("pasted-text") << 2;
    QTest::newRow("list-results") << 3;
  }
  void dropInputs() {
    QFETCH(int, kind);
    DialogObserver dialogs;
    QSignalSpy completed(window.get(), &MainWindow::processingCompleted);
    QSignalSpy shown(&dialogs, &DialogObserver::shown);
    const QString first = file("first #%.torrent");
    const QString second = file("second.torrent");
    dialogs.respond = [&](QDialog *dialog) {
      auto *review = qobject_cast<AddItemDialog *>(dialog);
      QVERIFY(review);
      QCOMPARE(review->getItems().size(), 2U);
      review->reject();
    };
    QMimeData mime;
    if (kind == 0)
      mime.setUrls({QUrl::fromLocalFile(first)});
    if (kind == 1)
      mime.setUrls({QUrl::fromLocalFile(first), QUrl::fromLocalFile(second)});
    if (kind == 2)
      mime.setText(first + "\n" + second);
    if (kind == 3)
      mime.setUrls({QUrl::fromLocalFile(
          file("list.txt", (first + "\n" + second).toUtf8()))});
    QDragEnterEvent enter(QPoint(20, 20), Qt::CopyAction, &mime, Qt::LeftButton,
                          Qt::NoModifier);
    QApplication::sendEvent(window.get(), &enter);
    QVERIFY(enter.isAccepted());
    QDropEvent drop(QPointF(20, 20), Qt::CopyAction, &mime, Qt::LeftButton,
                    Qt::NoModifier);
    QApplication::sendEvent(window.get(), &drop);
    QVERIFY(drop.isAccepted());
    QTRY_COMPARE(completed.count(), 1);
    QCOMPARE(shown.count(), kind == 0 ? 0 : 1);
    QCOMPARE(storage->loadAllItems().size(), kind == 0 ? 1U : 0U);
  }

  void cliAndSocketBatches() {
    ApplicationRequestDispatcher dispatcher;
    connectDispatcher(dispatcher);
    QSignalSpy completed(window.get(), &MainWindow::processingCompleted);
    QSignalSpy requested(
        &dispatcher, &ApplicationRequestDispatcher::processAddedLinesRequested);
    const QString serverName =
        "KMagMuxUi-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    SingleInstanceServer server(serverName, &dispatcher);
    QVERIFY(server.tryAcquire());
    DialogObserver dialogs;
    int reviews = 0;
    dialogs.respond = [&](QDialog *dialog) {
      auto *review = qobject_cast<AddItemDialog *>(dialog);
      QVERIFY(review);
      QCOMPARE(review->getItems().size(), 2U);
      ++reviews;
      QVERIFY(completed.count() < reviews); // Completion follows confirmation.
      review->reject();
    };
    const QStringList inputs = {file("first.torrent"), file("second.torrent")};
    CliDelivery delivery;
    delivery.dispatcher = &dispatcher;
    StartupCoordinator coordinator(serverName, &delivery);
    QStringList args = {"kmagmux"};
    args.append(inputs);
    QCOMPARE(coordinator.coordinate(args).action,
             CoordinatorAction::RequestDelivered);
    QCOMPARE(delivery.delivered, inputs);
    QTRY_COMPARE(completed.count(), 1);
    // Real socket framing and dispatcher integration, two queued batches.
    QLocalSocket socket;
    socket.connectToServer(serverName);
    QVERIFY(socket.waitForConnected());
    auto send = [&]() {
      IpcProtocol::Request request;
      request.requestId = QUuid::createUuid().toString();
      request.type = IpcProtocol::RequestType::AddInputs;
      request.payload = inputs;
      QByteArray payload;
      QDataStream payloadStream(&payload, QIODevice::WriteOnly);
      IpcProtocol::setupStream(payloadStream);
      payloadStream << request;
      QByteArray block;
      QDataStream stream(&block, QIODevice::WriteOnly);
      IpcProtocol::setupStream(stream);
      stream << static_cast<quint32>(payload.size());
      block.append(payload);
      socket.write(block);
      socket.flush();
    };
    send();
    QTRY_COMPARE(completed.count(), 2);
    // The server closes each connection after its response.
    socket.abort();
    socket.connectToServer(serverName);
    QVERIFY(socket.waitForConnected());
    send();
    QTRY_COMPARE(completed.count(), 3);
    QCOMPARE(requested.count(), 3);
    QCOMPARE(reviews, 3);
    QVERIFY(storage->loadAllItems().empty());
  }

  void ipcQueueCompletionAcrossDialogs() {
    ApplicationRequestDispatcher dispatcher;
    connectDispatcher(dispatcher);
    DialogObserver dialogs;
    QSignalSpy requested(
        &dispatcher, &ApplicationRequestDispatcher::processAddedLinesRequested);
    QSignalSpy completed(window.get(), &MainWindow::processingCompleted);
    const QStringList inputs = {file("one.torrent"), file("two.torrent")};
    int reviews = 0;
    dialogs.respond = [&](QDialog *dialog) {
      if (auto *review = qobject_cast<AddItemDialog *>(dialog)) {
        ++reviews;
        QCOMPARE(requested.count(), reviews);
        QCOMPARE(completed.count(), reviews - 1);
        if (reviews == 1)
          click(review, "Add to Inbox");
        else
          click(review, "Process...");
      } else {
        QVERIFY(qobject_cast<ProcessItemDialog *>(dialog));
        QCOMPARE(completed.count(), 1);
        dialog->reject();
      }
    };
    for (int i = 0; i < 2; ++i) {
      IpcProtocol::Request request;
      request.requestId = QUuid::createUuid().toString();
      request.type = IpcProtocol::RequestType::AddInputs;
      request.payload = inputs;
      QCOMPARE(dispatcher.dispatch(request),
               IpcProtocol::ResponseStatus::Accepted);
    }
    QCOMPARE(requested.count(), 1);
    dispatcher.completeCurrentProcessing("unrelated-local-operation");
    QCOMPARE(requested.count(), 1);
    QTRY_COMPARE(completed.count(), 2);
    QCOMPARE(requested.count(), 2);
    QVERIFY(completed[0][0] != completed[1][0]);
    QCOMPARE(reviews, 2);
    QCOMPARE(storage->loadAllItems().size(), 2U);
  }

  void storageTransitionsAndStatus() {
    auto *counts = window->findChild<StatusCountsWidget *>();
    auto *tracker = window->findChild<UnseenTabTracker *>();
    QVERIFY(counts);
    QVERIFY(tracker);
    auto count = [counts](const QString &name) {
      return counts->findChild<QLabel *>(name)->text();
    };
    QCOMPARE(count("Inbox"), "Inbox 0");
    tabs()->setCurrentIndex(2);
    Item item;
    item.id = "transition";
    item.sourcePath = "magnet:?xt=urn:btih:test";
    item.state = ItemState::Unprocessed;
    QVERIFY(storage->saveItem(item));
    QCOMPARE(count("Inbox"), "Inbox 1");
    QCOMPARE(tracker->unseenCount(0), 1);
    auto *search = tabs()->widget(0)->findChild<QLineEdit *>();
    search->setText("no matches");
    QCOMPARE(count("Inbox"), "Inbox 1");
    QCOMPARE(tracker->unseenCount(0), 1);
    screenshot(window.get(), "state-indicators");
    const QList<ItemState> states = {ItemState::Queued,    ItemState::Held,
                                     ItemState::Scheduled, ItemState::Failed,
                                     ItemState::Done,      ItemState::Archived};
    const QList<int> indices = {1, 1, 1, 4, 2, 3};
    tabs()->setCurrentIndex(0);
    for (int i = 0; i < states.size(); ++i) {
      item.state = states[i];
      QVERIFY(storage->saveItem(item));
      QCOMPARE(tracker->unseenCount(indices[i]), 1);
      QVERIFY(storage->saveItem(item)); // no-op reload must not inflate it
      QCOMPARE(tracker->unseenCount(indices[i]), 1);
      QCOMPARE(count("Inbox"), "Inbox 0");
      QCOMPARE(count("Queue"), i < 3 ? "Queue 1" : "Queue 0");
      QCOMPARE(count("Errors"), i == 3 ? "Errors 1" : "Errors 0");
      QCOMPARE(count("Done"), i == 4 ? "Done 1" : "Done 0");
    }
    QVERIFY(storage->deleteItem(item.id));
    QCOMPARE(tracker->unseenCount(3), 0);
    // Deletions must refresh each status category, not only Archive.
    const QList<ItemState> deletions = {ItemState::Unprocessed, ItemState::Held,
                                        ItemState::Failed, ItemState::Done};
    const QStringList categories = {"Inbox", "Queue", "Errors", "Done"};
    for (int i = 0; i < deletions.size(); ++i) {
      item.state = deletions[i];
      QVERIFY(storage->saveItem(item));
      QCOMPARE(count(categories[i]), categories[i] + " 1");
      QVERIFY(storage->deleteItem(item.id));
      QCOMPARE(count(categories[i]), categories[i] + " 0");
    }
    auto *status = window->findChild<ClickableLabel *>();
    QVERIFY(status);
    window->findChild<Engine *>()->actionMessage("A transient status message");
    QCOMPARE(status->text(), "A transient status message");
    QCOMPARE(count("Inbox"), "Inbox 0");
    QTest::mouseClick(status, Qt::LeftButton);
    QVERIFY(ActivityLogWindow::instance()->isVisible());
    ActivityLogWindow::instance()->hide();
  }
};

int main(int argc, char **argv) {
  QTemporaryDir isolated("/tmp/kmagmux-ui-tests-XXXXXX");
  if (!isolated.isValid())
    return 1;
  qputenv("XDG_DATA_HOME", isolated.filePath("data").toUtf8());
  qputenv("XDG_CONFIG_HOME", isolated.filePath("config").toUtf8());
  qputenv("XDG_CACHE_HOME", isolated.filePath("cache").toUtf8());
  qputenv("XDG_STATE_HOME", isolated.filePath("state").toUtf8());
  QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
  QApplication app(argc, argv);
  KLocalizedString::setApplicationDomain("kmagmux");
  QApplication::setWindowIcon(QIcon(":/icons/kmagmux.svg"));
  QCoreApplication::setOrganizationName("KMagMuxTests");
  QSettings::setDefaultFormat(QSettings::IniFormat);
  TestMainWindow test;
  return QTest::qExec(&test, argc, argv);
}
#include "TestMainWindow.moc"
