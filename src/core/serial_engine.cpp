#include "serial_engine.h"
#include <QDateTime>
#include <spdlog/spdlog.h>

SerialEngine::SerialEngine(QObject* parent)
    : QObject(parent)
    , serial_(nullptr)
    , workerThread_(nullptr)
    , reconnectTimer_(nullptr)
    , autoReconnect_(true)
    , reconnectDelay_(1)
{
}

SerialEngine::~SerialEngine()
{
    close();
}

void SerialEngine::startWorkerThread()
{
    workerThread_ = new QThread(this);
    serial_ = new QSerialPort();
    serial_->moveToThread(workerThread_);

    reconnectTimer_ = new QTimer();
    reconnectTimer_->moveToThread(workerThread_);
    reconnectTimer_->setSingleShot(true);

    connect(workerThread_, &QThread::started, this, [this]() {
        serial_->setPortName(config_.port);
        serial_->setBaudRate(config_.baudrate);
        serial_->setDataBits(config_.databits);
        serial_->setParity(config_.parity);
        serial_->setStopBits(config_.stopbits);
        serial_->setFlowControl(config_.flowcontrol);

        connect(serial_, &QSerialPort::readyRead, this, &SerialEngine::onReadyRead);
        connect(serial_, &QSerialPort::errorOccurred, this, &SerialEngine::onError);
        connect(reconnectTimer_, &QTimer::timeout, this, &SerialEngine::tryReconnect);

        if (serial_->open(QIODevice::ReadWrite)) {
            spdlog::info("Serial port {} opened @ {}bps", config_.port.toStdString(), config_.baudrate);
            reconnectDelay_ = 1;
            emit statusChanged(config_.port, true, config_);
        } else {
            spdlog::warn("Failed to open serial port {}: {}", config_.port.toStdString(),
                         serial_->errorString().toStdString());
            emit errorOccurred(config_.port, serial_->errorString());
            if (autoReconnect_) {
                reconnectTimer_->start(reconnectDelay_ * 1000);
            }
        }
    });

    workerThread_->start();
}

void SerialEngine::stopWorkerThread()
{
    if (reconnectTimer_) {
        reconnectTimer_->stop();
    }

    if (serial_) {
        if (serial_->isOpen()) {
            serial_->close();
        }
    }

    if (workerThread_) {
        workerThread_->quit();
        workerThread_->wait(3000);
        delete workerThread_;
        workerThread_ = nullptr;
    }

    delete serial_;
    serial_ = nullptr;
    delete reconnectTimer_;
    reconnectTimer_ = nullptr;
}

void SerialEngine::open(const SerialConfig& config)
{
    close();
    config_ = config;
    startWorkerThread();
}

void SerialEngine::close()
{
    stopWorkerThread();
    if (!config_.port.isEmpty()) {
        emit statusChanged(config_.port, false, config_);
    }
}

bool SerialEngine::isOpen() const
{
    return serial_ && serial_->isOpen();
}

qint64 SerialEngine::sendText(const QString& data, const QString& append)
{
    if (!serial_ || !serial_->isOpen()) {
        return -1;
    }

    QByteArray payload = data.toUtf8();
    payload.append(buildAppend(append));

    qint64 written = serial_->write(payload);
    if (written > 0) {
        emit dataSent(config_.port, written);
    }
    return written;
}

qint64 SerialEngine::sendHex(const QByteArray& data)
{
    if (!serial_ || !serial_->isOpen()) {
        return -1;
    }

    qint64 written = serial_->write(data);
    if (written > 0) {
        emit dataSent(config_.port, written);
    }
    return written;
}

qint64 SerialEngine::sendRaw(const QByteArray& data)
{
    if (!serial_ || !serial_->isOpen()) {
        return -1;
    }

    qint64 written = serial_->write(data);
    if (written > 0) {
        emit dataSent(config_.port, written);
    }
    return written;
}

void SerialEngine::setAutoReconnect(bool enabled)
{
    autoReconnect_ = enabled;
}

QString SerialEngine::portName() const
{
    return config_.port;
}

SerialConfig SerialEngine::config() const
{
    return config_;
}

void SerialEngine::onReadyRead()
{
    QByteArray data = serial_->readAll();
    if (!data.isEmpty()) {
        emit dataReceived(data, config_.port);
    }
}

void SerialEngine::onError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError || error == QSerialPort::TimeoutError) {
        return;
    }

    spdlog::warn("Serial port {} error: {}", config_.port.toStdString(),
                 serial_->errorString().toStdString());

    if (serial_->isOpen()) {
        serial_->close();
    }

    emit errorOccurred(config_.port, serial_->errorString());
    emit statusChanged(config_.port, false, config_);

    if (autoReconnect_) {
        reconnectTimer_->start(reconnectDelay_ * 1000);
    }
}

void SerialEngine::tryReconnect()
{
    if (serial_->isOpen()) {
        return;
    }

    serial_->setPortName(config_.port);
    serial_->setBaudRate(config_.baudrate);
    serial_->setDataBits(config_.databits);
    serial_->setParity(config_.parity);
    serial_->setStopBits(config_.stopbits);
    serial_->setFlowControl(config_.flowcontrol);

    if (serial_->open(QIODevice::ReadWrite)) {
        spdlog::info("Serial port {} reconnected", config_.port.toStdString());
        reconnectDelay_ = 1;
        emit statusChanged(config_.port, true, config_);
    } else {
        spdlog::warn("Reconnect to {} failed, next attempt in {}s",
                     config_.port.toStdString(), reconnectDelay_);
        reconnectDelay_ = qMin(reconnectDelay_ + 1, 5);
        reconnectTimer_->start(reconnectDelay_ * 1000);
    }
}

QByteArray SerialEngine::buildAppend(const QString& append) const
{
    if (append == "CR") {
        return QByteArray("\r");
    } else if (append == "LF") {
        return QByteArray("\n");
    } else if (append == "NONE") {
        return QByteArray();
    }
    return QByteArray("\r\n");
}
