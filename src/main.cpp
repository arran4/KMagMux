#include "core/StorageManager.h"
#include "core/ipc/ApplicationRequestDispatcher.h"
#include "core/ipc/SingleInstanceServer.h"
#include "core/ipc/StartupCoordinator.h"
#include "ui/MainWindow.h"
#include <KAboutData>
#include <KLocalizedString>
#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QMessageBox>
#include <QPointer>
#include <QUrl>

namespace {

QString setupApplication(QApplication &app) {
  QApplication::setApplicationName("KMagMux");
  QApplication::setOrganizationName("KMagMux");
  QApplication::setWindowIcon(QIcon(":/icons/kmagmux.svg"));

  QString appName = "kmagmux";
#ifdef QT_DEBUG
  appName = "kmagmux-dev1";
#endif

  app.setApplicationName(
      appName); // This will affect the config directory as well
  return appName + "_SingleInstance";
}

} // namespace

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);

  KLocalizedString::setApplicationDomain("kmagmux");
  KAboutData aboutData("kmagmux", i18n("KMagMux"), "0.1");
  KAboutData::setApplicationData(aboutData);

  QCommandLineParser parser;
  aboutData.setupCommandLine(&parser);
  parser.addOption(QCommandLineOption(QStringList() << "show" << "open", i18n("Restore and activate the main window (start if not running).")));
  parser.addOption(QCommandLineOption(QStringList() << "hide" << "close", i18n("Hide the main window without quitting (no-op if not running).")));
  parser.addOption(QCommandLineOption(QStringList() << "toggle", i18n("Toggle main window visibility (start if not running).")));
  parser.addOption(QCommandLineOption("hidden-primary", i18n("Internal flag to start primary process hidden.")));
  parser.addPositionalArgument("inputs", i18n("Files or URLs to add"), "[inputs...]");
  parser.process(app);
  aboutData.processCommandLine(&parser);

  const QString serverName = setupApplication(app);

  // Reconstruct the parsed arguments and positional inputs for the coordinator
  // This avoids passing the raw `QApplication::arguments()` which might contain KDE flags.
  QStringList parsedArgs;
  parsedArgs << QApplication::arguments().first(); // Program name
  if (parser.isSet("show") || parser.isSet("open")) {
    parsedArgs << "--show";
  }
  if (parser.isSet("hide") || parser.isSet("close")) {
    parsedArgs << "--hide";
  }
  if (parser.isSet("toggle")) {
    parsedArgs << "--toggle";
  }
  if (parser.isSet("hidden-primary")) {
    parsedArgs << "--hidden-primary";
  }
  parsedArgs.append(parser.positionalArguments());

  StartupCoordinator coordinator(serverName);
  CoordinatorResult coordResult = coordinator.coordinate(parsedArgs);

  switch (coordResult.action) {
  case CoordinatorAction::BecomePrimary:
    break; // Continue initialization
  case CoordinatorAction::RequestDelivered:
    return 0;
  case CoordinatorAction::UserCancelled:
    return 130;
  case CoordinatorAction::RequestFailed:
  case CoordinatorAction::SpawnFailed:
  default:
    return 1;
  }

  // Initialize Core Storage
  StorageManager storage;
  if (!storage.init()) {
    QMessageBox::critical(nullptr, "Error",
                          "Failed to initialize storage directories.");
    return 1;
  }

  MainWindow *window = new MainWindow(&storage);
  window->setObjectName("KMagMuxMainWindow");

  ApplicationRequestDispatcher dispatcher;
  QObject::connect(&dispatcher,
                   &ApplicationRequestDispatcher::activateWindowRequested,
                   window, &MainWindow::showMainWindow);
  QObject::connect(&dispatcher,
                   &ApplicationRequestDispatcher::showWindowRequested,
                   window, &MainWindow::showMainWindow);
  QObject::connect(&dispatcher,
                   &ApplicationRequestDispatcher::hideWindowRequested,
                   window, &MainWindow::hideMainWindow);
  QObject::connect(&dispatcher,
                   &ApplicationRequestDispatcher::toggleWindowRequested,
                   window, &MainWindow::toggleMainWindow);
  QObject::connect(&dispatcher,
                   &ApplicationRequestDispatcher::processAddedLinesRequested,
                   window, &MainWindow::processAddedLines);
  QObject::connect(window, &MainWindow::processingCompleted, &dispatcher,
                   &ApplicationRequestDispatcher::completeCurrentProcessing);

  SingleInstanceServer server(serverName, &dispatcher);

  if (!server.tryAcquire(std::move(coordResult.primaryLock))) {
    qWarning() << "Failed to acquire lock as primary after coordination.";
    return 1;
  }

  // Only show the window if it wasn't requested to start hidden.
  bool startHidden = parsedArgs.contains("--hidden-primary");
  if (!startHidden) {
    window->show();
  }

  return QApplication::exec();
}
