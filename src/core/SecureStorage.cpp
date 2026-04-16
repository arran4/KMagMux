#include "SecureStorage.h"
#include <QDebug>
#include <QEventLoop>
#include <QSettings>
#ifdef HAVE_QTKEYCHAIN
#include <qt6keychain/keychain.h>
#endif

QString SecureStorage::readPassword(const QString &service,
                                    const QString &key) {
  QString result;
#ifdef HAVE_QTKEYCHAIN
  QKeychain::ReadPasswordJob job(service);
  job.setKey(key);
  job.setAutoDelete(false);

  QEventLoop loop;
  QObject::connect(&job, &QKeychain::Job::finished, &loop, &QEventLoop::quit);
  job.start();
  loop.exec();

  if (job.error() == QKeychain::NoError) {
    result = job.textData();
  } else {
    const QSettings mainSettings;
    if (mainSettings.value("allowPlaintextStorage", false).toBool()) {
      QSettings settings;
      settings.beginGroup(service);
      result = settings.value(key, "").toString();
      settings.endGroup();
    } else {
      qWarning() << "SecureStorage: Failed to read password for service"
                 << service << "key" << key << ":"
                 << qPrintable(job.errorString());
    }
  }
#else
  const QSettings mainSettings;
  if (mainSettings.value("allowPlaintextStorage", false).toBool()) {
    QSettings settings;
    settings.beginGroup(service);
    result = settings.value(key, "").toString();
    settings.endGroup();
  } else {
    qWarning() << "SecureStorage: Failed to read password for service"
               << service << "key" << key << ":"
               << "QtKeychain not available";
  }
#endif

  return result;
}

void SecureStorage::writePassword(const QString &service, const QString &key,
                                  const QString &password) {
#ifdef HAVE_QTKEYCHAIN
  QKeychain::WritePasswordJob job(service);
  job.setKey(key);
  job.setTextData(password);
  job.setAutoDelete(false);

  QEventLoop loop;
  QObject::connect(&job, &QKeychain::Job::finished, &loop, &QEventLoop::quit);
  job.start();
  loop.exec();

  if (job.error() != QKeychain::NoError) {
    const QSettings mainSettings;
    if (mainSettings.value("allowPlaintextStorage", false).toBool()) {
      QSettings settings;
      settings.beginGroup(service);
      settings.setValue(key, password);
      settings.endGroup();
    } else {
      qWarning() << "SecureStorage: Failed to write password for service"
                 << service << "key" << key << ":"
                 << qPrintable(job.errorString());
    }
  }
#else
  const QSettings mainSettings;
  if (mainSettings.value("allowPlaintextStorage", false).toBool()) {
    QSettings settings;
    settings.beginGroup(service);
    settings.setValue(key, password);
    settings.endGroup();
  } else {
    qWarning() << "SecureStorage: Failed to write password for service"
               << service << "key" << key << ":"
               << "QtKeychain not available";
  }
#endif
}
