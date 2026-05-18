#ifndef SERIAL_ENGINE_H
#define SERIAL_ENGINE_H

#include <QObject>
#include <QSerialPort>
#include <QThread>
#include <QTimer>
#include <QByteArray>
#include <QString>

struct SerialConfig {
    QString port;
    qint32 baudrate = 115200;
    QSerialPort::DataBits databits = QSerialPort::Data8;
    QSerialPort::Parity parity = QSerialPort::NoParity;
    QSerialPort::StopBits stopbits = QSerialPort::OneStop;
    QSerialPort::FlowControl flowcontrol = QSerialPort::NoFlowControl;
};

class SerialEngine : public QObject {
    Q_OBJECT
public:
    explicit SerialEngine(QObject* parent = nullptr);
    ~SerialEngine();

    void open(const SerialConfig& config);
    void close();
    bool isOpen() const;

    qint64 sendText(const QString& data, const QString& append = "CRLF");
    qint64 sendHex(const QByteArray& data);
    qint64 sendRaw(const QByteArray& data);
    void setAutoReconnect(bool enabled);

    QString portName() const;
    SerialConfig config() const;

signals:
    void dataReceived(const QByteArray& data, const QString& port);
    void statusChanged(const QString& port, bool connected, const SerialConfig& config);
    void errorOccurred(const QString& port, const QString& error);
    void dataSent(const QString& port, qint64 bytes);

private slots:
    void onReadyRead();
    void onError(QSerialPort::SerialPortError error);
    void tryReconnect();

private:
    QSerialPort* serial_;
    QThread* workerThread_;
    QTimer* reconnectTimer_;
    SerialConfig config_;
    bool autoReconnect_;
    int reconnectDelay_;

    void startWorkerThread();
    void stopWorkerThread();
    QByteArray buildAppend(const QString& append) const;
};

#endif
