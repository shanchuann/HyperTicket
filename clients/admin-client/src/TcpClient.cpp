#include "TcpClient.hpp"
#include <QJsonObject>
#include <QJsonDocument>

static const int kConnectTimeoutMs  = 5000;   // 连接超时 5s
static const int kResponseTimeoutMs = 15000;  // 响应超时 15s

TcpClient::TcpClient(QObject *parent) : QObject(parent)
{
    socket_ = new QTcpSocket(this);
    connect(socket_, &QTcpSocket::connected,    this, &TcpClient::onConnected);
    connect(socket_, &QTcpSocket::disconnected, this, &TcpClient::onDisconnected);
    connect(socket_, &QTcpSocket::readyRead,    this, &TcpClient::onReadyRead);
    connect(socket_, &QAbstractSocket::errorOccurred, this, &TcpClient::onSocketError);

    connectTimer_ = new QTimer(this);
    connectTimer_->setSingleShot(true);
    connectTimer_->setInterval(kConnectTimeoutMs);
    connect(connectTimer_, &QTimer::timeout, this, &TcpClient::onConnectTimeout);

    responseTimer_ = new QTimer(this);
    responseTimer_->setSingleShot(true);
    responseTimer_->setInterval(kResponseTimeoutMs);
    connect(responseTimer_, &QTimer::timeout, this, [this]() {
        deliverError("请求超时，请检查服务器是否运行");
    });
}

bool TcpClient::connected() const
{
    return socket_->state() == QAbstractSocket::ConnectedState;
}

void TcpClient::connectToServer()
{
    if (socket_->state() != QAbstractSocket::UnconnectedState) return;
    connectTimer_->start();
    socket_->connectToHost(host_, static_cast<quint16>(port_));
}

int TcpClient::request(const QString &jsonPayload, QJSValue callback)
{
    int seq = ++seqCounter_;
    queue_.enqueue({ seq, jsonPayload, callback });
    if (!connected()) {
        connectToServer();
    } else {
        flushQueue();
    }
    return seq;
}

void TcpClient::flushQueue()
{
    if (waiting_ || queue_.isEmpty() || !connected()) return;
    auto pending = queue_.dequeue();
    currentSeq_      = pending.seq;
    currentCallback_ = pending.callback;
    waiting_         = true;
    responseTimer_->start();
    socket_->write((pending.json + '\n').toUtf8());
}

// 成功响应
void TcpClient::deliverResponse(const QString &json)
{
    responseTimer_->stop();
    int  seq = currentSeq_;
    QJSValue cb = currentCallback_;
    currentSeq_ = -1;
    currentCallback_ = QJSValue();
    waiting_ = false;

    emit responseReady(seq, json);

    // 兼容 callback 方式：通过 QTimer::singleShot 延迟到事件循环，避免重入
    if (cb.isCallable()) {
        QJSValueList args;
        args << QJSValue(json);
        cb.call(args);
    }

    flushQueue();
}

// 错误/超时
void TcpClient::deliverError(const QString &reason)
{
    responseTimer_->stop();
    QString errJson = QString(R"({"status":"ERR","reason":"%1"})").arg(reason);

    // 清空所有排队请求
    while (!queue_.isEmpty()) {
        auto p = queue_.dequeue();
        emit responseReady(p.seq, errJson);
        if (p.callback.isCallable()) {
            QJSValueList args; args << QJSValue(errJson);
            p.callback.call(args);
        }
    }

    // 当前等待中的请求
    if (waiting_) {
        int seq = currentSeq_;
        QJSValue cb = currentCallback_;
        currentSeq_ = -1;
        currentCallback_ = QJSValue();
        waiting_ = false;

        emit responseReady(seq, errJson);
        if (cb.isCallable()) {
            QJSValueList args; args << QJSValue(errJson);
            cb.call(args);
        }
    }
}

void TcpClient::onConnected()
{
    connectTimer_->stop();
    emit connectedChanged();
    flushQueue();
}

void TcpClient::onDisconnected()
{
    connectTimer_->stop();
    emit connectedChanged();
    deliverError("服务器连接已断开");
}

void TcpClient::onReadyRead()
{
    readBuffer_ += QString::fromUtf8(socket_->readAll());
    int idx;
    while ((idx = readBuffer_.indexOf('\n')) != -1) {
        QString line = readBuffer_.left(idx).trimmed();
        readBuffer_.remove(0, idx + 1);
        if (!line.isEmpty() && waiting_) {
            deliverResponse(line);
        }
    }
}

void TcpClient::onSocketError(QAbstractSocket::SocketError err)
{
    Q_UNUSED(err)
    QString msg = socket_->errorString();
    emit connectionError(msg);
    deliverError("无法连接到服务器：" + msg);
    // 确保 socket 回到未连接状态
    socket_->abort();
}

void TcpClient::onConnectTimeout()
{
    socket_->abort();
    deliverError("连接超时，请检查服务器是否运行（127.0.0.1:7000）");
}
