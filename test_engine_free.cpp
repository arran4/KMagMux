#include "core/StorageManager.h"
#include "core/Engine.h"
#include <QApplication>

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);
  StorageManager storage;
  storage.init();

  Engine* engine = new Engine(&storage);
  delete engine;

  return 0;
}
