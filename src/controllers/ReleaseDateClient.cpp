#include "ReleaseDateClient.h"

#include <QDate>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrlQuery>

#include <memory>

namespace {
const QString kUnknown = QStringLiteral("Release Date unknown");
const QString kNoConnection =
    QStringLiteral("No Connection to Musicbrainz for release dates");
constexpr qsizetype kMaxResponseBytes = 1024 * 1024;

QString escapeLucene(QString text) {
  // Quotes, backslashes and Lucene operators in titles must be literal.
  const QString special = QStringLiteral("+-!(){}[]^\"~*?:\\/|&");
  QString escaped;
  for (const QChar ch : text) {
    if (special.contains(ch)) {
      escaped += QLatin1Char('\\');
    }
    escaped += ch;
  }
  return escaped;
}

QString validDate(const QString &text) {
  static const QRegularExpression pattern(
      QStringLiteral(R"(^\d{4}(?:-\d{2}(?:-\d{2})?)?$)"));
  if (!pattern.match(text).hasMatch()) {
    return {};
  }
  const QString full = text.size() == 4 ? text + QStringLiteral("-01-01")
                       : text.size() == 7 ? text + QStringLiteral("-01") : text;
  return QDate::fromString(full, Qt::ISODate).isValid() ? text : QString();
}

QString parseDate(const QByteArray &body) {
  QJsonParseError error;
  const QJsonDocument json = QJsonDocument::fromJson(body, &error);
  if (error.error != QJsonParseError::NoError || !json.isObject()) {
    return kNoConnection;
  }
  const QJsonObject root = json.object();
  const QJsonArray recordings = root.value(QStringLiteral("recordings")).toArray();
  if (root.value(QStringLiteral("count")).toInt(-1) != 1 ||
      recordings.size() != 1) {
    return kUnknown;
  }
  const QString date = validDate(recordings.first().toObject()
                                     .value(QStringLiteral("first-release-date"))
                                     .toString());
  return date.isEmpty() ? kUnknown : date;
}
} // namespace

ReleaseDateClient::ReleaseDateClient(QObject *parent)
    : ReleaseDateClient(new QNetworkAccessManager,
                        QUrl(QStringLiteral("https://musicbrainz.org/ws/2/recording/")),
                        parent) {
  m_manager->setParent(this);
}

ReleaseDateClient::ReleaseDateClient(QNetworkAccessManager *manager,
                                     const QUrl &endpoint, QObject *parent)
    : QObject(parent), m_manager(manager), m_endpoint(endpoint) {
  m_dispatchTimer.setSingleShot(true);
  m_dispatchTimer.setTimerType(Qt::PreciseTimer);
  connect(&m_dispatchTimer, &QTimer::timeout, this, &ReleaseDateClient::dispatch);
}

QString ReleaseDateClient::releaseDate() const { return m_releaseDate; }

void ReleaseDateClient::setReleaseDate(const QString &date) {
  if (m_releaseDate != date) {
    m_releaseDate = date;
    emit releaseDateChanged();
  }
}

void ReleaseDateClient::clearTrack() {
  m_dispatchTimer.stop();
  m_key.clear();
  m_title.clear();
  m_artist.clear();
  setReleaseDate({});
}

void ReleaseDateClient::setTrack(const QString &title, const QString &firstArtist) {
  const QString cleanTitle = title.trimmed();
  const QString cleanArtist = firstArtist.trimmed();
  if (cleanTitle.isEmpty() || cleanArtist.isEmpty()) {
    m_dispatchTimer.stop();
    m_key.clear();
    setReleaseDate(kUnknown);
    return;
  }
  const QString key = cleanArtist + QChar(0x1f) + cleanTitle;
  if (m_key == key) {
    return;
  }
  m_key = key;
  m_title = cleanTitle;
  m_artist = cleanArtist;
  setReleaseDate({});
  if (QString *cached = m_cache.object(key)) {
    m_dispatchTimer.stop();
    setReleaseDate(*cached);
  } else {
    // A zero-delay timer coalesces MPRIS title and artist changes in one event.
    // Never start a second request until a full second after the previous one.
    const int delay = m_lastRequest.isValid()
                          ? static_cast<int>(qMax<qint64>(0, 1000 - m_lastRequest.elapsed()))
                          : 0;
    m_dispatchTimer.start(delay);
  }
}

void ReleaseDateClient::dispatch() {
  if (m_key.isEmpty()) {
    return;
  }
  if (m_lastRequest.isValid() && m_lastRequest.elapsed() < 1000) {
    m_dispatchTimer.start(static_cast<int>(1000 - m_lastRequest.elapsed()));
    return;
  }
  const QString key = m_key;
  QUrl url = m_endpoint;
  QUrlQuery params;
  params.addQueryItem(QStringLiteral("query"),
                      QStringLiteral("recording:\"%1\" AND artist:\"%2\"")
                          .arg(escapeLucene(m_title), escapeLucene(m_artist)));
  params.addQueryItem(QStringLiteral("fmt"), QStringLiteral("json"));
  params.addQueryItem(QStringLiteral("limit"), QStringLiteral("2"));
  url.setQuery(params);

  QNetworkRequest request(url);
  request.setRawHeader("User-Agent", "BierKistnRadio/0.1.0 (https://github.com/Xoechz/BierKistnRadio)");
  request.setRawHeader("Accept", "application/json");
  request.setTransferTimeout(15000);
  m_lastRequest.start();
  QNetworkReply *reply = m_manager->get(request);
  auto body = std::make_shared<QByteArray>();
  connect(reply, &QIODevice::readyRead, this, [reply, body]() {
    body->append(reply->readAll());
    if (body->size() > kMaxResponseBytes) {
      reply->abort();
    }
  });
  connect(reply, &QNetworkReply::finished, this, [this, reply, key, body]() {
    reply->deleteLater();
    body->append(reply->readAll());
    const QString result = reply->error() == QNetworkReply::NoError &&
                                   body->size() <= kMaxResponseBytes &&
                                   reply->attribute(QNetworkRequest::HttpStatusCodeAttribute)
                                           .toInt() == 200
                               ? parseDate(*body)
                               : kNoConnection;
    if (result != kNoConnection) {
      m_cache.insert(key, new QString(result));
    }
    if (key == m_key) {
      setReleaseDate(result);
    }
  });
}
