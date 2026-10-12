# 模块 13-01：UDP 传输机制详细设计文档

## 1. 概述

本文档详细分析 CycloneDDS 在 UDP 模式下的 Socket 创建、分配、组播管理和发送选择机制。UDP 是 RTPS 规范的默认传输协议，CycloneDDS 围绕它构建了一套精密的 Socket 架构：**4 个接收 Socket + 最多 4 个发送 Socket + 最多 3 个接收线程**。

**关键文件**：
- `src/core/ddsi/src/ddsi_init.c` — Socket 创建、线程分配、组播组加入
- `src/core/ddsi/src/ddsi_udp.c` — UDP Socket 底层操作（创建、组播选项、读写）
- `src/core/ddsi/src/ddsi_mcgroup.c` — 组播组成员管理（引用计数）
- `src/core/ddsi/src/ddsi_addrset.c` — 地址集构建（发送 Socket 选择）
- `src/core/ddsi/src/ddsi_receive.c` — 接收线程主循环
- `src/core/ddsi/include/dds/ddsi/ddsi_domaingv.h` — 全局变量声明

## 2. ddsi_factory_create_conn 详解

### 2.1 函数签名与调用链

```c
// ddsi__tran.h:369-376
inline dds_return_t ddsi_factory_create_conn (
  struct ddsi_tran_conn **conn,      // [out] 创建的连接
  struct ddsi_tran_factory *factory,  // 传输工厂 (UDP/TCP/RawEth)
  uint32_t port,                      // 端口号 (0=随机, RTPS端口=具体值)
  const struct ddsi_tran_qos *qos)    // ← 关键: 用途描述
{
  *conn = NULL;
  // 断言: 发送 Socket 必须指定接口, 接收 Socket 不指定接口
  if ((qos->m_interface != NULL) != (qos->m_purpose == DDSI_TRAN_QOS_XMIT_UC ||
                                      qos->m_purpose == DDSI_TRAN_QOS_XMIT_MC))
    return DDS_RETCODE_BAD_PARAMETER;
  if (port != DDSI_TRAN_RANDOM_PORT_NUMBER && !ddsi_is_valid_port(factory, port))
    return DDS_RETCODE_BAD_PARAMETER;
  return factory->m_create_conn_fn(conn, factory, port, qos);  // 分派到 UDP 实现
}
```

**第一个参数校验**是理解收发区别的关键：
- `m_interface != NULL` 仅在 `XMIT_UC` 或 `XMIT_MC` 时为 true
- 这意味着**发送 Socket 必须绑定到特定网络接口，接收 Socket 不绑定**

### 2.2 ddsi_tran_qos: 四种用途

```c
// ddsi__tran.h:303-308
struct ddsi_tran_qos {
  enum ddsi_tran_qos_purpose m_purpose;   // 用途
  int m_diffserv;                          // DiffServ/DSCP 标记
  struct ddsi_network_interface *m_interface; // 仅发送时指定
};

enum ddsi_tran_qos_purpose {
  DDSI_TRAN_QOS_XMIT_UC,  // 发送 Socket: 仅发单播
  DDSI_TRAN_QOS_XMIT_MC,  // 发送 Socket: 可发单播或组播
  DDSI_TRAN_QOS_RECV_UC,  // 接收 Socket: 接收单播
  DDSI_TRAN_QOS_RECV_MC   // 接收 Socket: 接收组播
};
```

### 2.3 四种用途在 ddsi_udp_create_conn 中的不同行为

`m_purpose` 在 `ddsi_udp_create_conn()` (`ddsi_udp.c:596-765`) 内部驱动 **四个布尔标志**，这四个标志决定了 Socket 的所有差异：

```c
static dds_return_t ddsi_udp_create_conn(conn_out, fact, port, qos)
{
  bool reuse_addr = false;           // 是否 SO_REUSEADDR
  bool bind_to_any = false;          // 绑定 0.0.0.0 还是具体 IP
  bool set_mc_xmit_options = false;  // 是否设置组播发送选项
  const char *purpose_str = NULL;

  switch (qos->m_purpose)
  {
    case DDSI_TRAN_QOS_XMIT_UC:       // 发送-仅单播
      reuse_addr          = false;     // 不需要端口复用
      bind_to_any         = false;     // 绑定到接口 IP
      set_mc_xmit_options = false;     // 不设置组播选项
      purpose_str = "transmit(uc)";
      break;

    case DDSI_TRAN_QOS_XMIT_MC:       // 发送-可组播
      reuse_addr          = false;     // 不需要端口复用
      bind_to_any         = false;     // 绑定到接口 IP
      set_mc_xmit_options = true;      // ← 设置组播选项!
      purpose_str = "transmit(uc/mc)";
      break;

    case DDSI_TRAN_QOS_RECV_UC:        // 接收-单播
      reuse_addr          = false;     // 单播端口独占
      bind_to_any         = true;      // ← 绑定 0.0.0.0
      set_mc_xmit_options = false;
      purpose_str = "unicast";
      break;

    case DDSI_TRAN_QOS_RECV_MC:        // 接收-组播
      reuse_addr          = true;      // ← 允许多进程共享端口!
      bind_to_any         = true;      // ← 绑定 0.0.0.0
      set_mc_xmit_options = false;
      purpose_str = "multicast";
      break;
  }
  // ... 后续流程见下文
}
```

### 2.4 四种用途的完整差异对比

```
┌─────────────────┬──────────────┬──────────────┬──────────────┬──────────────┐
│                 │  RECV_UC     │  RECV_MC     │  XMIT_UC    │  XMIT_MC    │
│                 │  接收单播     │  接收组播     │  发送仅单播  │  发送可组播  │
├─────────────────┼──────────────┼──────────────┼──────────────┼──────────────┤
│ m_interface     │  NULL        │  NULL        │  必须指定     │  必须指定    │
│ (绑定接口)       │ (不绑定接口)  │ (不绑定接口)  │ (绑定接口IP) │ (绑定接口IP) │
├─────────────────┼──────────────┼──────────────┼──────────────┼──────────────┤
│ reuse_addr      │  false       │  true ★      │  false       │  false      │
│ (SO_REUSEADDR)  │  端口独占     │  允许共享     │  端口独占     │  端口独占    │
├─────────────────┼──────────────┼──────────────┼──────────────┼──────────────┤
│ bind_to_any     │  true ★      │  true ★      │  false       │  false      │
│ (绑定地址)       │  0.0.0.0     │  0.0.0.0     │  接口IP地址   │  接口IP地址  │
├─────────────────┼──────────────┼──────────────┼──────────────┼──────────────┤
│ set_mc_xmit     │  false       │  false       │  false       │  true ★     │
│ (组播发送选项)    │              │              │              │ TTL+IF+LOOP │
├─────────────────┼──────────────┼──────────────┼──────────────┼──────────────┤
│ port 参数        │  RTPS规范端口 │  RTPS规范端口 │  0(随机)     │  0(随机)    │
│                 │  7410+2*ppid │  7400        │  OS分配      │  OS分配      │
├─────────────────┼──────────────┼──────────────┼──────────────┼──────────────┤
│ 调用位置         │ make_uc_     │ create_mc_   │ ddsi_init    │ ddsi_init   │
│                 │ sockets()    │ sockets()    │ 发送Socket段 │ 发送Socket段 │
├─────────────────┼──────────────┼──────────────┼──────────────┼──────────────┤
│ 创建数量         │  1~2 个       │  1~2 个       │ n_interfaces │ n_interfaces│
│                 │              │              │ 个 (最多4)    │ 个 (最多4)   │
└─────────────────┴──────────────┴──────────────┴──────────────┴──────────────┘
```

### 2.5 ddsi_udp_create_conn 的完整创建流程

```
ddsi_udp_create_conn(conn_out, factory, port, qos):
  │
  │  ┌──── 步骤 1: 确定 purpose → 设置四个布尔标志 ────┐
  │  │  reuse_addr, bind_to_any, set_mc_xmit_options   │
  │  └─────────────────────────────────────────────────┘
  │
  ├─ 步骤 2: 计算绑定地址
  │   if bind_to_any:
  │     socketname = 0.0.0.0:port     (或 [::]:port for IPv6)
  │   else:
  │     socketname = interface_ip:port (绑定到具体接口 IP)
  │
  ├─ 步骤 3: 创建 Socket
  │   ddsrt_socket(&sock, AF_INET, SOCK_DGRAM, 0)
  │
  ├─ 步骤 4: SO_REUSEADDR (仅 RECV_MC)
  │   if reuse_addr:
  │     ddsrt_setsockreuse(sock, true)
  │     → 允许同机器多个 DDS 进程共享组播端口
  │
  ├─ 步骤 5: 缓冲区配置 (所有用途)
  │   set_rcvbuf(sock, config.socket_rcvbuf_size)
  │   set_sndbuf(sock, config.socket_sndbuf_size)
  │
  ├─ 步骤 6: DontRoute (可选)
  │   if config.dontRoute:
  │     setsockopt(SO_DONTROUTE)
  │
  ├─ 步骤 7: IP_PKTINFO (可选, 接收用)
  │   if config.extended_packet_info:
  │     setsockopt(IP_PKTINFO)
  │     → 接收时可知数据包到达的接口和目标地址
  │
  ├─ 步骤 8: bind()
  │   bind(sock, socketname)
  │   ├─ RECV_UC: bind(0.0.0.0:7410)    → 接受所有来源
  │   ├─ RECV_MC: bind(0.0.0.0:7400)    → 接受所有来源, 多进程共享
  │   ├─ XMIT_UC: bind(192.168.1.1:0)   → 从指定接口发送, 随机端口
  │   └─ XMIT_MC: bind(192.168.1.1:0)   → 从指定接口发送, 随机端口
  │
  ├─ 步骤 9: 组播发送选项 (仅 XMIT_MC)
  │   if set_mc_xmit_options:
  │     setsockopt(IP_MULTICAST_IF,   interface_ip)  → 指定发送接口
  │     setsockopt(IP_MULTICAST_TTL,  config.ttl)    → 跳数限制
  │     setsockopt(IP_MULTICAST_LOOP, config.loop)   → 是否回环
  │
  ├─ 步骤 10: 初始化连接结构体
  │   conn->m_multicast = (purpose == RECV_MC)    → 标记为组播连接
  │   conn->m_read_fn   = ddsi_udp_conn_read      → 读函数
  │   conn->m_write_fn  = ddsi_udp_conn_write     → 写函数
  │   conn->m_port      = get_socket_port(sock)   → 实际端口
  │
  └─ 步骤 11: 注册到 ownaddrs 哈希表 (去重自发自收检测)
      ddsrt_hh_add_absent(fact->ownaddrs, &conn->m_addr)
```

### 2.6 四种调用场景的实际参数

```c
// ═══ 场景 1: 接收单播 Socket ═══
// ddsi_init.c:112-113
const struct ddsi_tran_qos qos = {
  .m_purpose   = DDSI_TRAN_QOS_RECV_UC,
  .m_diffserv  = 0,
  .m_interface  = NULL               // ← 不绑定接口
};
ddsi_factory_create_conn(&gv->disc_conn_uc,
                          gv->m_factory,
                          port_disc_uc,       // ← RTPS 规范端口 (如 7410)
                          &qos);
// → bind(0.0.0.0:7410), 无 SO_REUSEADDR, 无组播选项

// ═══ 场景 2: 接收组播 Socket ═══
// ddsi_init.c:635,646
const struct ddsi_tran_qos qos = {
  .m_purpose   = DDSI_TRAN_QOS_RECV_MC,
  .m_diffserv  = 0,
  .m_interface  = NULL               // ← 不绑定接口
};
ddsi_factory_create_conn(&disc,
                          gv->m_factory,
                          port_multi_disc,    // ← RTPS 组播端口 (如 7400)
                          &qos);
// → bind(0.0.0.0:7400), SO_REUSEADDR=true, 无组播选项
// 注意: 组播组加入 (IP_ADD_MEMBERSHIP) 不在此函数中, 在后续 joinleave_spdp_defmcip() 中

// ═══ 场景 3: 发送 Socket (接口支持组播) ═══
// ddsi_init.c:1558-1565
const struct ddsi_tran_qos qos = {
  .m_purpose   = DDSI_TRAN_QOS_XMIT_MC,
  .m_diffserv  = 0,
  .m_interface  = &gv->interfaces[i]  // ← 必须指定接口!
};
ddsi_factory_create_conn(&gv->xmit_conns[i],
                          fact,
                          0,                   // ← 端口=0, OS 随机分配!
                          &qos);
// → bind(192.168.1.1:random), IP_MULTICAST_IF + TTL + LOOP

// ═══ 场景 4: 发送 Socket (接口不支持组播) ═══
// ddsi_init.c:1558-1565 (allow_multicast=false 分支)
const struct ddsi_tran_qos qos = {
  .m_purpose   = DDSI_TRAN_QOS_XMIT_UC,      // ← 仅单播
  .m_diffserv  = 0,
  .m_interface  = &gv->interfaces[i]
};
ddsi_factory_create_conn(&gv->xmit_conns[i], fact, 0, &qos);
// → bind(10.0.0.1:random), 无组播选项
```

### 2.7 为什么这样设计？——收发 Socket 的根本差异

**核心矛盾**：接收和发送对 bind 地址有**相反的需求**。

```
┌─────────────────────────────────────────────────────────────────────┐
│  接收 Socket 的需求:                                                │
│    "我要接收发给任何地址的数据包"                                     │
│    → bind(0.0.0.0:known_port)                                      │
│    → 不绑定特定 IP → 无论目标地址是 192.168.1.1 还是 10.0.0.1       │
│      都能收到                                                       │
│    → 端口必须是 RTPS 规范规定的已知端口 (对端才知道发到哪)            │
│    → 不指定 m_interface (接收不区分来源接口)                         │
│                                                                     │
│  发送 Socket 的需求:                                                │
│    "我要从特定接口的 IP 地址发送数据包"                               │
│    → bind(interface_ip:0)                                           │
│    → 必须绑定特定 IP → 控制数据包的源 IP 地址                        │
│    → 端口可以随机 (对端不关心源端口)                                 │
│    → 必须指定 m_interface (不同接口的发送用不同 Socket)               │
└─────────────────────────────────────────────────────────────────────┘
```

**为什么接收 Socket 绑定 0.0.0.0 (INADDR_ANY)?**

```
机器有两个 IP: 192.168.1.1 (eth0), 10.0.0.1 (eth1)

如果 bind(192.168.1.1:7410):
  → 只能收到目标为 192.168.1.1 的包
  → 来自 10.0.0.x 网段直接发给 10.0.0.1:7410 的包 → 收不到!

如果 bind(0.0.0.0:7410):
  → 无论目标 IP 是 192.168.1.1 还是 10.0.0.1, 都能收到
  → 甚至 Fast-RTPS 错误地发往 127.0.0.1 的包也能收到

结论: 接收 Socket 必须绑定 INADDR_ANY 以最大化可达性
```

**为什么发送 Socket 绑定特定接口 IP?**

```
如果发送 Socket 也 bind(0.0.0.0:random):
  → 操作系统决定用哪个接口发送 → 不可控
  → 组播的 IP_MULTICAST_IF 可能与实际发送接口不匹配
  → Windows 上组播投递不可靠 (ddsi_domaingv.h:130-134 记录的 bug)

如果 bind(192.168.1.1:random):
  → 数据包源 IP 确定为 192.168.1.1 → 对端可以正确路由回复
  → 组播从 eth0 发出 → 与 IP_MULTICAST_IF 一致
  → 源 IP 与 SEDP 中广播的 locator 一致 → 对端匹配正确

结论: 发送 Socket 必须绑定具体接口以控制源 IP 和发送接口
```

**为什么 RECV_MC 需要 SO_REUSEADDR 而 RECV_UC 不需要?**

```
组播接收 (RECV_MC):
  同一机器上可能有 3 个 DDS 进程, 都在 domain 0
  全部需要 bind(0.0.0.0:7400)
  如果没有 SO_REUSEADDR → 只有第一个进程能 bind 成功
  → SO_REUSEADDR=true, 内核向所有绑定的 Socket 投递组播副本

单播接收 (RECV_UC):
  每个 participant 有自己的端口 (7410 + 2*ppid)
  正常情况下不会端口冲突
  如果冲突 → 应该报错 (EADDRINUSE), 而不是静默共享
  → SO_REUSEADDR=false, 端口冲突时快速失败
```

**为什么只有 XMIT_MC 设置组播发送选项?**

```
XMIT_MC: 接口支持组播 → 需要告诉内核:
  1. IP_MULTICAST_IF   → 从哪个接口发组播 (不设置则 OS 选, 可能选错)
  2. IP_MULTICAST_TTL  → 组播能跨越几个路由器 (默认 1, 通常需要更大)
  3. IP_MULTICAST_LOOP → 是否接收自己发的组播 (同机器多进程需要开启)

XMIT_UC: 接口不支持组播 → 所有数据走单播
  → 组播选项无意义, 设了也不会用

RECV_UC / RECV_MC: 接收 Socket 不用于发送
  → 组播发送选项无意义
  → 组播接收不需要 TTL/IF 选项 (这些控制的是发送行为)
  → 组播组加入 (IP_ADD_MEMBERSHIP) 在另一个流程中完成
```

### 2.8 ddsi_factory_create_conn 的前置断言——设计约束的形式化

```c
// ddsi__tran.h:371-372
// 这个断言编码了收发 Socket 的核心设计约束:
if ((qos->m_interface != NULL) != (qos->m_purpose == DDSI_TRAN_QOS_XMIT_UC ||
                                    qos->m_purpose == DDSI_TRAN_QOS_XMIT_MC))
  return DDS_RETCODE_BAD_PARAMETER;
```

翻译成自然语言：
- **发送 Socket 必须指定 m_interface**：因为发送 Socket 绑定到接口 IP
- **接收 Socket 不得指定 m_interface**：因为接收 Socket 绑定到 INADDR_ANY

这是一个**编译期不可检查、运行时立即检查**的设计约束。将约束编码在工厂函数的入口处，确保任何新的调用者都不会违反收发 Socket 的根本差异。

**设计哲学：契约式编程（Design by Contract）**。函数入口处的断言是调用者与被调用者之间的"合同条款"——发送方必须提供接口信息，接收方不得提供。违反合同立即返回错误，而不是静默创建一个行为不正确的 Socket。

### 2.9 设计总结: 为什么不能用一种 Socket 同时收发？

```
历史上 (CycloneDDS 早期版本):
  data_conn_uc 同时用于接收和发送
  → bind(0.0.0.0:7411) → 源 IP 由 OS 决定

问题 (ddsi_domaingv.h:127-149 完整记录):

  1. Windows 组播投递 bug:
     绑定 0.0.0.0 的 Socket 发送组播 →
     同机器上绑定了具体 IP 的 Socket (如 Fast-RTPS 的)
     导致 Windows 内核路由混乱 → 组播不可靠

  2. Fast-RTPS 互操作 bug:
     Fast-RTPS 收到 CycloneDDS 的 locator (如 192.168.1.1:7411)
     → 用 127.0.0.1:7411 替换 → 发送
     → 如果 CycloneDDS bind(192.168.1.1:7411) → 收不到

  矛盾:
     bind(0.0.0.0)     → 问题 1 无解
     bind(specific_ip) → 问题 2 无解

  解决:
     接收 Socket: bind(0.0.0.0:known_port)  → 解决问题 2
     发送 Socket: bind(specific_ip:random)   → 解决问题 1
     → 两类 Socket 各解决一个问题, 代价是多几个文件描述符

结论: 收发分离不是"优雅的设计", 而是对现实世界 bug 的工程妥协。
      代码注释明确写道: "It is rather sad that Cyclone needs to work
      around the bugs of the others, but it seems the only way to get
      the users what they expect."
```

## 3. 接收 Socket 架构

### 2.1 四个接收 Socket

CycloneDDS 创建 **4 个接收 Socket**（在某些配置下可能复用为更少）：

```
┌─────────────────────────────────────────────────────────────────┐
│                      接收 Socket 布局                            │
├──────────────┬──────────────┬────────────────┬──────────────────┤
│ Socket 名称   │ 变量名        │ 端口           │ 职责              │
├──────────────┼──────────────┼────────────────┼──────────────────┤
│ Discovery UC │ disc_conn_uc │ port_base +    │ SPDP/SEDP 单播   │
│              │              │ dg*domain +    │ 接收              │
│              │              │ d1 + pg*ppidx  │                  │
├──────────────┼──────────────┼────────────────┼──────────────────┤
│ Data UC      │ data_conn_uc │ port_base +    │ 用户数据单播接收  │
│              │              │ dg*domain +    │                  │
│              │              │ d3 + pg*ppidx  │                  │
├──────────────┼──────────────┼────────────────┼──────────────────┤
│ Discovery MC │ disc_conn_mc │ port_base +    │ SPDP/SEDP 组播   │
│              │              │ dg*domain + d0 │ 接收              │
├──────────────┼──────────────┼────────────────┼──────────────────┤
│ Data MC      │ data_conn_mc │ port_base +    │ 用户数据组播接收  │
│              │              │ dg*domain + d2 │                  │
└──────────────┴──────────────┴────────────────┴──────────────────┘

默认端口公式 (RTPS §9.6.1):
  Discovery MC: 7400 + 250 × domain_id + 0
  Data MC:      7400 + 250 × domain_id + 1
  Discovery UC: 7400 + 250 × domain_id + 10 + 2 × participant_index
  Data UC:      7400 + 250 × domain_id + 11 + 2 × participant_index
```

### 2.2 Socket 创建代码路径

```
ddsi_init()
  │
  ├─ make_uc_sockets(gv, &port_disc_uc, &port_data_uc, ppid)
  │   │                                         [ddsi_init.c:85-138]
  │   ├─ 计算 discovery 单播端口: ddsi_get_port(DDSI_PORT_UNI_DISC)
  │   ├─ 计算 data 单播端口:     ddsi_get_port(DDSI_PORT_UNI_DATA)
  │   │
  │   ├─ ddsi_factory_create_conn(&disc_conn_uc, port_disc, QOS_RECV_UC)
  │   │   → 创建 Discovery 单播 Socket, 绑定到 disc 端口
  │   │
  │   ├─ if (port_data == port_disc):
  │   │   │ data_conn_uc = disc_conn_uc          ← 端口相同时复用!
  │   │   └─ 只创建 1 个 Socket 而非 2 个
  │   │
  │   └─ else:
  │       └─ ddsi_factory_create_conn(&data_conn_uc, port_data, QOS_RECV_UC)
  │           → 创建 Data 单播 Socket
  │
  └─ create_multicast_sockets(gv)
      │                                         [ddsi_init.c:633-676]
      ├─ ddsi_factory_create_conn(&disc, port_multi_disc, QOS_RECV_MC)
      │   → 创建 Discovery 组播 Socket, SO_REUSEADDR=true
      │
      ├─ if (MSM_NO_UNICAST):
      │   │ data = disc                          ← NO_UNICAST 模式下复用!
      │   └─ 组播 Socket 只创建 1 个
      │
      └─ else:
          └─ ddsi_factory_create_conn(&data, port_multi_data, QOS_RECV_MC)
              → 创建 Data 组播 Socket
```

### 2.3 Socket 复用（别名）规则

```
正常模式 (SINGLE_UNICAST):
  disc_conn_uc ──→ [Socket A, port=7410]  独立
  data_conn_uc ──→ [Socket B, port=7411]  独立
  disc_conn_mc ──→ [Socket C, port=7400]  独立
  data_conn_mc ──→ [Socket D, port=7401]  独立
  → 4 个独立 Socket

NO_UNICAST 模式:
  disc_conn_uc ──→ [Socket A, port=7400]  ← 复用组播端口
  data_conn_uc ──→ [Socket A]             ← 与 disc_conn_uc 相同
  disc_conn_mc ──→ [Socket B, port=7400]  
  data_conn_mc ──→ [Socket B]             ← 与 disc_conn_mc 相同
  → 最少 2 个 Socket

端口相同时:
  如果 port_disc_uc == port_data_uc:
    data_conn_uc = disc_conn_uc             ← 自动复用
  → 3 个 Socket
```

## 3. 接收线程与 Socket 的映射

### 3.1 线程配置

```c
// ddsi_init.c:845-863
static bool use_multiple_receive_threads (const struct ddsi_config *cfg)
{
  switch (cfg->multiple_recv_threads) {
    case DDSI_BOOLDEF_FALSE:
    case DDSI_BOOLDEF_DEFAULT:
      return false;   // 默认关闭: 防火墙常阻断自中断包
    case DDSI_BOOLDEF_TRUE:
      return true;
  }
}
```

### 3.2 三种线程配置

```
配置 1: 单线程模式 (默认, MultipleReceiveThreads=false)
┌──────────────────────────────────────────────────────┐
│ Thread "recv" (RTM_MANY, sock_waitset 多路复用)       │
│   ├─ disc_conn_uc ──┐                                │
│   ├─ data_conn_uc   ├─ select/poll 多路复用           │
│   ├─ disc_conn_mc   │                                │
│   ├─ data_conn_mc ──┘                                │
│   └─ xmit_conns[0..n] (见下方 §3.2.1 说明)           │
└──────────────────────────────────────────────────────┘
→ 1 个线程处理所有 Socket


配置 2: 双线程模式 (MultipleReceiveThreads=true, ASM 组播)
┌──────────────────────────────────────────────────────┐
│ Thread "recv" (RTM_MANY, sock_waitset)               │
│   ├─ disc_conn_uc ──┐                                │
│   ├─ disc_conn_mc   ├─ 复用 (Discovery + UC data)    │
│   ├─ data_conn_uc ──┘                                │
│   └─ xmit_conns[0..n]                               │
├──────────────────────────────────────────────────────┤
│ Thread "recvMC" (RTM_SINGLE, 专用)                   │
│   └─ data_conn_mc ────── 直接 recvfrom() 阻塞读取    │
└──────────────────────────────────────────────────────┘
→ data_conn_mc 从 waitset 中移除, 由专用线程独占


配置 3: 三线程模式 (MultipleReceiveThreads=true, SINGLE_UNICAST)
┌──────────────────────────────────────────────────────┐
│ Thread "recv" (RTM_MANY, sock_waitset)               │
│   ├─ disc_conn_uc ──┐                                │
│   ├─ disc_conn_mc   ├─ 复用 (仅 Discovery)           │
│   └─ xmit_conns[0..n]                               │
├──────────────────────────────────────────────────────┤
│ Thread "recvMC" (RTM_SINGLE, 专用)                   │
│   └─ data_conn_mc ────── 专用组播数据                 │
├──────────────────────────────────────────────────────┤
│ Thread "recvUC" (RTM_SINGLE, 专用)                   │
│   └─ data_conn_uc ────── 专用单播数据                 │
└──────────────────────────────────────────────────────┘
→ Discovery 走 waitset 线程, Data 单播/组播各有专用线程
```

### 3.2.1 为什么 xmit_conns（发送 Socket）也加入 recv 线程的 waitset？

**关键澄清：数据发送不由 recv 线程执行。** 发送始终在应用线程或事件线程中完成（`dds_write()` → `ddsi_conn_write()`）。recv 线程只负责 `recvfrom()` → RTPS 解析 → 投递到 RHC。

xmit_conns 加入 waitset 的原因是**在发送 socket 上接收对端发来的数据**。

**根本原因**：UDP socket 是双向的——虽然我们创建 xmit_conns 是为了发送，但 UDP 没有"只发不收"的 socket。对端可能会往发送 socket 的端口发送数据。

**触发场景**（代码注释 `ddsi_receive.c:3618-3619`）：

```c
// OpenDDS doesn't respect the locator lists and insists on sending to the
// socket it received packets from
```

**问题详解**：

```
正常 RTPS 流程（规范行为）:
  CycloneDDS 通过 SPDP/SEDP 告诉 OpenDDS:
    "请把数据发到我的接收 socket: disc_conn_uc 端口 7410, data_conn_uc 端口 7411"

  OpenDDS 应该: sendto(7410/7411)  →  CycloneDDS 的接收 socket ✓

OpenDDS 的实际行为（不规范）:
  OpenDDS 从 xmit_conns[0]（端口=50123, 随机）收到 CycloneDDS 的数据包
  OpenDDS 回复到: sendto(50123)  →  CycloneDDS 的发送 socket!
  
  如果 CycloneDDS 不监听 xmit_conns[0]:
    → OpenDDS 的回复全部丢失
    → Discovery/数据传输彻底失败
```

**解决方案**：

```
recv 线程 waitset:
  disc_conn_uc   ← 正常单播接收 (规范行为的对端)
  disc_conn_mc   ← 正常组播接收
  data_conn_uc   ← 正常单播接收
  data_conn_mc   ← 正常组播接收
  xmit_conns[0]  ← OpenDDS 等不规范实现的回复    ← 互操作补丁
  xmit_conns[1]  ← 同上（多网卡时）
  ...

所有 socket 上收到的数据都进入同一个 do_packet() 处理流程，
经过 RTPS Header 校验 → submessage 分发 → RHC 投递，
无论数据从哪个 socket 进来，处理逻辑完全相同。
```

**设计哲学**：**防御性接收**。CycloneDDS 无法控制对端的行为，但可以确保无论数据从哪个 socket 到达，都能正确处理。这是以极小的代价（waitset 多监听几个 fd）换取与不规范实现的互操作性。

### 3.3 线程分配代码

```c
// ddsi_init.c:865-910
static int setup_and_start_recv_threads (struct ddsi_domaingv *gv)
{
  // Thread 0: 始终存在, 使用 waitset 多路复用
  gv->n_recv_threads = 1;
  gv->recv_threads[0].name = "recv";
  gv->recv_threads[0].arg.mode = DDSI_RTM_MANY;

  if (connless && !NO_UNICAST && multi_recv_thr)
  {
    // Thread 1: 可选, 专用于 data 组播
    if (ASM 组播已启用 && 非 SSM 地址)
    {
      gv->recv_threads[n].name = "recvMC";
      gv->recv_threads[n].arg.mode = DDSI_RTM_SINGLE;
      gv->recv_threads[n].arg.u.single.conn = gv->data_conn_mc;
      ddsi_conn_disable_multiplexing(gv->data_conn_mc);  // 从 waitset 移除
      n++;
    }
    // Thread 2: 可选, 专用于 data 单播
    if (SINGLE_UNICAST 模式)
    {
      gv->recv_threads[n].name = "recvUC";
      gv->recv_threads[n].arg.mode = DDSI_RTM_SINGLE;
      gv->recv_threads[n].arg.u.single.conn = gv->data_conn_uc;
      ddsi_conn_disable_multiplexing(gv->data_conn_uc);
      n++;
    }
  }

  // 为每个线程创建 rbufpool + 启动线程
  for (i = 0; i < n_recv_threads; i++) {
    gv->recv_threads[i].arg.rbpool = ddsi_rbufpool_new(...);
    if (RTM_MANY) gv->recv_threads[i].arg.u.many.ws = ddsi_sock_waitset_new();
    ddsi_create_thread(&gv->recv_threads[i].thrst, "recv", ddsi_recv_thread, &arg);
  }
}
```

### 3.4 RTM_MANY 线程的 waitset 构建

```c
// ddsi_receive.c 接收线程主函数中:
// RTM_MANY 模式下, 将所有未被专用线程占用的 Socket 加入 waitset

recv_thread_waitset_add_conn(waitset, gv->disc_conn_uc);   // Discovery UC
recv_thread_waitset_add_conn(waitset, gv->data_conn_uc);   // Data UC (如未被 recvUC 占用)
recv_thread_waitset_add_conn(waitset, gv->disc_conn_mc);   // Discovery MC
recv_thread_waitset_add_conn(waitset, gv->data_conn_mc);   // Data MC (如未被 recvMC 占用)
for (i = 0; i < n_interfaces; i++)
  recv_thread_waitset_add_conn(waitset, gv->xmit_conns[i]); // 发送 Socket (接收响应)

// recv_thread_waitset_add_conn 的关键逻辑:
//   如果该 conn 已经分配给某个 RTM_SINGLE 线程 → 跳过, 不加入 waitset
//   否则 → 加入 waitset
```

## 4. 发送 Socket 架构

### 4.1 发送 Socket 创建

```
发送 Socket 与接收 Socket 完全独立, 专用于发送。

原因 (ddsi_domaingv.h:127-149 注释):
  1. Windows 怪癖: 绑定到 0.0.0.0 的 Socket 发送组播时, 在某些条件下
     同机器上的其他 Socket 无法可靠收到
  2. Fast-RTPS/Connext 的 bug: 它们用 127.0.0.1 替换广播的地址,
     如果接收 Socket 绑定到特定 IP → 收不到
  3. 解决方案: 用独立的 Socket 发送, 接收 Socket 继续绑定到 0.0.0.0/INADDR_ANY
```

```
┌──────────────────────────────────────────────────────────────┐
│              发送 Socket 布局                                  │
│                                                              │
│  xmit_conns[0] ──→ [Socket, 绑定 interface[0] IP, 随机端口]  │
│  xmit_conns[1] ──→ [Socket, 绑定 interface[1] IP, 随机端口]  │
│  xmit_conns[2] ──→ [Socket, 绑定 interface[2] IP, 随机端口]  │
│  xmit_conns[3] ──→ [Socket, 绑定 interface[3] IP, 随机端口]  │
│                                                              │
│  最多 MAX_XMIT_CONNS=4 个, 每个网络接口一个                    │
│  端口 = 0 (操作系统随机分配)                                   │
│                                                              │
│  intf_xlocators[i].conn = xmit_conns[i]  ← 接口到 Socket 映射│
│  intf_xlocators[i].c    = interfaces[i].loc ← 接口地址        │
└──────────────────────────────────────────────────────────────┘
```

### 4.2 发送 Socket 创建代码

```c
// ddsi_init.c:1546-1576
// 为每个网络接口创建一个发送 Socket

for (size_t i = 0; i < MAX_XMIT_CONNS; i++)
  gv->xmit_conns[i] = NULL;

if (MSM_NO_UNICAST) {
  gv->xmit_conns[0] = gv->data_conn_uc;   // 复用接收 Socket
} else {
  for (int i = 0; i < gv->n_interfaces; i++) {
    const struct ddsi_tran_qos qos = {
      .m_purpose = interfaces[i].allow_multicast
                   ? DDSI_TRAN_QOS_XMIT_MC    // 支持组播 → 设置组播选项
                   : DDSI_TRAN_QOS_XMIT_UC,   // 不支持 → 仅单播选项
      .m_diffserv = 0,
      .m_interface = &gv->interfaces[i]       // 绑定到特定接口
    };
    ddsi_factory_create_conn(&gv->xmit_conns[i], fact, 0/*随机端口*/, &qos);
  }
}

// 建立 接口 ↔ 发送Socket 的映射表
for (int i = 0; i < gv->n_interfaces; i++) {
  gv->intf_xlocators[i].conn = gv->xmit_conns[i];
  gv->intf_xlocators[i].c    = gv->interfaces[i].loc;
  gv->intf_xlocators[i].c.port = ddsi_conn_port(gv->xmit_conns[i]);
}
```

### 4.3 发送时的 Socket 选择算法

发送 Socket 的选择分为 **两个阶段**：

```
阶段 1: 发现时预选 (Discovery-Time, 一次性计算)
═══════════════════════════════════════════════

远端 Locator 出现 (通过 SEDP 发现)
  │
  └─ ddsi_add_locator_to_addrset(gv, addrset, remote_locator)
      │                                      [ddsi_addrset.c:216-262]
      │
      ├─ 如果目标是组播地址:
      │   → 为每个网络接口都创建 xlocator
      │   for (i = 0; i < n_interfaces; i++) {
      │     xlocator = { .conn = xmit_conns[i], .c = remote_locator };
      │     add_to_addrset(xlocator);
      │   }
      │   → 发送组播时, 从每个接口都发一份
      │
      └─ 如果目标是单播地址:
          → 选择最佳匹配的接口
          for (i = 0; i < n_interfaces; i++) {
            switch (ddsi_is_nearby_address(gv, remote_locator, interfaces)) {
              case DNAR_SELF:    // 目标是自己 → 精确匹配
              case DNAR_LOCAL:   // 目标在同子网 → 精确匹配
                interf_idx = i;
                break;
              case DNAR_DISTANT: // 不在同子网 → 记为候选
                fallback_interf_idx = i;
                break;
              case DNAR_UNREACHABLE: // 不可达 → 跳过
                break;
            }
          }
          → xlocator = { .conn = xmit_conns[best_idx], .c = remote_locator };
          → add_to_addrset(xlocator);


阶段 2: 运行时发送 (Runtime, 零开销)
═════════════════════════════════════

dds_write()
  → 构建 xmsg, 设置目标
  → ddsi_xpack_send()
     │
     ├─ 遍历 addrset 中的每个 xlocator:
     │   for each (xlocator in addrset) {
     │     ddsi_conn_write(xlocator.conn,    ← 直接使用预选的 Socket!
     │                     &xlocator.c,       ← 直接使用目标地址!
     │                     msgfrags, flags);
     │   }
     │
     └─ → 运行时零决策开销, 所有路由已在发现阶段完成
```

### 4.4 xmsg 目标模式

```c
enum ddsi_xmsg_dstmode {
  NN_XMSG_DST_UNSET,   // 未设置目标
  NN_XMSG_DST_ONE,     // 发送到单个 peer (ddsi_xlocator_t: conn + locator)
  NN_XMSG_DST_ALL,     // 发送到所有订阅者 (addrset: MC + UC)
  NN_XMSG_DST_ALL_UC,  // 发送到所有单播订阅者 (addrset: 仅 UC)
};

使用场景:
  DST_ONE   → HEARTBEAT 发给特定 Reader
  DST_ALL   → DATA 发给所有匹配的 Reader (组播 + 单播)
  DST_ALL_UC → 组播不可用时退化为逐个单播
```

## 5. 组播 Socket 配置

### 5.1 组播接收 Socket 创建时的选项

```c
// ddsi_udp.c: ddsi_udp_create_conn()
// 组播接收 Socket (QOS_RECV_MC) 的特殊处理:

1. SO_REUSEADDR = true
   → 允许多个进程在同一端口上接收组播
   → 同一机器上多个 DDS 进程可以共存

2. 绑定到 INADDR_ANY (0.0.0.0)
   → 不绑定到特定接口, 接收所有接口上的组播
   → bind_to_any = true

3. 接收缓冲区: setsockopt(SO_RCVBUF, config.socket_rcvbuf_size)
   → 可配置, 默认通常为 1MB
   → 缓冲高峰期的突发数据

4. IP_PKTINFO / IPV6_RECVPKTINFO (可选)
   → 接收每个数据包的额外信息: 源接口、目标地址
   → 用于多接口场景下判断数据来自哪个接口
```

### 5.2 组播发送 Socket 创建时的选项

```c
// ddsi_udp.c: IPv4 组播发送选项 (set_mc_options_transmit_ipv4)

1. IP_MULTICAST_IF = interface[i].address
   → 指定组播数据从哪个网络接口发出
   → 每个 xmit_conn 绑定到特定接口
   → Linux 支持 ip_mreqn (带接口索引), 更可靠

2. IP_MULTICAST_TTL = config.multicast_ttl
   → 组播生存时间 (跳数限制)
   → 默认通常为 32
   → 控制组播可以传播多远

3. IP_MULTICAST_LOOP = config.enableMulticastLoopback
   → 是否接收自己发送的组播包
   → 同机器上有多个 Participant 时需要开启
   → 默认: 开启

// IPv6 对应选项:
//   IPV6_MULTICAST_IF   = interface[i].if_index
//   IPV6_MULTICAST_HOPS = multicast_ttl
//   IPV6_MULTICAST_LOOP = enableMulticastLoopback
```

### 5.3 组播组加入流程

```
ddsi_init() 完成 Socket 创建后:
  │
  └─ joinleave_spdp_defmcip(gv, join=1)
      │                               [ddsi_init.c:596-631]
      │
      ├─ 检查哪些接口允许组播:
      │   for each interface:
      │     if allow_multicast & DDSI_AMC_SPDP → include_spdp = true
      │     if allow_multicast & ~DDSI_AMC_SPDP → include_default = true
      │
      ├─ 构建要加入的组播地址集:
      │   if include_spdp  → add loc_spdp_mc (例: 239.255.0.1)
      │   if include_default → add loc_default_mc (例: 239.255.0.1)
      │
      └─ 对每个组播地址调用 joinleave_spdp_defmcip_helper():
          │
          ├─ ddsi_join_mc(gv, mship, disc_conn_mc, NULL, &mc_addr)
          │   → Discovery 组播 Socket 加入组播组
          │
          └─ ddsi_join_mc(gv, mship, data_conn_mc, NULL, &mc_addr)
              → Data 组播 Socket 加入组播组
```

### 5.4 每接口组播组加入

```c
// ddsi_mcgroup.c:192-235
// 核心: 对每个允许组播的网络接口, 分别调用 IP_ADD_MEMBERSHIP

static int joinleave_mcgroups(gv, conn, join, srcloc, mcloc)
{
  switch (gv->recvips_mode) {
    case RECVIPS_MODE_ANY:
      // 不指定接口, 使用 OS 默认
      joinleave_mcgroup(conn, join, srcloc, mcloc, NULL);
      break;

    case RECVIPS_MODE_ALL:
    case RECVIPS_MODE_PREFERRED:
      // 在每个允许组播的接口上都加入
      for (i = 0; i < n_interfaces; i++) {
        if (interfaces[i].allow_multicast) {
          joinleave_mcgroup(conn, join, srcloc, mcloc, &interfaces[i]);
          //                                            ^^^^^^^^^^^^^^
          // 传入具体接口 → IP_ADD_MEMBERSHIP 的 imr_interface 字段
        }
      }
      break;
  }
}
```

### 5.5 底层 IP_ADD_MEMBERSHIP 调用

```c
// ddsi_udp.c:779-830
// IPv4 ASM 组播加入:
static int joinleave_asm_mcgroup(socket, join, mcloc, interf)
{
  struct ip_mreq mreq;
  mreq.imr_multiaddr = multicast_group_address;  // 组播组地址
  if (interf)
    mreq.imr_interface = interf->address;          // 指定接口 IP
  else
    mreq.imr_interface = INADDR_ANY;               // OS 选择接口

  setsockopt(socket, IPPROTO_IP,
             join ? IP_ADD_MEMBERSHIP : IP_DROP_MEMBERSHIP,
             &mreq, sizeof(mreq));
}

// IPv6 ASM 组播加入:
  struct ipv6_mreq mreq;
  mreq.ipv6mr_multiaddr = multicast_group_address;
  mreq.ipv6mr_interface = interf->if_index;        // IPv6 用接口索引

  setsockopt(socket, IPPROTO_IPV6,
             join ? IPV6_JOIN_GROUP : IPV6_LEAVE_GROUP,
             &mreq, sizeof(mreq));

// SSM (Source-Specific Multicast) 加入:
  struct ip_mreq_source mreq;
  mreq.imr_sourceaddr  = source_address;           // 指定源地址
  mreq.imr_multiaddr   = multicast_group_address;
  mreq.imr_interface   = interf->address;

  setsockopt(socket, IPPROTO_IP,
             join ? IP_ADD_SOURCE_MEMBERSHIP : IP_DROP_SOURCE_MEMBERSHIP,
             &mreq, sizeof(mreq));
```

### 5.6 组播组成员引用计数

```c
// ddsi_mcgroup.c: 防止重复加入/过早离开

struct ddsi_mcgroup_membership_node {
  struct ddsi_tran_conn *conn;    // 哪个 Socket
  ddsi_locator_t srcloc;          // 源地址 (SSM)
  ddsi_locator_t mcloc;          // 组播组地址
  unsigned count;                 // 引用计数
};

ddsi_join_mc():
  if (reg_group_membership 返回 false):
    → 已经加入过 → 只增加 count, 不重复调用 setsockopt
  else:
    → 第一次加入 → 调用 setsockopt(IP_ADD_MEMBERSHIP)

ddsi_leave_mc():
  if (unreg_group_membership 返回 false):
    → count > 0 → 只减少 count, 不调用 setsockopt
  else:
    → count 减到 0 → 调用 setsockopt(IP_DROP_MEMBERSHIP)
```

## 6. 完整 Socket 生命周期总图

```
┌─────────────────────────────────────────────────────────────────────┐
│                    ddsi_init() Socket 创建时序                       │
│                                                                     │
│  1. make_uc_sockets()                                               │
│     ├─ disc_conn_uc = socket(DGRAM) + bind(disc_port)              │
│     └─ data_conn_uc = socket(DGRAM) + bind(data_port) [或复用]      │
│                                                                     │
│  2. create_multicast_sockets()                                      │
│     ├─ disc_conn_mc = socket(DGRAM) + SO_REUSEADDR + bind(mc_disc) │
│     └─ data_conn_mc = socket(DGRAM) + SO_REUSEADDR + bind(mc_data) │
│                                                                     │
│  3. 创建发送 Socket (每接口一个)                                      │
│     ├─ xmit_conns[0] = socket(DGRAM) + bind(intf[0], 随机端口)      │
│     │   + IP_MULTICAST_IF + IP_MULTICAST_TTL + IP_MULTICAST_LOOP    │
│     ├─ xmit_conns[1] = socket(DGRAM) + bind(intf[1], 随机端口)      │
│     └─ ...                                                          │
│                                                                     │
│  4. 加入组播组 (在每个接口上)                                         │
│     ├─ disc_conn_mc: IP_ADD_MEMBERSHIP(SPDP_MC, intf[0])           │
│     ├─ disc_conn_mc: IP_ADD_MEMBERSHIP(SPDP_MC, intf[1])           │
│     ├─ data_conn_mc: IP_ADD_MEMBERSHIP(DEFAULT_MC, intf[0])        │
│     └─ data_conn_mc: IP_ADD_MEMBERSHIP(DEFAULT_MC, intf[1])        │
│                                                                     │
│  5. 分配接收线程                                                     │
│     ├─ Thread "recv":   waitset{disc_uc, disc_mc, [data_uc/mc],    │
│     │                           xmit_conns[0..n]}                   │
│     ├─ Thread "recvMC": data_conn_mc (可选, 专用)                   │
│     └─ Thread "recvUC": data_conn_uc (可选, 专用)                   │
└─────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────┐
│                    运行时数据流                                      │
│                                                                     │
│  发送:                                                              │
│    dds_write(data)                                                  │
│      → 序列化 → xmsg → xpack                                       │
│      → 查 addrset → 取 xlocator (预选的 conn + 目标地址)            │
│      → ddsi_conn_write(xmit_conns[i], dst_locator, msgfrags)       │
│      → sendmsg(sock, iov, dst_addr)                                │
│                                                                     │
│  接收:                                                              │
│    recv_thread: select/poll → 有数据                                 │
│      → ddsi_conn_read(conn, rbuf)                                   │
│      → recvmsg(sock, &msghdr)                                      │
│      → ddsi_handle_rtps_message(rmsg)                               │
│      → 分发到 RHC                                                   │
└─────────────────────────────────────────────────────────────────────┘
```

## 7. 设计逻辑与设计思想

### 7.1 为什么接收 Socket 和发送 Socket 完全分离？

**设计哲学：用额外 Socket 换取跨实现互操作性**

源码注释 (`ddsi_domaingv.h:127-149`) 明确记录了这个设计的原因：

```
问题根源:
  1. Windows: 绑定到 0.0.0.0 的 Socket 发送组播 → 同机器的其他 Socket
     在特定条件下无法可靠收到 (Windows 内核的路由行为)
  2. Fast-RTPS: 用 127.0.0.1 替换对端广播的地址, 然后向 127.0.0.1 发送
     → 如果接收 Socket 绑定到特定 IP → 收不到
  3. Connext: 类似 Fast-RTPS 的行为

矛盾:
  - 绑定到 0.0.0.0 → Fast-RTPS/Connext 的单播能收到, 但 Windows 组播不可靠
  - 绑定到特定 IP → Windows 组播可靠, 但 Fast-RTPS/Connext 的单播收不到

解决: 接收 Socket 绑定到 0.0.0.0, 发送 Socket 绑定到特定接口 IP
  → 两个问题同时解决, 代价只是多几个 Socket
```

**底层思想**：**实际工程中的设计决策往往是对其他实现 bug 的妥协**。RTPS 规范没有要求分离收发 Socket，但现实中的互操作需求迫使 CycloneDDS 用额外资源（几个 Socket 描述符）换取正确性。这是**防御性编程的系统级应用**。

### 7.2 为什么默认只用一个接收线程？

**设计哲学：简单可靠优先于理论最优**

源码注释 (`ddsi_init.c:851-853`):

```c
// Too many people run into trouble with firewalls blocking the packets
// Cyclone sends to itself for interrupting the blocking reads.
```

多线程接收需要一种方式**中断阻塞在 `recvfrom()` 上的线程**（例如在关闭时）。CycloneDDS 的做法是向自己的 Socket 发一个唤醒包。但防火墙规则经常**阻断这种自发自收的包**，导致线程无法被唤醒——进程无法干净退出。

单线程 + `select/poll` 的方式规避了这个问题：
- `select()` 的超时或 pipe 中断不需要网络包
- 一个线程处理所有 Socket，避免了跨线程唤醒问题

**底层思想**：**默认配置应该在最苛刻的环境（严格防火墙）下都能工作**。高性能模式（多线程）作为可选的 opt-in 配置。这是"开箱即用"原则——用户不应该为了基本功能而调整防火墙规则。

### 7.3 为什么要为组播数据创建专用接收线程？

**设计哲学：隔离高吞吐数据流，保护低延迟控制流**

当多线程模式开启时，`data_conn_mc` 被分配给专用的 "recvMC" 线程：

```
问题场景:
  Publisher 以 100MB/s 的速率发送组播数据
  同时 SPDP 每 30 秒发送一个 ~200 字节的消息

  如果所有 Socket 在同一个 select() 中:
    select() 返回 → 大概率是 data_conn_mc 可读
    → 处理大量数据包
    → disc_conn_mc 上的 SPDP 消息可能要等数毫秒才被处理
    → Discovery 延迟增加

  分离后:
    "recvMC" 线程: 专门处理 data_conn_mc 的高速数据流
    "recv" 线程:   处理 Discovery Socket → 立即响应 SPDP/SEDP
    → Discovery 延迟不受数据速率影响
```

**底层思想**：这是 **QoS 隔离（Quality of Service Isolation）**的实现。网络协议栈中，控制面流量（路由协议、ARP）和数据面流量分离是基本原则。CycloneDDS 通过线程分离实现了应用层的控制/数据面隔离。

### 7.4 为什么组播 Socket 需要 SO_REUSEADDR？

**设计哲学：允许同一机器上多个 DDS 进程共存**

```
场景: 同一台机器上运行 3 个 DDS 应用, 都在 domain 0

  如果没有 SO_REUSEADDR:
    App1: bind(7400) → 成功
    App2: bind(7400) → EADDRINUSE 失败!
    App3: bind(7400) → EADDRINUSE 失败!
    → 只有第一个应用能创建组播 Socket

  有 SO_REUSEADDR:
    App1: bind(7400) → 成功
    App2: bind(7400) → 成功
    App3: bind(7400) → 成功
    → 三个应用都能接收组播数据 (内核向所有绑定的 Socket 投递副本)
```

组播 Socket 必须开启 `SO_REUSEADDR`（或 `SO_REUSEPORT`），否则同一 domain 中只有第一个启动的进程能工作。

**底层思想**：这是组播协议的基本要求——组播语义是"一对多"，接收端应该能有多个。`SO_REUSEADDR` 让内核的组播投递语义与应用层一致。

### 7.5 为什么发送 Socket 选择在发现阶段预计算？

**设计哲学：热路径零决策**

```
方案 A: 运行时选择 (每次 write 都路由)
  dds_write()
    → for each dest: which_interface(dest) → O(n_interfaces) 比较
    → 每秒可能执行百万次

方案 B: 发现时预选 (一次计算, 缓存结果)
  SEDP 发现远端 → ddsi_add_locator_to_addrset()
    → ddsi_is_nearby_address() → 选择 xmit_conns[i]
    → 存入 xlocator.conn
  dds_write()
    → 直接用 xlocator.conn → O(1) 查表
    → 端点的整个生命周期只计算一次
```

发现事件是低频的（秒/分钟级），而 `dds_write()` 可能是微秒级。将路由决策移到低频路径上，是典型的**预计算优化**。

**底层思想**：**Amortize expensive computations over cheap operations**。这与 IP 路由中的"转发表缓存"同源——路由计算由控制面完成（低频），数据面只做查表转发（高频）。`ddsi_xlocator_t` 就是 DDS 的"转发表条目"。

### 7.6 为什么组播发送要从每个接口都发一份？

**设计哲学：覆盖所有潜在的接收路径**

```c
// ddsi_addrset.c:222-229
if (ddsi_is_mcaddr(gv, loc)) {
  for (int i = 0; i < gv->n_interfaces; i++) {
    add_xlocator_to_addrset(gv, as, &{
      .conn = gv->xmit_conns[i],  // 从每个接口都发
      .c = *loc
    });
  }
}
```

原因：组播的可达性取决于**发送接口**。如果一台机器有两个网络接口（`eth0: 192.168.1.x`, `eth1: 10.0.0.x`），组播只从 `eth0` 发出 → `10.0.0.x` 网段的接收者收不到。

```
机器 A (双网卡):
  eth0 (192.168.1.1) ──→ 组播到 239.255.0.1 ──→ 网段 1 的接收者 ✓
  eth1 (10.0.0.1)    ──→ 组播到 239.255.0.1 ──→ 网段 2 的接收者 ✓

如果只从 eth0 发:
  → 网段 2 的接收者收不到
```

**底层思想**：**组播是链路层行为, 不跨接口**。每个网络接口对应一个独立的组播域。要覆盖所有网络分区，必须从每个接口都发一份。这与 OSPF 路由协议在每个接口上独立运行 Hello 协议的原理相同。

### 7.7 为什么需要组播组成员的引用计数？

**设计哲学：多对一映射的安全管理**

```
场景: 同一 Domain 中有 3 个 Writer, 都使用组播地址 239.255.0.1

  Writer1 创建 → ddsi_join_mc(239.255.0.1) → IP_ADD_MEMBERSHIP → count=1
  Writer2 创建 → ddsi_join_mc(239.255.0.1) → 已加入, count=2
  Writer3 创建 → ddsi_join_mc(239.255.0.1) → 已加入, count=3

  Writer1 删除 → ddsi_leave_mc(239.255.0.1) → count=2, 不退出组播组
  Writer2 删除 → ddsi_leave_mc(239.255.0.1) → count=1, 不退出组播组
  Writer3 删除 → ddsi_leave_mc(239.255.0.1) → count=0, IP_DROP_MEMBERSHIP

如果没有引用计数:
  Writer1 删除 → IP_DROP_MEMBERSHIP → Writer2, Writer3 收不到组播了!
```

**底层思想**：**引用计数是共享资源生命周期管理的标准模式**。组播组成员关系是一种共享资源（多个逻辑实体共享同一个内核级的组播订阅），必须等到所有使用者都释放后才能真正清理。

## 8. 与规范的关系

- **RTPS v2.5 §9.6.1**：端口映射公式（`port_base + port_dg × domain_id + offset`）
- **RTPS v2.5 §9.4**：UDP/IP PSM（Platform Specific Mapping）
- **RTPS v2.5 §9.4.5.3**：组播地址范围（239.255.0.0/16）
- **RTPS v2.5 §8.5.3.2**：SPDP 组播端点定义
- **RFC 3376**：IGMPv3（IP_ADD_MEMBERSHIP / IP_ADD_SOURCE_MEMBERSHIP）
- **RFC 4604**：SSM（Source-Specific Multicast）

## 9. 总结

CycloneDDS 的 UDP 传输机制可概括为 **4 收 + N 发 + 3 线程 + 预计算路由**：

| 维度 | 设计决策 | 原因 |
|------|---------|------|
| 接收 Socket | 4 个 (disc/data × UC/MC) | 控制面/数据面分离 + 单播/组播分离 |
| 发送 Socket | 每接口 1 个, 独立于接收 | Windows 互操作 bug + 接口隔离 |
| 接收线程 | 1~3 个, 默认 1 | 防火墙兼容性 → 默认单线程; 性能 → 可选多线程 |
| 线程分配 | recvMC/recvUC 专用线程 | 防止数据洪流淹没 Discovery |
| 组播加入 | 每接口 IP_ADD_MEMBERSHIP | 组播不跨接口, 必须每接口加入 |
| 发送选择 | 发现时预计算, 缓存在 xlocator | 热路径零开销 |
| 组播发送 | 从所有接口都发 | 覆盖所有网络分区 |
| 组成员管理 | 引用计数 | 多个逻辑实体共享组播订阅 |
