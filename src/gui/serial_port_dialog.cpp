#include "serial_port_dialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFormLayout>
#include <QGroupBox>

SerialPortDialog::SerialPortDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("串口配置");
    setMinimumWidth(400);
    setupUi();
}

void SerialPortDialog::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);

    QFormLayout* form = new QFormLayout();
    form->setSpacing(8);

    nameEdit_ = new QLineEdit(this);
    nameEdit_->setPlaceholderText("可选标识名称");
    form->addRow("名称", nameEdit_);

    portCombo_ = new QComboBox(this);
    portCombo_->setEditable(true);
    portCombo_->setInsertPolicy(QComboBox::NoInsert);
    for (const auto& info : QSerialPortInfo::availablePorts()) {
        portCombo_->addItem(info.portName());
    }
    form->addRow("串口号", portCombo_);

    baudCombo_ = new QComboBox(this);
    baudCombo_->setEditable(true);
    baudCombo_->addItems({"3000000", "2500000", "2000000", "1500000", "1000000",
                          "921600", "460800", "256000", "230400", "128000",
                          "115200", "76800", "57600", "38400", "31250",
                          "28800", "19200", "14400", "9600"});
    baudCombo_->setCurrentText("115200");
    form->addRow("波特率", baudCombo_);

    dataBitsCombo_ = new QComboBox(this);
    dataBitsCombo_->addItems({"8", "7", "6", "5"});
    dataBitsCombo_->setCurrentIndex(0);
    form->addRow("数据位", dataBitsCombo_);

    parityCombo_ = new QComboBox(this);
    parityCombo_->addItems({"无校验", "偶校验", "奇校验", "标记校验", "空格校验"});
    parityCombo_->setCurrentIndex(0);
    form->addRow("校验位", parityCombo_);

    stopBitsCombo_ = new QComboBox(this);
    stopBitsCombo_->addItems({"1", "1.5", "2"});
    stopBitsCombo_->setCurrentIndex(0);
    form->addRow("停止位", stopBitsCombo_);

    mainLayout->addLayout(form);

    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    QPushButton* cancelBtn = new QPushButton("取消", this);
    cancelBtn->setObjectName("bottomBtn");
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(cancelBtn);
    QPushButton* okBtn = new QPushButton("确定", this);
    okBtn->setObjectName("bottomBtn");
    okBtn->setDefault(true);
    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
    btnLayout->addWidget(okBtn);
    mainLayout->addLayout(btnLayout);
}

void SerialPortDialog::setSavedPort(const SavedPort& sp)
{
    nameEdit_->setText(sp.name);
    portCombo_->setCurrentText(sp.port);
    baudCombo_->setCurrentText(QString::number(sp.baudrate));

    switch (sp.databits) {
        case QSerialPort::Data5: dataBitsCombo_->setCurrentText("5"); break;
        case QSerialPort::Data6: dataBitsCombo_->setCurrentText("6"); break;
        case QSerialPort::Data7: dataBitsCombo_->setCurrentText("7"); break;
        default: dataBitsCombo_->setCurrentText("8"); break;
    }

    switch (sp.parity) {
        case QSerialPort::EvenParity: parityCombo_->setCurrentIndex(1); break;
        case QSerialPort::OddParity: parityCombo_->setCurrentIndex(2); break;
        case QSerialPort::MarkParity: parityCombo_->setCurrentIndex(3); break;
        case QSerialPort::SpaceParity: parityCombo_->setCurrentIndex(4); break;
        default: parityCombo_->setCurrentIndex(0); break;
    }

    switch (sp.stopbits) {
        case QSerialPort::OneAndHalfStop: stopBitsCombo_->setCurrentIndex(1); break;
        case QSerialPort::TwoStop: stopBitsCombo_->setCurrentIndex(2); break;
        default: stopBitsCombo_->setCurrentIndex(0); break;
    }
}

SavedPort SerialPortDialog::savedPort() const
{
    SavedPort sp;
    sp.name = nameEdit_->text().trimmed();
    sp.port = portCombo_->currentText().trimmed();
    sp.baudrate = baudCombo_->currentText().toInt();

    sp.databits = static_cast<QSerialPort::DataBits>(
        dataBitsCombo_->currentText().toInt());

    int parityIdx = parityCombo_->currentIndex();
    switch (parityIdx) {
        case 1: sp.parity = QSerialPort::EvenParity; break;
        case 2: sp.parity = QSerialPort::OddParity; break;
        case 3: sp.parity = QSerialPort::MarkParity; break;
        case 4: sp.parity = QSerialPort::SpaceParity; break;
        default: sp.parity = QSerialPort::NoParity; break;
    }

    int stopIdx = stopBitsCombo_->currentIndex();
    switch (stopIdx) {
        case 1: sp.stopbits = QSerialPort::OneAndHalfStop; break;
        case 2: sp.stopbits = QSerialPort::TwoStop; break;
        default: sp.stopbits = QSerialPort::OneStop; break;
    }

    return sp;
}