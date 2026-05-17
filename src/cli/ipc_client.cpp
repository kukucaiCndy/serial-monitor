#include "ipc_client.h"
#include "ipc_protocol.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <spdlog/spdlog.h>

IPCClient::IPCClient(QObject* parent)
    : QObject(parent)
    , socket_(nullptr)
{
}

IPCClient::~IPCClient()
{
    disconnect();
}

bool IPCClient::connectToServer(const QString& name)
{
    if (socket_) {
        socket_->disconnectFromServer();
        socket_->deleteLater();
    }

    socket_ = new QLocalSocket(this);
    connect(socket_, &QLocalSocket::connected, this, &IPCClient::onConnected);
    connect(socket_, &QLocalSocket::disconnected, this, &IPCClient::onDisconnected);
    connect(socket_, &QLocalSocket::readyRead, this, &IPCClient::onReadyRead);

    socket_->connectToServer(name);
    return socket_->waitForConnected(3000);
}

void IPCClient::disconnect()
{
    if (socket_ && socket_->isOpen()) {
        QJsonObject params;
        params["client_type"] = "cli";
        QByteArray msg = IpcProtocol::buildMessage("goodbye", params);
        socket_->write(msg);
        socket_->flush();
        socket_->disconnectFromServer();
    }
}

bool IPCClient::isConnected() const
{
    return socket_ && socket_->state() == QLocalSocket::ConnectedState;
}

void IPCClient::sendCommand(const QString& command, const QJsonObject& params,
                            const QString& requestId)
{
    if (!socket_ || !socket_->isOpen()) {
        spdlog::warn("Cannot send command: not connected");
        return;
    }

    QByteArray msg = IpcProtocol::buildMessage(command, params, requestId);
    socket_->write(msg);
    socket_->flush();
}

void IPCClient::onConnected()
{
    QJsonObject helloParams;
    helloParams["version"] = "2.0";
    helloParams["client_type"] = "cli";
    QByteArray msg = IpcProtocol::buildMessage("hello", helloParams);
    socket_->write(msg);
    socket_->flush();

    spdlog::info("IPC client connected");
    emit connected();
}

void IPCClient::onDisconnected()
{
    spdlog::info("IPC client disconnected");
    emit disconnected();
}

void IPCClient::onReadyRead()
{
    readBuffer_.append(socket_->readAll());
    processBuffer();
}

void IPCClient::processBuffer()
{
    while (true) {
        int newlinePos = readBuffer_.indexOf('\n');
        if (newlinePos < 0) break;

        QByteArray line = readBuffer_.left(newlinePos).trimmed();
        readBuffer_.remove(0, newlinePos + 1);

        if (line.isEmpty()) continue;

        QJsonObject msg = IpcProtocol::parseMessage(line);
        if (msg.isEmpty()) continue;

        QString type = msg["type"].toString();
        QJsonObject payload = msg["payload"].toObject();

        if (type == "log_entry") {
            emit logReceived(payload);
        } else if (type == "status_update") {
            emit statusChanged(payload);
        } else if (type == "response") {
            QString id = msg["id"].toString();
            bool success = payload["success"].toBool();
            emit responseReceived(id, success, payload);
        } else if (type == "error") {
            QString message = payload["message"].toString();
            emit errorOccurred(message);
        } else if (type == "server_shutdown") {
            spdlog::info("Server is shutting down");
            emit errorOccurred("Server closed");
        }
    }
}