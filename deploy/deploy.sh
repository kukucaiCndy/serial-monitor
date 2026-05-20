#!/bin/bash
# emberInter 尘智串口调试工具 - 部署打包脚本
set -e

PROJECT_DIR="/f/work/software/serial-monitor"
BUILD_DIR="$PROJECT_DIR/build"
DEPLOY_DIR="$PROJECT_DIR/deploy/emberInter"
DIST_DIR="$PROJECT_DIR/release"

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

echo "=== 递归解析 DLL 依赖 ==="

SYSTEM_DLLS="kernel32.dll user32.dll gdi32.dll advapi32.dll shell32.dll
comdlg32.dll ole32.dll oleaut32.dll winspool.drv comctl32.dll
winmm.dll ws2_32.dll rpcrt4.dll imm32.dll msvcrt.dll shlwapi.dll
uxtheme.dll dwmapi.dll version.dll setupapi.dll cfgmgr32.dll
secur32.dll crypt32.dll wldap32.dll dnsapi.dll iphlpapi.dll
bcrypt.dll ncrypt.dll powrprof.dll profapi.dll netapi32.dll mpr.dll
userenv.dll dxgi.dll d3d11.dll usp10.dll dwrite.dll"

is_system_dll() {
    local name="$1"
    local lower=$(echo "$name" | tr '[:upper:]' '[:lower:]')
    for sys in $SYSTEM_DLLS; do
        [ "$lower" = "$sys" ] && return 0
    done
    case "$lower" in
        msvcp*.dll|vcruntime*.dll) return 0 ;;
    esac
    return 1
}

copied_dlls=""

copy_dll_recursive() {
    local target="$1"
    local dlls=$(objdump -p "$target" 2>/dev/null | grep "DLL Name:" | awk '{print $3}')
    
    for dll in $dlls; do
        [ -z "$dll" ] && continue
        
        is_system_dll "$dll" && continue
        
        echo "$copied_dlls" | grep -qi "$dll" && continue
        
        found=$(find "$MINGW_DIR/bin" -maxdepth 1 -iname "$dll" -print -quit 2>/dev/null)
        if [ -z "$found" ]; then
            found=$(find "$MINGW_DIR" -maxdepth 3 -iname "$dll" -print -quit 2>/dev/null)
        fi
        
        if [ -n "$found" ]; then
            echo "  复制: $dll"
            cp "$found" "$DEPLOY_DIR/"
            copied_dlls="$copied_dlls"$'\n'"$dll"
            copy_dll_recursive "$found"
        fi
    done
}

copy_dll_recursive "$BUILD_DIR/bin/serial-monitor.exe"
copy_dll_recursive "$BUILD_DIR/bin/serial-monitor-cli.exe"

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