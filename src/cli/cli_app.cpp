#include "cli_app.h"
#include "ipc_protocol.h"
#include "log_parser.h"
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QTextStream>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QTimer>
#include <iostream>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[91m"
#define COLOR_GREEN   "\033[92m"
#define COLOR_YELLOW  "\033[93m"
#define COLOR_CYAN    "\033[96m"
#define COLOR_GRAY    "\033[90m"

static void printColored(QTextStream& out, const QString& text, const QString& level)
{
    QString color;
    if (level == "ERROR")      color = COLOR_RED;
    else if (level == "WARN")  color = COLOR_YELLOW;
    else if (level == "INFO")  color = COLOR_GREEN;
    else if (level == "DEBUG") color = COLOR_CYAN;
    else if (level == "TRACE") color = COLOR_GRAY;
    else                       color = COLOR_RESET;
    out << color << text << COLOR_RESET << Qt::endl;
}

CLIApp::CLIApp(const QString& ipcName)
    : ipc_(new IPCClient(this))
    , ipcName_(ipcName)
    , jsonMode_(false)
    , interactiveMode_(false)
    , hexMode_(false)
    , showTimestamp_(true)
    , pendingRequestCount_(0)
    , shouldQuit_(false)
{
    connect(ipc_, &IPCClient::logReceived, this, &CLIApp::onLogReceived);
    connect(ipc_, &IPCClient::statusChanged, this, &CLIApp::onStatusChanged);
    connect(ipc_, &IPCClient::responseReceived,
            this, &CLIApp::onResponseReceived);
    connect(ipc_, &IPCClient::errorOccurred, [](const QString& err) {
        QTextStream out(stdout);
        out << COLOR_RED << "[ERROR] " << err << COLOR_RESET << Qt::endl;
    });
}

int CLIApp::run(int argc, char* argv[])
{
    spdlog::set_level(spdlog::level::off);

    QCoreApplication app(argc, argv);
    app_ = &app;
    app.setApplicationName("emberInter-cli");
    app.setApplicationVersion("2.0.0");

    QCommandLineParser parser;
    parser.setApplicationDescription("emberInter 尘智串口调试工具 - CLI Client");
    parser.addHelpOption();
    parser.addVersionOption();

    parser.addOption(QCommandLineOption("p", "串口名称", "PORT"));
    parser.addOption(QCommandLineOption("b", "波特率 (默认: 115200)", "RATE", "115200"));
    parser.addOption(QCommandLineOption("f", "过滤关键词", "KEYWORD"));
    parser.addOption(QCommandLineOption("o", "保存日志到文件", "FILE"));
    parser.addOption(QCommandLineOption("hex", "HEX显示模式"));
    parser.addOption(QCommandLineOption("no-timestamp", "不显示时间戳"));
    parser.addOption(QCommandLineOption("clear", "启动时清空日志"));
    parser.addOption(QCommandLineOption("list", "列出可用串口"));
    parser.addOption(QCommandLineOption("cli", "交互CLI模式"));
    parser.addOption(QCommandLineOption("json", "JSON输出模式"));
    parser.addOption(QCommandLineOption("ipc", "IPC服务名", "NAME", "serial_monitor_ipc"));
    parser.addOption(QCommandLineOption("send", "发送数据后退出", "DATA"));
    parser.addOption(QCommandLineOption("connect", "连接串口", "PORT"));
    parser.addOption(QCommandLineOption("baudrate", "--connect的波特率", "RATE", "115200"));
    parser.addOption(QCommandLineOption("get-logs", "获取最近N条日志", "COUNT"));
    parser.addOption(QCommandLineOption("get-status", "获取连接状态"));

    parser.process(app);

    jsonMode_ = parser.isSet("json");
    hexMode_ = parser.isSet("hex");
    showTimestamp_ = !parser.isSet("no-timestamp");
    filter_ = parser.value("f");
    outputFile_ = parser.value("o");
    ipcName_ = parser.value("ipc");

    if (!ipc_->connectToServer(ipcName_)) {
        QTextStream err(stderr);
        err << "错误: GUI 服务未运行 (IPC: " << ipcName_ << ")" << Qt::endl;
        err << "请先启动 GUI 程序" << Qt::endl;
        return 2;
    }

    QTimer::singleShot(500, [this]() {
        if (pendingRequestCount_ == 0 && !interactiveMode_) {
            shouldQuit_ = true;
            QCoreApplication::quit();
        }
    });

    if (parser.isSet("list")) {
        QJsonObject params;
        QString reqId = nextReqId();
        addPending();
        ipc_->sendCommand("list_ports", params, reqId);
    }

    else if (parser.isSet("get-status")) {
        QJsonObject params;
        QString reqId = nextReqId();
        addPending();
        ipc_->sendCommand("get_status", params, reqId);
    }

    else if (parser.isSet("get-logs")) {
        int count = parser.value("get-logs").toInt();
        QJsonObject params;
        params["count"] = count;
        params["filter"] = parser.value("f");
        QString reqId = nextReqId();
        addPending();
        ipc_->sendCommand("get_logs", params, reqId);
    }

    else if (parser.isSet("send")) {
        QString data = parser.value("send");
        QJsonObject params;
        params["data"] = data;
        params["port"] = parser.value("p");
        params["append"] = "CRLF";
        QString reqId = nextReqId();
        addPending();
        ipc_->sendCommand("send_text", params, reqId);
    }

    else if (parser.isSet("connect")) {
        QString port = parser.value("connect");
        int baud = parser.value("baudrate").toInt();
        QJsonObject params;
        params["port"] = port;
        params["baudrate"] = baud;
        QString reqId = nextReqId();
        addPending();
        ipc_->sendCommand("connect", params, reqId);
    }

    else if (parser.isSet("cli")) {
        QString port = parser.value("p");
        if (port.isEmpty()) {
            QTextStream out(stdout);
            out << "错误: --cli 需要 --port 参数" << Qt::endl;
            return 1;
        }
        QTimer::singleShot(0, [this, port]() { runInteractive(port); });
        return app.exec();
    }

    else if (!parser.value("p").isEmpty()) {
        interactiveMode_ = true;
        QTimer::singleShot(0, [this]() {
            QTextStream out(stdout);
            out << COLOR_GREEN << "[系统] 正在监听串口日志，按 Ctrl+C 停止。"
                << COLOR_RESET << Qt::endl;
        });
        QObject::connect(&app, &QCoreApplication::aboutToQuit, [this]() {
            QTextStream out(stdout);
            out << COLOR_GRAY << "[系统] CLI 已断开 (GUI 服务继续运行)"
                << COLOR_RESET << Qt::endl;
        });
        return app.exec();
    }

    else {
        printUsage();
        return 1;
    }

    int result = app.exec();
    return result;
}

void CLIApp::addPending()
{
    pendingRequestCount_++;
}

void CLIApp::donePending()
{
    pendingRequestCount_--;
    if (pendingRequestCount_ <= 0 && shouldQuit_ && app_) {
        QTimer::singleShot(0, app_, &QCoreApplication::quit);
    }
}

QString CLIApp::nextReqId()
{
    return QString("req_%1").arg(++reqCounter_);
}

int CLIApp::runInteractive(const QString& port)
{
    interactiveMode_ = true;

    QTextStream out(stdout);
    out << COLOR_CYAN << QString(60, '=') << COLOR_RESET << Qt::endl;
    out << COLOR_CYAN << "  emberInter 尘智串口调试工具 CLI" << COLOR_RESET << Qt::endl;
    out << COLOR_GRAY << "  Port: " << port << COLOR_RESET << Qt::endl;
    out << COLOR_CYAN << QString(60, '=') << COLOR_RESET << Qt::endl;
    out << Qt::endl;
    out << COLOR_GREEN << "[系统] 输入 'help' 查看命令, 'quit' 退出"
        << COLOR_RESET << Qt::endl << Qt::endl;

    QTextStream in(stdin);
    while (true) {
        out << "> " << Qt::flush;
        QString cmd = in.readLine().trimmed();
        if (cmd.isEmpty()) continue;
        if (cmd == "q" || cmd == "quit" || cmd == "exit") break;
        handleCommand(cmd);
    }

    out << COLOR_GRAY << "[SYSTEM] CLI disconnected (GUI service continues)"
        << COLOR_RESET << Qt::endl;
    return 0;
}

void CLIApp::handleCommand(const QString& cmd)
{
    QTextStream out(stdout);

    if (cmd == "h" || cmd == "help" || cmd == "?") {
        printHelp();
        return;
    }
    if (cmd == "c" || cmd == "clear") {
        ipc_->sendCommand("clear_logs");
        out << COLOR_GREEN << "[系统] 日志已清空" << COLOR_RESET << Qt::endl;
        return;
    }
    if (cmd == "s" || cmd == "status") {
        ipc_->sendCommand("get_status", QJsonObject(), nextReqId());
        return;
    }
    if (cmd == "list" || cmd == "ls") {
        ipc_->sendCommand("list_ports", QJsonObject(), nextReqId());
        return;
    }
    if (cmd == "hex") {
        hexMode_ = true;
        out << COLOR_GREEN << "[系统] HEX 模式已开启" << COLOR_RESET << Qt::endl;
        return;
    }
    if (cmd == "text") {
        hexMode_ = false;
        out << COLOR_GREEN << "[系统] 文本模式已开启" << COLOR_RESET << Qt::endl;
        return;
    }
    if (cmd == "ts" || cmd == "timestamp") {
        showTimestamp_ = !showTimestamp_;
        out << COLOR_GREEN << "[系统] 时间戳: "
            << (showTimestamp_ ? "开" : "关") << COLOR_RESET << Qt::endl;
        return;
    }
    if (cmd.startsWith("send ")) {
        QJsonObject params;
        params["data"] = cmd.mid(5);
        params["append"] = "CRLF";
        ipc_->sendCommand("send_text", params);
        out << COLOR_GREEN << ">>> 发送: " << cmd.mid(5) << COLOR_RESET << Qt::endl;
        return;
    }
    if (cmd.startsWith("sendhex ")) {
        QJsonObject params;
        params["data"] = cmd.mid(8);
        ipc_->sendCommand("send_hex", params);
        out << COLOR_GREEN << ">>> HEX 发送: " << cmd.mid(8) << COLOR_RESET << Qt::endl;
        return;
    }
    if (cmd.startsWith("filter ")) {
        QString kw = cmd.mid(7).trimmed();
        filter_ = kw;
        QJsonObject params;
        params["keyword"] = kw;
        ipc_->sendCommand("set_filter", params);
        out << COLOR_GREEN << "[系统] 过滤: "
            << (kw.isEmpty() ? "无" : kw) << COLOR_RESET << Qt::endl;
        return;
    }
    if (cmd.startsWith("export ")) {
        QJsonObject params;
        params["file_path"] = cmd.mid(7).trimmed();
        params["format"] = "json";
        ipc_->sendCommand("export_logs", params);
        out << COLOR_GREEN << "[系统] 正在导出..." << COLOR_RESET << Qt::endl;
        return;
    }
    if (cmd.startsWith("connect ")) {
        QStringList parts = cmd.mid(8).split(' ', Qt::SkipEmptyParts);
        QJsonObject params;
        params["port"] = parts.value(0);
        params["baudrate"] = parts.value(1, "115200").toInt();
        ipc_->sendCommand("connect", params);
        return;
    }

    QJsonObject params;
    params["data"] = cmd;
    params["append"] = "CRLF";
    ipc_->sendCommand("send_text", params);
    out << COLOR_GREEN << ">>> 发送: " << cmd << COLOR_RESET << Qt::endl;
}

void CLIApp::onLogReceived(const QJsonObject& log)
{
    QTextStream out(stdout);
    QString text = log["text"].toString();
    QString level = log["level"].toString();

    if (!filter_.isEmpty() && !text.contains(filter_, Qt::CaseInsensitive)) {
        return;
    }

    if (jsonMode_) {
        QJsonDocument doc(log);
        out << doc.toJson(QJsonDocument::Compact) << Qt::endl;
        return;
    }

    if (hexMode_) {
        QByteArray raw = QByteArray::fromHex(log["raw_hex"].toString().toLatin1());
        if (!raw.isEmpty()) {
            out << LogParser::formatHex(raw, 0) << Qt::endl;
        }
        return;
    }

    LogEntry entry(log["timestamp"].toString(), level, text,
                   QByteArray(), log["port"].toString());
    QString display = LogParser::formatDisplay(entry, showTimestamp_);
    printColored(out, display, level);

    if (!outputFile_.isEmpty()) {
        QFile f(outputFile_);
        if (f.open(QIODevice::Append)) {
            f.write(display.toUtf8() + "\n");
            f.close();
        }
    }
}

void CLIApp::onStatusChanged(const QJsonObject& status)
{
    QTextStream out(stdout);
    QString port = status["port"].toString();
    bool connected = status["connected"].toBool();

    if (jsonMode_) {
        QJsonDocument doc(status);
        out << doc.toJson(QJsonDocument::Compact) << Qt::endl;
        return;
    }

    out << COLOR_GREEN << "[系统] " << port << " "
        << (connected ? "已连接" : "已断开") << COLOR_RESET << Qt::endl;
}

void CLIApp::onResponseReceived(const QString& id, bool success, const QJsonObject& data)
{
    Q_UNUSED(id);
    donePending();

    QTextStream out(stdout);

    if (data.contains("ports")) {
        QJsonArray ports = data["ports"].toArray();
        if (ports.isEmpty()) {
            out << COLOR_GRAY << "[系统] 未找到串口设备" << COLOR_RESET << Qt::endl;
        } else {
            for (const auto& val : ports) {
                QJsonObject p = val.toObject();
                QString marker = p["recommended"].toBool() ? " *" : "  ";
                out << COLOR_GREEN << marker << COLOR_RESET
                    << p["name"].toString().leftJustified(8)
                    << COLOR_GRAY << p["description"].toString()
                    << COLOR_RESET << Qt::endl;
            }
        }
        return;
    }

    if (data.contains("total_count")) {
        int total = data["total_count"].toInt();
        int returned = data["returned_count"].toInt();
        QJsonArray entries = data["entries"].toArray();

        out << COLOR_GRAY << "[系统] 日志缓冲区: " << total
            << " 条, 显示 " << returned << COLOR_RESET << Qt::endl;

        for (const auto& val : entries) {
            QJsonObject entry = val.toObject();
            QString timestamp = entry["timestamp"].toString();
            QString level = entry["level"].toString();
            QString text = entry["text"].toString();

            if (jsonMode_) {
                QJsonDocument doc(entry);
                out << doc.toJson(QJsonDocument::Compact) << Qt::endl;
            } else {
                LogEntry e(timestamp, level, text, QByteArray(), entry["port"].toString());
                QString display = LogParser::formatDisplay(e, showTimestamp_);
                printColored(out, display, level);
            }
        }
        return;
    }

    if (data.contains("port") && !data.contains("entries")) {
        out << COLOR_GREEN << "[系统] 串口: " << data["port"].toString()
            << " | 连接: " << (data["connected"].toBool() ? "是" : "否")
            << " | 缓冲: " << data["buffer_size"].toInt()
            << " | RX: " << data["rx_bytes"].toDouble()
            << " | TX: " << data["tx_bytes"].toDouble()
            << COLOR_RESET << Qt::endl;
        return;
    }

    out << (success ? COLOR_GREEN : COLOR_RED)
        << "[" << (success ? "OK" : "FAIL") << "] "
        << data["message"].toString()
        << COLOR_RESET << Qt::endl;
}

void CLIApp::printHelp() const
{
    QTextStream out(stdout);
    out << Qt::endl;
    out << "命令:" << Qt::endl;
    out << "  q/quit/exit           - 退出CLI (GUI继续运行)" << Qt::endl;
    out << "  c/clear               - 清空日志缓存" << Qt::endl;
    out << "  s/status              - 显示连接状态" << Qt::endl;
    out << "  list                  - 列出可用串口" << Qt::endl;
    out << "  connect <port> [baud] - 连接串口" << Qt::endl;
    out << "  disc/disconnect       - 断开串口" << Qt::endl;
    out << "  send <data>           - 发送文本数据" << Qt::endl;
    out << "  sendhex <hex>         - 发送HEX数据" << Qt::endl;
    out << "  filter <keyword>      - 设置过滤关键词" << Qt::endl;
    out << "  hex / text            - 切换显示模式" << Qt::endl;
    out << "  timestamp / ts        - 切换时间戳" << Qt::endl;
    out << "  export <file>         - 导出JSON日志" << Qt::endl;
    out << "  help / ?              - 显示帮助" << Qt::endl;
    out << Qt::endl;
}

void CLIApp::printUsage() const
{
    QTextStream out(stdout);
    out << "用法: emberInter-cli [选项]" << Qt::endl;
    out << "  -p, --port PORT       串口名称" << Qt::endl;
    out << "  --list                列出可用串口" << Qt::endl;
    out << "  --get-status          显示连接状态" << Qt::endl;
    out << "  --get-logs N          获取最近N条日志" << Qt::endl;
    out << "  --json                JSON输出模式" << Qt::endl;
    out << "  --cli                 交互模式" << Qt::endl;
    out << "  --hex                 HEX显示模式" << Qt::endl;
    out << "  -f, --filter KW       过滤关键词" << Qt::endl;
    out << "  -o, --output FILE     保存到文件" << Qt::endl;
    out << "  --ipc NAME            IPC服务名称" << Qt::endl;
    out << "  -h, --help            显示帮助" << Qt::endl;
}