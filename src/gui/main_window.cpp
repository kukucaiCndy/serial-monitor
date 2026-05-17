#include "main_window.h"
#include "config_manager.h"
#include "settings_dialog.h"
#include "log_parser.h"
#include "log_exporter.h"
#include "ipc_protocol.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMenuBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QCloseEvent>
#include <QFile>
#include <QApplication>
#include <QStyleFactory>
#include <QJsonArray>
#include <QSerialPortInfo>
#include <spdlog/spdlog.h>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , splitter_(nullptr)
    , sidebar_(nullptr)
    , portList_(nullptr)
    , portCombo_(nullptr)
    , baudCombo_(nullptr)
    , paramsCombo_(nullptr)
    , connectBtn_(nullptr)
    , tabWidget_(nullptr)
    , addTabBtn_(nullptr)
    , logView_(nullptr)
    , filterEdit_(nullptr)
    , displayModeCombo_(nullptr)
    , pauseBtn_(nullptr)
    , clearBtn_(nullptr)
    , exportBtn_(nullptr)
    , sendPanel_(nullptr)
    , statusBar_(nullptr)
    , ipcServer_(nullptr)
    , logBuffer_(new LogBuffer(10000))
    , rxBytes_(0)
    , txBytes_(0)
{
    setWindowTitle("EmberIntel Serial Monitor");
    setMinimumSize(900, 600);

    setupUi();
    setupConnections();
    loadTheme();
    loadConfig();
    scanSerialPorts();
}

MainWindow::~MainWindow()
{
    saveConfig();
}

void MainWindow::loadTheme()
{
    QFile qss(":/styles/dark_theme.qss");
    if (qss.open(QFile::ReadOnly)) {
        QString style = QString::fromUtf8(qss.readAll());
        qApp->setStyleSheet(style);
        qss.close();
    }
}

void MainWindow::setupUi()
{
    splitter_ = new QSplitter(Qt::Horizontal, this);
    splitter_->setHandleWidth(1);
    splitter_->setChildrenCollapsible(false);

    sidebar_ = new QWidget(this);
    sidebar_->setObjectName("sidebar");
    setupSidebar(sidebar_);

    QWidget* mainArea = new QWidget(this);
    mainArea->setObjectName("mainArea");
    setupMainArea(mainArea);

    splitter_->addWidget(sidebar_);
    splitter_->addWidget(mainArea);
    splitter_->setStretchFactor(0, 0);
    splitter_->setStretchFactor(1, 1);
    splitter_->setSizes({260, 740});

    setCentralWidget(splitter_);
    resize(1080, 720);
}

void MainWindow::setupSidebar(QWidget* sidebar)
{
    QVBoxLayout* layout = new QVBoxLayout(sidebar);
    layout->setContentsMargins(0, 8, 0, 8);
    layout->setSpacing(0);

    QLabel* brandLabel = new QLabel("EMBERINTEL", sidebar);
    brandLabel->setStyleSheet("color: #22C55E; font-size: 11px; font-weight: 800; "
                              "letter-spacing: 3px; padding: 8px 20px 12px 20px;");
    layout->addWidget(brandLabel);

    QLabel* connTitle = new QLabel("CONNECTION", sidebar);
    connTitle->setObjectName("sectionTitle");
    layout->addWidget(connTitle);

    QWidget* connectPanel = new QWidget(sidebar);
    connectPanel->setObjectName("connectPanel");
    QVBoxLayout* cpLayout = new QVBoxLayout(connectPanel);
    cpLayout->setContentsMargins(0, 4, 0, 4);
    cpLayout->setSpacing(0);

    cpLayout->addWidget(new QLabel("Port", connectPanel));
    portCombo_ = new QComboBox(connectPanel);
    portCombo_->setEditable(true);
    portCombo_->setInsertPolicy(QComboBox::NoInsert);
    cpLayout->addWidget(portCombo_);

    cpLayout->addWidget(new QLabel("Baud Rate", connectPanel));
    baudCombo_ = new QComboBox(connectPanel);
    baudCombo_->setEditable(true);
    baudCombo_->addItems({"115200", "921600", "460800", "230400", "57600", "38400", "19200", "9600"});
    baudCombo_->setCurrentText("115200");
    cpLayout->addWidget(baudCombo_);

    cpLayout->addWidget(new QLabel("Config", connectPanel));
    paramsCombo_ = new QComboBox(connectPanel);
    paramsCombo_->setObjectName("connectParams");
    paramsCombo_->addItems({"8N1", "7E1", "7O1", "8N2"});
    cpLayout->addWidget(paramsCombo_);

    connectBtn_ = new QPushButton("CONNECT", connectPanel);
    connectBtn_->setObjectName("connectBtn");
    connectBtn_->setCursor(Qt::PointingHandCursor);
    cpLayout->addWidget(connectBtn_);

    layout->addWidget(connectPanel);

    layout->addSpacing(8);

    QLabel* portsTitle = new QLabel("SERIAL PORTS", sidebar);
    portsTitle->setObjectName("sectionTitle");
    layout->addWidget(portsTitle);

    portList_ = new QListWidget(sidebar);
    portList_->setObjectName("portList");
    portList_->setCursor(Qt::PointingHandCursor);
    layout->addWidget(portList_, 1);

    QPushButton* refreshBtn = new QPushButton("REFRESH", sidebar);
    refreshBtn->setObjectName("refreshBtn");
    refreshBtn->setCursor(Qt::PointingHandCursor);
    connect(refreshBtn, &QPushButton::clicked, this, &MainWindow::onRefreshPorts);
    layout->addWidget(refreshBtn);

    layout->addSpacing(8);

    QPushButton* settingsBtn = new QPushButton("SETTINGS", sidebar);
    settingsBtn->setObjectName("refreshBtn");
    settingsBtn->setCursor(Qt::PointingHandCursor);
    connect(settingsBtn, &QPushButton::clicked, this, &MainWindow::onSettings);
    layout->addWidget(settingsBtn);
}

void MainWindow::setupMainArea(QWidget* mainArea)
{
    QVBoxLayout* layout = new QVBoxLayout(mainArea);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    QWidget* tabBar = new QWidget(mainArea);
    QHBoxLayout* tbLayout = new QHBoxLayout(tabBar);
    tbLayout->setContentsMargins(8, 6, 8, 0);
    tbLayout->setSpacing(0);

    tabWidget_ = new SerialTabWidget(mainArea);
    tbLayout->addWidget(tabWidget_, 1);

    addTabBtn_ = new QPushButton("+", mainArea);
    addTabBtn_->setObjectName("addTabBtn");
    addTabBtn_->setFixedSize(32, 32);
    addTabBtn_->setCursor(Qt::PointingHandCursor);
    addTabBtn_->setToolTip("New Tab");
    tbLayout->addWidget(addTabBtn_);
    layout->addWidget(tabBar);

    logView_ = new LogView(mainArea);
    logView_->setObjectName("logView");
    layout->addWidget(logView_, 1);

    QWidget* toolbar = new QWidget(mainArea);
    toolbar->setObjectName("toolbar");
    QHBoxLayout* tlLayout = new QHBoxLayout(toolbar);
    tlLayout->setContentsMargins(8, 6, 8, 6);
    tlLayout->setSpacing(6);

    filterEdit_ = new QLineEdit(toolbar);
    filterEdit_->setPlaceholderText("Filter logs... (Ctrl+F)");
    filterEdit_->setClearButtonEnabled(true);
    tlLayout->addWidget(filterEdit_, 1);

    displayModeCombo_ = new QComboBox(toolbar);
    displayModeCombo_->addItems({"Text", "HEX"});
    tlLayout->addWidget(displayModeCombo_);

    pauseBtn_ = new QPushButton("Pause", toolbar);
    pauseBtn_->setCheckable(true);
    pauseBtn_->setCursor(Qt::PointingHandCursor);
    tlLayout->addWidget(pauseBtn_);

    clearBtn_ = new QPushButton("Clear", toolbar);
    clearBtn_->setObjectName("clearBtn");
    clearBtn_->setCursor(Qt::PointingHandCursor);
    tlLayout->addWidget(clearBtn_);

    exportBtn_ = new QPushButton("Export", toolbar);
    exportBtn_->setCursor(Qt::PointingHandCursor);
    tlLayout->addWidget(exportBtn_);

    layout->addWidget(toolbar);

    sendPanel_ = new SendPanel(mainArea);
    sendPanel_->setObjectName("sendPanel");
    layout->addWidget(sendPanel_);

    statusBar_ = new StatusBar(mainArea);
    statusBar_->setObjectName("statusBar");
    layout->addWidget(statusBar_);
}

void MainWindow::setupConnections()
{
    connect(connectBtn_, &QPushButton::clicked, this, &MainWindow::onConnectOrDisconnect);
    connect(pauseBtn_, &QPushButton::toggled, this, &MainWindow::onPauseToggled);
    connect(clearBtn_, &QPushButton::clicked, this, &MainWindow::onClear);
    connect(exportBtn_, &QPushButton::clicked, this, &MainWindow::onExport);
    connect(filterEdit_, &QLineEdit::textChanged, this, &MainWindow::onFilterChanged);
    connect(displayModeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            [this](int idx) { logView_->setHexMode(idx == 1); });
    connect(addTabBtn_, &QPushButton::clicked, this, &MainWindow::onAddTab);
    connect(portList_, &QListWidget::currentTextChanged, this, &MainWindow::onPortSelected);
    connect(tabWidget_, &SerialTabWidget::tabChanged, this, &MainWindow::onTabChanged);

    connect(sendPanel_, &SendPanel::sendText, [this](const QString& data, const QString& append) {
        TabInfo* tab = tabWidget_->currentTabInfo();
        if (tab && tab->engine && tab->connected) {
            qint64 sent = tab->engine->sendText(data, append);
            if (sent > 0) txBytes_ += sent;
        }
    });
    connect(sendPanel_, &SendPanel::sendHex, [this](const QByteArray& data) {
        TabInfo* tab = tabWidget_->currentTabInfo();
        if (tab && tab->engine && tab->connected) {
            qint64 sent = tab->engine->sendHex(data);
            if (sent > 0) txBytes_ += sent;
        }
    });

    ipcServer_ = new IPCServer(this);
    ipcServer_->start();

    connect(ipcServer_, &IPCServer::commandReceived,
            [this](const QString& clientId, const QString& cmd,
                   const QJsonObject& params, const QString& reqId) {
        QJsonObject data;

        if (cmd == "list_ports") {
            QJsonArray ports;
            for (const auto& info : QSerialPortInfo::availablePorts()) {
                QJsonObject p;
                p["name"] = info.portName();
                p["description"] = info.description();
                p["vid"] = QString::number(info.vendorIdentifier(), 16);
                p["pid"] = QString::number(info.productIdentifier(), 16);
                bool rec = info.description().contains("USB", Qt::CaseInsensitive)
                        || info.description().contains("JLink", Qt::CaseInsensitive)
                        || info.description().contains("nRF", Qt::CaseInsensitive)
                        || info.description().contains("CDC", Qt::CaseInsensitive);
                p["recommended"] = rec;
                ports.append(p);
            }
            data["ports"] = ports;
            ipcServer_->sendResponse(clientId, reqId, true, data);
        }
        else if (cmd == "get_status") {
            TabInfo* tab = tabWidget_->currentTabInfo();
            if (tab) {
                data["port"] = tab->port;
                data["connected"] = tab->connected;
                data["buffer_size"] = logBuffer_->size();
                data["rx_bytes"] = rxBytes_;
                data["tx_bytes"] = txBytes_;
            }
            ipcServer_->sendResponse(clientId, reqId, true, data);
        }
        else if (cmd == "get_logs") {
            int count = params["count"].toInt(100);
            QString filter = params["filter"].toString();
            QVector<LogEntry> entries;
            if (filter.isEmpty()) {
                entries = logBuffer_->getRecent(count);
            } else {
                entries = logBuffer_->getFiltered(filter, count);
            }
            QJsonArray arr;
            for (const auto& e : entries) {
                arr.append(IpcProtocol::buildLogEntryMessage(e));
            }
            data["total_count"] = logBuffer_->size();
            data["returned_count"] = entries.size();
            data["entries"] = arr;
            ipcServer_->sendResponse(clientId, reqId, true, data);
        }
        else if (cmd == "clear_logs") {
            logView_->clear();
            logBuffer_->clear();
            data["message"] = "Logs cleared";
            ipcServer_->sendResponse(clientId, reqId, true, data);
        }
        else if (cmd == "set_filter") {
            QString kw = params["keyword"].toString();
            logView_->setFilter(kw);
            data["keyword"] = kw;
            ipcServer_->sendResponse(clientId, reqId, true, data);
        }
        else {
            data["message"] = "Unknown command: " + cmd;
            ipcServer_->sendResponse(clientId, reqId, false, data);
        }
    });
}

void MainWindow::scanSerialPorts()
{
    portList_->clear();
    portCombo_->clear();

    QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
    for (const auto& info : ports) {
        QString desc = info.description().isEmpty() ? info.portName()
            : QString("%1 - %2").arg(info.portName(), info.description());

        portCombo_->addItem(info.portName());
        portList_->addItem(desc);

        bool recommended = info.description().contains("USB", Qt::CaseInsensitive)
                        || info.description().contains("JLink", Qt::CaseInsensitive)
                        || info.description().contains("nRF", Qt::CaseInsensitive)
                        || info.description().contains("CDC", Qt::CaseInsensitive);

        if (recommended) {
            QListWidgetItem* item = portList_->item(portList_->count() - 1);
            QFont f = item->font();
            f.setBold(true);
            item->setFont(f);
            item->setForeground(QColor("#22C55E"));
        }
    }

    if (ports.isEmpty()) {
        portList_->addItem("No serial ports found");
        portList_->item(0)->setFlags(Qt::NoItemFlags);
    }

    statusBar_->setIpcClientCount(ipcServer_->clientCount());
}

void MainWindow::onRefreshPorts()
{
    scanSerialPorts();
}

void MainWindow::onPortSelected(const QString& port)
{
    QString portName = port.split(" - ").first().trimmed();
    portCombo_->setCurrentText(portName);
}

void MainWindow::onConnectOrDisconnect()
{
    TabInfo* tab = tabWidget_->currentTabInfo();
    if (!tab) return;

    if (tab->connected) {
        if (tab->engine) {
            tab->engine->close();
            tab->engine->deleteLater();
            tab->engine = nullptr;
        }
        tab->connected = false;
        tab->port.clear();
        tabWidget_->setTabConnected(tabWidget_->currentIndex(), false);
        updateConnectButtonState(false);
        statusBar_->setConnectionStatus(false);
        return;
    }

    QString port = portCombo_->currentText().trimmed();
    if (port.isEmpty()) {
        QMessageBox::warning(this, "No Port", "Please select or enter a serial port.");
        return;
    }

    SerialConfig config;
    config.port = port;
    config.baudrate = baudCombo_->currentText().toInt();

    QString params = paramsCombo_->currentText();
    if (params.length() >= 3) {
        config.databits = params[0] == '7' ? QSerialPort::Data7 : QSerialPort::Data8;
        QChar p = params[1];
        if (p == 'E') config.parity = QSerialPort::EvenParity;
        else if (p == 'O') config.parity = QSerialPort::OddParity;
        else config.parity = QSerialPort::NoParity;
        config.stopbits = params[2] == '2' ? QSerialPort::TwoStop : QSerialPort::OneStop;
    }

    tab->port = port;
    tab->engine = new SerialEngine(this);
    connect(tab->engine, &SerialEngine::dataReceived,
            this, &MainWindow::onSerialDataReceived);
    connect(tab->engine, &SerialEngine::statusChanged,
            this, &MainWindow::onSerialStatusChanged);
    connect(tab->engine, &SerialEngine::dataSent,
            [this](const QString&, qint64 bytes) { txBytes_ += bytes; });

    tab->engine->open(config);

    QString tabName = QString("%1 @ %2").arg(port).arg(config.baudrate);
    tabWidget_->updateTabName(tabWidget_->currentIndex(), tabName);
}

void MainWindow::updateConnectButtonState(bool connected)
{
    if (connected) {
        connectBtn_->setText("DISCONNECT");
        connectBtn_->setObjectName("disconnectBtn");
    } else {
        connectBtn_->setText("CONNECT");
        connectBtn_->setObjectName("connectBtn");
    }

    connectBtn_->style()->unpolish(connectBtn_);
    connectBtn_->style()->polish(connectBtn_);
}

void MainWindow::onSettings()
{
    SettingsDialog dlg(this);
    dlg.loadFromConfig();
    if (dlg.exec() == QDialog::Accepted) {
        dlg.saveToConfig();
        auto& cfg = ConfigManager::instance().config();
        logView_->setShowTimestamp(cfg.display.showTimestamp);
        logBuffer_->setMaxSize(cfg.display.bufferSize);
    }
}

void MainWindow::onClear()
{
    logView_->clear();
    logBuffer_->clear();
}

void MainWindow::onExport()
{
    QString path = QFileDialog::getSaveFileName(this, "Export Logs", "", "JSON Files (*.json)");
    if (path.isEmpty()) return;

    TabInfo* tab = tabWidget_->currentTabInfo();
    LogExporter::exportToJson(logBuffer_->getAll(),
                              tab ? tab->port : QString(), path);
}

void MainWindow::onPauseToggled(bool paused)
{
    logView_->setAutoScroll(!paused);
    pauseBtn_->setText(paused ? "Resume" : "Pause");
}

void MainWindow::onFilterChanged(const QString& text)
{
    logView_->setFilter(text);
}

void MainWindow::onTabChanged(int index)
{
    TabInfo* tab = tabWidget_->tabInfo(index);
    if (tab) {
        updateConnectButtonState(tab->connected);
        if (!tab->port.isEmpty()) {
            portCombo_->setCurrentText(tab->port);
        }
    }
}

void MainWindow::onAddTab()
{
    int idx = tabWidget_->addSerialTab("", "");
    tabWidget_->setCurrentIndex(idx);
    updateConnectButtonState(false);
}

void MainWindow::onSerialDataReceived(const QByteArray& data, const QString& port)
{
    rxBytes_ += data.size();

    static QByteArray remainder;
    QStringList lines = LogParser::extractLines(data, remainder);

    for (const QString& line : lines) {
        LogEntry entry = LogParser::parseLine(line.toUtf8(), port);
        logBuffer_->append(entry);
        logView_->appendEntry(entry);
        ipcServer_->broadcastLog(entry);
    }

    TabInfo* tab = tabWidget_->currentTabInfo();
    if (tab && tab->connected) {
        statusBar_->setStats(rxBytes_, txBytes_, 0);
    }
}

void MainWindow::onSerialStatusChanged(const QString& port, bool connected,
                                        const SerialConfig& config)
{
    for (int i = 0; i < tabWidget_->count(); ++i) {
        TabInfo* tab = tabWidget_->tabInfo(i);
        if (tab && tab->port == port) {
            tab->connected = connected;
            tabWidget_->setTabConnected(i, connected);
            if (connected) {
                tabWidget_->updateTabName(i, QString("%1 @ %2").arg(port).arg(config.baudrate));
            }
        }
    }

    TabInfo* current = tabWidget_->currentTabInfo();
    if (current && current->port == port) {
        updateConnectButtonState(connected);
    }

    statusBar_->setPortInfo(port, config.baudrate);
    statusBar_->setConnectionStatus(connected);
    statusBar_->setIpcClientCount(ipcServer_->clientCount());

    ipcServer_->broadcastStatus(port, connected, config, rxBytes_, txBytes_, 0);

    if (!connected) {
        rxBytes_ = 0;
        txBytes_ = 0;
    }
}

void MainWindow::loadConfig()
{
    auto& cfg = ConfigManager::instance().config();
    resize(cfg.windowWidth, cfg.windowHeight);

    logView_->setShowTimestamp(cfg.display.showTimestamp);
    logView_->setHexMode(cfg.display.hexMode);
    logView_->setAutoScroll(cfg.display.autoScroll);
    logBuffer_->setMaxSize(cfg.display.bufferSize);

    if (cfg.display.fontSize >= 8) {
        QFont f = logView_->font();
        f.setPointSize(cfg.display.fontSize);
        logView_->setFont(f);
    }

    for (const auto& tab : cfg.tabs) {
        tabWidget_->addSerialTab(tab.port, tab.name);
    }
    if (tabWidget_->count() == 0) {
        tabWidget_->addSerialTab("", "");
    }
}

void MainWindow::saveConfig()
{
    auto& cfg = ConfigManager::instance().config();
    cfg.windowWidth = width();
    cfg.windowHeight = height();
    cfg.display.showTimestamp = logView_->showTimestamp();
    cfg.display.hexMode = logView_->hexMode();
    cfg.display.autoScroll = logView_->autoScroll();
    cfg.display.bufferSize = logBuffer_->maxSize();
    cfg.display.fontSize = logView_->font().pointSize();

    cfg.tabs.clear();
    for (const auto& t : tabWidget_->allTabs()) {
        TabConfig tc;
        tc.port = t->port;
        tc.name = t->name;
        cfg.tabs.append(tc);
    }

    ConfigManager::instance().save();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    ipcServer_->stop();
    saveConfig();
    event->accept();
}