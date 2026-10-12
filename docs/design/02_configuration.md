# 模块 2：配置系统（Configuration）设计文档

## 1. 概述

配置系统负责从 XML 字符串中解析 CycloneDDS 的运行参数，校验合法性，补充默认值，并将结果存入 `struct ddsi_config` 供运行时使用。

**关键文件**：
- `src/core/ddsi/src/ddsi_config.c` — 配置解析（2700+ 行）
- `src/core/ddsi/src/ddsi__config_impl.h` — 解析器内部实现
- `src/core/ddsi/include/dds/ddsi/ddsi_config.h` — `struct ddsi_config` 公开定义
- `src/core/ddsi/defconfig.c` — 默认值定义

## 2. 核心数据结构

### 2.1 配置主结构

```c
struct ddsi_config {
  // 规范兼容级别
  enum ddsi_standards_conformance standards_conformance;  // PEDANTIC/STRICT/LAX

  // 传输选择
  enum ddsi_transport_selector transport_selector;         // UDP/TCP/RAWETH/VNET/NONE

  // 发现
  int32_t domainId;
  enum ddsi_boolean_default multicast;                     // TRUE/FALSE/DEFAULT
  struct ddsi_config_peer_listelem *peers;                  // 静态对端列表

  // Participant 索引
  enum ddsi_participant_index participant_index;            // AUTO/NONE/DEFAULT

  // 端口计算
  int port_base, port_dg, port_pg, port_d0, port_d1;

  // 重传策略
  enum ddsi_retransmit_merging retransmit_merging;

  // 时间参数
  int64_t spdp_interval;
  int64_t lease_duration;

  // 缓冲区
  uint32_t whc_lowwater_mark, whc_highwater_mark;
  uint32_t delivery_queue_maxsamples;

  // 网络
  int socket_rcvbuf_size, socket_sndbuf_size;
  uint32_t recv_thread_stop_maxretries;

  // ... 更多字段
};
```

### 2.2 规范兼容级别

```c
enum ddsi_standards_conformance {
  DDSI_SC_PEDANTIC,   // 严格遵循 DDS/RTPS 规范
  DDSI_SC_STRICT,     // 合理严格（默认）
  DDSI_SC_LAX         // 宽松，可与有 bug 的实现互操作
};
```

### 2.3 传输选择器

```c
enum ddsi_transport_selector {
  DDSI_TRANS_DEFAULT,   // 自动选择
  DDSI_TRANS_UDP,       // UDP 单播/多播
  DDSI_TRANS_UDP6,      // IPv6 UDP
  DDSI_TRANS_TCP,       // TCP
  DDSI_TRANS_TCP6,      // IPv6 TCP
  DDSI_TRANS_RAWETH,    // 原始以太网帧
  DDSI_TRANS_NONE       // 无网络（仅进程内通信）
};
```

## 3. 机制设计

### 3.1 两阶段配置处理

```
阶段 1：解析 (ddsi_config_init)
  XML 字符串 → SAX 式解析 → 填充 struct ddsi_config
  ├─ 每个 XML 元素有对应的 update_fun_t 回调
  ├─ 枚举类型在解析时即转为内部 enum 值
  └─ 字符串值暂存，等待后处理

阶段 2：准备 (ddsi_config_prep)
  struct ddsi_config → 校验 + 补充 + 解析 → 就绪的运行时配置
  ├─ 网络接口名 → 解析为 IP 地址和 MTU
  ├─ 端口号计算 (基于 domain_id + participant_index)
  ├─ Locator 生成 (多播/单播地址)
  ├─ 线程亲和性解析 (CPU affinity mask)
  └─ 互斥配置项冲突检测
```

### 3.2 端口计算公式

遵循 RTPS v2.5 §9.6.1 规定的端口映射：

```
discovery_multicast_port = port_base + port_dg * domainId + port_d0
discovery_unicast_port   = port_base + port_dg * domainId + port_d1 + port_pg * participantId
data_multicast_port      = port_base + port_dg * domainId + port_d2
data_unicast_port        = port_base + port_dg * domainId + port_d3 + port_pg * participantId
```

### 3.3 默认值体系

`defconfig.c` 为所有可配置参数定义了合理的默认值。配置解析后，通过 `present` 位掩码判断用户是否显式设置了某参数——未设置的使用默认值。

## 4. 设计逻辑与设计思想

### 4.1 为什么选择 XML 作为配置格式？

**设计哲学：人类可读 + 结构化 + 运行时注入**

替代方案的劣势：
- **编译时常量**：修改配置需要重新编译，不适合生产环境
- **环境变量**：扁平结构，无法表达嵌套关系（如 `Transport > UDP > SocketBufferSize`）
- **INI 文件**：只支持一层嵌套，无法表达 DDS 配置的深层结构
- **JSON/YAML**：2010 年代的 DDS 生态还未广泛采用

XML 的优势：
- **自然嵌套**：DDS 配置天然是分层的（Domain → Transport → Socket → BufferSize）
- **Schema 可验证**：XML Schema 可以在解析前校验结构合法性
- **字符串注入**：`dds_create_domain(id, xml_string)` 允许程序化生成配置，无需文件系统依赖
- **环境变量扩展**：XML 中可以嵌入 `${ENV_VAR}` 语法，运行时解析

**底层思想**：**声明式配置优于命令式配置**。XML 描述的是"期望的最终状态"，而不是"如何一步步设置"。`ddsi_config_prep()` 负责将声明转化为运行时参数。

### 4.2 为什么分两阶段处理配置？

**设计哲学：解耦验证时机**

阶段 1（解析）只做**语法层面**的工作：
- XML 合法性
- 枚举值合法性（`"udp"` → `DDSI_TRANS_UDP`）
- 数值范围粗校验

阶段 2（准备）做**语义层面**的工作：
- 网络接口名 `"eth0"` 解析为实际 IP 地址——这需要访问操作系统网络栈
- 端口计算需要 domain_id，而 domain_id 可能在 XML 之外通过 API 参数覆盖
- 多播是否可用取决于网卡能力，在解析 XML 时还不知道

**为什么不合并？**

如果在 XML 解析过程中就访问网络栈，那么解析器就依赖了运行时状态。这导致：
- 单元测试无法独立测试配置解析（需要模拟网卡）
- 配置校验错误和网络错误混在一起，难以诊断
- XML 解析变成有副作用的操作

**底层思想**：**纯函数解析 + 副作用准备**。阶段 1 是纯函数（input: XML string → output: config struct）。阶段 2 是有副作用的（访问网络栈、文件系统）。分离两者使得阶段 1 完全可测试。

### 4.3 规范兼容级别的设计哲学

**设计哲学：Postel 法则的分级实现**

Postel 法则（鲁棒性原则）：

> "Be conservative in what you send, be liberal in what you accept."

CycloneDDS 将这一原则分为三个级别：

**PEDANTIC（学究模式）**：
- 严格遵循 DDS/RTPS 规范的每一个 MUST/SHALL
- 拒绝任何非标准的消息或配置
- 用途：规范一致性测试、互操作性测试

**STRICT（严格模式，默认）**：
- 遵循规范的核心要求
- 对于规范中的 SHOULD/RECOMMENDED，采纳但不强制
- 用途：生产环境的标准配置

**LAX（宽松模式）**：
- 容忍其他实现的已知 bug
- 接受非标准的消息格式（只要能安全解析）
- 用途：与旧版本或有 bug 的 DDS 实现互操作

**底层思想**：**规范是理想，互操作是现实**。DDS 生态中有多家供应商（RTI、ADLINK、eProsima 等），每家实现都有细微差异。三级兼容性让用户根据场景在"正确性"和"互操作性"之间做出明确选择，而不是隐式地"猜测"对方的行为。

### 4.4 为什么配置结构有 400+ 字段？

**设计哲学：可调优性 > 简洁性**

DDS/RTPS 是工业级中间件，部署场景差异巨大：
- 嵌入式实时系统（FreeRTOS，256KB RAM）
- 自动驾驶车载网络（低延迟、高吞吐）
- 云端微服务（TCP、跨数据中心）
- 安全关键系统（加密、鉴权）

每个场景对缓冲区大小、超时时间、重传策略、线程数、传输类型的需求完全不同。通过暴露底层参数，CycloneDDS 允许用户精确调优——而不是通过一个"高级/低级"的二选一开关来猜测用户意图。

**底层思想**：**显式优于隐式（The Zen of Python）**。每个可配置参数都对应一个明确的运行时行为。用户看到 `whc_highwater_mark = 500KB`，就知道 Writer History Cache 最多缓存 500KB 的未确认数据。没有隐藏的自适应逻辑在背后覆盖用户设置。

### 4.5 默认值的设计原则

CycloneDDS 的默认值遵循"**开箱即用**"原则：

- `transport = UDP`：最通用的传输方式
- `multicast = DEFAULT`：有多播能力就用，没有就退化为单播
- `lease_duration = 10s`：够快检测到节点宕机，又不会因网络抖动误判
- `spdp_interval = ~80% × lease_duration`：确保在租约到期前至少发送一次 SPDP
- `standards_conformance = STRICT`：足够正确，又不过于苛刻

**底层思想**：**零配置可用（Zero-Configuration Usability）**。用户不写任何 XML，`dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL)` 就能工作。默认值不是随意选的——每个默认值都经过生产环境验证。

## 5. 与规范的关系

- **RTPS v2.5 §9.6.1**：端口计算公式严格遵循规范，确保不同厂商实现使用相同端口
- **RTPS v2.5 §8.5.3.2**：SPDP 重发间隔的默认值与规范建议一致
- **DDS v1.4 §2.1.2.2**：Domain ID 的语义对应配置中的 `domainId` 字段

## 6. 总结

配置系统的设计哲学可概括为**声明式 + 两阶段 + 分级容错**：
1. XML 声明式配置表达"期望状态"，不涉及运行时
2. 解析与准备分离，纯函数解析 + 有副作用的准备
3. 三级规范兼容性让用户在正确性与互操作性之间显式选择
4. 400+ 字段提供工业级可调优能力，默认值确保零配置可用
