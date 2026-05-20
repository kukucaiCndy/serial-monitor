#!/bin/bash
# EmberInter deb 打包脚本
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/build"
DIST_DIR="$PROJECT_DIR/release"
VERSION="1.2.0"
PACK_NAME="emberinter_${VERSION}"
PACK_DIR="$SCRIPT_DIR/${PACK_NAME}"

# 1. 确认已编译
if [ ! -f "$BUILD_DIR/bin/serial-monitor" ]; then
    echo "错误: 未找到编译产物，请先执行 cmake 构建"
    echo "  mkdir -p $BUILD_DIR && cd $BUILD_DIR"
    echo "  cmake .. -DCMAKE_BUILD_TYPE=Release"
    echo "  cmake --build . -j\$(nproc)"
    exit 1
fi

# 2. 清理旧打包目录
rm -rf "$PACK_DIR" "$SCRIPT_DIR/emberinter_${VERSION}_amd64.deb"
mkdir -p "$DIST_DIR"

# 3. 创建目录结构
mkdir -p "$PACK_DIR/DEBIAN"
mkdir -p "$PACK_DIR/opt/emberinter/bin"
mkdir -p "$PACK_DIR/opt/emberinter/resources/icons"
mkdir -p "$PACK_DIR/opt/emberinter/resources/styles"
mkdir -p "$PACK_DIR/usr/share/applications"
mkdir -p "$PACK_DIR/usr/share/icons/hicolor/256x256/apps"
mkdir -p "$PACK_DIR/usr/share/doc/emberinter"

# 4. 复制文件
echo "=== 复制二进制文件 ==="
cp "$BUILD_DIR/bin/serial-monitor" "$PACK_DIR/opt/emberinter/bin/"
cp "$BUILD_DIR/bin/serial-monitor-cli" "$PACK_DIR/opt/emberinter/bin/"
chmod 755 "$PACK_DIR/opt/emberinter/bin/"*

echo "=== 复制资源文件 ==="
cp "$PROJECT_DIR/resources/styles/dark_theme.qss" "$PACK_DIR/opt/emberinter/resources/styles/"
cp "$PROJECT_DIR/resources/icons/logo.png" "$PACK_DIR/opt/emberinter/resources/icons/"
cp "$PROJECT_DIR/resources/icons/close.svg" "$PACK_DIR/opt/emberinter/resources/icons/"
cp "$PROJECT_DIR/resources/icons/logo.png" "$PACK_DIR/usr/share/icons/hicolor/256x256/apps/emberinter.png"

# 5. 创建 DEBIAN/control
cat > "$PACK_DIR/DEBIAN/control" << 'EOF'
Package: emberinter
Version: 1.2.0
Section: electronics
Priority: optional
Architecture: amd64
Maintainer: kukucaiCndy <kukucaiCndy@github.com>
Depends: libqt5widgets5 (>= 5.15), libqt5serialport5 (>= 5.15), libqt5network5 (>= 5.15), libqt5gui5 (>= 5.15), libqt5core5a (>= 5.15), libfmt10 (>= 10), libc6 (>= 2.34), libstdc++6 (>= 12)
Description: EmberInter Debug Tool - 尘智串口调试工具
 EmberInter 是一款面向嵌入式开发者的跨平台串口监控调试工具。
 同时支持 CLI 命令行模式（供 AI Agent/脚本使用）和 GUI 图形界面模式（供开发者使用）。
 功能特性：
  - 串口自动检测与连接
  - 彩色日志显示（自动识别 ERROR/WARN/INFO/DEBUG/TRACE 级别）
  - HEX 十六进制显示模式与混合模式
  - 多串口标签页同时监控
  - 数据发送（文本/HEX/循环发送）
  - 日志筛选过滤与 JSON 格式导出
  - 配置持久化
  - 暗色主题支持
EOF

# 6. 创建 postinst
cat > "$PACK_DIR/DEBIAN/postinst" << 'EOF'
#!/bin/bash
if [ -x /usr/bin/update-desktop-database ]; then
    update-desktop-database -q /usr/share/applications
fi
if [ -x /usr/bin/gtk-update-icon-cache ]; then
    gtk-update-icon-cache -q -t -f /usr/share/icons/hicolor
fi
ln -sf /opt/emberinter/bin/serial-monitor /usr/local/bin/serial-monitor
ln -sf /opt/emberinter/bin/serial-monitor-cli /usr/local/bin/emberinter-cli
echo "EmberInter 尘智串口调试工具已安装成功！"
echo "  GUI: 从应用菜单启动 'EmberInter'，或运行 serial-monitor"
echo "  CLI: 运行 emberinter-cli"
exit 0
EOF
chmod 755 "$PACK_DIR/DEBIAN/postinst"

# 7. 创建 postrm
cat > "$PACK_DIR/DEBIAN/postrm" << 'EOF'
#!/bin/bash
rm -f /usr/local/bin/serial-monitor /usr/local/bin/emberinter-cli
if [ -x /usr/bin/update-desktop-database ]; then
    update-desktop-database -q /usr/share/applications
fi
exit 0
EOF
chmod 755 "$PACK_DIR/DEBIAN/postrm"

# 8. 创建 .desktop 文件
cat > "$PACK_DIR/usr/share/applications/emberinter.desktop" << 'EOF'
[Desktop Entry]
Name=EmberInter
Name[zh_CN]=尘智串口调试工具
Comment=Cross-platform Serial Monitor Debug Tool
Comment[zh_CN]=跨平台串口监控调试工具
Exec=/opt/emberinter/bin/serial-monitor
Icon=emberinter
Terminal=false
Type=Application
Categories=Development;Debugger;Electronics;
Keywords=serial;debug;embedded;monitor;串口;调试;嵌入式;
StartupNotify=true
EOF

# 9. 创建 changelog 和 copyright
cat > "$PACK_DIR/usr/share/doc/emberinter/changelog" << EOF
emberinter (${VERSION}) unstable; urgency=medium

  * Debian packaging release
  * CLI + GUI dual-mode serial debug tool

 -- kukucaiCndy <kukucaiCndy@github.com>  $(date -R)
EOF
gzip -9 -n "$PACK_DIR/usr/share/doc/emberinter/changelog"

cat > "$PACK_DIR/usr/share/doc/emberinter/copyright" << 'EOF'
Format: https://www.debian.org/doc/packaging-manuals/copyright-format/1.0/
Upstream-Name: EmberInterDebugTool
Upstream-Contact: kukucaiCndy <kukucaiCndy@github.com>
Source: https://github.com/kukucaiCndy/serial-monitor

Files: *
Copyright: 2025-2026 kukucaiCndy
License: MIT

License: MIT
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 .
 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.
 .
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.
EOF

# 10. 构建 deb
echo ""
echo "=== 构建 deb 包 ==="
dpkg-deb --build "$PACK_DIR" "$DIST_DIR/${PACK_NAME}_amd64.deb"

echo ""
echo "=== 完成 ==="
ls -lh "$DIST_DIR/${PACK_NAME}_amd64.deb"
echo ""
echo "安装: sudo dpkg -i $DIST_DIR/${PACK_NAME}_amd64.deb"
echo "卸载: sudo dpkg -r emberinter"
