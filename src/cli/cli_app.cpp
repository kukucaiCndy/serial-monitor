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
    QCoreApplication app(argc, argv);
    app_ = &app;
    app.setApplicationName("serial-monitor-cli");
    app.setApplicationVersion("2.0.0");

    QCommandLineParser parser;
    parser.setApplicationDescription("EmberIntel Serial Monitor - CLI Client");
    parser.addHelpOption();
    parser.addVersionOption();

    parser.addOption(QCommandLineOption("p", "Port name", "PORT"));
    parser.addOption(QCommandLineOption("b", "Baud rate (default: 115200)", "RATE", "115200"));
    parser.addOption(QCommandLineOption("f", "Filter keyword", "KEYWORD"));
    parser.addOption(QCommandLineOption("o", "Save logs to file", "FILE"));
    parser.addOption(QCommandLineOption("hex", "HEX display mode"));
    parser.addOption(QCommandLineOption("no-timestamp", "Disable timestamps"));
    parser.addOption(QCommandLineOption("clear", "Clear logs on start"));
    parser.addOption(QCommandLineOption("list", "List available serial ports"));
    parser.addOption(QCommandLineOption("cli", "Interactive CLI mode"));
    parser.addOption(QCommandLineOption("json", "JSON output mode"));
    parser.addOption(QCommandLineOption("ipc", "IPC server name", "NAME", "serial_monitor_ipc"));
    parser.addOption(QCommandLineOption("send", "Send data and exit", "DATA"));
    parser.addOption(QCommandLineOption("get-logs", "Get recent log count", "COUNT"));
    parser.addOption(QCommandLineOption("get-status", "Get connection status"));

    parser.process(app);

    jsonMode_ = parser.isSet("json");
    hexMode_ = parser.isSet("hex");
    showTimestamp_ = !parser.isSet("no-timestamp");
    filter_ = parser.value("f");
    outputFile_ = parser.value("o");
    ipcName_ = parser.value("ipc");

    if (!ipc_->connectToServer(ipcName_)) {
        QTextStream err(stderr);
        err << "Error: GUI service not running (IPC: " << ipcName_ << ")" << Qt::endl;
        err << "Please start serial-monitor.exe first" << Qt::endl;
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
        Q_UNUSED(parser);
    }

    else if (parser.isSet("cli")) {
        QString port = parser.value("p");
        if (port.isEmpty()) {
            QTextStream out(stdout);
            out << "Error: --cli requires --port" << Qt::endl;
            return 1;
        }
        QTimer::singleShot(0, [this, port]() { runInteractive(port); });
        return app.exec();
    }

    else if (!parser.value("p").isEmpty()) {
        interactiveMode_ = true;
        QTimer::singleShot(0, [this]() {
            QTextStream out(stdout);
            out << COLOR_GREEN << "[SYSTEM] Monitoring serial logs. Press Ctrl+C to stop."
                << COLOR_RESET << Qt::endl;
        });
        QObject::connect(&app, &QCoreApplication::aboutToQuit, [this]() {
            QTextStream out(stdout);
            out << COLOR_GRAY << "[SYSTEM] CLI disconnected (GUI service continues)"
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
    out << COLOR_CYAN << "  EmberIntel Serial Monitor CLI" << COLOR_RESET << Qt::endl;
    out << COLOR_GRAY << "  Port: " << port << COLOR_RESET << Qt::endl;
    out << COLOR_CYAN << QString(60, '=') << COLOR_RESET << Qt::endl;
    out << Qt::endl;
    out << COLOR_GREEN << "[SYSTEM] Type 'help' for commands, 'quit' to exit"
        << COLOR_RESET << Qt::endl << Qt::endl;

    QTextStream in(stdin);
    while (true) {
        out << "> " << flush;
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
        out << COLOR_GREEN << "[SYSTEM] Logs cleared" << COLOR_RESET << Qt::endl;
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
        out << COLOR_GREEN << "[SYSTEM] HEX mode on" << COLOR_RESET << Qt::endl;
        return;
    }
    if (cmd == "text") {
        hexMode_ = false;
        out << COLOR_GREEN << "[SYSTEM] Text mode on" << COLOR_RESET << Qt::endl;
        return;
    }
    if (cmd == "ts" || cmd == "timestamp") {
        showTimestamp_ = !showTimestamp_;
        out << COLOR_GREEN << "[SYSTEM] Timestamp: "
            << (showTimestamp_ ? "ON" : "OFF") << COLOR_RESET << Qt::endl;
        return;
    }
    if (cmd.startsWith("send ")) {
        QJsonObject params;
        params["data"] = cmd.mid(5);
        params["append"] = "CRLF";
        ipc_->sendCommand("send_text", params);
        out << COLOR_GREEN << ">>> Sent: " << cmd.mid(5) << COLOR_RESET << Qt::endl;
        return;
    }
    if (cmd.startsWith("sendhex ")) {
        QJsonObject params;
        params["data"] = cmd.mid(8);
        ipc_->sendCommand("send_hex", params);
        out << COLOR_GREEN << ">>> HEX Sent: " << cmd.mid(8) << COLOR_RESET << Qt::endl;
        return;
    }
    if (cmd.startsWith("filter ")) {
        QString kw = cmd.mid(7).trimmed();
        filter_ = kw;
        QJsonObject params;
        params["keyword"] = kw;
        ipc_->sendCommand("set_filter", params);
        out << COLOR_GREEN << "[SYSTEM] Filter: "
            << (kw.isEmpty() ? "none" : kw) << COLOR_RESET << Qt::endl;
        return;
    }
    if (cmd.startsWith("export ")) {
        QJsonObject params;
        params["file_path"] = cmd.mid(7).trimmed();
        params["format"] = "json";
        ipc_->sendCommand("export_logs", params);
        out << COLOR_GREEN << "[SYSTEM] Exporting..." << COLOR_RESET << Qt::endl;
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
    out << COLOR_GREEN << ">>> Sent: " << cmd << COLOR_RESET << Qt::endl;
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

    out << COLOR_GREEN << "[SYSTEM] " << port << " "
        << (connected ? "connected" : "disconnected") << COLOR_RESET << Qt::endl;
}

void CLIApp::onResponseReceived(const QString& id, bool success, const QJsonObject& data)
{
    Q_UNUSED(id);
    donePending();

    QTextStream out(stdout);

    if (data.contains("ports")) {
        QJsonArray ports = data["ports"].toArray();
        if (ports.isEmpty()) {
            out << COLOR_GRAY << "[SYSTEM] No serial ports found" << COLOR_RESET << Qt::endl;
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

        out << COLOR_GRAY << "[SYSTEM] Log buffer: " << total
            << " entries, showing " << returned << COLOR_RESET << Qt::endl;

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
        out << COLOR_GREEN << "[SYSTEM] Port: " << data["port"].toString()
            << " | Connected: " << (data["connected"].toBool() ? "yes" : "no")
            << " | Buffer: " << data["buffer_size"].toInt()
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
    out << "Commands:" << Qt::endl;
    out << "  q/quit/exit           - Quit CLI (GUI stays running)" << Qt::endl;
    out << "  c/clear               - Clear log buffer" << Qt::endl;
    out << "  s/status              - Show connection status" << Qt::endl;
    out << "  list                  - List serial ports" << Qt::endl;
    out << "  connect <port> [baud] - Connect serial port" << Qt::endl;
    out << "  disc/disconnect       - Disconnect serial port" << Qt::endl;
    out << "  send <data>           - Send text data" << Qt::endl;
    out << "  sendhex <hex>         - Send HEX data" << Qt::endl;
    out << "  filter <keyword>      - Set filter keyword" << Qt::endl;
    out << "  hex / text            - Toggle display mode" << Qt::endl;
    out << "  timestamp / ts        - Toggle timestamp" << Qt::endl;
    out << "  export <file>         - Export to JSON" << Qt::endl;
    out << "  help / ?              - Show this help" << Qt::endl;
    out << Qt::endl;
}

void CLIApp::printUsage() const
{
    QTextStream out(stdout);
    out << "Usage: serial-monitor-cli [options]" << Qt::endl;
    out << "  -p, --port PORT       Serial port name" << Qt::endl;
    out << "  --list                List serial ports" << Qt::endl;
    out << "  --get-status          Show connection status" << Qt::endl;
    out << "  --get-logs N          Get recent N log entries" << Qt::endl;
    out << "  --json                JSON output mode" << Qt::endl;
    out << "  --cli                 Interactive mode" << Qt::endl;
    out << "  --hex                 HEX display mode" << Qt::endl;
    out << "  -f, --filter KW       Filter keyword" << Qt::endl;
    out << "  -o, --output FILE     Save to file" << Qt::endl;
    out << "  --ipc NAME            IPC server name" << Qt::endl;
    out << "  -h, --help            Show help" << Qt::endl;
}