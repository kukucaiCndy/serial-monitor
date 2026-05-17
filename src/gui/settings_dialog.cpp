#include "settings_dialog.h"
#include "config_manager.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QLabel>
#include <QGroupBox>

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
    , themeCombo_(nullptr)
    , fontSizeSpin_(nullptr)
    , bufferSizeSpin_(nullptr)
    , timestampCheck_(nullptr)
    , autoScrollCheck_(nullptr)
    , autoReconnectCheck_(nullptr)
    , ipcNameEdit_(nullptr)
{
    setWindowTitle("设置");
    setupUi();
}

void SettingsDialog::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    QGroupBox* displayGroup = new QGroupBox("显示", this);
    QFormLayout* displayLayout = new QFormLayout(displayGroup);

    themeCombo_ = new QComboBox(this);
    themeCombo_->addItem("深色");
    themeCombo_->addItem("浅色");
    displayLayout->addRow("主题:", themeCombo_);

    fontSizeSpin_ = new QSpinBox(this);
    fontSizeSpin_->setRange(8, 24);
    fontSizeSpin_->setValue(12);
    displayLayout->addRow("字体大小:", fontSizeSpin_);

    bufferSizeSpin_ = new QSpinBox(this);
    bufferSizeSpin_->setRange(1000, 100000);
    bufferSizeSpin_->setSingleStep(1000);
    bufferSizeSpin_->setValue(10000);
    displayLayout->addRow("缓冲行数:", bufferSizeSpin_);

    timestampCheck_ = new QCheckBox("显示时间戳", this);
    timestampCheck_->setChecked(true);
    displayLayout->addRow(timestampCheck_);

    autoScrollCheck_ = new QCheckBox("自动滚动", this);
    autoScrollCheck_->setChecked(true);
    displayLayout->addRow(autoScrollCheck_);

    mainLayout->addWidget(displayGroup);

    QGroupBox* connGroup = new QGroupBox("连接", this);
    QFormLayout* connLayout = new QFormLayout(connGroup);

    autoReconnectCheck_ = new QCheckBox("自动重连", this);
    autoReconnectCheck_->setChecked(true);
    connLayout->addRow(autoReconnectCheck_);

    ipcNameEdit_ = new QLineEdit("serial_monitor_ipc", this);
    connLayout->addRow("IPC 名称:", ipcNameEdit_);

    mainLayout->addWidget(connGroup);
    mainLayout->addStretch();

    QDialogButtonBox* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttons);
}

void SettingsDialog::loadFromConfig()
{
    auto& cfg = ConfigManager::instance().config();
    themeCombo_->setCurrentText(cfg.display.theme == "dark" ? "深色" : "浅色");
    fontSizeSpin_->setValue(cfg.display.fontSize);
    bufferSizeSpin_->setValue(cfg.display.bufferSize);
    timestampCheck_->setChecked(cfg.display.showTimestamp);
    autoScrollCheck_->setChecked(cfg.display.autoScroll);
    autoReconnectCheck_->setChecked(cfg.display.autoReconnect);
    ipcNameEdit_->setText(cfg.ipcName);
}

void SettingsDialog::saveToConfig()
{
    auto& cfg = ConfigManager::instance().config();
    cfg.display.theme = themeCombo_->currentText() == "深色" ? "dark" : "light";
    cfg.display.fontSize = fontSizeSpin_->value();
    cfg.display.bufferSize = bufferSizeSpin_->value();
    cfg.display.showTimestamp = timestampCheck_->isChecked();
    cfg.display.autoScroll = autoScrollCheck_->isChecked();
    cfg.display.autoReconnect = autoReconnectCheck_->isChecked();
    cfg.ipcName = ipcNameEdit_->text();
    ConfigManager::instance().save();
}
