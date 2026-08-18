# 模块 13：传输层（Transport）设计文档

## 1. 概述

传输层为 DDSI-RTPS 协议提供网络 I/O 抽象。通过工厂模式支持 UDP、TCP 和 Raw Ethernet 三种传输协议，使协议层无需关心底层传输细节。传输层管理 Socket 的创建、绑定、发送和接收。

**关键文件**：
- `src/core/ddsi/src/ddsi_tran.c` — 传输抽象层实现
- `src/core/ddsi/src/ddsi__tran.h` — 传输层内部头文件
- `src/core/ddsi/src/ddsi_udp.c` — UDP 传输实现
- `src/core/ddsi/src/ddsi_tcp.c` — TCP 传输实现
- `src/core/ddsi/src/ddsi_raweth.c` — Raw Ethernet 传输实现

## 2. 核心数据结构

### 2.1 传输工厂

```c
struct ddsi_tran_factory {
  ddsi_tran_create_conn_fn m_create_conn_fn;     // 创建连接
  ddsi_tran_release_conn_fn m_release_conn_fn;   // 释放连接
  ddsi_tran_supports_fn m_supports_fn;           // 是否支持某 Locator 类型
  ddsi_tran_create_listener_fn m_create_listener_fn;  // 创建监听器（TCP）
  ddsi_tran_release_listener_fn m_release_listener_fn;
  char m_typename[32];           // "udp", "tcp", "raweth"
  int32_t m_kind;                // DDSI_LOCATOR_KIND_UDPv4 / TCPv4 / RAWETH
  struct ddsi_tran_factory *m_factory;  // 工厂链表
};
```

### 2.2 传输连接

```c
struct ddsi_tran_conn {
  ddsi_tran_read_fn m_read_fn;            // 读取函数指针
  ddsi_tran_write_fn m_write_fn;          // 写入函数指针
  ddsi_tran_peer_locator_fn m_peer_locator_fn;  // 获取对端地址
  ddsi_tran_disable_multiplexing_fn m_disable_multiplexing_fn;
  ddsi_tran_locator_fn m_locator_fn;      // 获取本地地址
  struct ddsi_tran_factory *m_factory;    // 所属工厂
  ddsrt_socket_t m_sock;                  // 底层 socket
  ddsi_locator_t m_base;                  // 基地址
  bool m_multicast;                       // 是否多播连接
  bool m_closed;                          // 是否已关闭
  ddsrt_atomic_uint32_t m_count;          // 引用计数
};
```

### 2.3 Locator（网络地址抽象）

```c
typedef struct ddsi_locator {
  int32_t kind;                  // 地址类型 (UDPv4=1, UDPv6=2, TCPv4, RAWETH...)
  uint32_t port;                 // 端口号
  unsigned char address[16];     // 地址（IPv4 用后 4 字节，IPv6 用全部 16 字节）
} ddsi_locator_t;
```

## 3. 机制设计

### 3.1 传输工厂注册

```
ddsi_init() 初始化时:
  ├─ 根据配置的 transport_selector:
  │   ├─ TRANS_UDP → ddsi_udp_factory_create()
  │   │   → 注册 m_typename="udp", m_kind=UDPv4/UDPv6
  │   ├─ TRANS_TCP → ddsi_tcp_factory_create()
  │   │   → 注册 m_typename="tcp", m_kind=TCPv4/TCPv6
  │   └─ TRANS_RAWETH → ddsi_raweth_factory_create()
  │       → 注册 m_typename="raweth", m_kind=RAWETH
  │
  └─ 工厂链表: gv->ddsi_tran_factories → factory1 → factory2 → ...
```

### 3.2 Socket 布局（UDP 模式）

```
                       ┌─────────────────┐
  Discovery Multicast  │  disc_conn_mc   │ ← SPDP/SEDP 多播收发
                       ├─────────────────┤
  Discovery Unicast    │  disc_conn_uc   │ ← SPDP/SEDP 单播收发
                       ├─────────────────┤
  Data Multicast       │  data_conn_mc   │ ← 用户数据多播收发
                       ├─────────────────┤
  Data Unicast         │  data_conn_uc   │ ← 用户数据单播收发
                       └─────────────────┘

  发送连接数组 xmit_conns[]:
    [0] = disc_conn_uc  (Discovery 发送)
    [1] = data_conn_uc  (Data 发送)
    [2..3] = 多接口场景的额外连接
```

### 3.3 端口计算

```
RTPS 规范 §9.6.1 端口映射公式:

  Discovery Multicast:
    port = port_base + port_dg × domain_id + d0
    → 默认: 7400 + 250 × domain_id + 0

  Discovery Unicast:
    port = port_base + port_dg × domain_id + d1 + port_pg × participant_index
    → 默认: 7400 + 250 × domain_id + 10 + 2 × participant_index

  Data Multicast:
    port = port_base + port_dg × domain_id + d2
    → 默认: 7400 + 250 × domain_id + 1

  Data Unicast:
    port = port_base + port_dg × domain_id + d3 + port_pg × participant_index
    → 默认: 7400 + 250 × domain_id + 11 + 2 × participant_index
```

### 3.4 发送路径

```
ddsi_conn_write(conn, locator, msg_fragments, flags)
  │
  ├─ 调用 conn->m_write_fn()
  │   ├─ UDP: ddsi_udp_conn_write()
  │   │   └─ ddsrt_sendmsg(sock, &msghdr, 0)
  │   │       → msghdr 包含 scatter-gather I/O 向量
  │   │       → 一次系统调用发送多个碎片
  │   │
  │   ├─ TCP: ddsi_tcp_conn_write()
  │   │   └─ 需要添加 RTPS 消息长度前缀
  │   │   └─ send(sock, data, len, 0)
  │   │
  │   └─ RawEth: ddsi_raweth_conn_write()
  │       └─ 构建 Ethernet 帧头 + sendto()
  │
  └─ 返回发送字节数
```

### 3.5 接收路径

```
Receive 线程:
  ddsi_conn_read(conn, buffer, maxlen, ...)
  │
  ├─ conn->m_read_fn()
  │   ├─ UDP: ddsi_udp_conn_read()
  │   │   └─ ddsrt_recvmsg(sock, &msghdr, 0)
  │   │       → 获取源地址、目标地址、接收接口
  │   │
  │   ├─ TCP: ddsi_tcp_conn_read()
  │   │   └─ 先读 4 字节长度前缀
  │   │   └─ 再读完整消息体
  │   │
  │   └─ RawEth: ddsi_raweth_conn_read()
  │       └─ recvfrom() + 解析 Ethernet 帧头
  │
  └─ 返回: 数据 + 源 Locator + 目标 Locator
```

## 4. 设计逻辑与设计思想

### 4.1 为什么用工厂模式抽象传输层？

**设计哲学：开放-封闭原则（Open-Closed Principle）**

传输层需要支持多种网络协议（UDP、TCP、Raw Ethernet），且可能在未来扩展（如共享内存、DPDK）。如果在协议层用 if-else 区分传输：

```c
// 不好的设计:
if (transport == UDP) {
  sendto(sock, data, len, ...);
} else if (transport == TCP) {
  send(sock, &len_prefix, 4, ...);
  send(sock, data, len, ...);
} else if (transport == RAWETH) {
  build_eth_header(...);
  sendto(sock, frame, ...);
}
// 每增加一种传输都要修改协议层代码
```

工厂模式的抽象：

```c
// 好的设计:
ddsi_conn_write(conn, locator, msg, flags);
// → 内部调用 conn->m_write_fn() → 自动分派到正确的实现
// 增加新传输只需: 新建工厂 + 实现函数指针 → 协议层零修改
```

**底层思想**：这是 **Strategy Pattern（策略模式）**的变体——传输策略在初始化时选定，之后统一使用。类似于 Linux 内核的 VFS（虚拟文件系统）层：所有文件系统实现 `struct file_operations`，VFS 层通过函数指针分派，无需关心底层是 ext4 还是 NFS。

### 4.2 为什么将 Discovery 和 Data 分成不同的 Socket？

**设计哲学：控制面与数据面分离**

4 个 Socket 的分工：

```
控制面 (Discovery):        数据面 (Data):
  disc_conn_mc (多播)        data_conn_mc (多播)
  disc_conn_uc (单播)        data_conn_uc (单播)
```

分离的必要性：

1. **流量隔离**：高速数据发送可能填满 Socket 缓冲区。如果 Discovery 共用同一个 Socket，SPDP/SEDP 消息可能被数据流"淹没"，导致发现延迟或失败。

2. **缓冲区配置**：Discovery Socket 需要小缓冲区（消息小但重要），Data Socket 需要大缓冲区（消息大但可容忍少量丢失）。

3. **端口分离**：RTPS 规范要求 Discovery 和 Data 使用不同端口（§9.6.1），以便防火墙和 QoS 策略区分对待。

4. **优先级**：操作系统层面可以对不同 Socket 设置不同的优先级（SO_PRIORITY）。

**底层思想**：**控制面/数据面分离是网络系统的基本设计原则**。SDN（Software-Defined Networking）将控制面完全分离到控制器；MPLS 区分 LDP（控制）和数据转发路径；ATM 区分信令信道和数据信道。原因始终相同：**控制流量的可靠性要求高于数据流量，必须独立保护**。

### 4.3 为什么同时支持多播和单播？

**设计哲学：冗余路径最大化可达性**

多播和单播各有优劣：

| 特性 | 多播 | 单播 |
|------|------|------|
| 效率 | 1-to-N: 一次发送 | 1-to-1: N 次发送 |
| 跨子网 | ✗（通常不可达） | ✓ |
| 云环境 | ✗（常被禁止） | ✓ |
| 零配置 | ✓（自动发现） | ✗（需要 peers 列表）|

CycloneDDS 的策略：**同时使用两者**

```
SPDP 发送:
  → 发送到 SPDP 多播地址 (239.255.0.1:7400+...)   [覆盖同子网]
  → 发送到所有已知的单播 peer 地址              [覆盖跨子网]
  → 如果配置了静态 peers → 单播到配置地址       [覆盖云环境]
```

**底层思想**：**多路径冗余（Multi-Path Redundancy）**。在不确定的网络环境中，没有单一路径能保证 100% 可达。多播和单播是两条独立的通信路径，同时使用两者就像双引擎飞机——单引擎失效时仍能安全运行。

### 4.4 为什么 TCP 需要额外的 Listener 线程？

**设计哲学：连接建立不能阻塞数据接收**

UDP 是无连接协议——Receive 线程直接 `recvfrom()` 即可接收任何发送方的数据。

TCP 是面向连接的——在接收数据前必须先 `accept()` 建立连接：

```
如果在 Receive 线程中做 accept():
  recv_thread:
    while (true) {
      accept(listen_sock)   // 阻塞等待新连接!
      // → 此时无法接收已有连接的数据!
    }

分离 Listener 线程:
  listener_thread:
    while (true) {
      new_conn = accept(listen_sock)    // 专门等待新连接
      register_to_recv_thread(new_conn) // 注册到接收线程
    }
  recv_thread:
    while (true) {
      poll(all_connections)             // 处理所有已建立连接的数据
    }
```

**底层思想**：**Reactor 模式**。Listener 线程是 Acceptor（接受新连接），Receive 线程是 Reactor（处理已有连接上的 I/O 事件）。这与 Nginx 的 accept mutex 设计、Java NIO 的 Boss/Worker 线程模型同源。

### 4.5 为什么传输层使用 Locator 而非直接使用 Socket 地址？

**设计哲学：协议无关的寻址**

`ddsi_locator_t` 是一个通用的网络地址：

```c
struct ddsi_locator {
  int32_t kind;           // 地址类型
  uint32_t port;          // 端口
  unsigned char address[16]; // 地址字节
};
```

它可以表示：
- IPv4 地址（kind=LOCATOR_KIND_UDPv4, address 后 4 字节）
- IPv6 地址（kind=LOCATOR_KIND_UDPv6, address 全部 16 字节）
- TCP 连接地址（kind=LOCATOR_KIND_TCPv4）
- Raw Ethernet MAC 地址（kind=LOCATOR_KIND_RAWETH）
- 共享内存地址（扩展 kind）

如果直接使用 `struct sockaddr_in`：
- 只能表示 IPv4——IPv6、Raw Ethernet 需要不同结构
- 传输无关的代码（Discovery、匹配）必须区分地址类型
- RTPS 报文中的 Locator 编码需要额外转换

Locator 是 RTPS 规范 §8.3.4 定义的**规范类型**——实现直接使用规范类型，消除了转换开销和语义差异。

**底层思想**：**间接层（Level of Indirection）**。如同 DNS 将域名映射到 IP 地址，Locator 将逻辑地址映射到物理传输地址。增加一层间接使得传输层可以独立演进——添加新传输只需定义新的 Locator kind，无需修改上层协议代码。

### 4.6 为什么 UDP 使用 scatter-gather I/O（sendmsg/recvmsg）？

**设计哲学：零拷贝数据传输**

RTPS 消息由多个片段组成：Header + InfoTimestamp + Data + InfoDST + ...

传统方式需要先拷贝到连续缓冲区：

```c
// 传统方式:
char buffer[2048];
memcpy(buffer, header, header_len);
memcpy(buffer + header_len, timestamp, timestamp_len);
memcpy(buffer + header_len + timestamp_len, data, data_len);
sendto(sock, buffer, total_len, ...);  // 4 次内存拷贝!
```

scatter-gather I/O 直接发送分散的缓冲区：

```c
// scatter-gather I/O:
ddsrt_iovec_t iov[3];
iov[0] = { .iov_base = header, .iov_len = header_len };
iov[1] = { .iov_base = timestamp, .iov_len = timestamp_len };
iov[2] = { .iov_base = data, .iov_len = data_len };

ddsrt_msghdr_t msg = { .msg_iov = iov, .msg_iovlen = 3 };
ddsrt_sendmsg(sock, &msg, 0, &nsent);  // 零拷贝!
```

优势：
1. **零拷贝**：避免多次 memcpy，减少 CPU 开销
2. **减少系统调用**：一次 sendmsg() 发送所有片段，而非多次 sendto()
3. **DMA 友好**：网卡可以直接从多个缓冲区 DMA 读取数据

**底层思想**：**零拷贝（Zero-Copy）I/O**。现代操作系统和网卡都支持 scatter-gather DMA，应用层应该利用这一特性避免不必要的内存拷贝。类似技术：sendfile()、splice()、mmap()。

### 4.7 为什么要设置 Socket 缓冲区大小？

**设计哲学：适配流量突发**

默认的 Socket 缓冲区（通常几十 KB）不足以应对 DDS 的流量模式：

```
典型场景: 发布 1000 个样本，每个 1KB
  → 1 秒内发送 1MB 数据
  → 如果接收端处理慢（CPU 繁忙），OS 接收缓冲区很快填满
  → 填满后到达的数据包被丢弃 → 触发 NACK/重传 → 性能下降
```

CycloneDDS 的策略：

```c
// 默认接收缓冲区: 1MB（可配置）
set_rcvbuf(sock, 1048576);

// 尝试设置，读回实际值:
setsockopt(sock, SO_RCVBUF, &req_size, sizeof(req_size));
getsockopt(sock, SO_RCVBUF, &act_size, &optlen);
if (act_size < min_size) {
  GVERROR("failed to set buffer size");
}
```

为什么不能无限大？
- **内存限制**：大缓冲区占用大量内核内存
- **延迟**：缓冲区越大，数据在队列中等待时间越长
- **公平性**：一个 Socket 独占大量内存可能影响其他进程

**底层思想**：**缓冲即延迟，但也是吞吐的保障**（Bufferbloat vs Throughput）。网络系统设计需要在延迟和吞吐间平衡——缓冲区太小会丢包，太大会增加延迟。

### 4.8 为什么需要 IP_PKTINFO（扩展包信息）？

**设计哲学：多网卡环境下的地址识别**

在多网卡场景中，接收到的数据包可能来自任意接口：

```
机器有 2 个网卡:
  eth0: 192.168.1.100
  eth1: 10.0.0.100

收到来自 192.168.1.200 的包 → 应该通过 eth0 回复还是 eth1？
```

传统 `recvfrom()` 只返回源地址，不返回**目标地址**和**接收接口**。

IP_PKTINFO 提供扩展信息：

```c
struct in_pktinfo {
  int ipi_ifindex;       // 接收接口索引
  struct in_addr ipi_spec_dst;  // 目标地址
  struct in_addr ipi_addr;      // 实际目标地址
};
```

用途：

1. **单播回复路由**：收到单播包后，从相同接口回复
2. **多播过滤**：区分"发送到本机地址的多播"和"真正的多播"
3. **接口特定处理**：某些网卡可能需要特殊处理（如 VLAN）

**底层思想**：**对称路由（Symmetric Routing）**。网络通信应该遵循"从哪来回哪去"的原则，避免路由不对称导致的防火墙问题。

### 4.9 为什么维护 ownaddrs 哈希表？

**设计哲学：避免 PCAP 环回包重复记录**

在启用 PCAP 抓包时，CycloneDDS 会记录所有收发的数据包。问题：

```
场景: 本机发送包到自己的多播地址
  1. send() → 记录一次发送
  2. 环回到 recv() → 又记录一次接收
  → PCAP 文件中同一个包出现 2 次!
```

`ownaddrs` 哈希表存储所有本机创建的 Socket 地址：

```c
// 发送时记录:
ddsrt_hh_add(fact->ownaddrs, &conn->m_addr);

// 接收时过滤:
if (ddsrt_hh_lookup(fact->ownaddrs, &src_addr)) {
  // 源地址是本机 → 跳过 PCAP 记录
}
```

**底层思想**：**去重（Deduplication）**。分布式系统中的环回路径会产生重复事件，需要在合适的层级过滤。类似：TCP 的序列号去重、分布式日志的去重。

## 5. 与规范的关系

- **RTPS v2.5 §8.3.4**：Locator_t 定义
- **RTPS v2.5 §9.4**：UDP/IP PSM（Platform Specific Mapping）
- **RTPS v2.5 §9.6.1**：端口映射公式
- **RTPS v2.5 §9.4.5.3**：多播地址范围定义
- **DDS v1.4 §2.1.3**：传输层独立性要求

## 6. 总结

传输层的设计哲学可概括为**工厂抽象 + 控制/数据分离 + 冗余路径 + 零拷贝 + 协议无关寻址**：

1. **工厂模式**：使传输层可插拔扩展，协议层零修改
2. **控制/数据分离**：独立 Socket 防止数据流淹没控制流
3. **多播 + 单播并行**：冗余路径最大化网络可达性
4. **TCP Listener 分离**：连接建立与数据接收解耦
5. **Locator 抽象**：传输协议无关的统一寻址
6. **Scatter-gather I/O**：零拷贝发送多片段消息
7. **Socket 缓冲区调优**：适配流量突发模式
8. **IP_PKTINFO**：多网卡环境下的路由对称性
9. **ownaddrs 去重**：避免 PCAP 环回包重复
