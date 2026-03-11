#include "core/StorageManager.h"
#include "ui/MainWindow.h"
#include <QApplication>
#include <QTimer>

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);
  app.setApplicationName("KMagMuxTest");

  StorageManager storage;
  storage.init();

  MainWindow* window = new MainWindow(&storage);
  window->show();

  QTimer::singleShot(1000, [window]() {
      window->close(); // trigger close event
  });
  return app.exec();
}
