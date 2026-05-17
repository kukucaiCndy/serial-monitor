#include "config_manager.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <spdlog/spdlog.h>

ConfigManager& ConfigManager::instance()
{
    static ConfigManager inst;
    return inst;
}

QString ConfigManager::defaultConfigPath() const
{
    QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(appData);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    return dir.filePath("config.json");
}

bool ConfigManager::load(const QString& path)
{
    if (!path.isEmpty()) {
        configPath_ = path;
    } else {
        configPath_ = defaultConfigPath();
    }

    QFile file(configPath_);
    if (!file.exists()) {
        spdlog::info("Config file not found at {}, using defaults", configPath_.toStdString());
        return false;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        spdlog::warn("Cannot open config file: {}", configPath_.toStdString());
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) {
        spdlog::warn("Invalid config JSON, using defaults");
        return false;
    }

    QJsonObject root = doc.object();

    config_.port = root["port"].toString();
    config_.baudrate = root["baudrate"].toInt(115200);
    config_.databits = root["databits"].toInt(8);
    config_.parity = root["parity"].toString("N");
    config_.stopbits = root["stopbits"].toInt(1);
    config_.flowcontrol = root["flowcontrol"].toString("N");

    QJsonObject display = root["display"].toObject();
    config_.display.theme = display["theme"].toString("dark");
    config_.display.showTimestamp = display["timestamp"].toBool(true);
    config_.display.hexMode = display["hex_mode"].toBool(false);
    config_.display.fontSize = display["font_size"].toInt(12);
    config_.display.bufferSize = display["buffer_size"].toInt(10000);
    config_.display.autoScroll = display["auto_scroll"].toBool(true);
    config_.display.autoReconnect = display["auto_reconnect"].toBool(true);

    QJsonObject filters = root["filters"].toObject();
    QJsonArray filterArr = filters["history"].toArray();
    for (const auto& val : filterArr) {
        config_.filterHistory.append(val.toString());
    }

    QJsonObject send = root["send"].toObject();
    QJsonArray sendArr = send["history"].toArray();
    for (const auto& val : sendArr) {
        config_.send.history.append(val.toString());
    }
    config_.send.append = send["append"].toString("CRLF");

    QJsonObject gui = root["gui"].toObject();
    config_.windowWidth = gui["window_width"].toInt(1000);
    config_.windowHeight = gui["window_height"].toInt(700);
    config_.ipcName = gui["ipc_name"].toString("serial_monitor_ipc");
    QJsonArray tabsArr = gui["tabs"].toArray();
    for (const auto& val : tabsArr) {
        QJsonObject tab = val.toObject();
        TabConfig tc;
        tc.port = tab["port"].toString();
        tc.name = tab["name"].toString();
        config_.tabs.append(tc);
    }

    spdlog::info("Config loaded from {}", configPath_.toStdString());
    return true;
}

bool ConfigManager::save(const QString& path)
{
    if (!path.isEmpty()) {
        configPath_ = path;
    } else if (configPath_.isEmpty()) {
        configPath_ = defaultConfigPath();
    }

    QJsonObject display;
    display["theme"] = config_.display.theme;
    display["timestamp"] = config_.display.showTimestamp;
    display["hex_mode"] = config_.display.hexMode;
    display["font_size"] = config_.display.fontSize;
    display["buffer_size"] = config_.display.bufferSize;
    display["auto_scroll"] = config_.display.autoScroll;
    display["auto_reconnect"] = config_.display.autoReconnect;

    QJsonObject filters;
    QJsonArray filterArr;
    for (const auto& f : config_.filterHistory) {
        filterArr.append(f);
    }
    filters["history"] = filterArr;

    QJsonObject send;
    QJsonArray sendArr;
    for (const auto& s : config_.send.history) {
        sendArr.append(s);
    }
    send["history"] = sendArr;
    send["append"] = config_.send.append;

    QJsonObject gui;
    gui["window_width"] = config_.windowWidth;
    gui["window_height"] = config_.windowHeight;
    gui["ipc_name"] = config_.ipcName;
    QJsonArray tabsArr;
    for (const auto& t : config_.tabs) {
        QJsonObject tab;
        tab["port"] = t.port;
        tab["name"] = t.name;
        tabsArr.append(tab);
    }
    gui["tabs"] = tabsArr;

    QJsonObject root;
    root["version"] = "2.0";
    root["port"] = config_.port;
    root["baudrate"] = config_.baudrate;
    root["databits"] = config_.databits;
    root["parity"] = config_.parity;
    root["stopbits"] = config_.stopbits;
    root["flowcontrol"] = config_.flowcontrol;
    root["display"] = display;
    root["filters"] = filters;
    root["send"] = send;
    root["gui"] = gui;

    QDir().mkpath(QFileInfo(configPath_).absolutePath());

    QFile file(configPath_);
    if (!file.open(QIODevice::WriteOnly)) {
        spdlog::error("Cannot write config file: {}", configPath_.toStdString());
        return false;
    }

    QJsonDocument doc(root);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    spdlog::info("Config saved to {}", configPath_.toStdString());
    return true;
}

AppConfig& ConfigManager::config()
{
    return config_;
}

QString ConfigManager::configPath() const
{
    return configPath_;
}

void ConfigManager::resetToDefault()
{
    config_ = AppConfig();
}
