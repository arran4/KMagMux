#include <QCoreApplication>
#include <QStringList>
#include <QMap>
#include <iostream>

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    QMap<QString, void*> myMap;
    myMap.insert("a", nullptr);
    QStringList lst;
    for (auto it = myMap.constBegin(); it != myMap.constEnd(); ++it) {
        lst << it.key();
    }
    return 0;
}
