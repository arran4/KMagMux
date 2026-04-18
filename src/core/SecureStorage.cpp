#include "SecureStorage.h"
#include <QDebug>
#include <QSettings>
#include <kwallet.h>

QString SecureStorage::readPassword(const QString &service,
                                    const QString &key) {
  QString result;
  KWallet::Wallet *wallet = KWallet::Wallet::openWallet(
      KWallet::Wallet::LocalWallet(), 0, KWallet::Wallet::Synchronous);

  if (wallet && wallet->isOpen()) {
    if (!wallet->hasFolder("KMagMux")) {
      wallet->createFolder("KMagMux");
    }
    wallet->setFolder("KMagMux");
    if (wallet->readPassword(service + "_" + key, result) != 0) {
      qWarning() << "SecureStorage: Failed to read password for service"
                 << service << "key" << key << "from KWallet";
    }
    delete wallet;
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
                 << "KWallet not available";
    }
  }

  return result;
}

void SecureStorage::writePassword(const QString &service, const QString &key,
                                  const QString &password) {
  KWallet::Wallet *wallet = KWallet::Wallet::openWallet(
      KWallet::Wallet::LocalWallet(), 0, KWallet::Wallet::Synchronous);

  if (wallet && wallet->isOpen()) {
    if (!wallet->hasFolder("KMagMux")) {
      wallet->createFolder("KMagMux");
    }
    wallet->setFolder("KMagMux");
    if (wallet->writePassword(service + "_" + key, password) != 0) {
      qWarning() << "SecureStorage: Failed to write password for service"
                 << service << "key" << key << "to KWallet";
    }
    delete wallet;
  } else {
    const QSettings mainSettings;
    if (mainSettings.value("allowPlaintextStorage", false).toBool()) {
      QSettings settings;
      settings.beginGroup(service);
      settings.setValue(key, password);
      settings.endGroup();
    } else {
      qWarning() << "SecureStorage: Failed to write password for service"
                 << service << "key" << key << ":"
                 << "KWallet not available";
    }
  }
}
