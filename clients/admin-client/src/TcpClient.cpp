#include "TcpClient.hpp"

TcpClient::TcpClient(QObject *parent) : QObject(parent)
{
    socket_ = new QTcpSocket(this);
    connect(socket_, &QTcpSocket::connected,    this, &TcpClient::onConnected);
    connect(socket_, &QTcpSocket::disconnected, this, &TcpClient::onDisconnected);
    connect(socket_, &QTcpSocket::readyRead,    this, &TcpClient::onReadyRead);
    connect(socket_, &QAbstractSocket::errorOccurred, this, &TcpClient::onSocketError);
}

bool TcpClient::connected() const
{
    return socket_->state() == QAbstractSocket::ConnectedState;
}

void TcpClient::connectToServer()
{
    if (socket_->state() != QAbstractSocket::UnconnectedState) return;
    socket_->connectToHost(host_, static_cast<quint16>(port_));
}

void TcpClient::request(const QString &jsonPayload, QJSValue callback)
{
    queue_.enqueue({ jsonPayload, callback });
    if (!connected()) {
        connectToServer();
    } else {
        flushQueue();
    }
}

void TcpClient::flushQueue()
{
    if (waiting_ || queue_.isEmpty() || !connected()) return;
    auto pending = queue_.dequeue();
    currentCallback_ = pending.callback;
    waiting_ = true;
    socket_->write((pending.json + '\n').toUtf8());
}

static void callbackWithError(QJSValue &cb, const QString &reason)
{
    if (!cb.isCallable()) return;
    QJSValueList args;
    args << QJSValue(QString("{\"status\":\"ERR\",\"reason\":\"%1\"}").arg(reason));
    cb.call(args);
}

void TcpClient::onConnected()
{
    emit connectedChanged();
    flushQueue();
}

void TcpClient::onDisconnected()
{
    emit connectedChanged();
    while (!queue_.isEmpty()) {
        auto pending = queue_.dequeue();
        callbackWithError(pending.callback, "连接已断开");
    }
    if (waiting_) {
        callbackWithError(currentCallback_, "连接已断开");
        waiting_ = false;
        currentCallback_ = QJSValue();
    }
}

void TcpClient::onReadyRead()
{
    readBuffer_ += QString::fromUtf8(socket_->readAll());
    int idx;
    while ((idx = readBuffer_.indexOf('\n')) != -1) {
        QString line = readBuffer_.left(idx).trimmed();
        readBuffer_.remove(0, idx + 1);

        if (!line.isEmpty() && waiting_) {
            waiting_ = false;
            QJSValue cb = currentCallback_;
            currentCallback_ = QJSValue();
            flushQueue(); // 先 flush 再回调，避免回调里发请求被漏处理

            if (cb.isCallable()) {
                QJSValueList args;
                args << QJSValue(line);
                cb.call(args);
            }
        }
    }
}

void TcpClient::onSocketError(QAbstractSocket::SocketError)
{
    emit connectionError(socket_->errorString());
    onDisconnected();
}
