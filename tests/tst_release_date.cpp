#include "ReleaseDateClient.h"

#include <QNetworkAccessManager>
#include <QQueue>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrlQuery>
#include <QtTest/QtTest>

class RecordingServer : public QTcpServer {
public:
  struct Response {
    int status;
    QByteArray body;
  };

  QList<QByteArray> requests;
  QList<qint64> times;
  QElapsedTimer clock;

  RecordingServer() {
    clock.start();
    connect(this, &QTcpServer::newConnection, this, [this]() {
      QTcpSocket *socket = nextPendingConnection();
      connect(socket, &QTcpSocket::readyRead, socket, [this, socket]() {
        QByteArray &data = m_buffers[socket];
        data += socket->readAll();
        if (!data.contains("\r\n\r\n")) {
          return;
        }
        requests.append(data);
        times.append(clock.elapsed());
        m_buffers.remove(socket);
        m_pending.enqueue(socket);
        flush();
      });
      connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
    });
  }

  void enqueue(int status, const QByteArray &body) {
    m_responses.enqueue({status, body});
    flush();
  }

private:
  QQueue<Response> m_responses;
  QQueue<QTcpSocket *> m_pending;
  QHash<QTcpSocket *, QByteArray> m_buffers;

  void flush() {
    while (!m_pending.isEmpty() && !m_responses.isEmpty()) {
      QTcpSocket *socket = m_pending.dequeue();
      const Response response = m_responses.dequeue();
      socket->write("HTTP/1.1 " + QByteArray::number(response.status) +
                    (response.status == 200 ? " OK" : " Unavailable") +
                    "\r\nContent-Type: application/json\r\nContent-Length: " +
                    QByteArray::number(response.body.size()) +
                    "\r\nConnection: close\r\n\r\n" + response.body);
      socket->disconnectFromHost();
    }
  }
};

class TestReleaseDate : public QObject {
  Q_OBJECT

private slots:
  void testLookupAndCache();
  void testUnknownAndConnectionFailure();
  void testStaleResponseAndRateLimit();
};

void TestReleaseDate::testLookupAndCache() {
  RecordingServer server;
  QVERIFY(server.listen(QHostAddress::LocalHost));
  server.enqueue(200, R"({"count":1,"recordings":[{"first-release-date":"2005-08-09"}]})");
  QNetworkAccessManager manager;
  ReleaseDateClient client(
      &manager, QUrl(QStringLiteral("http://127.0.0.1:%1/ws/2/recording/")
                         .arg(server.serverPort())));
  QSignalSpy changed(&client, &ReleaseDateClient::releaseDateChanged);
  client.setTrack(QStringLiteral("Exodus (Live)"), QStringLiteral("Brand of Sacrifice"));
  QTRY_COMPARE(client.releaseDate(), QStringLiteral("2005-08-09"));
  QCOMPARE(changed.size(), 1);
  QCOMPARE(server.requests.size(), 1);
  const QByteArray request = server.requests.first();
  const QByteArray urlPart = request.split(' ').at(1);
  const QUrl url(QStringLiteral("http://localhost") + QString::fromLatin1(urlPart));
  const QUrlQuery query(url);
  QCOMPARE(query.queryItemValue(QStringLiteral("query")),
           QStringLiteral("recording:\"Exodus \\(Live\\)\" AND artist:\"Brand of Sacrifice\""));
  QCOMPARE(query.queryItemValue(QStringLiteral("fmt")), QStringLiteral("json"));
  QCOMPARE(query.queryItemValue(QStringLiteral("limit")), QStringLiteral("2"));
  QVERIFY(request.contains("User-Agent: BierKistnRadio/0.1.0 "));

  client.clearTrack();
  QCOMPARE(client.releaseDate(), QString());
  client.setTrack(QStringLiteral("Exodus (Live)"), QStringLiteral("Brand of Sacrifice"));
  QCOMPARE(client.releaseDate(), QStringLiteral("2005-08-09"));
  QCOMPARE(server.requests.size(), 1);
}

void TestReleaseDate::testUnknownAndConnectionFailure() {
  RecordingServer server;
  QVERIFY(server.listen(QHostAddress::LocalHost));
  QNetworkAccessManager manager;
  ReleaseDateClient client(
      &manager, QUrl(QStringLiteral("http://127.0.0.1:%1/ws/2/recording/")
                         .arg(server.serverPort())));
  client.setTrack(QStringLiteral(""), QStringLiteral("Artist"));
  QCOMPARE(client.releaseDate(), QStringLiteral("Release Date unknown"));
  QCOMPARE(server.requests.size(), 0);

  server.enqueue(200, R"({"count":2,"recordings":[{},{}]})");
  client.setTrack(QStringLiteral("Ambiguous"), QStringLiteral("Artist"));
  QTRY_COMPARE(client.releaseDate(), QStringLiteral("Release Date unknown"));
  QTRY_COMPARE(server.requests.size(), 1);
  server.enqueue(503, R"({"error":"busy"})");
  client.setTrack(QStringLiteral("Offline"), QStringLiteral("Artist"));
  QTRY_COMPARE(client.releaseDate(),
               QStringLiteral("No Connection to Musicbrainz for release dates"));

  client.clearTrack();
  QCOMPARE(client.releaseDate(), QString());
  server.enqueue(200, R"({"count":0,"recordings":[]})");
  client.setTrack(QStringLiteral("Missing"), QStringLiteral("Artist"));
  QTRY_COMPARE(client.releaseDate(), QStringLiteral("Release Date unknown"));
}

void TestReleaseDate::testStaleResponseAndRateLimit() {
  RecordingServer server;
  QVERIFY(server.listen(QHostAddress::LocalHost));
  QNetworkAccessManager manager;
  ReleaseDateClient client(
      &manager, QUrl(QStringLiteral("http://127.0.0.1:%1/ws/2/recording/")
                         .arg(server.serverPort())));
  client.setTrack(QStringLiteral("Old"), QStringLiteral("Artist"));
  QTRY_COMPARE(server.requests.size(), 1);
  client.setTrack(QStringLiteral("New"), QStringLiteral("Artist"));
  server.enqueue(200, R"({"count":1,"recordings":[{"first-release-date":"1990"}]})");
  QTest::qWait(50);
  QCOMPARE(client.releaseDate(), QString()); // old track must not overwrite New
  server.enqueue(200, R"({"count":1,"recordings":[{"first-release-date":"2001-02"}]})");
  QTRY_COMPARE(client.releaseDate(), QStringLiteral("2001-02"));
  QCOMPARE(server.requests.size(), 2);
  QVERIFY(server.times.at(1) - server.times.at(0) >= 1000);
}

QTEST_MAIN(TestReleaseDate)
#include "tst_release_date.moc"
