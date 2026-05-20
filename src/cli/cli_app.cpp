#include "cli_app.h"
#include "ipc_protocol.h"
#include "log_parser.h"
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QTextStream>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QTimer>
#include <QRegularExpression>
#include <iostream>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[91m"
#define COLOR_GREEN   "\033[92m"
#define COLOR_YELLOW  "\033[93m"
#define COLOR_CYAN    "\033[96m"
#define COLOR_GRAY    "\033[90m"

static inline QString Utf8(const char* s) { return QString::fromUtf8(s); }

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
        out.setCodec("UTF-8");
        out << COLOR_RED << Utf8("[ERROR] ") << err << COLOR_RESET << Qt::endl;
    });
}

int CLIApp::run(int argc, char* argv[])
{
    spdlog::set_level(spdlog::level::off);

    QCoreApplication app(argc, argv);
    app_ = &app;
    app.setApplicationName("EmberInterDebugTool-cli");
    app.setApplicationVersion(APP_VERSION);

    QCommandLineParser parser;
    parser.setApplicationDescription("EmberInterDebugTool - 尘智串口调试工具 CLI");
    parser.addOption(QCommandLineOption(QStringList() << "h" << "help", "显示帮助信息"));
    parser.addOption(QCommandLineOption(QStringList() << "v" << "version", "显示版本信息"));

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
    parser.addOption(QCommandLineOption("send-hex", "发送HEX数据后退出", "HEX"));
    parser.addOption(QCommandLineOption("send-file", "发送二进制文件后退出", "FILE"));
    parser.addOption(QCommandLineOption("connect", "连接串口", "PORT"));
    parser.addOption(QCommandLineOption("baudrate", "--connect的波特率", "RATE", "115200"));
    parser.addOption(QCommandLineOption("get-logs", "获取最近N条日志", "COUNT"));
    parser.addOption(QCommandLineOption("get-status", "获取连接状态"));

    parser.process(app);

    if (parser.isSet("help") || parser.isSet("version")) {
        QTextStream out(stdout);
        out.setCodec("UTF-8");
        if (parser.isSet("help")) {
            out << parser.helpText() << Qt::endl;
        } else {
            out << app.applicationName() << " " << app.applicationVersion() << Qt::endl;
        }
        return 0;
    }

    jsonMode_ = parser.isSet("json");
    hexMode_ = parser.isSet("hex");
    showTimestamp_ = !parser.isSet("no-timestamp");
    filter_ = parser.value("f");
    outputFile_ = parser.value("o");
    ipcName_ = parser.value("ipc");

    bool needsIpc = parser.isSet("list") || parser.isSet("get-status") ||
                    parser.isSet("get-logs") || parser.isSet("send") ||
                    parser.isSet("send-hex") ||
                    parser.isSet("send-file") ||
                    parser.isSet("connect") || parser.isSet("cli") ||
                    !parser.value("p").isEmpty();

    if (!needsIpc) {
        printUsage();
        return 1;
    }

    if (!ipc_->connectToServer(ipcName_)) {
        QTextStream err(stderr);
        err.setCodec("UTF-8");
        err << Utf8("错误: GUI 服务未运行 (IPC: ") << ipcName_ << ")" << Qt::endl;
        err << Utf8("请先启动 GUI 程序") << Qt::endl;
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

    else if (parser.isSet("send-file")) {
        QString filePath = parser.value("send-file");
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            QTextStream err(stderr);
            err.setCodec("UTF-8");
            err << Utf8("错误: 无法打开文件 ") << filePath << Qt::endl;
            return 3;
        }
        QByteArray binaryData = file.readAll();
        file.close();
        QJsonObject params;
        params["data"] = QString::fromLatin1(binaryData.toBase64());
        params["port"] = parser.value("p");
        params["filename"] = QFileInfo(filePath).fileName();
        params["size"] = binaryData.size();
        QString reqId = nextReqId();
        addPending();
        ipc_->sendCommand("send_file", params, reqId);
    }

    else if (parser.isSet("send-hex")) {
        QString hexStr = parser.value("send-hex");
        hexStr.remove(QRegularExpression("\\s"));
        QJsonObject params;
        params["data"] = hexStr;
        params["port"] = parser.value("p");
        QString reqId = nextReqId();
        addPending();
        ipc_->sendCommand("send_hex", params, reqId);
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
            out.setCodec("UTF-8");
            out << Utf8("错误: --cli 需要 --port 参数") << Qt::endl;
            return 1;
        }
        QTimer::singleShot(0, [this, port]() { runInteractive(port); });
        return app.exec();
    }

    else if (!parser.value("p").isEmpty()) {
        interactiveMode_ = true;
        QTimer::singleShot(0, [this]() {
            QTextStream out(stdout);
            out.setCodec("UTF-8");
            out << COLOR_GREEN << Utf8("[系统] 正在监听串口日志，按 Ctrl+C 停止。")
                << COLOR_RESET << Qt::endl;
        });
        QObject::connect(&app, &QCoreApplication::aboutToQuit, [this]() {
            QTextStream out(stdout);
            out.setCodec("UTF-8");
            out << COLOR_GRAY << Utf8("[系统] CLI 已断开 (GUI 服务继续运行)")
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
    out.setCodec("UTF-8");
    out << COLOR_CYAN << QString(60, '=') << COLOR_RESET << Qt::endl;
    out << COLOR_CYAN << Utf8("  EmberInterDebugTool v" APP_VERSION " - 尘智 | 微尘藏星火,终端蕴尘智") << COLOR_RESET << Qt::endl;
    out << COLOR_GRAY << "  Port: " << port << COLOR_RESET << Qt::endl;
    out << COLOR_CYAN << QString(60, '=') << COLOR_RESET << Qt::endl;
    out << Qt::endl;
    out << COLOR_GREEN << Utf8("[系统] 输入 'help' 查看命令, 'quit' 退出")
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
    out.setCodec("UTF-8");

    if (cmd == "h" || cmd == "help" || cmd == "?") {
        printHelp();
        return;
    }
    if (cmd == "c" || cmd == "clear") {
        ipc_->sendCommand("clear_logs");
        out << COLOR_GREEN << Utf8("[系统] 日志已清空") << COLOR_RESET << Qt::endl;
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
        out << COLOR_GREEN << Utf8("[系统] HEX 模式已开启") << COLOR_RESET << Qt::endl;
        return;
    }
    if (cmd == "text") {
        hexMode_ = false;
        out << COLOR_GREEN << Utf8("[系统] 文本模式已开启") << COLOR_RESET << Qt::endl;
        return;
    }
    if (cmd == "ts" || cmd == "timestamp") {
        showTimestamp_ = !showTimestamp_;
        out << COLOR_GREEN << Utf8("[系统] 时间戳: ")
            << Utf8(showTimestamp_ ? "开" : "关") << COLOR_RESET << Qt::endl;
        return;
    }
    if (cmd.startsWith("send ")) {
        QJsonObject params;
        params["data"] = cmd.mid(5);
        params["append"] = "CRLF";
        ipc_->sendCommand("send_text", params);
        out << COLOR_GREEN << ">>> " << Utf8("发送: ") << cmd.mid(5) << COLOR_RESET << Qt::endl;
        return;
    }
    if (cmd.startsWith("sendhex ")) {
        QJsonObject params;
        params["data"] = cmd.mid(8);
        ipc_->sendCommand("send_hex", params);
        out << COLOR_GREEN << ">>> HEX " << Utf8("发送: ") << cmd.mid(8) << COLOR_RESET << Qt::endl;
        return;
    }
    if (cmd.startsWith("sendfile ")) {
        QString filePath = cmd.mid(9).trimmed();
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            out << COLOR_RED << Utf8("[错误] 无法打开文件: ") << filePath << COLOR_RESET << Qt::endl;
            return;
        }
        QByteArray binaryData = file.readAll();
        file.close();
        QJsonObject params;
        params["data"] = QString::fromLatin1(binaryData.toBase64());
        params["filename"] = QFileInfo(filePath).fileName();
        params["size"] = binaryData.size();
        ipc_->sendCommand("send_file", params);
        out << COLOR_GREEN << ">>> " << Utf8("发送文件: ") << QFileInfo(filePath).fileName()
            << " (" << binaryData.size() << Utf8(" 字节)") << COLOR_RESET << Qt::endl;
        return;
    }
    if (cmd.startsWith("filter ")) {
        QString kw = cmd.mid(7).trimmed();
        filter_ = kw;
        QJsonObject params;
        params["keyword"] = kw;
        ipc_->sendCommand("set_filter", params);
        out << COLOR_GREEN << Utf8("[系统] 过滤: ")
            << (kw.isEmpty() ? Utf8("无") : kw) << COLOR_RESET << Qt::endl;
        return;
    }
    if (cmd.startsWith("export ")) {
        QJsonObject params;
        params["file_path"] = cmd.mid(7).trimmed();
        params["format"] = "json";
        ipc_->sendCommand("export_logs", params);
        out << COLOR_GREEN << Utf8("[系统] 正在导出...") << COLOR_RESET << Qt::endl;
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
    if (cmd == "disconnect" || cmd == "disc") {
        ipc_->sendCommand("disconnect");
        out << COLOR_GREEN << Utf8("[系统] 正在断开...") << COLOR_RESET << Qt::endl;
        return;
    }

    QJsonObject params;
    params["data"] = cmd;
    params["append"] = "CRLF";
    ipc_->sendCommand("send_text", params);
    out << COLOR_GREEN << ">>> " << Utf8("发送: ") << cmd << COLOR_RESET << Qt::endl;
}

void CLIApp::onLogReceived(const QJsonObject& log)
{
    QTextStream out(stdout);
    out.setCodec("UTF-8");
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
    out.setCodec("UTF-8");
    QString port = status["port"].toString();
    bool connected = status["connected"].toBool();

    if (jsonMode_) {
        QJsonDocument doc(status);
        out << doc.toJson(QJsonDocument::Compact) << Qt::endl;
        return;
    }

    out << COLOR_GREEN << Utf8("[系统] ") << port << " "
        << Utf8(connected ? "已连接" : "已断开") << COLOR_RESET << Qt::endl;
}

void CLIApp::onResponseReceived(const QString& id, bool success, const QJsonObject& data)
{
    Q_UNUSED(id);
    donePending();

    QTextStream out(stdout);
    out.setCodec("UTF-8");

    if (data.contains("ports")) {
        QJsonArray ports = data["ports"].toArray();
        if (ports.isEmpty()) {
            out << COLOR_GRAY << Utf8("[系统] 未找到串口设备") << COLOR_RESET << Qt::endl;
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

        out << COLOR_GRAY << Utf8("[系统] 日志缓冲区: ") << total
            << Utf8(" 条, 显示 ") << returned << COLOR_RESET << Qt::endl;

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
        out << COLOR_GREEN << Utf8("[系统] 串口: ") << data["port"].toString()
            << Utf8(" | 连接: ") << Utf8(data["connected"].toBool() ? "是" : "否")
            << Utf8(" | 缓冲: ") << data["buffer_size"].toInt()
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
    out << "EmberInterDebugTool CLI v" APP_VERSION << Qt::endl;
    out << Qt::endl;
    out << Utf8("会话管理:") << Qt::endl;
    out << Utf8("  connect <port> [baud] - 连接串口") << Qt::endl;
    out << Utf8("  disconnect             - 断开当前串口") << Qt::endl;
    out << Utf8("  disc                   - 断开当前串口 (简写)") << Qt::endl;
    out << Utf8("  status                 - 显示连接状态") << Qt::endl;
    out << Utf8("  list                   - 列出可用串口") << Qt::endl;
    out << Qt::endl;
    out << Utf8("数据发送:") << Qt::endl;
    out << Utf8("  send <data>            - 发送文本数据 (自动追加CRLF)") << Qt::endl;
    out << Utf8("  sendhex <hex>          - 发送HEX数据") << Qt::endl;
    out << Utf8("  sendfile <file>        - 发送二进制文件") << Qt::endl;
    out << Qt::endl;
    out << Utf8("日志操作:") << Qt::endl;
    out << Utf8("  clear                  - 清空日志缓存") << Qt::endl;
    out << Utf8("  filter <keyword>       - 设置过滤关键词 (空=取消过滤)") << Qt::endl;
    out << Utf8("  hex / text             - 切换 HEX/文本 显示模式") << Qt::endl;
    out << Utf8("  timestamp / ts         - 切换时间戳显示") << Qt::endl;
    out << Utf8("  export <file>          - 导出日志为JSON文件") << Qt::endl;
    out << Qt::endl;
    out << Utf8("其他:") << Qt::endl;
    out << Utf8("  help / ?               - 显示此帮助") << Qt::endl;
    out << Utf8("  quit / q / exit        - 退出CLI (GUI继续运行)") << Qt::endl;
    out << Qt::endl;
    out << Utf8("提示: 未识别的命令将作为文本数据发送到串口") << Qt::endl;
    out << Qt::endl;
}

void CLIApp::printUsage() const
{
    QTextStream out(stdout);
    out << QString::fromUtf8("EmberInterDebugTool v" APP_VERSION " - 尘智 | 微尘藏星火,终端蕴尘智") << Qt::endl << Qt::endl;
    out << Utf8("用法: EmberInterDebugTool-cli [选项]") << Qt::endl << Qt::endl;
    out << Utf8("监听模式 (需先启动GUI):") << Qt::endl;
    out << Utf8("  -p, --port PORT       指定串口, 实时接收日志 (需先通过GUI连接该串口)") << Qt::endl;
    out << Utf8("  --cli                 交互CLI模式, 需配合 -p PORT 使用") << Qt::endl;
    out << Utf8("  -f, --filter KW       过滤关键词, 只显示包含关键词的日志") << Qt::endl;
    out << Utf8("  -o, --output FILE     同时保存日志到文件") << Qt::endl;
    out << Utf8("  --hex                 HEX显示模式") << Qt::endl;
    out << Utf8("  --no-timestamp        不显示时间戳") << Qt::endl;
    out << Utf8("  --json                JSON输出模式") << Qt::endl;
    out << Utf8("  --clear               启动时清空GUI日志缓冲") << Qt::endl;
    out << Utf8("  --ipc NAME            IPC服务名称 (默认: serial_monitor_ipc)") << Qt::endl;
    out << Qt::endl;
    out << Utf8("操作命令 (需先启动GUI):") << Qt::endl;
    out << Utf8("  --connect PORT        连接指定串口, 可用 --baudrate 指定波特率") << Qt::endl;
    out << Utf8("  --baudrate RATE       配合 --connect 使用, 设置波特率 (默认: 115200)") << Qt::endl;
    out << Utf8("  --send DATA           发送文本数据 (自动追加CRLF), 完成后退出") << Qt::endl;
    out << Utf8("  --send-hex HEX        发送HEX数据, 完成后退出") << Qt::endl;
    out << Utf8("  --send-file FILE      发送二进制文件 (Base64编码传输), 完成后退出") << Qt::endl;
    out << Utf8("  --list                列出可用串口设备") << Qt::endl;
    out << Utf8("  --get-status          显示当前连接状态") << Qt::endl;
    out << Utf8("  --get-logs N          获取最近N条日志") << Qt::endl;
    out << Qt::endl;
    out << Utf8("帮助:") << Qt::endl;
    out << Utf8("  -h, --help            显示此帮助信息") << Qt::endl;
    out << Utf8("  -v, --version         显示版本信息") << Qt::endl;
    out << Qt::endl;
    out << Utf8("示例:") << Qt::endl;
    out << Utf8("  EmberInterDebugTool-cli --list                            # 列出所有串口") << Qt::endl;
    out << Utf8("  EmberInterDebugTool-cli --connect COM3 --baudrate 9600    # 连接COM3") << Qt::endl;
    out << Utf8("  EmberInterDebugTool-cli -p COM3                           # 监听COM3日志") << Qt::endl;
    out << Utf8("  EmberInterDebugTool-cli -p COM3 --cli                     # 交互模式监听COM3") << Qt::endl;
    out << Utf8("  EmberInterDebugTool-cli --send \"AT+GMR\" -p COM3           # 发送命令并观察回复") << Qt::endl;
    out << Utf8("  EmberInterDebugTool-cli --send-hex \"FF AB 03\" -p COM3      # 发送HEX字节") << Qt::endl;
    out << Utf8("  EmberInterDebugTool-cli --send-file firmware.bin -p COM3    # 发送二进制固件") << Qt::endl;
    out << Utf8("  EmberInterDebugTool-cli --get-logs 50                     # 获取最近50条日志") << Qt::endl;
    out << Utf8("  EmberInterDebugTool-cli -p COM3 -f ERROR                  # 只显示含ERROR的日志") << Qt::endl;
    out << Utf8("  EmberInterDebugTool-cli -p COM3 -o debug.log              # 监听并保存到文件") << Qt::endl;
    out << Utf8("  EmberInterDebugTool-cli --get-status                      # 查看连接状态") << Qt::endl;
}