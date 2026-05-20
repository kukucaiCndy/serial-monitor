# EmberInterDebugTool 项目文档

> 微尘藏星火，终端蕴尘智 — 基于 Qt5 的跨平台串口监控调试工具

## 项目信息

| 项目 | 详情 |
|------|------|
| 名称 | EmberInterDebugTool（尘智串口调试工具） |
| 版本 | 1.0.0 |
| 语言标准 | C++17 |
| 构建系统 | CMake >= 3.16 |
| 许可证 | MIT |
| 仓库 | https://github.com/kukucaiCndy/serial-monitor |

## 依赖项

| 依赖 | 用途 | 安装方式 |
|------|------|----------|
| Qt5 (Widgets/Gui/SerialPort/Network) | GUI、串口、IPC | MSYS2: `pacman -S mingw-w64-x86_64-qt5` |
| rapidjson | JSON 解析 | MSYS2: `pacman -S mingw-w64-x86_64-rapidjson` |
| spdlog | 日志库 | MSYS2: `pacman -S mingw-w64-x86_64-spdlog` |
| fmt | 格式化库 | MSYS2: `pacman -S mingw-w64-x86_64-fmt` |
| GoogleTest (可选) | 单元测试 | MSYS2: `pacman -S mingw-w64-x86_64-gtest` |

## 快速构建

### 环境要求

- MSYS2 MinGW64 环境（`$MSYSTEM = MINGW64`）
- 确保 `/mingw64/bin` 在 PATH 中

### 构建步骤

```bash
cd serial-monitor

# 配置（MinGW Makefiles）
mkdir build && cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release

# 编译
cmake --build . -j8

# 产物
ls build/bin/
# serial-monitor.exe       GUI 主程序
# serial-monitor-cli.exe   CLI 命令行工具
```

## 产物说明

| 产物 | 说明 |
|------|------|
| `build/bin/serial-monitor.exe` | GUI 图形界面程序 (~5.5MB) |
| `build/bin/serial-monitor-cli.exe` | CLI 命令行程序 (~1MB) |
| `libserialmonitor_core.a` | 核心静态库（编译中间产物） |

## 打包发布

完整的打包发布流程包含 3 个步骤：收集依赖 → 生成安装包 → 发布到 GitHub。

### 1. 收集依赖 (deploy.sh)

编译产物仅包含 `.exe`，运行时还需要 Qt 和 MinGW 的 DLL。`deploy/deploy.sh` 负责：

- 递归收集 EXE 和所有 DLL 的传递依赖（使用 `objdump` 分析导入表）
- 复制 Qt 平台插件（`qwindows.dll`）
- 复制资源文件（QSS 主题、图标）
- 生成 `.bat` 启动脚本

```bash
# 确保已经完成 Release 构建
bash deploy/deploy.sh
```

**输出目录结构**：

```
deploy/emberInter/
├── serial-monitor.exe         # GUI 主程序
├── serial-monitor-cli.exe     # CLI 工具
├── *.dll                      # 所有运行时 DLL（递归收集，约 29 个）
├── platforms/
│   └── qwindows.dll           # Qt 平台插件
├── imageformats/              # Qt 图片格式插件
├── styles/
│   └── dark_theme.qss         # 暗色主题
├── icons/
│   └── logo.png                # 应用图标
├── emberInter.bat             # GUI 启动脚本
└── emberInter-cli.bat         # CLI 启动脚本
```

> **注意**：脚本使用 `copy_dll_recursive()` 递归收集依赖。Qt5Core.dll 依赖 `libicuin78.dll`、`libicuuc78.dll`、`libdouble-conversion.dll` 等大量传递依赖，仅收集 EXE 直接依赖是不够的。

### 2. 生成安装包 (Inno Setup)

[Inno Setup 6](https://jrsoftware.org/isinfo.php) 用于制作 Windows 安装程序。

```bash
# 在 deploy 目录执行
cd deploy
"c:/Program Files (x86)/Inno Setup 6/ISCC.exe" emberInter.iss
```

**安装包特性**：

| 配置 | 值 |
|------|-----|
| 安装目录 | `C:\Program Files\EmberInterDebugTool` |
| 权限要求 | 管理员 |
| 快捷方式 | 桌面 + 开始菜单（各含 GUI 和 CLI 入口） |
| 压缩 | LZMA 固实压缩 |
| 输出 | `release/emberInter-Setup-1.0.0.exe` (~30MB) |

### 3. 发布到 GitHub

#### 方式一：GitHub CLI (gh)

```bash
gh release create v1.0.0 release/emberInter-Setup-1.0.0.exe \
  --title "v1.0.0 - EmberInterDebugTool" \
  --notes "Release v1.0.0"
```

#### 方式二：GitHub API (curl)

```bash
TOKEN="gho_xxxxxxxxxxxxxxxxxxxx"

# 1. 创建 tag 并推送
git tag v1.0.0 && git push origin v1.0.0

# 2. 创建 Release
curl -X POST \
  -H "Authorization: token $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"tag_name":"v1.0.0","name":"v1.0.0","draft":false}' \
  https://api.github.com/repos/kukucaiCndy/serial-monitor/releases

# 3. 上传安装包（替换 RELEASE_ID）
curl -X POST \
  -H "Authorization: token $TOKEN" \
  -H "Content-Type: application/vnd.microsoft.portable-executable" \
  --data-binary @release/emberInter-Setup-1.0.0.exe \
  "https://uploads.github.com/repos/kukucaiCndy/serial-monitor/releases/{RELEASE_ID}/assets?name=emberInter-Setup-1.0.0.exe"
```

## 常见问题

### 安装后启动报缺失 DLL

**症状**：安装完成后运行软件，弹出 `找不到 libicuin78.dll`、`libdouble-conversion.dll`、`libpcre2-16-0.dll` 等错误。

**原因**：`deploy.sh` 只收集了 EXE 的直接依赖，遗漏了 Qt DLL 的传递依赖。

**解决**：确保 `deploy.sh` 使用 `copy_dll_recursive()` 递归收集。所有必需的传递依赖 DLL：

| 来源 | DLL |
|------|-----|
| Qt5Core | `libdouble-conversion` `libicuin78` `libicuuc78` `libicudt78` `libpcre2-16-0` `libzstd` `zlib1` |
| Qt5Gui | `libharfbuzz-0` `libmd4c` `libpng16-16` |
| harfbuzz | `libfreetype-6` `libglib-2.0-0` `libgraphite2` |
| glib | `libintl-8` `libiconv-2` `libpcre2-8-0` |
| freetype | `libbz2-1` `libbrotlidec` `libbrotlicommon` |

### CMake 找不到 Qt5

确保在 MSYS2 MinGW64 环境下运行，Qt5 的 CMake 配置位于 `/mingw64/lib/cmake/Qt5/`。

```bash
# 手动指定 Qt5 路径
cmake .. -G "MinGW Makefiles" -DQt5_DIR=/mingw64/lib/cmake/Qt5
```

### 代理配置

环境默认配置了代理 `http://127.0.0.1:7890`。代理未启动时 Git/curl 会连接失败。

```bash
# 绕过代理
unset http_proxy https_proxy HTTP_PROXY HTTPS_PROXY

# 或启用代理
export http_proxy=http://127.0.0.1:7890
export https_proxy=http://127.0.0.1:7890
```

## 版本号更新

发布新版本时需要同步更新以下文件中的版本号：

| 文件 | 位置 | 内容 |
|------|------|------|
| [CMakeLists.txt](CMakeLists.txt) | `project(... VERSION x.y.z)` | 第 2 行 |
| [deploy/emberInter.iss](deploy/emberInter.iss) | `#define MyAppVersion "x.y.z"` | 第 6 行 |
| `git tag` | `vx.y.z` | 发布标签 |

## 相关文件索引

| 文件 | 用途 |
|------|------|
| [CMakeLists.txt](CMakeLists.txt) | 顶层构建配置 |
| [deploy/deploy.sh](deploy/deploy.sh) | 递归收集 DLL 依赖并生成部署目录 |
| [deploy/emberInter.iss](deploy/emberInter.iss) | Inno Setup 安装脚本 |
| [resources/styles/dark_theme.qss](resources/styles/dark_theme.qss) | 暗色主题样式 |
| [resources/icons/logo.png](resources/icons/logo.png) | 应用图标 |
| [PRD.md](PRD.md) | 产品需求文档 |
| [docs/](docs/) | 设计文档（IPC 协议、架构等） |