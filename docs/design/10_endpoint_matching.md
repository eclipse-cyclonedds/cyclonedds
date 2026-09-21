# 模块 10：端点匹配（Endpoint Matching）设计文档

## 1. 概述

端点匹配模块在 Discovery 完成后，判断哪些 Writer 和 Reader 可以通信。匹配基于 Topic 名称、Partition 命名空间和 QoS 兼容性。匹配成功后建立通信链路。

**关键文件**：
- `src/core/ddsi/src/ddsi_endpoint_match.c` — 匹配实现
- `src/core/ddsi/src/ddsi__endpoint_match.h` — 匹配结构与声明
- `src/core/ddsi/src/ddsi_qosmatch.c` — QoS 兼容性检查

## 2. 核心数据结构

### 2.1 匹配记录

```c
// Reader 侧记录匹配的 Proxy Writer
struct ddsi_rd_pwr_match {
  ddsi_guid_t pwr_guid;             // 匹配的 proxy writer GUID
  unsigned pwr_alive: 1;            // 对端是否存活
  uint32_t pwr_alive_vclock;        // 活性时钟
  enum prmss_state sync_state;      // 同步状态
  struct ddsi_reorder *reorder;     // 专用重排序缓冲（OUT_OF_SYNC 时使用）
  // 安全相关
  struct ddsi_crypto_handle *crypto_handle;
};

// Writer 侧记录匹配的 Proxy Reader
struct ddsi_wr_prd_match {
  ddsi_guid_t prd_guid;             // 匹配的 proxy reader GUID
  unsigned assumed_in_sync: 1;      // 假定同步标志
  ddsi_seqno_t seq;                 // 确认到的最大 seqnum
  int32_t num_reliable_readers_where_seq_equals_max;  // 同步计数
  // HEARTBEAT/ACK_NACK 状态
  uint32_t next_acknack_seq;        // 下一个预期的 ACK_NACK 计数
  ddsrt_avl_node_t all_readers_treenode;  // 全局匹配树节点
};
```

### 2.2 同步状态机

```c
enum prmss_state {
  PRMSS_OUT_OF_SYNC,    // Reader 缺少历史数据，需要追赶
  PRMSS_TLCATCHUP,      // Transient-Local 追赶中
  PRMSS_SYNC            // 已同步，正常接收
};
```

### 2.3 QoS 匹配掩码

```c
// RxO (Requested-Offered) QoS 掩码——参与匹配的 QoS
#define DDSI_QP_RXO_MASK (
  DDSI_QP_DURABILITY |
  DDSI_QP_RELIABILITY |
  DDSI_QP_PRESENTATION |
  DDSI_QP_DEADLINE |
  DDSI_QP_LATENCY_BUDGET |
  DDSI_QP_OWNERSHIP |
  DDSI_QP_LIVELINESS |
  DDSI_QP_DESTINATION_ORDER |
  DDSI_QP_DATA_REPRESENTATION
)
```

## 3. 机制设计

### 3.1 四向匹配触发

```
匹配在以下任何事件发生时触发:

1. 本地 Writer 创建 → ddsi_match_writer_with_proxy_readers()
   → 遍历所有已知 proxy_reader, 逐一检查匹配

2. 本地 Reader 创建 → ddsi_match_reader_with_proxy_writers()
   → 遍历所有已知 proxy_writer, 逐一检查匹配

3. Proxy Writer 创建 (SEDP) → ddsi_match_proxy_writer_with_readers()
   → 遍历所有本地 reader, 逐一检查匹配

4. Proxy Reader 创建 (SEDP) → ddsi_match_proxy_reader_with_writers()
   → 遍历所有本地 writer, 逐一检查匹配
```

### 3.2 匹配算法

```
topickind_qos_match_p_lock(wr_qos, rd_qos):
  │
  ├─ Step 1: Topic 名称匹配
  │   └─ wr.topic_name == rd.topic_name (精确匹配)
  │
  ├─ Step 2: Partition 匹配
  │   └─ partitions_match_p(wr.partition, rd.partition)
  │       ├─ 空 partition 匹配所有 (默认 partition)
  │       ├─ 精确字符串比较
  │       └─ 通配符匹配: "control/*" 匹配 "control/engine"
  │
  ├─ Step 3: RxO QoS 兼容性
  │   ├─ Reliability: rd.kind ≤ wr.kind
  │   │   (Reader 要求 RELIABLE → Writer 必须是 RELIABLE)
  │   ├─ Durability: rd.kind ≤ wr.kind
  │   │   (Reader 要求 TRANSIENT_LOCAL → Writer 至少 TRANSIENT_LOCAL)
  │   ├─ Deadline: rd.period ≥ wr.period
  │   │   (Reader 接受更长的 deadline)
  │   ├─ Latency Budget: rd.duration ≥ wr.duration
  │   ├─ Ownership: rd.kind == wr.kind
  │   │   (SHARED 和 EXCLUSIVE 不能混用)
  │   ├─ Liveliness: rd.kind ≤ wr.kind
  │   ├─ Presentation: rd.access_scope ≤ wr.access_scope
  │   ├─ Destination Order: rd.kind == wr.kind
  │   └─ Data Representation: 有公共 codec
  │
  ├─ Step 4: 类型兼容性 (如果启用 XTypes)
  │   └─ ddsi_type_pair_t 检查类型可赋值性
  │
  └─ 返回: 匹配成功 / 匹配失败 (+ 失败原因)
```

### 3.3 匹配后的同步状态初始化

```
新匹配建立时, Reader 的同步状态:

  if Reader 是 BEST_EFFORT:
    → PRMSS_SYNC (直接同步, 不关心历史)

  if Reader 是内置端点:
    → PRMSS_SYNC (内置端点容忍丢失)

  if Proxy Writer 已发送过 HEARTBEAT:
    → PRMSS_OUT_OF_SYNC (知道 Writer 的 seqnum 范围, 需要追赶)
    → 创建专用 reorder 缓冲
    → 发送 ACK_NACK 请求历史数据

  if Proxy Writer 还没发送 HEARTBEAT:
    → PRMSS_SYNC (乐观假设, 等待第一个 HEARTBEAT 后可能降级)
```

### 3.4 锁序协议

```
匹配过程中需要同时锁定两个实体 (Writer + Reader / 本地 + Proxy):

规则: 按实体地址的大小排序获取锁
  if (addr_A < addr_B):
    lock(A); lock(B);
  else:
    lock(B); lock(A);

→ 全序保证: 任何两个线程对同一对实体的锁获取顺序相同
→ 不可能出现死锁
```

## 4. 设计逻辑与设计思想

### 4.1 为什么匹配是四向触发的？

**设计哲学：去中心化的即时反应**

在分布式系统中，本地 Writer 和远端 Reader 可能以任何顺序出现：

```
场景 1: Writer 先于 Reader
  t=0: Writer_A 创建 → 遍历 proxy_reader → 无匹配
  t=1: Proxy_Reader_B 出现 (SEDP) → 遍历 local_writer → 匹配 Writer_A ✓

场景 2: Reader 先于 Writer
  t=0: Proxy_Reader_B 出现 → 遍历 local_writer → 无匹配
  t=1: Writer_A 创建 → 遍历 proxy_reader → 匹配 Proxy_Reader_B ✓
```

如果只在一侧触发匹配（例如只在本地实体创建时），场景 1 中的 Proxy_Reader_B 永远不会与 Writer_A 匹配——必须等到 Writer_A 被删除并重新创建才行。

**底层思想**：**对称性原则**。两个事件（本地创建、远端发现）都可能先发生，因此两侧都必须主动尝试匹配。这消除了对发现时序的假设，使系统在任何事件顺序下都正确。

### 4.2 为什么用地址排序的锁序协议？

**设计哲学：总序消除死锁**

匹配过程需要同时持有两个实体的锁（例如 Writer A 和 Proxy Reader B）。如果两个线程同时尝试匹配 (A,B) 和 (B,A)，不同的锁序可能导致死锁：

```
Thread 1: lock(A) → lock(B) → ...
Thread 2: lock(B) → lock(A) → 死锁!
```

按地址排序（总序）确保所有线程都以相同的顺序获取锁：

```
Thread 1: lock(min(A,B)) → lock(max(A,B)) → ...
Thread 2: lock(min(B,A)) → lock(max(B,A)) → 同序, 不会死锁
```

**为什么用地址而不是 GUID？**
- 地址是进程内唯一的，比较开销是一条指针比较指令
- GUID 是 16 字节的结构体，比较开销更大
- 地址比较在 CPU 层面是免费的（单条 CMP 指令）

**底层思想**：**Resource Ordering（资源排序）是经典的死锁预防策略**，出自 Dijkstra 的银行家算法思想。通过建立全局资源排序，消除循环等待条件——死锁的四个必要条件之一。

### 4.3 为什么有三个同步状态（OUT_OF_SYNC / TLCATCHUP / SYNC）？

**设计哲学：精确描述 Reader 的追赶进度**

当一个 RELIABLE Reader 新匹配到一个已经运行中的 Writer 时，Writer 可能已经发送了很多 sample。Reader 需要"追赶"这些历史数据。

三个状态描述了追赶的不同阶段：

**PRMSS_OUT_OF_SYNC（未同步）**：
- Reader 明确知道自己缺少 sample（已收到 Writer 的 HEARTBEAT，知道 seqnum 范围）
- 维护专用的 reorder 缓冲：因为这个阶段可能同时收到历史重传和新数据
- 持续发送 ACK_NACK 请求缺失的 sample

**PRMSS_TLCATCHUP（Transient-Local 追赶）**：
- 用于 TRANSIENT_LOCAL Durability 的特殊情况
- Writer 需要发送存储的历史数据给新 Reader
- Reader 需要区分"历史数据"和"实时数据"的边界

**PRMSS_SYNC（已同步）**：
- Reader 已经追赶完毕，正常接收新数据
- 释放专用 reorder 缓冲（节省内存）
- 使用标准的接收路径

**底层思想**：**状态机精确建模系统行为**。三个状态不是任意选择，而是 RTPS 规范 §8.4.10 中 StatefulReader 行为的忠实映射。每个状态对应不同的消息处理逻辑，用状态机管理比用 if-else 更清晰、更不易出错。

### 4.4 为什么 Partition 支持通配符？

**设计哲学：灵活的命名空间隔离**

DDS Partition 是一种逻辑隔离机制——同一 Topic 的 Writer 和 Reader 只有在 Partition 有交集时才能匹配。

通配符支持的场景：
```
Writer: partition = "sensor/temperature/zone_*"
Reader A: partition = "sensor/temperature/zone_1"   → 匹配 ✓
Reader B: partition = "sensor/temperature/zone_2"   → 匹配 ✓
Reader C: partition = "sensor/pressure/zone_1"      → 不匹配 ✗
```

如果没有通配符，Writer 需要显式列出所有可能的 Partition——在动态系统中不可行。

**底层思想**：**Partition 是 DDS 的"VLAN"**。就像网络中的 VLAN 提供逻辑隔离但允许跨 VLAN 路由，Partition 提供逻辑隔离但通过通配符允许灵活的跨分区通信。

### 4.5 匹配失败时的通知机制

匹配失败（QoS 不兼容）时，不是静默忽略，而是**主动通知用户**：

```
QoS 匹配失败:
  → Writer 侧: 触发 OFFERED_INCOMPATIBLE_QOS_STATUS
    → 回调 on_offered_incompatible_qos(writer, status)
    → status 包含: 不兼容的 QoS 策略 ID + 计数

  → Reader 侧: 触发 REQUESTED_INCOMPATIBLE_QOS_STATUS
    → 回调 on_requested_incompatible_qos(reader, status)
```

**设计哲学**：**静默失败是 bug 的温床**。用户写了一个 RELIABLE Writer 和一个 RELIABLE Reader，但忘了设置 Reader 的 Partition。如果不通知 QoS 不兼容，用户会花数小时调试"为什么收不到数据"。通过 status 回调，系统告诉用户"匹配失败了，原因是 Partition 不兼容"。

**底层思想**：**Fail-visible > Fail-silent**。系统应该让失败可见，而不是静默吞噬错误。

## 5. 与规范的关系

- **DDS v1.4 §2.2.3**：QoS 兼容性规则（RxO 表）
- **DDS v1.4 §2.2.4.1**：OFFERED/REQUESTED_INCOMPATIBLE_QOS_STATUS 定义
- **RTPS v2.5 §8.5.4.4**：端点匹配条件
- **RTPS v2.5 §8.4.10**：StatefulReader 同步状态（对应 PRMSS_*）

## 6. 总结

端点匹配的设计哲学可概括为**对称触发 + 总序锁定 + 状态机追赶 + 失败可见**：
1. 四向触发消除对 Discovery 时序的依赖
2. 地址排序锁序协议以最小开销预防死锁
3. 三状态同步机精确建模 Reader 追赶过程
4. Partition 通配符提供灵活的命名空间隔离
5. QoS 不兼容时主动通知用户，而非静默失败
