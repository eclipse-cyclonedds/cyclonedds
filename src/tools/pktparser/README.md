# pktparser - RTPS/DDS 数据包分析工具

一个独立的 Go 语言工具，用于分析 DDS (Data Distribution Service) 网络通信的 PCAP 文件，提供详细的网络拓扑分析、实体发现、性能统计和可视化报告。

## 快速开始

```bash
# 1. 编译
go build -o pktparser

# 2. 运行
./pktparser your_capture.pcap

# 3. 查看 HTML 可视化报告
# 默认输出: your_capture_analysis.html
```

## 环境要求

| 依赖 | 说明 |
|------|------|
| **Go** | 1.21 或更高版本 |
| **gopacket** | 网络包解析库 (已内置在 `pkg/gopacket/`) |

**无需外部 DDS 库** - 工具完全独立，不依赖 AutoCoreDDS 主项目代码。

## 编译选项

```bash
# 默认编译 (当前平台)
go build -o pktparser

# 交叉编译 ARM64 (如嵌入式设备)
GOARCH=arm64 GOOS=linux go build -o pktparser-arm64

# 交叉编译 x86_64
GOARCH=amd64 GOOS=linux go build -o pktparser-amd64
```

### 编译后文件说明

编译后会在项目根目录生成可执行文件，项目结构：

```
pktparser/
├── main.go              # 主程序入口
├── internal/            # 内部包
│   ├── rtps/           # RTPS 协议解析核心
│   ├── analyse/        # 数据分析模块
│   ├── visualizer/     # 可视化模块
│   └── config/         # 配置管理
└── pkg/
    ├── gopacket/       # 网络包解析库
    └── xlog/           # 日志系统
```

## 使用方法

### 基础用法

```bash
./pktparser <pcap_file>
```

### 完整参数

```bash
./pktparser capture.pcap [--output <output_file>] [--no-viz]
```

### 命令行参数

| 参数 | 说明 | 默认值 |
|------|------|--------|
| `<pcap_file>` | 输入的 PCAP 文件路径 | - |
| `--output <file>` | 指定输出 HTML 文件路径 | `<pcap文件>_analysis.html` |

### 示例

```bash
# 基本用法
./pktparser capture.pcap

# 指定输出文件名
./pktparser capture.pcap --output /path/to/report.html

# 仅控制台输出 (不生成 HTML)
./pktparser capture.pcap --no-viz
```

## 功能特性

### 1. RTPS 协议支持
- ✅ 标准子消息 (DATA, HEARTBEAT, ACKNACK, GAP, INFO_TS 等)
- ✅ 安全子消息 (SEC_BODY, SEC_PREFIX, SEC_POSTFIX 等)
- ✅ 厂商特定扩展子消息
- ✅ IPv4 分片包重组

### 2. 实体发现
- 📍 **SPDP** - 简单参与者发现协议
- 📍 **SEDP** - 简单端点发现协议
- 📍 内置端点自动识别
- 📍 用户端点发现

### 3. 网络分析
- 📊 拓扑发现 (自动构建网络拓扑)
- 📊 性能统计 (吞吐量、延迟、丢包率)
- 📊 流量分析 (单播/组播)
- 📊 连接状态跟踪

### 4. 可视化输出
- 🌐 交互式 HTML 报告 (响应式 Web 界面)
- 🌐 网络拓扑图
- 🌐 统计图表
- 🌐 详细实体列表和搜索

## 输出说明

### 控制台输出

```
🔍 Found 5 participants
📝 Found 10 writers, 15 readers
📊 Found 5 topics
📍 Found 20 locators
...
```

### HTML 报告内容

生成的 HTML 文件包含：

- **概览页** - 整体统计和快速导航
- **网络拓扑** - 分层树状结构
- **进程列表** - 按 PID 排序
- **参与者详情** - 包含实体关联
- **主题关系** - 发布/订阅关系
- **端点信息** - 写入器/读取器详情
- **QoS 配置** - 可靠性、持久性等

## 获取 PCAP 文件

```bash
# 使用 tcpdump 捕获 DDS 流量
sudo tcpdump -i any udp -P -w capture.pcap

# 使用 tcpdump 捕获特定 DDS 端口
sudo tcpdump -i any port 7400 -P -w capture.pcap

# 使用 Wireshark 图形界面捕获
wireshark
```

## 常见问题

**Q: 编译时找不到 gopacket？**
A: 确保 `pkg/gopacket/` 目录存在，`go.mod` 中有正确的 `replace` 指引。

**Q: 运行时提示 "无法打开 pcap 文件"？**
A: 确认文件路径正确，且文件格式为标准 PCAP/PCAPNG。

**Q: 如何获取特定端口的流量？**
A: 捕获时使用 BPF 过滤器（见上方"获取 PCAP 文件"部分）。

## 依赖项

- **Go 1.19+** - Go 语言运行环境
- **gopacket** - 网络包处理库 (本地 `pkg/gopacket/`)
- **Chart.js** - 图表可视化库 (HTML 中自动引用 CDN)
- **标准库** - encoding/binary, time, strings 等

## 更多信息

详细架构文档请参考：
- [架构概览](pktparser.md)
- [Parse 函数详解](Parse.md)
