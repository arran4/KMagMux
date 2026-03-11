#include "SecureStorage.h"
#include <QDebug>
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
    QSettings mainSettings;
    if (mainSettings.value("allowPlaintextStorage", false).toBool()) {
      QSettings settings;
      settings.beginGroup(service);
      result = settings.value(key, "").toString();
      settings.endGroup();
    } else {
      qWarning() << "SecureStorage: Failed to read password for service" << service
                 << "key" << key << ":" << qPrintable(job.errorString());
    }
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
    QSettings mainSettings;
    if (mainSettings.value("allowPlaintextStorage", false).toBool()) {
      QSettings settings;
      settings.beginGroup(service);
      settings.setValue(key, password);
      settings.endGroup();
    } else {
      qWarning() << "SecureStorage: Failed to write password for service" << service
                 << "key" << key << ":" << qPrintable(job.errorString());
    }
  }
}
