# EmberInterDebugTool - 尘智串口调试工具

> 微尘藏星火，终端蕴尘智

基于 Qt5 的跨平台串口监控调试工具，提供 GUI 图形界面和 CLI 命令行两种使用方式，两者通过 IPC 实时联动。

## 特性

- 🖥️ **GUI + CLI 双模式** - GUI 负责串口连接管理，CLI 可通过命令行或交互模式与 GUI 联动
- 🗂️ **会话管理** - 左侧"我的会话"列表管理多个串口配置，右侧标签页对应显示
- 🎨 **彩色日志** - 按日志级别着色（TX 绿色、ERROR 红色、WARN 黄色等），发送/接收数据一目了然
- ⏱️ **时间戳** - 每条日志自动添加时间戳
- 🔍 **实时筛选** - 支持关键字过滤
- 💾 **大缓冲区** - 10000 行日志缓存，防止丢失关键信息
- 🔄 **自动重连** - 串口断开后自动重试
- 🌙 **暗色主题** - 默认暗色 QSS 主题，保护眼睛
- 📡 **IPC 实时通信** - CLI 和 GUI 通过 QLocalSocket 实时同步状态

## 系统要求

### 编译依赖

| 依赖 | 用途 |
|------|------|
| CMake >= 3.16 | 构建系统 |
| Qt5 (Widgets, SerialPort, Network) | GUI、串口、IPC 通信 |
| spdlog | 日志库 |
| RapidJSON | JSON 解析 |
| fmt | 格式化库 |
| GCC >= 12 / Clang / MSVC | C++17 编译器 |

### 运行环境

- Windows 7+ / Linux (Debian/Ubuntu/Deepin) / macOS
- 可用串口设备（CP210x、FTDI、CH340、JLink CDC 等）

## 快速开始

### Linux 安装（Debian/Ubuntu/Deepin）

直接安装预编译的 deb 包：

```bash
# 安装
sudo dpkg -i emberinter_1.2.0_amd64.deb
sudo apt-get install -f   # 自动补全依赖

# 启动 GUI
serial-monitor

# 或从应用菜单搜索 "EmberInter" / "尘智串口调试工具"

# CLI 模式
emberinter-cli --list
```

卸载：
```bash
sudo dpkg -r emberinter
```

### Linux 串口权限配置

Linux 下普通用户访问串口设备（如 `/dev/ttyUSB0`）通常需要 `dialout` 组权限。若遇到 "Permission denied" 错误，可通过 udev 规则自动授权。

#### 方法一：将用户加入 dialout 组（推荐，永久生效）

```bash
sudo usermod -a -G dialout $USER
# 注销并重新登录后生效
```

#### 方法二：udev 规则自动授权（即插即用）

创建 `/etc/udev/rules.d/90-serial.rules`：

```bash
sudo tee /etc/udev/rules.d/90-serial.rules << 'EOF'
# USB 串口设备（通配 ttyUSB* / ttyACM*）
KERNEL=="ttyUSB[0-9]*", MODE="0666"
KERNEL=="ttyACM[0-9]*", MODE="0666"
EOF

# 重载规则
sudo udevadm control --reload-rules
sudo udevadm trigger
```

> **提示**: 若不能识别根据设备的 Vendor/Product ID添加规则，可运行 `lsusb` 查看，或拔插设备后 `dmesg | tail` 查看内核日志。

### 源码构建

#### Linux

```bash
# 安装编译依赖（Debian/Ubuntu/Deepin）
sudo apt-get install -y \
    cmake g++ \
    qtbase5-dev libqt5serialport5-dev \
    rapidjson-dev libspdlog-dev libfmt-dev

# 克隆仓库
git clone https://github.com/kukucaiCndy/serial-monitor.git
cd serial-monitor

# 配置与构建
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)

# 输出文件
# build/bin/serial-monitor         (GUI)
# build/bin/serial-monitor-cli     (CLI)
```

#### Windows (MSYS2 MinGW64)

```bash
# 安装编译依赖
pacman -S mingw-w64-x86_64-cmake mingw-w64-x86_64-gcc
pacman -S mingw-w64-x86_64-qt5 mingw-w64-x86_64-rapidjson
pacman -S mingw-w64-x86_64-spdlog mingw-w64-x86_64-fmt

# 配置与构建
mkdir build && cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build . -j8

# 输出文件
# build/bin/serial-monitor.exe     (GUI)
# build/bin/serial-monitor-cli.exe (CLI)
```

### 运行

1. **启动 GUI**
   ```bash
   ./serial-monitor        # Linux
   ./serial-monitor.exe    # Windows
   ```
   在界面左侧点击 `[+]` 添加串口会话，配置端口、波特率等参数。
   右键会话 → 连接，即可开始监控。

2. **命令行操作**（需要 GUI 已在运行）
   ```bash
   # 查看帮助
   ./serial-monitor-cli --help

   # 列出可用串口
   ./serial-monitor-cli --list

   # 连接串口（Linux 端口如 /dev/ttyUSB0）
   ./serial-monitor-cli --connect COM3 --baudrate 115200

   # 监听日志
   ./serial-monitor-cli -p COM3

   # 发送数据
   ./serial-monitor-cli --send "AT+GMR" -p COM3
   ```

## GUI 使用说明

### 会话管理

- **添加会话**: 点击左侧"我的会话"旁的 `[+]` 按钮，弹出配置对话框
  - 会话名称（自定义标识）
  - 串口号（COM1、/dev/ttyUSB0 等）
  - 波特率（9600、115200 等）
  - 数据位（5/6/7/8）
  - 校验位（无/奇/偶）
  - 停止位（1/1.5/2）
- **编辑会话**: 右键会话 → 编辑
- **删除会话**: 右键会话 → 删除
- **连接/断开**: 右键会话 → 连接/断开，或单击已连接的会话

### 标签页

- 每个已连接的会话对应一个标签页
- 关闭标签页时自动断开对应串口
- 标签页上显示关闭按钮，悬停时高亮

### 日志显示

- **TX（发送）**: 绿色显示
- **RX（接收）**: 按日志级别着色（INFO 绿色、WARN 黄色、ERROR 红色、DEBUG 蓝色、TRACE 灰色）
- 支持时间戳显示、HEX 模式、关键字过滤
- 支持自动滚动 / 手动滚动查看历史

### 数据发送

- 右下角发送面板支持文本和 HEX 两种发送模式
- 可设置追加 CR/LF/CRLF 换行符

## CLI 使用说明

CLI 通过 IPC 与 GUI 通信，**使用前请先启动 GUI 程序**。

### 命令行参数

#### 监听模式

| 参数 | 说明 |
|------|------|
| `-p, --port PORT` | 指定串口，实时接收日志 |
| `--cli` | 交互 CLI 模式，需配合 `-p PORT` 使用 |
| `-f, --filter KW` | 过滤关键词，只显示包含关键词的日志 |
| `-o, --output FILE` | 同时保存日志到文件 |
| `--hex` | HEX 显示模式 |
| `--no-timestamp` | 不显示时间戳 |
| `--json` | JSON 输出模式 |
| `--clear` | 启动时清空 GUI 日志缓冲 |
| `--ipc NAME` | IPC 服务名称（默认: serial_monitor_ipc） |

#### 操作命令

| 参数 | 说明 |
|------|------|
| `--connect PORT` | 连接指定串口 |
| `--baudrate RATE` | 配合 `--connect` 使用，设置波特率（默认: 115200） |
| `--send DATA` | 发送文本数据（自动追加 CRLF），完成后退出 |
| `--list` | 列出可用串口设备 |
| `--get-status` | 显示当前连接状态 |
| `--get-logs N` | 获取最近 N 条日志 |

#### 帮助

| 参数 | 说明 |
|------|------|
| `-h, --help` | 显示帮助信息 |
| `-v, --version` | 显示版本信息 |

### 使用示例

```bash
# 列出所有可用串口
./serial-monitor-cli --list

# 连接 COM3 并设置波特率为 9600
./serial-monitor-cli --connect COM3 --baudrate 9600

# 监听 COM3 的实时日志
./serial-monitor-cli -p COM3

# 交互模式监听 COM3（支持输入命令）
./serial-monitor-cli -p COM3 --cli

# 发送 AT 命令并观察回复
./serial-monitor-cli --send "AT+GMR" -p COM3

# 获取最近 50 条日志
./serial-monitor-cli --get-logs 50

# 只显示含 ERROR 的日志
./serial-monitor-cli -p COM3 -f ERROR

# 监听并保存日志到文件
./serial-monitor-cli -p COM3 -o debug.log

# 查看当前连接状态
./serial-monitor-cli --get-status
```

### 交互模式

当使用 `--cli` 参数进入交互模式后，可以使用以下命令：

**会话管理:**
```
connect <port> [baud]    连接串口
disconnect / disc        断开当前串口
status                   显示连接状态
list                     列出可用串口
```

**数据发送:**
```
send <data>              发送文本数据（自动追加 CRLF）
sendhex <hex>            发送 HEX 数据
```

**日志操作:**
```
clear                    清空日志缓存
filter <keyword>         设置过滤关键词（空 = 取消过滤）
hex / text               切换 HEX/文本 显示模式
timestamp / ts           切换时间戳显示
export <file>            导出日志为 JSON 文件
```

**其他:**
```
help / ?                 显示帮助
quit / q / exit          退出 CLI（GUI 继续运行）
```

> **提示**: 未识别的命令将作为文本数据直接发送到串口。

### 交互模式示例

```
$ ./serial-monitor-cli -p COM3 --cli
============================================================
  EmberInterDebugTool v1.2.0 - 尘智 | 微尘藏星火,终端蕴尘智
  Port: COM3
============================================================

[系统] 输入 'help' 查看命令, 'quit' 退出

> list
  COM3    USB Serial Port
  COM5    Bluetooth Serial

> send AT
>>> 发送: AT

> filter ERROR
[系统] 过滤: ERROR

> quit
[SYSTEM] CLI disconnected (GUI service continues)
```

## 架构概览

```
┌─────────────────────────────────────────────┐
│                  GUI (Qt5)                   │
│  ┌──────────┐  ┌─────────────────────────┐  │
│  │ 会话列表  │  │     标签页 (LogView)     │  │
│  │          │  │  ┌─────────────────────┐ │  │
│  │ Session1 │  │  │  彩色日志 (QPlainTxt) │  │  │
│  │ Session2 │  │  │  TX绿 RX彩色         │  │  │
│  │   ...    │  │  └─────────────────────┘ │  │
│  │          │  │  ┌─────────────────────┐ │  │
│  │  [+]-+   │  │  │  发送面板            │ │  │
│  └──────────┘  │  └─────────────────────┘ │  │
│                └─────────────────────────┘  │
│  ┌──────────────────────────────────────┐   │
│  │          IPC Server (QLocalServer)    │   │
│  └──────────────┬───────────────────────┘   │
└─────────────────┼───────────────────────────┘
                  │ QLocalSocket
┌─────────────────┼───────────────────────────┐
│  CLI (Qt5)      │                            │
│  ┌──────────────┴───────────────────────┐   │
│  │      IPC Client (QLocalSocket)       │   │
│  │   logReceived / statusChanged / ...  │   │
│  └──────────────────────────────────────┘   │
└─────────────────────────────────────────────┘
```

- **GUI** 负责串口连接、数据收发、日志显示
- **CLI** 通过 IPC 向 GUI 发送命令，并接收实时日志流
- **SerialEngine** 封装 QSerialPort，提供统一的串口操作接口
- **LogBuffer** 环形缓冲区，缓存最近 10000 条日志
- **ConfigManager** JSON 配置持久化（会话列表、窗口状态等）

## 项目结构

```
serial-monitor/
├── src/
│   ├── core/               # 核心库
│   │   ├── serial_engine   # 串口引擎
│   │   ├── log_buffer      # 日志缓冲区
│   │   ├── log_parser      # 日志解析/格式化
│   │   ├── log_exporter    # 日志导出
│   │   ├── config_manager  # 配置管理
│   │   └── ipc_protocol    # IPC 协议定义
│   ├── gui/                # GUI 模块
│   │   ├── main_window     # 主窗口
│   │   ├── log_view        # 日志视图
│   │   ├── send_panel      # 发送面板
│   │   ├── status_bar      # 状态栏
│   │   ├── serial_port_dialog  # 串口配置对话框
│   │   ├── serial_tab_widget   # 标签页组件
│   │   └── ipc_server      # IPC 服务端
│   └── cli/                # CLI 模块
│       ├── cli_app         # CLI 应用逻辑
│       └── ipc_client      # IPC 客户端
├── resources/
│   ├── styles/             # QSS 样式表
│   └── icons/              # 图标资源
├── tests/                  # 单元测试
├── deploy/                 # 部署/打包脚本
└── CMakeLists.txt
```

## 打包发布

### deb 包（Linux）

```bash
# 1. 编译项目
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)

# 2. 打包为 .deb
cd ..
bash deb_pack/build_deb.sh
# 输出: release/emberinter_1.2.0_amd64.deb
```

### Windows 安装包

详见 [deploy/deploy.sh](deploy/deploy.sh) 和 [deploy/emberInter.iss](deploy/emberInter.iss)，使用 Inno Setup 生成安装程序。

## 许可证

MIT License