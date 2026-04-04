#ifndef SECURESTORAGE_H
#define SECURESTORAGE_H

#include <QString>

class SecureStorage
{
public:
    static QString readPassword(const QString &service, const QString &key);
    static void writePassword(const QString &service, const QString &key, const QString &password);
};

#endif // SECURESTORAGE_H
