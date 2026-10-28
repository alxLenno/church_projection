#pragma once
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QImage>
#include <QBuffer>
#include <QElapsedTimer>
#include <functional>
#include <QHash>
#include <QUrl>
#include <QCryptographicHash>
#include <memory>

// Loopback-only frame delivery. OBS Browser Source polls independently of
// native window exposure, so covered/minimised windows do not stall the ticker.
class LocalObsFeed : public QObject {
public:
  explicit LocalObsFeed(std::function<QImage()> render, QObject *parent = nullptr)
      : QObject(parent), renderer(std::move(render)), server(this) {
    setRenderer({}, renderer);
    setRenderer("normal", renderer);
    setRenderer("obs", renderer);
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
          QByteArray key=path.mid(1);
          const bool frameRequest=key=="frame" || key.endsWith("/frame");
          if(frameRequest) key=key=="frame"?QByteArray():key.left(key.size()-6);
          else if(key.endsWith('/'))key.chop(1);
          auto stream=streams.value(QString::fromUtf8(key));
          const QByteArray prefix=key.isEmpty()?QByteArray():"/"+key;
          if (request.startsWith("GET ") && stream && !frameRequest) {
            type = "text/html; charset=utf-8";
            body = R"HTML(<!doctype html><html><head><meta charset="utf-8"><title>ChurchProjection OBS Feed</title><style>html,body{margin:0;width:100%;height:100%;overflow:hidden;background:transparent}img{width:100%;height:100%;object-fit:contain}</style></head><body><img id="stage"><script>const frame=document.getElementById('stage');function next(){frame.src='/frame?t='+Date.now()}frame.onload=()=>setTimeout(next,50);frame.onerror=()=>setTimeout(next,1000);next();</script></body></html>)HTML";
            body.replace("/frame?t=",prefix+"/frame?t=");
            if(key=="normal")body.replace("ChurchProjection OBS Feed","ChurchProjection Normal Output");
            if(key=="obs")body.replace("ChurchProjection OBS Feed","ChurchProjection Formatted OBS Output");
          } else if (request.startsWith("GET ") && stream && frameRequest) {

            if (!stream->clock.isValid() || stream->clock.elapsed() >= 45) {
              QImage frame = stream->render();
              stream->cached.clear(); QBuffer buffer(&stream->cached); buffer.open(QIODevice::WriteOnly);
              stream->type=frame.hasAlphaChannel()?"image/png":"image/jpeg";
              frame.save(&buffer, frame.hasAlphaChannel()?"PNG":"JPEG", frame.hasAlphaChannel()?1:90); stream->clock.restart();
            }
            type=stream->type;body = stream->cached;
          } else { status = "404 Not Found"; type = "text/plain"; body = "Not found"; }
          socket->write("HTTP/1.1 " + status + "\r\nContent-Type: " + type +
                        "\r\nCache-Control: no-store\r\nConnection: close\r\nContent-Length: " + QByteArray::number(body.size()) + "\r\n\r\n" + body);
          socket->disconnectFromHost();
        });
      }
    });
  }
  static QString layoutEndpoint(const QString &name) {
    return "layout/"+QString::fromLatin1(QUrl::toPercentEncoding(name));
  }
  static QString legacyLayoutEndpoint(const QString &name) {
    return "layout/"+QString::fromLatin1(QCryptographicHash::hash(name.toUtf8(),QCryptographicHash::Sha256).toHex());
  }
  void setLayoutRenderer(const QString &name,std::function<QImage()> value) {
    setRenderer(layoutEndpoint(name),value);
    const auto legacy=legacyLayoutEndpoint(name);
    if(!streams.contains(legacy))setRenderer(legacy,std::move(value));
  }
  void setRenderer(const QString &key,std::function<QImage()> value) {
    auto stream=std::make_shared<Stream>();stream->render=std::move(value);streams.insert(key,stream);
  }
  void setNormalRenderer(std::function<QImage()> value) { setRenderer("normal",std::move(value)); }
  void setFormattedRenderer(std::function<QImage()> value) { setRenderer("obs",std::move(value)); }
  bool start() {
    if (server.isListening()) return true;
    for (quint16 port = 18765; port < 18775; ++port)
      if (server.listen(QHostAddress::LocalHost, port)) return true;
    return false;
  }
  QString url(const QString &feed = {}) const { return server.isListening() ? QString("http://127.0.0.1:%1/").arg(server.serverPort())+feed : QString(); }
private:
  struct Stream {std::function<QImage()> render;QByteArray cached,type;QElapsedTimer clock;};
  QHash<QString,std::shared_ptr<Stream>> streams;
  std::function<QImage()> renderer;
  QTcpServer server;

};
