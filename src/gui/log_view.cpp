#include "log_view.h"
#include "log_parser.h"
#include <QContextMenuEvent>
#include <QMouseEvent>
#include <QMenu>
#include <QScrollBar>
#include <QTextCursor>
#include <QApplication>
#include <QClipboard>
#include <QMimeData>

static const int MAX_DISPLAY_LINES = 50000;

LogView::LogView(QWidget* parent)
    : QPlainTextEdit(parent)
    , hexMode_(false)
    , showTimestamp_(true)
    , autoScroll_(true)
    , userScrolledUp_(false)
    , programmaticScroll_(false)
{
    setReadOnly(true);
    setUndoRedoEnabled(false);
    setMaximumBlockCount(MAX_DISPLAY_LINES);
    setFont(QFont("Consolas", 11));
    setWordWrapMode(QTextOption::NoWrap);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setMinimumHeight(200);

    setStyleSheet(
        "QPlainTextEdit {"
        "  background-color: #0F172A;"
        "  color: #E0E0E0;"
        "  border: none;"
        "  selection-background-color: #1E40AF;"
        "  selection-color: #FFFFFF;"
        "}"
    );

    connect(verticalScrollBar(), &QScrollBar::valueChanged, [this](int value) {
        if (programmaticScroll_) return;
        if (value < verticalScrollBar()->maximum()) {
            userScrolledUp_ = true;
        } else {
            userScrolledUp_ = false;
        }
    });
}

void LogView::appendEntry(const LogEntry& entry)
{
    entries_.append(entry);

    if (!filter_.isEmpty() && !entry.text.contains(filter_, Qt::CaseInsensitive)) {
        return;
    }

    renderEntry(entry);

    if (autoScroll_ && !userScrolledUp_) {
        QScrollBar* vbar = verticalScrollBar();
        programmaticScroll_ = true;
        vbar->setValue(vbar->maximum());
        programmaticScroll_ = false;
    }
}

void LogView::setEntries(const QVector<LogEntry>& entries)
{
    entries_ = entries;
    rebuildAll();
}

void LogView::clearLog()
{
    entries_.clear();
    clear();
}

void LogView::setHexMode(bool enabled)
{
    if (hexMode_ != enabled) {
        hexMode_ = enabled;
        rebuildAll();
    }
}

void LogView::setShowTimestamp(bool enabled)
{
    if (showTimestamp_ != enabled) {
        showTimestamp_ = enabled;
        rebuildAll();
    }
}

void LogView::setAutoScroll(bool enabled)
{
    autoScroll_ = enabled;
    userScrolledUp_ = false;
    if (enabled) {
        QScrollBar* vbar = verticalScrollBar();
        programmaticScroll_ = true;
        vbar->setValue(vbar->maximum());
        programmaticScroll_ = false;
    }
}

void LogView::setFilter(const QString& keyword)
{
    filter_ = keyword;
    rebuildAll();
}

bool LogView::hexMode() const { return hexMode_; }
bool LogView::showTimestamp() const { return showTimestamp_; }
bool LogView::autoScroll() const { return autoScroll_; }
QString LogView::filter() const { return filter_; }

void LogView::scrollToBottom()
{
    QScrollBar* vbar = verticalScrollBar();
    vbar->setValue(vbar->maximum());
}

void LogView::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu* menu = createStandardContextMenu();
    menu->addSeparator();
    QAction* copyAllAction = menu->addAction("复制全部");
    connect(copyAllAction, &QAction::triggered, [this]() {
        QApplication::clipboard()->setText(toPlainText());
    });
    QAction* clearAction = menu->addAction("清空");
    connect(clearAction, &QAction::triggered, [this]() {
        clearLog();
    });
    menu->exec(event->globalPos());
    delete menu;
}

void LogView::mousePressEvent(QMouseEvent* event)
{
    QPlainTextEdit::mousePressEvent(event);
    if (event->button() == Qt::LeftButton) {
        userScrolledUp_ = true;
    }
}

void LogView::rebuildAll()
{
    clear();
    for (const auto& entry : entries_) {
        if (filter_.isEmpty() || entry.text.contains(filter_, Qt::CaseInsensitive)) {
            renderEntry(entry);
        }
    }
    if (autoScroll_ && !userScrolledUp_) {
        QScrollBar* vbar = verticalScrollBar();
        programmaticScroll_ = true;
        vbar->setValue(vbar->maximum());
        programmaticScroll_ = false;
    }
}

void LogView::renderEntry(const LogEntry& entry)
{
    QColor color = getLineColor(entry.level);
    QString htmlColor = colorToHtml(color);
    QString text = makeDisplayText(entry);

    QString html = QString("<span style=\"color:%1;\">%2</span>")
                       .arg(htmlColor, text.toHtmlEscaped());

    QTextCursor cursor = QTextCursor(document());
    cursor.movePosition(QTextCursor::End);
    cursor.insertHtml(html);
    cursor.insertBlock();
}

QString LogView::makeDisplayText(const LogEntry& entry) const
{
    if (hexMode_ && !entry.rawBytes.isEmpty()) {
        return LogParser::formatHex(entry.rawBytes, 0);
    }

    QString tsPart;
    if (showTimestamp_) {
        tsPart = QString("[%1] ").arg(entry.timestamp);
    }
    QString levelPart;
    if (!entry.level.isEmpty()) {
        levelPart = QString("[%1] ").arg(entry.level);
    }
    return tsPart + levelPart + entry.text;
}

QColor LogView::getLineColor(const QString& level) const
{
    if (level == "ERROR") return QColor("#FF6B6B");
    if (level == "WARN")  return QColor("#FFD93D");
    if (level == "INFO")  return QColor("#6BCB77");
    if (level == "DEBUG") return QColor("#4D96FF");
    if (level == "TRACE") return QColor("#9CA3AF");
    return QColor("#E0E0E0");
}

QString LogView::colorToHtml(const QColor& color) const
{
    return color.name();
}