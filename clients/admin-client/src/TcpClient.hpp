#pragma once
#include <QObject>
#include <QTcpSocket>
#include <QQueue>
#include <QJSValue>

// 异步 TCP 客户端，供 QML 通过 context property 调用。
// 请求串行化（一次一个）。
// QML 侧调用：tcpClient.request(JSON.stringify(payload), function(jsonStr){ var r = JSON.parse(jsonStr); ... })
class TcpClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(QString host READ host WRITE setHost NOTIFY hostChanged)
    Q_PROPERTY(int port READ port WRITE setPort NOTIFY portChanged)

public:
    explicit TcpClient(QObject *parent = nullptr);

    bool connected() const;
    QString host() const { return host_; }
    int port() const { return port_; }
    void setHost(const QString &h) { host_ = h; emit hostChanged(); }
    void setPort(int p) { port_ = p; emit portChanged(); }

    // payload: JSON 字符串；callback: function(jsonResponseString)
    Q_INVOKABLE void request(const QString &jsonPayload, QJSValue callback);
    Q_INVOKABLE void connectToServer();

signals:
    void connectedChanged();
    void hostChanged();
    void portChanged();
    void connectionError(const QString &message);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onSocketError(QAbstractSocket::SocketError err);

private:
    void flushQueue();

    QTcpSocket *socket_;
    QString readBuffer_;
    QString host_ = "127.0.0.1";
    int port_ = 7000;

    struct Pending { QString json; QJSValue callback; };
    QQueue<Pending> queue_;
    bool waiting_ = false;
    QJSValue currentCallback_;
};
