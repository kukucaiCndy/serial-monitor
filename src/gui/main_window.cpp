#include "main_window.h"
#include "config_manager.h"
#include "settings_dialog.h"
#include "serial_port_dialog.h"
#include "log_parser.h"
#include "log_exporter.h"
#include "ipc_protocol.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QCloseEvent>
#include <QFile>
#include <QPixmap>
#include <QApplication>
#include <QDateTime>
#include <QStyleFactory>
#include <QJsonArray>
#include <QSerialPortInfo>
#include <QRegularExpression>
#include <QMenu>
#include <spdlog/spdlog.h>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , splitter_(nullptr)
    , sidebar_(nullptr)
    , savedPortList_(nullptr)
    , tabWidget_(nullptr)
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
    , statusTimer_(new QTimer(this))
    , connectionTimerRunning_(false)
    , rxBytes_(0)
    , txBytes_(0)
{
    setWindowTitle("EmberInterDebugTool v" APP_VERSION " - 尘智 | 微尘藏星火,终端蕴尘智");
    setMinimumSize(900, 600);

    setupUi();
    setupConnections();
    loadTheme();
    loadConfig();
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

void MainWindow::onAbout()
{
    QMessageBox about(this);
    about.setWindowTitle("关于 EmberInterDebugTool");
    about.setIconPixmap(QPixmap(":/icons/logo.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    about.setText(
        "<h2>EmberInterDebugTool</h2>"
        "<p style=\"color:#94A3B8; font-size:15px;\">尘智</p>"
        "<p style=\"color:#64748B; font-size:12px;\">"
        "微尘藏星火，终端蕴尘智<br>"
        "以尘之微，铸智之核<br>"
        "赋能边缘嵌入式"
        "</p>"
        "<hr>"
        "<p>版本: " APP_VERSION "</p>"
    );
    about.exec();
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
    splitter_->setSizes({220, 860});

    setCentralWidget(splitter_);
    resize(1080, 720);
}

void MainWindow::setupSidebar(QWidget* sidebar)
{
    QVBoxLayout* layout = new QVBoxLayout(sidebar);
    layout->setContentsMargins(0, 8, 0, 8);
    layout->setSpacing(0);

    QWidget* brandWidget = new QWidget(sidebar);
    brandWidget->setObjectName("brandWidget");
    QHBoxLayout* brandLayout = new QHBoxLayout(brandWidget);
    brandLayout->setContentsMargins(12, 8, 12, 8);
    brandLayout->setSpacing(8);

    QLabel* logoLabel = new QLabel(brandWidget);
    QPixmap logoPix(":/icons/logo.png");
    logoLabel->setPixmap(logoPix.scaled(28, 28, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    logoLabel->setFixedSize(28, 28);
    brandLayout->addWidget(logoLabel);

    QLabel* enName = new QLabel("EmberInterDebugTool", brandWidget);
    enName->setStyleSheet("color: #22C55E; font-size: 11px; font-weight: 800;");
    brandLayout->addWidget(enName);

    QLabel* cnName = new QLabel("尘智", brandWidget);
    cnName->setStyleSheet("color: #94A3B8; font-size: 12px; font-weight: 600;");
    brandLayout->addWidget(cnName);

    brandLayout->addStretch();
    layout->addWidget(brandWidget);

    QWidget* sectionHeader = new QWidget(sidebar);
    sectionHeader->setObjectName("sectionHeader");
    QHBoxLayout* sectionLayout = new QHBoxLayout(sectionHeader);
    sectionLayout->setContentsMargins(0, 0, 12, 0);
    sectionLayout->setSpacing(0);

    QLabel* connTitle = new QLabel("我的会话", sidebar);
    connTitle->setObjectName("sectionTitle");
    sectionLayout->addWidget(connTitle, 1);

    QPushButton* addSessionBtn = new QPushButton("+", sidebar);
    addSessionBtn->setObjectName("addSessionBtn");
    addSessionBtn->setFixedSize(28, 28);
    addSessionBtn->setCursor(Qt::PointingHandCursor);
    addSessionBtn->setToolTip("新建会话");
    connect(addSessionBtn, &QPushButton::clicked, this, &MainWindow::onAddSavedPort);
    sectionLayout->addWidget(addSessionBtn);

    layout->addWidget(sectionHeader);

    savedPortList_ = new QListWidget(sidebar);
    savedPortList_->setObjectName("savedPortList");
    savedPortList_->setCursor(Qt::PointingHandCursor);
    savedPortList_->setContextMenuPolicy(Qt::CustomContextMenu);
    savedPortList_->setTextElideMode(Qt::ElideRight);
    connect(savedPortList_, &QListWidget::itemClicked, this, &MainWindow::onSavedPortClicked);
    connect(savedPortList_, &QListWidget::customContextMenuRequested, this, &MainWindow::onSavedPortContextMenu);
    layout->addWidget(savedPortList_, 1);

    layout->addSpacing(8);

    QHBoxLayout* bottomBtnLayout = new QHBoxLayout();
    bottomBtnLayout->setContentsMargins(12, 0, 12, 8);
    bottomBtnLayout->setSpacing(6);

    QPushButton* settingsBtn = new QPushButton("设置", sidebar);
    settingsBtn->setObjectName("bottomBtn");
    settingsBtn->setCursor(Qt::PointingHandCursor);
    connect(settingsBtn, &QPushButton::clicked, this, &MainWindow::onSettings);
    bottomBtnLayout->addWidget(settingsBtn);

    QPushButton* aboutBtn = new QPushButton("关于", sidebar);
    aboutBtn->setObjectName("bottomBtn");
    aboutBtn->setCursor(Qt::PointingHandCursor);
    connect(aboutBtn, &QPushButton::clicked, this, &MainWindow::onAbout);
    bottomBtnLayout->addWidget(aboutBtn);

    layout->addLayout(bottomBtnLayout);
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
    filterEdit_->setPlaceholderText("过滤日志...");
    filterEdit_->setClearButtonEnabled(true);
    tlLayout->addWidget(filterEdit_, 1);

    displayModeCombo_ = new QComboBox(toolbar);
    displayModeCombo_->addItems({"文本", "HEX"});
    tlLayout->addWidget(displayModeCombo_);

    pauseBtn_ = new QPushButton("暂停", toolbar);
    pauseBtn_->setCheckable(true);
    pauseBtn_->setCursor(Qt::PointingHandCursor);
    tlLayout->addWidget(pauseBtn_);

    clearBtn_ = new QPushButton("清空", toolbar);
    clearBtn_->setObjectName("clearBtn");
    clearBtn_->setCursor(Qt::PointingHandCursor);
    tlLayout->addWidget(clearBtn_);

    exportBtn_ = new QPushButton("导出", toolbar);
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
    connect(pauseBtn_, &QPushButton::toggled, this, &MainWindow::onPauseToggled);
    connect(clearBtn_, &QPushButton::clicked, this, &MainWindow::onClear);
    connect(exportBtn_, &QPushButton::clicked, this, &MainWindow::onExport);
    connect(filterEdit_, &QLineEdit::textChanged, this, &MainWindow::onFilterChanged);
    connect(displayModeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            [this](int idx) { logView_->setHexMode(idx == 1); });
    connect(tabWidget_, &SerialTabWidget::tabChanged, this, &MainWindow::onTabChanged);
    connect(tabWidget_, &SerialTabWidget::tabCloseRequested, this, &MainWindow::onTabClosed);

    connect(sendPanel_, &SendPanel::sendText, [this](const QString& data, const QString& append) {
        TabInfo* tab = tabWidget_->currentTabInfo();
        if (tab && tab->engine && tab->connected) {
            qint64 sent = tab->engine->sendText(data, append);
            if (sent > 0) {
                txBytes_ += sent;
                QString displayed = data + (append == "CRLF" ? "\r\n" : append == "LF" ? "\n" : "");
                LogEntry entry(QDateTime::currentDateTime().toString("HH:mm:ss.zzz"),
                               "TX", displayed.toUtf8(), displayed.toUtf8(), tab->port);
                logBuffer_->append(entry);
                logView_->appendEntry(entry);
                ipcServer_->broadcastLog(entry);
            }
        }
    });
    connect(sendPanel_, &SendPanel::sendHex, [this](const QByteArray& data) {
        TabInfo* tab = tabWidget_->currentTabInfo();
        if (tab && tab->engine && tab->connected) {
            qint64 sent = tab->engine->sendHex(data);
            if (sent > 0) {
                txBytes_ += sent;
                QString hexStr = QString(data.toHex(' ').toUpper());
                LogEntry entry(QDateTime::currentDateTime().toString("HH:mm:ss.zzz"),
                               "TX", hexStr, data, tab->port);
                logBuffer_->append(entry);
                logView_->appendEntry(entry);
                ipcServer_->broadcastLog(entry);
            }
        }
    });

    ipcServer_ = new IPCServer(this);
    ipcServer_->start();

    connect(statusTimer_, &QTimer::timeout, [this]() {
        qint64 uptime = connectionTimerRunning_ ? connectionTimer_.elapsed() / 1000 : 0;
        statusBar_->setStats(rxBytes_, txBytes_, uptime);
        statusBar_->setIpcClientCount(ipcServer_->clientCount());
    });
    statusTimer_->start(1000);

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
            logView_->clearLog();
            logBuffer_->clear();
            data["message"] = "日志已清空";
            ipcServer_->sendResponse(clientId, reqId, true, data);
        }
        else if (cmd == "set_filter") {
            QString kw = params["keyword"].toString();
            logView_->setFilter(kw);
            filterEdit_->setText(kw);
            data["keyword"] = kw;
            ipcServer_->sendResponse(clientId, reqId, true, data);
        }
        else if (cmd == "connect") {
            QString port = params["port"].toString();
            int baud = params["baudrate"].toInt(115200);
            if (port.isEmpty()) {
                data["message"] = "缺少串口号";
                ipcServer_->sendResponse(clientId, reqId, false, data);
                return;
            }

            TabInfo* tab = tabWidget_->currentTabInfo();
            if (tab && tab->connected && tab->port == port) {
                data["port"] = port;
                data["connected"] = true;
                data["message"] = QString("已连接 %1 @ %2").arg(port).arg(baud);
                ipcServer_->sendResponse(clientId, reqId, true, data);
                return;
            }

            if (tab && tab->connected) {
                disconnectCurrentPort();
            }

            for (int i = 0; i < savedPorts_.size(); ++i) {
                if (savedPorts_[i].port == port) {
                    PendingConnectRequest pending;
                    pending.clientId = clientId;
                    pending.requestId = reqId;
                    pending.port = port;
                    pending.baudrate = baud;
                    pendingConnects_[port] = pending;
                    connectSavedPort(i);
                    return;
                }
            }

            SavedPort sp;
            sp.port = port;
            sp.name = port;
            sp.baudrate = baud;
            savedPorts_.append(sp);
            QListWidgetItem* item = new QListWidgetItem(sp.summary(), savedPortList_);
            item->setData(Qt::UserRole, savedPorts_.size() - 1);
            savedPortList_->addItem(item);

            PendingConnectRequest pending;
            pending.clientId = clientId;
            pending.requestId = reqId;
            pending.port = port;
            pending.baudrate = baud;
            pendingConnects_[port] = pending;
            connectSavedPort(savedPorts_.size() - 1);
        }
        else if (cmd == "disconnect") {
            disconnectCurrentPort();
            data["message"] = "已断开";
            ipcServer_->sendResponse(clientId, reqId, true, data);
        }
        else if (cmd == "send_text") {
            QString text = params["data"].toString();
            QString append = params["append"].toString("CRLF");
            TabInfo* tab = tabWidget_->currentTabInfo();
            if (tab && tab->engine && tab->connected) {
                qint64 sent = tab->engine->sendText(text, append);
                if (sent > 0) {
                    txBytes_ += sent;
                    QString displayed = text + (append == "CRLF" ? "\r\n" : append == "LF" ? "\n" : "");
                    LogEntry entry(QDateTime::currentDateTime().toString("HH:mm:ss.zzz"),
                                   "TX", displayed.toUtf8(), displayed.toUtf8(), tab->port);
                    logBuffer_->append(entry);
                    logView_->appendEntry(entry);
                    ipcServer_->broadcastLog(entry);
                }
                data["bytes_sent"] = sent;
                data["message"] = QString("已发送 %1 字节").arg(sent);
                ipcServer_->sendResponse(clientId, reqId, sent > 0, data);
            } else {
                data["message"] = "没有活动连接";
                ipcServer_->sendResponse(clientId, reqId, false, data);
            }
        }
        else if (cmd == "send_hex") {
            QString hexStr = params["data"].toString();
            hexStr.remove(QRegularExpression("\\s"));
            if (hexStr.length() % 2 != 0) {
                data["message"] = "HEX数据格式无效";
                ipcServer_->sendResponse(clientId, reqId, false, data);
                return;
            }
            QByteArray bytes;
            for (int i = 0; i < hexStr.length(); i += 2) {
                bool ok;
                unsigned char byte = static_cast<unsigned char>(
                    hexStr.mid(i, 2).toUInt(&ok, 16));
                if (!ok) { data["message"] = "HEX数据无效"; ipcServer_->sendResponse(clientId, reqId, false, data); return; }
                bytes.append(static_cast<char>(byte));
            }
            TabInfo* tab = tabWidget_->currentTabInfo();
            if (tab && tab->engine && tab->connected) {
                qint64 sent = tab->engine->sendHex(bytes);
                if (sent > 0) {
                    txBytes_ += sent;
                    QString displayHex = QString(bytes.toHex(' ').toUpper());
                    LogEntry entry(QDateTime::currentDateTime().toString("HH:mm:ss.zzz"),
                                   "TX", displayHex, bytes, tab->port);
                    logBuffer_->append(entry);
                    logView_->appendEntry(entry);
                    ipcServer_->broadcastLog(entry);
                }
                data["bytes_sent"] = sent;
                data["message"] = QString("已发送 %1 字节 (HEX)").arg(sent);
                ipcServer_->sendResponse(clientId, reqId, sent > 0, data);
            } else {
                data["message"] = "没有活动连接";
                ipcServer_->sendResponse(clientId, reqId, false, data);
            }
        }
        else if (cmd == "send_file") {
            QString base64 = params["data"].toString();
            QByteArray bytes = QByteArray::fromBase64(base64.toLatin1());
            if (bytes.isEmpty()) {
                data["message"] = "二进制数据为空或无效";
                ipcServer_->sendResponse(clientId, reqId, false, data);
                return;
            }
            QString filename = params["filename"].toString();
            TabInfo* tab = tabWidget_->currentTabInfo();
            if (tab && tab->engine && tab->connected) {
                qint64 sent = tab->engine->sendRaw(bytes);
                if (sent > 0) {
                    txBytes_ += sent;
                    QString displayHex = QString(bytes.toHex(' ').toUpper());
                    QString label = filename.isEmpty()
                        ? QString("BIN (%1字节)").arg(bytes.size())
                        : QString("%1 (%2字节)").arg(filename).arg(bytes.size());
                    LogEntry entry(QDateTime::currentDateTime().toString("HH:mm:ss.zzz"),
                                   "TX", label + "\n" + displayHex, bytes, tab->port);
                    logBuffer_->append(entry);
                    logView_->appendEntry(entry);
                    ipcServer_->broadcastLog(entry);
                }
                data["bytes_sent"] = sent;
                data["filename"] = filename;
                data["file_size"] = bytes.size();
                data["message"] = QString("已发送文件 %1 (%2 字节)")
                    .arg(filename.isEmpty() ? "binary" : filename)
                    .arg(sent);
                ipcServer_->sendResponse(clientId, reqId, sent > 0, data);
            } else {
                data["message"] = "没有活动连接";
                ipcServer_->sendResponse(clientId, reqId, false, data);
            }
        }
        else if (cmd == "export_logs") {
            QString path = params["file_path"].toString();
            if (path.isEmpty()) {
                data["message"] = "缺少文件路径";
                ipcServer_->sendResponse(clientId, reqId, false, data);
                return;
            }
            TabInfo* tab = tabWidget_->currentTabInfo();
            bool ok = LogExporter::exportToJson(logBuffer_->getAll(),
                        tab ? tab->port : "", path);
            data["file_path"] = path;
            data["entry_count"] = logBuffer_->size();
            data["message"] = ok ? "导出成功" : "导出失败";
            ipcServer_->sendResponse(clientId, reqId, ok, data);
        }
        else if (cmd == "set_display_mode") {
            bool hex = params["hex"].toBool(false);
            logView_->setHexMode(hex);
            displayModeCombo_->setCurrentIndex(hex ? 1 : 0);
            data["hex_mode"] = hex;
            ipcServer_->sendResponse(clientId, reqId, true, data);
        }
        else if (cmd == "set_timestamp") {
            bool show = params["show"].toBool(true);
            logView_->setShowTimestamp(show);
            data["show_timestamp"] = show;
            ipcServer_->sendResponse(clientId, reqId, true, data);
        }
        else if (cmd == "pause_logs") {
            bool pause = params["pause"].toBool(true);
            logView_->setAutoScroll(!pause);
            pauseBtn_->setChecked(pause);
            data["paused"] = pause;
            ipcServer_->sendResponse(clientId, reqId, true, data);
        }
        else {
            data["message"] = "未知命令: " + cmd;
            ipcServer_->sendResponse(clientId, reqId, false, data);
        }
    });
}

void MainWindow::onAddSavedPort()
{
    SerialPortDialog dlg(this);

    if (dlg.exec() == QDialog::Accepted) {
        SavedPort sp = dlg.savedPort();
        if (sp.port.isEmpty()) return;

        sp.name = sp.name.isEmpty() ? sp.port : sp.name;
        savedPorts_.append(sp);

        QListWidgetItem* item = new QListWidgetItem(sp.summary(), savedPortList_);
        item->setData(Qt::UserRole, savedPorts_.size() - 1);
        savedPortList_->addItem(item);

        int tabIdx = tabWidget_->addSerialTab(sp.port, sp.name);
        tabWidget_->setCurrentIndex(tabIdx);

        connectSavedPort(savedPorts_.size() - 1);
        saveConfig();
    }
}

void MainWindow::onEditSavedPort()
{
    QListWidgetItem* item = savedPortList_->currentItem();
    if (!item) return;
    int idx = item->data(Qt::UserRole).toInt();
    if (idx < 0 || idx >= savedPorts_.size()) return;

    SerialPortDialog dlg(this);
    dlg.setSavedPort(savedPorts_[idx]);
    if (dlg.exec() == QDialog::Accepted) {
        savedPorts_[idx] = dlg.savedPort();
        item->setText(savedPorts_[idx].summary());
        saveConfig();
    }
}

void MainWindow::onDeleteSavedPort()
{
    QListWidgetItem* item = savedPortList_->currentItem();
    if (!item) return;
    int idx = item->data(Qt::UserRole).toInt();
    if (idx < 0 || idx >= savedPorts_.size()) return;

    if (QMessageBox::question(this, "确认", QString("确定删除串口 \"%1\"?").arg(savedPorts_[idx].summary()),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) return;

    TabInfo* tab = tabWidget_->currentTabInfo();
    if (tab && tab->port == savedPorts_[idx].port && tab->connected) {
        disconnectCurrentPort();
    }

    savedPorts_.removeAt(idx);
    delete item;

    for (int i = 0; i < savedPortList_->count(); ++i) {
        savedPortList_->item(i)->setData(Qt::UserRole, i);
    }

    saveConfig();
}

void MainWindow::onDisconnectAction()
{
    disconnectCurrentPort();
}

void MainWindow::onConnectAction()
{
    QListWidgetItem* item = savedPortList_->currentItem();
    if (!item) return;
    onSavedPortClicked(item);
}

void MainWindow::onSavedPortClicked(QListWidgetItem* item)
{
    if (!item) return;
    int idx = item->data(Qt::UserRole).toInt();
    if (idx < 0 || idx >= savedPorts_.size()) return;

    TabInfo* tab = tabWidget_->currentTabInfo();
    if (tab && tab->port == savedPorts_[idx].port && tab->connected) {
        return;
    }

    connectSavedPort(idx);
}

void MainWindow::onSavedPortContextMenu(const QPoint& pos)
{
    QListWidgetItem* item = savedPortList_->itemAt(pos);
    if (!item) return;
    savedPortList_->setCurrentItem(item);

    QMenu menu(this);

    int idx = item->data(Qt::UserRole).toInt();

    bool isConnected = false;
    for (TabInfo* t : tabWidget_->allTabs()) {
        if (t->port == savedPorts_[idx].port && t->connected) {
            isConnected = true;
            break;
        }
    }

    if (isConnected) {
        QAction* disconnectAction = menu.addAction("断开");
        connect(disconnectAction, &QAction::triggered, this, &MainWindow::onDisconnectAction);
    } else {
        QAction* connectAction = menu.addAction("连接");
        connect(connectAction, &QAction::triggered, this, &MainWindow::onConnectAction);
    }

    QAction* editAction = menu.addAction("编辑配置");
    connect(editAction, &QAction::triggered, this, &MainWindow::onEditSavedPort);
    QAction* deleteAction = menu.addAction("删除");
    connect(deleteAction, &QAction::triggered, this, &MainWindow::onDeleteSavedPort);
    menu.exec(savedPortList_->mapToGlobal(pos));
}

void MainWindow::connectSavedPort(int savedIdx)
{
    if (savedIdx < 0 || savedIdx >= savedPorts_.size()) return;
    const SavedPort& sp = savedPorts_[savedIdx];

    SavedPort spCopy = sp;

    TabInfo* tab = tabWidget_->currentTabInfo();
    if (!tab) {
        int newIdx = tabWidget_->addSerialTab("", "");
        tabWidget_->setCurrentIndex(newIdx);
        tab = tabWidget_->currentTabInfo();
        if (!tab) return;
    }

    if (tab->connected) {
        disconnectCurrentPort();
    }

    tab->port = spCopy.port;
    tab->engine = new SerialEngine(this);
    connect(tab->engine, &SerialEngine::dataReceived,
            this, &MainWindow::onSerialDataReceived);
    connect(tab->engine, &SerialEngine::statusChanged,
            this, &MainWindow::onSerialStatusChanged);
    connect(tab->engine, &SerialEngine::dataSent,
            [this](const QString&, qint64 bytes) { txBytes_ += bytes; });

    auto& cfg = ConfigManager::instance().config();
    tab->engine->setAutoReconnect(cfg.display.autoReconnect);
    tab->engine->open(spCopy.toSerialConfig());

    QString tabName = QString("%1 @ %2").arg(spCopy.port).arg(spCopy.baudrate);
    tabWidget_->updateTabName(tabWidget_->currentIndex(), tabName);
}

void MainWindow::disconnectCurrentPort()
{
    TabInfo* tab = tabWidget_->currentTabInfo();
    if (!tab || !tab->engine) return;

    tab->engine->close();
    tab->engine->deleteLater();
    tab->engine = nullptr;
    tab->connected = false;
    tab->port.clear();
    tabWidget_->setTabConnected(tabWidget_->currentIndex(), false);
    statusBar_->setConnectionStatus(false);
    connectionTimerRunning_ = false;
}

void MainWindow::onSettings()
{
    SettingsDialog dlg(this);
    dlg.loadFromConfig();
    if (dlg.exec() == QDialog::Accepted) {
        dlg.saveToConfig();
        auto& cfg = ConfigManager::instance().config();
        logView_->setShowTimestamp(cfg.display.showTimestamp);
        logView_->setAutoScroll(cfg.display.autoScroll);
        logBuffer_->setMaxSize(cfg.display.bufferSize);

        if (cfg.display.fontSize >= 8) {
            QFont f = logView_->font();
            f.setPointSize(cfg.display.fontSize);
            logView_->setFont(f);
        }

        for (const auto& t : tabWidget_->allTabs()) {
            if (t->engine) {
                t->engine->setAutoReconnect(cfg.display.autoReconnect);
            }
        }
    }
}

void MainWindow::onClear()
{
    logView_->clearLog();
    logBuffer_->clear();
}

void MainWindow::onExport()
{
    QString path = QFileDialog::getSaveFileName(this, "导出日志", "", "JSON文件 (*.json)");
    if (path.isEmpty()) return;

    TabInfo* tab = tabWidget_->currentTabInfo();
    LogExporter::exportToJson(logBuffer_->getAll(),
                              tab ? tab->port : QString(), path);
}

void MainWindow::onPauseToggled(bool paused)
{
    logView_->setAutoScroll(!paused);
    pauseBtn_->setText(paused ? "继续" : "暂停");
}

void MainWindow::onFilterChanged(const QString& text)
{
    logView_->setFilter(text);
}

void MainWindow::onTabChanged(int index)
{
    if (index < 0) {
        logView_->clearLog();
        logBuffer_->clear();
        statusBar_->setPortInfo("", 0);
        statusBar_->setConnectionStatus(false);
        return;
    }

    TabInfo* tab = tabWidget_->tabInfo(index);
    if (tab) {
        statusBar_->setPortInfo(tab->port, 0);
        statusBar_->setConnectionStatus(tab->connected);
    }
}

void MainWindow::onTabClosed(int index)
{
    TabInfo* tab = tabWidget_->tabInfo(index);
    if (tab && tab->engine && tab->connected) {
        tab->engine->close();
        tab->engine->deleteLater();
        tab->engine = nullptr;
        tab->connected = false;
        tab->port.clear();
        statusBar_->setConnectionStatus(false);
        connectionTimerRunning_ = false;
    }
    tabWidget_->removeTab(index);
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

    if (connected) {
        if (!connectionTimerRunning_) {
            connectionTimer_.start();
            connectionTimerRunning_ = true;
        }
    }

    statusBar_->setPortInfo(port, config.baudrate);
    statusBar_->setConnectionStatus(connected);
    statusBar_->setIpcClientCount(ipcServer_->clientCount());

    if (pendingConnects_.contains(port)) {
        auto pending = pendingConnects_.take(port);
        QJsonObject resp;
        resp["port"] = port;
        resp["connected"] = connected;
        resp["message"] = connected ?
            QString("已连接 %1 @ %2").arg(port).arg(config.baudrate) :
            QString("无法连接 %1").arg(port);
        ipcServer_->sendResponse(pending.clientId, pending.requestId,
                                 connected, resp);
    }

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

    savedPorts_ = cfg.savedPorts;
    for (int i = 0; i < savedPorts_.size(); ++i) {
        QListWidgetItem* item = new QListWidgetItem(savedPorts_[i].summary(), savedPortList_);
        item->setData(Qt::UserRole, i);
        savedPortList_->addItem(item);
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

    cfg.savedPorts = savedPorts_;
    ConfigManager::instance().save();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    ipcServer_->stop();
    saveConfig();
    event->accept();
}