#include "core/StorageManager.h"
#include "core/ipc/ApplicationRequestDispatcher.h"
#include "core/ipc/SingleInstanceServer.h"
#include "core/ipc/StartupCoordinator.h"
#include "ui/MainWindow.h"
#include <KAboutData>
#include <KLocalizedString>
#include <QApplication>
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
  const KAboutData aboutData("kmagmux", i18n("KMagMux"), "0.1");
  KAboutData::setApplicationData(aboutData);

  const QString serverName = setupApplication(app);
  const QStringList args = QApplication::arguments();

  StartupCoordinator coordinator(serverName);
  CoordinatorResult coordResult = coordinator.coordinate(args);

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
                   window, [window]() {
                     window->show();
                     window->raise();
                     window->activateWindow();
                   });
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

  window->show();

  return QApplication::exec();
}
