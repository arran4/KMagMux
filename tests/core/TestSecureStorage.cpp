#include <QTest>
#include <QCoreApplication>
#include <QSettings>
#include "core/SecureStorage.h"

class TestSecureStorage : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        QCoreApplication::setOrganizationName("KMagMuxTest");
        QCoreApplication::setApplicationName("TestSecureStorage");
        // Disable D-Bus session to ensure KWallet fails
        qputenv("DBUS_SESSION_BUS_ADDRESS", "");
    }

    void init() {
        QSettings settings;
        settings.clear();
    }

    void testPlaintextFallbackDisabled() {
        QString pass = SecureStorage::readPassword("testService", "testKey");
        QCOMPARE(pass, QString(""));
    }

    void testPlaintextFallbackEnabled() {
        QSettings mainSettings;
        mainSettings.setValue("allowPlaintextStorage", true);

        SecureStorage::writePassword("testService", "testKey", "mySecret");

        QString pass = SecureStorage::readPassword("testService", "testKey");
        QCOMPARE(pass, QString("mySecret"));
    }
};

QTEST_MAIN(TestSecureStorage)
#include "TestSecureStorage.moc"
