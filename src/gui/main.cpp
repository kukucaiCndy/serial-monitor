#include <QApplication>
#include <QIcon>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include "main_window.h"
#include "config_manager.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#endif

int main(int argc, char* argv[])
{
    auto fileLogger = spdlog::basic_logger_mt("app", "serial-monitor.log");
    spdlog::set_default_logger(fileLogger);
    spdlog::set_level(spdlog::level::info);
    spdlog::info("emberInter Serial Monitor GUI starting");

    QApplication app(argc, argv);
    app.setApplicationName("emberInter 尘智串口调试工具");
    app.setApplicationVersion("2.0.0");
    app.setOrganizationName("emberInter");
    app.setWindowIcon(QIcon(":/icons/logo.png"));

    ConfigManager::instance().load();

    MainWindow mainWindow;
    mainWindow.show();

#ifdef Q_OS_WIN
    BOOL useDarkMode = TRUE;
    DwmSetWindowAttribute(reinterpret_cast<HWND>(mainWindow.winId()),
                          DWMWA_USE_IMMERSIVE_DARK_MODE,
                          &useDarkMode, sizeof(useDarkMode));
#endif

    int result = app.exec();

    spdlog::info("emberInter Serial Monitor GUI exiting");
    return result;
}
