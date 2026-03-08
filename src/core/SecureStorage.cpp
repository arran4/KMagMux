#include "SecureStorage.h"
#include <QEventLoop>
#include <QSettings>
#include <qt6keychain/keychain.h>

QString SecureStorage::readPassword(const QString &service,
                                    const QString &key) {
  QKeychain::ReadPasswordJob job(service);
  job.setKey(key);
  job.setAutoDelete(false);

  QEventLoop loop;
  QObject::connect(&job, &QKeychain::Job::finished, &loop, &QEventLoop::quit);
  job.start();
  loop.exec();

  QString result;
  if (job.error() == QKeychain::NoError) {
    result = job.textData();
  } else {
    // Fallback to QSettings if not available (e.g. testing or unsupported)
    QSettings settings;
    settings.beginGroup(service);
    result = settings.value(key, "").toString();
    settings.endGroup();
  }

  return result;
}

void SecureStorage::writePassword(const QString &service, const QString &key,
                                  const QString &password) {
  QKeychain::WritePasswordJob job(service);
  job.setKey(key);
  job.setTextData(password);
  job.setAutoDelete(false);

  QEventLoop loop;
  QObject::connect(&job, &QKeychain::Job::finished, &loop, &QEventLoop::quit);
  job.start();
  loop.exec();

  if (job.error() != QKeychain::NoError) {
    // Fallback to QSettings if writing failed
    QSettings settings;
    settings.beginGroup(service);
    settings.setValue(key, password);
    settings.endGroup();
  }
}
