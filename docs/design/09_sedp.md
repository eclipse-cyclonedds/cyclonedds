# 模块 9：SEDP（端点发现）设计文档

## 1. 概述

SEDP（Simple Endpoint Discovery Protocol）负责在 SPDP 建立 Participant 关系后，交换各 Participant 的 DataWriter/DataReader 信息，触发端点匹配。它是 Discovery 协议的第二级。

**关键文件**：
- `src/core/ddsi/src/ddsi_discovery_endpoint.c` — SEDP 端点发布
- `src/core/ddsi/src/ddsi__discovery_endpoint.h` — SEDP 声明
- `src/core/ddsi/src/ddsi_proxy_endpoint.c` — Proxy 端点管理

## 2. 核心数据结构

### 2.1 Proxy Participant

```c
struct ddsi_proxy_participant {
  ddsi_guid_t guid;                    // GUID
  struct ddsi_lease *lease;            // 租约
  ddsrt_avl_tree_t proxypp_pp_match;   // 匹配的本地 Participant
  // 内置端点 proxy
  struct ddsi_proxy_writer *spdp_pwr;
  struct ddsi_proxy_reader *sedp_prd_publications;
  struct ddsi_proxy_reader *sedp_prd_subscriptions;
  // 用户端点
  ddsrt_avl_tree_t writers;            // 所有 proxy writer
  ddsrt_avl_tree_t readers;            // 所有 proxy reader
};
```

### 2.2 Proxy Writer / Reader

```c
struct ddsi_proxy_writer {
  struct ddsi_endpoint_common e;        // 通用端点信息 (GUID, QoS)
  ddsrt_avl_tree_t readers;            // 匹配的本地 Reader AVL 树
  struct ddsi_whc *whc;                // 历史缓存（用于 transient-local）
  ddsi_seqno_t last_seq;               // 最后序列号
  struct ddsi_defrag *defrag;          // 碎片重组器
  struct ddsi_reorder *reorder;        // 重排序器
  struct ddsi_dqueue *dqueue;          // 投递队列
  bool deliver_synchronously;          // 是否同步投递
};

struct ddsi_proxy_reader {
  struct ddsi_endpoint_common e;        // 通用端点信息
  ddsrt_avl_tree_t writers;            // 匹配的本地 Writer AVL 树
  bool is_fict_trans_reader;           // 虚拟 transient reader 标志
};
```

## 3. 机制设计

### 3.1 SEDP 发布流程

```
本地 Writer 创建时:
  ddsi_sedp_write_writer(wr)
    └─ sedp_write_endpoint_impl()
       ├─ 构建参数列表 (plist):
       │   ├─ PID_ENDPOINT_GUID        → Writer GUID
       │   ├─ PID_TOPIC_NAME           → Topic 名称
       │   ├─ PID_TYPE_NAME            → 类型名称
       │   ├─ ddsi_xqos_delta(wr_qos, default_qos)
       │   │   → 只包含与默认值不同的 QoS
       │   ├─ PID_UNICAST_LOCATOR      → 单播地址（如果不同于 Participant 默认）
       │   ├─ PID_MULTICAST_LOCATOR    → 多播地址
       │   └─ PID_TYPE_INFORMATION     → 类型发现信息（XTypes）
       │
       └─ 通过 SEDP 内置 Writer 发布
          → 使用 RELIABLE QoS（确保所有 Participant 收到）
          → 使用 TRANSIENT_LOCAL（新加入的 Participant 可以收到历史数据）
```

### 3.2 SEDP 接收处理

```
收到远端 SEDP 消息:
  ddsi_handle_sedp_alive_endpoint()
    ├─ 解析参数列表 → 重建完整 QoS (delta + defaults)
    ├─ 查找 proxy_participant (GUID prefix)
    │   └─ 不存在 → 丢弃（SPDP 还没完成）
    │
    ├─ 创建 proxy_writer 或 proxy_reader:
    │   ├─ 分配并初始化结构体
    │   ├─ 存入 proxy_participant 的 AVL 树
    │   ├─ 存入全局 entity_index
    │   ├─ 创建 defrag + reorder（proxy_writer）
    │   └─ 初始化匹配 AVL 树
    │
    └─ 触发端点匹配:
       ├─ ddsi_match_proxy_writer_with_readers()
       └─ ddsi_match_proxy_reader_with_writers()
```

### 3.3 QoS Delta 编码

```
发送:
  full_qos = { reliability=RELIABLE, durability=VOLATILE, deadline=∞, ... }
  default_qos = { reliability=BEST_EFFORT, durability=VOLATILE, deadline=∞, ... }
  delta = ddsi_xqos_delta(full, default)
  → delta = { reliability=RELIABLE }  // 只有 reliability 不同

接收:
  delta = { reliability=RELIABLE }
  merged = ddsi_xqos_merge(delta, default_qos)
  → merged = { reliability=RELIABLE, durability=VOLATILE, deadline=∞, ... }
```

## 4. 设计逻辑与设计思想

### 4.1 为什么只发送 QoS Delta？

**设计哲学：带宽最小化 + 已知基线假设**

DDS 有 22 个 QoS 策略，完整编码可能需要数百字节。但绝大多数应用只修改 1~3 个策略（通常是 Reliability 和 Durability），其余使用默认值。

**数学论证**：假设每个 QoS 策略编码需要 20 字节，22 个策略 = 440 字节。如果只有 2 个非默认 → Delta = 40 字节。节约 **91%** 的带宽。

在大规模系统中（1000 个端点），完整编码的 SEDP 流量 = 440KB/轮，Delta 编码 = 40KB/轮。这是**数量级的差异**。

**前提条件**：所有 DDS 实现必须对默认 QoS 值达成一致。DDSI 规范确保了这一点——默认值是规范性的。

**底层思想**：**差分编码（Delta Encoding）**是信息论的基本优化——只传输变化量。与视频编码中的 I-frame/P-frame 类似：SEDP 的默认 QoS 是 I-frame，Delta 是 P-frame。

### 4.2 为什么需要 Proxy 实体？

**设计哲学：远端状态的本地代理**

Proxy 实体（proxy_participant, proxy_writer, proxy_reader）是远端实体在本地的**镜像**。它们不是远端实体的完整复制，而是本地处理所需的**最小状态集**。

**为什么不直接用 entity_index 中的 GUID 引用远端实体？**

因为远端实体不在本地内存中。需要为每个远端 Writer/Reader 维护本地状态：
- **proxy_writer** 需要：defrag（碎片重组）、reorder（重排序）、matched readers（匹配的本地 Reader）
- **proxy_reader** 需要：matched writers（匹配的本地 Writer）、ACK 状态

**Proxy 的生命周期与远端独立**：远端 Writer 可能因网络中断暂时不可达，但本地 proxy_writer 仍然存在，保持着重排序缓冲和匹配关系。当网络恢复时，不需要重新建立匹配。

**底层思想**：这是**代理模式（Proxy Pattern）**的经典应用。Proxy 封装了"远程对象的本地表示"，屏蔽了网络不确定性。类似于 RPC 框架中的 stub/skeleton 概念。

### 4.3 为什么 SEDP 使用 RELIABLE + TRANSIENT_LOCAL？

**设计哲学：发现信息的零丢失保证**

SEDP 消息必须满足：
1. **不丢失**：如果 Writer 的发现信息丢失，对应的 Reader 永远无法匹配
2. **新加入者可获取历史**：一个新 Participant 需要知道已经存在的所有端点

**RELIABLE** 确保条件 1：通过 HEARTBEAT/ACK_NACK 保证每个 Participant 都收到。

**TRANSIENT_LOCAL** 确保条件 2：Writer 保留历史数据，新匹配的 Reader 可以获取之前发布的所有端点信息。

**为什么不用 BEST_EFFORT？** 在有损网络中，一个丢失的 SEDP 消息意味着两个端点永远无法匹配——这是不可接受的。

**底层思想**：**元数据的可靠性要求高于数据的可靠性**。用户数据丢失可能可以容忍（BEST_EFFORT 场景），但系统元数据（发现信息）的丢失会导致功能完全失效。

### 4.4 为什么 Proxy 端点用 AVL 树存储？

proxy_participant 的 `writers` 和 `readers` 都是 AVL 树，按 GUID 排序。

**原因**：
- **O(log n) 精确查找**：收到 DATA 时需要通过 Writer GUID 查找 proxy_writer
- **O(n) 有序遍历**：删除 proxy_participant 时需要遍历并删除所有子端点
- **范围查询**：匹配逻辑中可能需要遍历某个 Participant 的所有端点

**为什么不用哈希表？** AVL 树在删除 proxy_participant 时的有序遍历性能更好——删除操作会遍历所有子端点执行清理。哈希表的遍历是无序的且有 overhead。

**底层思想**：与实体管理模块相同——**数据结构选择应服务于最危险的操作（cascading deletion）**。

## 5. 与规范的关系

- **RTPS v2.5 §8.5.4**：Simple Endpoint Discovery Protocol
- **RTPS v2.5 §8.5.4.2**：SEDP 内置端点 EntityId
- **RTPS v2.5 §8.5.4.3**：SEDPdiscoveredWriterData / SEDPdiscoveredReaderData 格式
- **DDS v1.4 §2.2.5**：PublicationBuiltinTopicData / SubscriptionBuiltinTopicData

## 6. 总结

SEDP 的设计哲学可概括为**Delta 编码 + Proxy 代理 + 可靠元数据传输**：
1. QoS Delta 编码节约 90%+ 带宽
2. Proxy 实体作为远端状态的本地代理，屏蔽网络不确定性
3. RELIABLE + TRANSIENT_LOCAL 确保发现信息零丢失
4. AVL 树优化级联删除场景
