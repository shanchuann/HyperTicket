#pragma once
#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <QQueue>
#include <QJSValue>
#include <QHash>

// 异步 TCP 客户端。
// QML 侧通过 responseReady(seq, jsonStr) 信号接收响应，
// request() 返回序列号，QML 用 onResponseReady 匹配。
// 也支持旧的 callback 方式（内部转发到信号）。
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

    // 发送请求，返回序列号。响应通过 responseReady(seq, json) 信号到达。
    // callback 可选，若提供则自动在信号触发时调用（兼容旧写法）。
    Q_INVOKABLE int request(const QString &jsonPayload, QJSValue callback = QJSValue());
    Q_INVOKABLE void connectToServer();

signals:
    void connectedChanged();
    void hostChanged();
    void portChanged();
    void connectionError(const QString &message);
    // 响应就绪：seq 是 request() 的返回值，json 是完整响应字符串
    void responseReady(int seq, const QString &json);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onSocketError(QAbstractSocket::SocketError err);
    void onConnectTimeout();

private:
    void flushQueue();
    void deliverResponse(const QString &json);
    void deliverError(const QString &reason);

    QTcpSocket *socket_;
    QTimer     *connectTimer_;  // 连接超时
    QTimer     *responseTimer_; // 响应超时

    QString readBuffer_;
    QString host_ = "127.0.0.1";
    int     port_ = 7000;

    int seqCounter_ = 0;

    struct Pending {
        int     seq;
        QString json;
        QJSValue callback;
    };
    QQueue<Pending> queue_;
    bool    waiting_ = false;
    int     currentSeq_ = -1;
    QJSValue currentCallback_;
};
