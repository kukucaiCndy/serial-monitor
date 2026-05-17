#ifndef LOG_VIEW_H
#define LOG_VIEW_H

#include <QPlainTextEdit>
#include <QVector>
#include "log_buffer.h"

class LogView : public QPlainTextEdit {
    Q_OBJECT
public:
    explicit LogView(QWidget* parent = nullptr);

    void appendEntry(const LogEntry& entry);
    void setEntries(const QVector<LogEntry>& entries);
    void clearLog();
    void setHexMode(bool enabled);
    void setShowTimestamp(bool enabled);
    void setAutoScroll(bool enabled);
    void setFilter(const QString& keyword);

    bool hexMode() const;
    bool showTimestamp() const;
    bool autoScroll() const;
    QString filter() const;

public slots:
    void scrollToBottom();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    QVector<LogEntry> entries_;
    bool hexMode_;
    bool showTimestamp_;
    bool autoScroll_;
    bool userScrolledUp_;
    bool programmaticScroll_;
    QString filter_;

    void rebuildAll();
    void renderEntry(const LogEntry& entry);
    QString makeDisplayText(const LogEntry& entry) const;
    QColor getLineColor(const QString& level) const;
    QString colorToHtml(const QColor& color) const;
};

#endif