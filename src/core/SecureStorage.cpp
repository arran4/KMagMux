#include "SecureStorage.h"
#include <QDebug>
#include <QSettings>
#include <KWallet>

QString SecureStorage::readPassword(const QString &service,
                                    const QString &key) {
  QString result;
  KWallet::Wallet *wallet = KWallet::Wallet::openWallet(
      KWallet::Wallet::LocalWallet(), 0, KWallet::Wallet::Synchronous);

  bool readSuccess = false;
  if (wallet && wallet->isOpen()) {
    if (!wallet->hasFolder(service)) {
      wallet->createFolder(service);
    }
    wallet->setFolder(service);
    if (wallet->readPassword(key, result) == 0) {
      readSuccess = true;
    } else {
      qWarning() << "SecureStorage: Failed to read password for service"
                 << service << "key" << key << "from KWallet";
    }
    delete wallet;
  }

  if (!readSuccess) {
    const QSettings mainSettings;
    if (mainSettings.value("allowPlaintextStorage", false).toBool()) {
      QSettings settings;
      settings.beginGroup(service);
      result = settings.value(key, "").toString();
      settings.endGroup();
    } else {
      qWarning() << "SecureStorage: Failed to read password for service"
                 << service << "key" << key << ":"
                 << "KWallet not available or key not found";
    }
  }

  return result;
}

void SecureStorage::writePassword(const QString &service, const QString &key,
                                  const QString &password) {
  KWallet::Wallet *wallet = KWallet::Wallet::openWallet(
      KWallet::Wallet::LocalWallet(), 0, KWallet::Wallet::Synchronous);

  bool writeSuccess = false;
  if (wallet && wallet->isOpen()) {
    if (!wallet->hasFolder(service)) {
      wallet->createFolder(service);
    }
    wallet->setFolder(service);
    if (wallet->writePassword(key, password) == 0) {
      writeSuccess = true;
    } else {
      qWarning() << "SecureStorage: Failed to write password for service"
                 << service << "key" << key << "to KWallet";
    }
    delete wallet;
  }

  if (!writeSuccess) {
    const QSettings mainSettings;
    if (mainSettings.value("allowPlaintextStorage", false).toBool()) {
      QSettings settings;
      settings.beginGroup(service);
      settings.setValue(key, password);
      settings.endGroup();
    } else {
      qWarning() << "SecureStorage: Failed to write password for service"
                 << service << "key" << key << ":"
                 << "KWallet not available or write failed";
    }
  }
}
