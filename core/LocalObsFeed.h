#pragma once
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QImage>
#include <QBuffer>
#include <QElapsedTimer>
#include <functional>

// Loopback-only frame delivery. OBS Browser Source polls independently of
// native window exposure, so covered/minimised windows do not stall the ticker.
class LocalObsFeed : public QObject {
public:
  explicit LocalObsFeed(std::function<QImage()> render, QObject *parent = nullptr)
      : QObject(parent), renderer(std::move(render)), server(this) {
    connect(&server, &QTcpServer::newConnection, this, [this] {
      while (server.hasPendingConnections()) {
        auto *socket = server.nextPendingConnection();
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        QTimer::singleShot(5000, socket, [socket] { socket->abort(); });
        connect(socket, &QTcpSocket::readyRead, socket, [this, socket] {
          QByteArray request = socket->property("request").toByteArray() + socket->readAll();
          if (request.size() > 8192) { socket->abort(); return; }
          if (!request.contains("\r\n\r\n")) { socket->setProperty("request", request); return; }
          if (socket->property("answered").toBool()) return;
          socket->setProperty("answered", true);
          const QByteArray path = request.split(' ').value(1).split('?').value(0);
          QByteArray body, type, status = "200 OK";
          if (request.startsWith("GET ") && path == "/") {
            type = "text/html; charset=utf-8";
            body = R"HTML(<!doctype html><html><head><meta charset="utf-8"><title>ChurchProjection OBS Feed</title><style>html,body{margin:0;width:100%;height:100%;overflow:hidden;background:black}img{width:100%;height:100%;object-fit:contain}</style></head><body><img id="stage"><script>const frame=document.getElementById('stage');function next(){frame.src='/frame?t='+Date.now()}frame.onload=()=>setTimeout(next,50);frame.onerror=()=>setTimeout(next,1000);next();</script></body></html>)HTML";
          } else if (request.startsWith("GET ") && path == "/frame") {
            type = "image/jpeg";
            if (!cacheClock.isValid() || cacheClock.elapsed() >= 45) {
              QImage frame = renderer();
              cached.clear(); QBuffer buffer(&cached); buffer.open(QIODevice::WriteOnly);
              frame.save(&buffer, "JPEG", 90); cacheClock.restart();
            }
            body = cached;
          } else { status = "404 Not Found"; type = "text/plain"; body = "Not found"; }
          socket->write("HTTP/1.1 " + status + "\r\nContent-Type: " + type +
                        "\r\nCache-Control: no-store\r\nConnection: close\r\nContent-Length: " + QByteArray::number(body.size()) + "\r\n\r\n" + body);
          socket->disconnectFromHost();
        });
      }
    });
  }
  bool start() {
    if (server.isListening()) return true;
    for (quint16 port = 18765; port < 18775; ++port)
      if (server.listen(QHostAddress::LocalHost, port)) return true;
    return false;
  }
  QString url() const { return server.isListening() ? QString("http://127.0.0.1:%1/").arg(server.serverPort()) : QString(); }
private:
  std::function<QImage()> renderer;
  QTcpServer server;
  QByteArray cached;
  QElapsedTimer cacheClock;
};
