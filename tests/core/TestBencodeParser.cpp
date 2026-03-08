#include <QTest>
#include <QByteArray>
#include <QVariantMap>
#include <QCryptographicHash>
#include "../../src/core/BencodeParser.h"

class TestBencodeParser : public QObject {
  Q_OBJECT

private slots:
  void testParseInteger() {
    BencodeParser parser;
    QByteArray data = "d1:ai42ee";
    QVERIFY(parser.parse(data));
    QVariantMap map = parser.dictionary();
    QCOMPARE(map.value("a").toInt(), 42);
  }

  void testParseString() {
    BencodeParser parser;
    QByteArray data = "d1:a4:teste";
    QVERIFY(parser.parse(data));
    QVariantMap map = parser.dictionary();
    QCOMPARE(map.value("a").toString(), QString("test"));
  }

  void testParseList() {
    BencodeParser parser;
    QByteArray data = "d1:al4:testi42eee";
    QVERIFY(parser.parse(data));
    QVariantMap map = parser.dictionary();
    QVariantList list = map.value("a").toList();
    QCOMPARE(list.size(), 2);
    QCOMPARE(list[0].toString(), QString("test"));
    QCOMPARE(list[1].toInt(), 42);
  }

  void testInfoHash() {
    BencodeParser parser;
    QByteArray infoDict = "d4:name4:test12:piece lengthi262144e6:pieces20:12345678901234567890e";
    QByteArray data = "d4:info" + infoDict + "e";
    QVERIFY(parser.parse(data));

    QByteArray expectedHash = QCryptographicHash::hash(infoDict, QCryptographicHash::Sha1);
    QCOMPARE(parser.infoHash(), expectedHash);
  }

  void testInvalidData() {
    BencodeParser parser;
    QByteArray data = "d1:a4:tese"; // String length mismatch
    QVERIFY(!parser.parse(data));
    QVERIFY(!parser.errorString().isEmpty());

    data = "d1:ai42e"; // Missing end of dictionary
    QVERIFY(!parser.parse(data));

    data = "i42e"; // Root is not a dictionary
    QVERIFY(!parser.parse(data));
  }
};

QTEST_MAIN(TestBencodeParser)
#include "TestBencodeParser.moc"
