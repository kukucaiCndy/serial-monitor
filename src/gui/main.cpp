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
    spdlog::info("EmberInterDebugTool GUI starting");

    QApplication app(argc, argv);
    app.setApplicationName("EmberInterDebugTool");
    app.setApplicationVersion(APP_VERSION);
    app.setOrganizationName("EmberInter");
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

    spdlog::info("EmberInterDebugTool GUI exiting");
    return result;
}
