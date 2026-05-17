#ifndef IPC_CLIENT_H
#define IPC_CLIENT_H

#include <QObject>
#include <QLocalSocket>
#include <QJsonObject>
#include <QByteArray>

class IPCClient : public QObject {
    Q_OBJECT
public:
    explicit IPCClient(QObject* parent = nullptr);
    ~IPCClient();

    bool connectToServer(const QString& name = "serial_monitor_ipc");
    void disconnect();
    bool isConnected() const;
    void sendCommand(const QString& command, const QJsonObject& params = {},
                     const QString& requestId = "");

signals:
    void connected();
    void disconnected();
    void logReceived(const QJsonObject& log);
    void statusChanged(const QJsonObject& status);
    void responseReceived(const QString& id, bool success, const QJsonObject& data);
    void errorOccurred(const QString& error);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();

private:
    QLocalSocket* socket_;
    QByteArray readBuffer_;
    void processBuffer();
};

#endif