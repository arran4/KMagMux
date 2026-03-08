#ifndef BENCODEPARSER_H
#define BENCODEPARSER_H

#include <QByteArray>
#include <QString>
#include <QVariant>
#include <QVariantMap>

class BencodeParser {
public:
  BencodeParser();
  bool parse(const QByteArray &data);

  QVariantMap dictionary() const;
  QByteArray infoHash() const;
  QString errorString() const;

private:
  QVariant parseElement(const QByteArray &data, int &pos);
  QVariant parseInteger(const QByteArray &data, int &pos);
  QByteArray parseByteString(const QByteArray &data, int &pos);
  QVariant parseList(const QByteArray &data, int &pos);
  QVariant parseDictionary(const QByteArray &data, int &pos);

  QVariantMap m_dictionary;
  QByteArray m_infoHash;
  QString m_errorString;
  const QByteArray *m_dataPtr;
};

#endif // BENCODEPARSER_H
