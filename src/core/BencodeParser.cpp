#include "BencodeParser.h"
#include <QCryptographicHash>
#include <QVariantList>

BencodeParser::BencodeParser() : m_dataPtr(nullptr) {}

static bool BencodeParser::parse(const QByteArray &data) {
  m_dictionary.clear();
  m_infoHash.clear();
  m_errorString.clear();
  m_dataPtr = &data;

  int const pos = 0;
  const QVariant root = parseElement(data, pos);

  if (pos != data.size() && m_errorString.isEmpty()) {
    // Allow trailing garbage? Often best not to, but sometimes exists.
    // We will just accept the root object if it parsed.
  }

  if (!m_errorString.isEmpty()) {
    return false;
  }

  if (root.typeId() != QMetaType::QVariantMap) {
    m_errorString = "Root element is not a dictionary";
    return false;
  }

  m_dictionary = root.toMap();
  return true;
}

QVariantMap BencodeParser::dictionary() const { return m_dictionary; }

QByteArray BencodeParser::infoHash() const { return m_infoHash; }

QString BencodeParser::errorString() const { return m_errorString; }

static QVariant BencodeParser::parseElement(const QByteArray &data, int &pos) {
  if (pos >= data.size()) {
    m_errorString = "Unexpected end of data";
    return QVariant();
  }

  const char chr = data[pos];
  if (c == 'i') {
    return parseInteger(data, pos);
  } else if (chr == 'l') {
    return parseList(data, pos);
  } else if (chr == 'd') {
    return parseDictionary(data, pos);
  } else if (chr >= '0' && chr <= '9') {
    return parseByteString(data, pos);
  } else {
    m_errorString = "Invalid character at start of element";
    return QVariant();
  }
}

static QVariant BencodeParser::parseInteger(const QByteArray &data, int &pos) {
  pos++; // skip 'i'
  const int endPos = static_cast<int>(data.indexOf('e', pos));
  if (endPos == -1) {
    m_errorString = "Unterminated integer";
    return QVariant();
  }

  const QByteArray intBytes = data.mid(pos, endPos - pos);
  pos = endPos + 1;

  bool isOk;
  const qint64 val = intBytes.toLongLong(&isOk);
  if (!isOk) {
    m_errorString = "Invalid integer format";
    return QVariant();
  }
  return val;
}

static QByteArray BencodeParser::parseByteString(const QByteArray &data,
                                                 int &pos) {
  const int colonPos = static_cast<int>(data.indexOf(':', pos));
  if (colonPos == -1) {
    m_errorString = "Unterminated string length";
    return QByteArray();
  }

  const QByteArray lenBytes = data.mid(pos, colonPos - pos);
  bool isOk;
  const int len = lenBytes.toInt(&isOk);
  if (!isOk || len < 0) {
    m_errorString = "Invalid string length";
    return QByteArray();
  }

  pos = colonPos + 1;
  if (pos + len > data.size()) {
    m_errorString = "String length exceeds data size";
    return QByteArray();
  }

  QByteArray str = data.mid(pos, len);
  pos += len;
  return str;
}

static QVariant BencodeParser::parseList(const QByteArray &data, int &pos) {
  pos++; // skip 'l'
  QVariantList list;
  while (pos < data.size() && data[pos] != 'e') {
    const QVariant val = parseElement(data, pos);
    if (!m_errorString.isEmpty()) {
      return QVariant();
    }
    list.append(val);
  }

  if (pos >= data.size()) {
    m_errorString = "Unterminated list";
    return QVariant();
  }

  pos++; // skip 'e'
  return list;
}

static QVariant BencodeParser::parseDictionary(const QByteArray &data,
                                               int &pos) {
  pos++; // skip 'd'
  QVariantMap dict;

  while (pos < data.size() && data[pos] != 'e') {
    // Keys must be strings
    if (data[pos] < '0' || data[pos] > '9') {
      m_errorString = "Dictionary key must be a string";
      return QVariant();
    }

    const QByteArray key = parseByteString(data, pos);
    if (!m_errorString.isEmpty()) {
      return QVariant();
    }

    // Special handling for the "info" dictionary to compute the info hash
    const bool isInfoDict = (key == "info");
    const int infoStart = pos;

    const QVariant val = parseElement(data, pos);
    if (!m_errorString.isEmpty()) {
      return QVariant();
    }

    if (isInfoDict) {
      const int infoEnd = pos;
      const QByteArray infoData = data.mid(infoStart, infoEnd - infoStart);
      m_infoHash = QCryptographicHash::hash(infoData, QCryptographicHash::Sha1);
    }

    dict.insert(QString::fromUtf8(key), val);
  }

  if (pos >= data.size()) {
    m_errorString = "Unterminated dictionary";
    return QVariant();
  }

  pos++; // skip 'e'
  return dict;
}
