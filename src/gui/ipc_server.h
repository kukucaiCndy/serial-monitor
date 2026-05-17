#ifndef IPC_SERVER_H
#define IPC_SERVER_H

#include <QObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QJsonObject>
#include <QVector>
#include "log_buffer.h"
#include "serial_engine.h"

class IPCServer : public QObject {
    Q_OBJECT
public:
    explicit IPCServer(QObject* parent = nullptr);
    ~IPCServer();

    bool start(const QString& name = "serial_monitor_ipc");
    void stop();
    void broadcastLog(const LogEntry& entry);
    void broadcastStatus(const QString& port, bool connected,
                         const SerialConfig& config, qint64 rxBytes,
                         qint64 txBytes, qint64 uptimeSeconds);
    void sendResponse(const QString& clientId, const QString& requestId,
                      bool success, const QJsonObject& data);
    int clientCount() const;

signals:
    void clientConnected(const QString& clientId);
    void clientDisconnected(const QString& clientId);
    void commandReceived(const QString& clientId, const QString& command,
                         const QJsonObject& params, const QString& requestId);

private slots:
    void onNewConnection();
    void onDisconnected();
    void onReadyRead();

private:
    QLocalServer* server_;
    QVector<QLocalSocket*> clients_;
    QVector<QByteArray> readBuffers_;

    void sendToClient(QLocalSocket* client, const QByteArray& message);
    void processClientData(QLocalSocket* client, int clientIndex);
};

#endif
