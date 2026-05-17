#!/bin/bash
# emberInter 尘智串口调试工具 - 部署打包脚本
set -e

PROJECT_DIR="/f/work/software/serial-monitor"
BUILD_DIR="$PROJECT_DIR/build"
DEPLOY_DIR="$PROJECT_DIR/deploy/emberInter"
DIST_DIR="$PROJECT_DIR/dist"

MINGW_DIR="/mingw64"

echo "=== 创建部署目录结构 ==="
rm -rf "$DEPLOY_DIR" "$DIST_DIR"
mkdir -p "$DEPLOY_DIR"
mkdir -p "$DEPLOY_DIR/platforms"
mkdir -p "$DEPLOY_DIR/styles"
mkdir -p "$DEPLOY_DIR/icons"
mkdir -p "$DIST_DIR"

echo "=== 复制可执行文件 ==="
cp "$BUILD_DIR/bin/serial-monitor.exe" "$DEPLOY_DIR/"
cp "$BUILD_DIR/bin/serial-monitor-cli.exe" "$DEPLOY_DIR/"

echo "=== 解析 DLL 依赖 ==="
collect_dlls() {
    local exe="$1"
    objdump -p "$exe" 2>/dev/null | grep "DLL Name:" | awk '{print $3}'
}

ALL_EXES=("$BUILD_DIR/bin/serial-monitor.exe" "$BUILD_DIR/bin/serial-monitor-cli.exe")
copied_dlls=""

for exe in "${ALL_EXES[@]}"; do
    while IFS= read -r dll; do
        [ -z "$dll" ] && continue
        dll_lower=$(echo "$dll" | tr '[:upper:]' '[:lower:]')
        
        # Skip Windows system DLLs
        case "$dll_lower" in
            kernel32.dll|user32.dll|gdi32.dll|advapi32.dll|shell32.dll)
                continue ;;
            comdlg32.dll|ole32.dll|oleaut32.dll|winspool.drv|comctl32.dll)
                continue ;;
            winmm.dll|ws2_32.dll|rpcrt4.dll|imm32.dll|msvcrt.dll|shlwapi.dll)
                continue ;;
            uxtheme.dll|dwmapi.dll|version.dll|setupapi.dll|cfgmgr32.dll)
                continue ;;
            secur32.dll|crypt32.dll|wldap32.dll|dnsapi.dll|iphlpapi.dll)
                continue ;;
            bcrypt.dll|ncrypt.dll|powrprof.dll|profapi.dll)
                continue ;;
            msvcp*.dll|vcruntime*.dll)
                continue ;;
        esac
        
        # Skip already copied
        echo "$copied_dlls" | grep -q "$dll" && continue
        
        # Find the DLL
        found=$(find "$MINGW_DIR/bin" -maxdepth 1 -iname "$dll" -print -quit 2>/dev/null)
        if [ -z "$found" ]; then
            found=$(find "$MINGW_DIR" -maxdepth 3 -iname "$dll" -print -quit 2>/dev/null)
        fi
        
        if [ -n "$found" ]; then
            echo "  复制: $dll"
            cp "$found" "$DEPLOY_DIR/"
            copied_dlls="$copied_dlls"$'\n'"$dll"
        fi
    done < <(collect_dlls "$exe")
done

# Additional DLLs that might not appear in objdump but are needed at runtime
extras=(
    "libgcc_s_seh-1.dll"
    "libstdc++-6.dll"
    "libwinpthread-1.dll"
    "libssp-0.dll"
    "libfmt.dll"
    "Qt5Core.dll"
    "Qt5Gui.dll"
    "Qt5Widgets.dll"
    "Qt5SerialPort.dll"
    "Qt5Network.dll"
)
for dll in "${extras[@]}"; do
    echo "$copied_dlls" | grep -qi "$dll" && continue
    found=$(find "$MINGW_DIR/bin" -maxdepth 1 -iname "$dll" -print -quit 2>/dev/null)
    if [ -n "$found" ]; then
        echo "  补充: $dll"
        cp "$found" "$DEPLOY_DIR/"
    fi
done

echo "=== 复制 Qt 平台插件 ==="
cp "$MINGW_DIR/share/qt5/plugins/platforms/qwindows.dll" "$DEPLOY_DIR/platforms/"

echo "=== 复制资源文件 ==="
cp "$PROJECT_DIR/resources/styles/dark_theme.qss" "$DEPLOY_DIR/styles/"
cp "$PROJECT_DIR/resources/icons/logo.png" "$DEPLOY_DIR/icons/"

echo "=== 创建启动脚本 ==="
cat > "$DEPLOY_DIR/emberInter.bat" << 'BAT'
@echo off
chcp 65001 > nul
set PATH=%~dp0;%~dp0platforms;%PATH%
start "" "%~dp0serial-monitor.exe"
BAT

cat > "$DEPLOY_DIR/emberInter-cli.bat" << 'BAT'
@echo off
chcp 65001 > nul
set PATH=%~dp0;%~dp0platforms;%PATH%
"%~dp0serial-monitor-cli.exe" %*
BAT

echo ""
echo "=== 部署完成 ==="
echo "输出目录: $DEPLOY_DIR"
ls -la "$DEPLOY_DIR/" | head -40
echo ""
echo "可执行文件大小:"
ls -lh "$DEPLOY_DIR/serial-monitor.exe" "$DEPLOY_DIR/serial-monitor-cli.exe"