# 模块 8：SPDP（参与者发现）设计文档

## 1. 概述

SPDP（Simple Participant Discovery Protocol）负责在 DDS Domain 内自动发现其他 Participant。每个 Participant 定期广播自己的存在信息，同时监听其他 Participant 的广播。它是整个 Discovery 协议栈的第一级。

**关键文件**：
- `src/core/ddsi/src/ddsi_discovery_spdp.c` — SPDP 消息处理
- `src/core/ddsi/src/ddsi__discovery_spdp.h` — SPDP 声明
- `src/core/ddsi/src/ddsi_spdp_schedule.c` — SPDP 定时调度
- `src/core/ddsi/src/ddsi__spdp_schedule.h` — 调度声明

## 2. 核心数据结构

### 2.1 SPDP 调度器

```c
struct spdp_admin {
  ddsrt_avl_tree_t live_locators;    // 活跃 Locator AVL 树
  ddsrt_avl_tree_t aging_locators;   // 老化 Locator AVL 树
  ddsrt_fibheap_t schedule_heap;     // 调度堆（按下次发送时间排序）
};
```

### 2.2 Locator 状态

```c
struct spdp_loc {
  ddsi_locator_t loc;              // 网络地址
  int32_t ref_count;               // 引用计数（多少个 proxy participant 使用此地址）
  ddsrt_mtime_t next_send_time;    // 下次 SPDP 发送时间
  enum spdp_loc_state state;       // LIVE / AGING
};
```

### 2.3 已删除 Participant 缓存

```c
struct ddsi_deleted_participants_admin {
  ddsrt_mutex_t lock;
  int64_t delay;                    // 保留延迟时间
  ddsrt_avl_tree_t deleted;        // 已删除 Participant GUID 集合
};
```

### 2.4 SPDP 消息内容（参数列表）

```
SPDP 消息携带的参数:
  ├─ PID_PARTICIPANT_GUID         → Participant 全局唯一标识
  ├─ PID_PROTOCOL_VERSION         → RTPS 协议版本
  ├─ PID_VENDORID                 → 厂商标识 (CycloneDDS)
  ├─ PID_BUILTIN_ENDPOINT_SET     → 支持的内置端点集合
  ├─ PID_PARTICIPANT_LEASE_DURATION → 租约时长
  ├─ PID_DEFAULT_UNICAST_LOCATOR  → 默认单播地址
  ├─ PID_DEFAULT_MULTICAST_LOCATOR→ 默认多播地址
  ├─ PID_METATRAFFIC_UNICAST_LOCATOR → 元数据单播地址
  ├─ PID_METATRAFFIC_MULTICAST_LOCATOR → 元数据多播地址
  ├─ PID_DOMAIN_TAG               → Domain 标签 (可选)
  └─ PID_IDENTITY_TOKEN / PID_PERMISSIONS_TOKEN → 安全信息 (可选)
```

## 3. 机制设计

### 3.1 SPDP 发送调度

```
Participant 创建时:
  → 立即发送第一个 SPDP 消息（到多播 + 所有已知单播地址）
  → 注册到 spdp_schedule 的 Fibonacci 堆

定期发送:
  事件线程检查 schedule_heap:
    → 堆顶 Locator 的 next_send_time 到期
    → 发送 SPDP 到该 Locator
    → 更新 next_send_time:
       ├─ LIVE Locator: next = now + min(lease_duration × 0.8 - 2s, 30s)
       └─ AGING Locator: next = now + aging_interval（指数退避）
    → 重新插入堆
```

### 3.2 Locator 状态机

```
新 Locator 出现 (收到来自该地址的 SPDP):
  → 创建 spdp_loc, state = LIVE, ref_count = 1

同一 Locator 发现更多 Participant:
  → ref_count++

Participant 从该 Locator 消失:
  → ref_count--
  → if ref_count == 0:
     ├─ state = AGING
     ├─ 开始指数退避调度
     └─ 继续发送 SPDP（频率递减）

AGING Locator 超时 / 过久无响应:
  → 删除 spdp_loc, 停止发送
```

### 3.3 SPDP 接收处理

```
ddsi_handle_spdp(gv, rmsg, ...)
  │
  ├─ 解析参数列表 (plist)
  │   └─ 提取 GUID, 版本, Locator, lease_duration, ...
  │
  ├─ 校验:
  │   ├─ GUID prefix 不是自己 (忽略自己的 SPDP)
  │   ├─ domain_id 匹配
  │   └─ 不在 deleted_participants 缓存中 (防止僵尸复活)
  │
  ├─ 判断消息类型:
  │   ├─ ALIVE (statusinfo == 0):
  │   │   └─ handle_spdp_alive()
  │   │       ├─ 查找 proxy_participant (by GUID)
  │   │       ├─ 如果不存在 → 创建 proxy_participant
  │   │       │   ├─ 存储 Locator 信息
  │   │       │   ├─ 创建 lease (ddsi_lease_new)
  │   │       │   ├─ 创建内置端点 proxy (SEDP reader/writer)
  │   │       │   └─ 触发 SEDP 端点匹配
  │   │       └─ 如果已存在 → 续约 (ddsi_lease_renew)
  │   │
  │   └─ DISPOSE / UNREGISTER (statusinfo != 0):
  │       └─ handle_spdp_dispose()
  │           ├─ 查找 proxy_participant
  │           ├─ 加入 deleted_participants 缓存
  │           └─ 删除 proxy_participant 及其所有端点
  │
  └─ 更新 spdp_schedule 的 Locator 状态
```

## 4. 设计逻辑与设计思想

### 4.1 为什么 SPDP 间隔基于租约时长？

**设计哲学：活性检测的时间预算**

RTPS 规范定义了 **Participant Lease Duration**——如果在此时间内没有收到 Participant 的任何消息，就认为它已经宕机/离开。

SPDP 间隔 = `min(lease_duration × 0.8 - 2s, 30s)` 的设计逻辑：

- **0.8 × lease_duration**：在租约到期前留出 20% 的裕量，确保至少有一次 SPDP 在租约到期前到达
- **减 2 秒**：为网络传输延迟和对端处理延迟留出余量
- **上限 30 秒**：即使租约很长（如 5 分钟），也不超过 30 秒发送一次——保证合理的发现延迟

**底层思想**：**安全裕量设计（Safety Margin）**。工程设计中的经典原则：关键时间约束不应踩在边界上。20% 的裕量 + 2 秒的绝对裕量为网络不确定性提供缓冲。

### 4.2 为什么有 AGING Locator 和指数退避？

**设计哲学：优雅降级而非硬切断**

当一个远端 Participant 消失时（网线断开、进程崩溃），CycloneDDS 面临两难：
- **立即停止发送 SPDP**：如果是临时网络故障，重新连通后需要等对方主动 announce 才能重新发现
- **永远继续发送 SPDP**：浪费带宽，尤其在对端永久离开的情况下

**AGING + 指数退避**的折衷：
```
对端消失后:
  t=0: 每 10s 发送 SPDP (正常频率)
  t=30s: 每 20s 发送
  t=90s: 每 40s 发送
  t=210s: 每 80s 发送
  ...
  t=N: 停止发送 (AGING 超时)
```

这提供了**自愈能力**：
- 短暂网络中断（秒级）：AGING 期间的高频 SPDP 确保快速重新发现
- 长时间中断（分钟级）：指数退避逐步减少带宽消耗
- 永久中断（小时级）：最终停止发送

**底层思想**：**指数退避是分布式系统中处理不确定性的通用模式**。与 TCP 重传退避、Ethernet CSMA/CD 退避、DHCP 发现退避同源。它在"快速恢复"和"资源节约"之间取得数学最优的平衡。

### 4.3 为什么需要 deleted_participants 缓存？

**设计哲学：防止时间窗口内的状态竞争**

考虑以下场景：
```
t=0: Participant_A 在节点 1 运行
t=1: Participant_A 发送 SPDP ALIVE 消息 (通过多播)
t=2: Participant_A 崩溃, 节点 1 发送 SPDP DISPOSE
t=3: 节点 2 先收到 DISPOSE, 删除 proxy_participant_A
t=4: 节点 2 后收到 ALIVE (t=1 发送的, 因为网络延迟)
     → 如果没有 deleted_participants 缓存:
       → 节点 2 重新创建 proxy_participant_A → "僵尸复活"!
       → Participant_A 实际上已经不存在了
```

`deleted_participants` 缓存记住"最近删除过的 GUID"，持续一段时间（`delay`）。在此期间，即使收到该 GUID 的 ALIVE 消息也不会重新创建 proxy_participant。

**底层思想**：**墓碑机制（Tombstone Pattern）**。在分布式系统中，删除操作和创建操作可能乱序到达。墓碑（tombstone）记住"删除已发生"，防止乱序的创建操作覆盖删除。这与分布式数据库中的 tombstone marker 同源。

### 4.4 为什么 SPDP 使用多播 + 单播并行发送？

**设计哲学：可达性最大化**

SPDP 同时使用两种发送方式：
- **多播**：发送到规范定义的 SPDP 多播地址，所有同域 Participant 都能收到
- **单播**：发送到配置文件中的 `peers` 列表和已知的远端地址

为什么不只用多播？
- 跨子网时多播不可达
- 某些网络环境禁止多播（公有云、某些企业网络）
- 虚拟机/容器环境中多播可能不可靠

为什么不只用单播？
- 新 Participant 加入时，不知道其他 Participant 的地址
- 多播是零配置发现的基础

**底层思想**：**冗余路径提高鲁棒性**。分布式系统的网络层不可信——任何单一路径都可能失败。多播和单播是两条独立的通信路径，同时使用两者让发现协议更健壮。

### 4.5 为什么 SPDP 消息只含 Participant 级信息？

SPDP 消息**不**包含 Writer/Reader 的信息——那是 SEDP 的职责。

**设计哲学：分层发现 = 可伸缩性**

如果 SPDP 包含所有 Writer/Reader 信息：
- 一个有 1000 个 Writer 的 Participant 的 SPDP 消息会非常大
- 每次 SPDP 重发都要携带完整的端点列表
- 新增/删除 Writer 需要重新发送整个 SPDP 消息

分层设计：
- **SPDP**：只发现 Participant（轻量，固定大小，多播频繁发送）
- **SEDP**：在 SPDP 建立的 Participant 关系上，通过可靠传输交换端点信息

**底层思想**：**阶段式发现（Phased Discovery）**。类似于 DNS 的分层解析：先找到权威服务器（SPDP），再向服务器查询具体记录（SEDP）。这种分层避免了单一协议的膨胀问题。

### 4.6 为什么用 Fibonacci 堆管理 SPDP 调度？

与模块 3 中租约管理的理由相同，但从 SPDP 调度的角度补充：

SPDP 调度的热操作是 `decrease-key`（续约或调整发送时间）。每次收到远端 SPDP 响应后，可能需要调整该 Locator 的下次发送时间。Fibonacci 堆的 O(1) `decrease-key` 确保了调度调整不成为瓶颈。

## 5. 与规范的关系

- **RTPS v2.5 §8.5.3**：Simple Participant Discovery Protocol 规范
- **RTPS v2.5 §8.5.3.2**：SPDP 内置端点（EntityId = ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER/READER）
- **RTPS v2.5 §8.5.3.3**：SPDPdiscoveredParticipantData 的参数列表格式
- **DDS v1.4 §2.2.5**：ParticipantBuiltinTopicData 的字段定义

## 6. 总结

SPDP 的设计哲学可概括为**多路径发现 + 指数退避 + 墓碑保护**：
1. 基于租约时长的 SPDP 间隔提供安全裕量
2. AGING + 指数退避在自愈能力与资源节约之间平衡
3. deleted_participants 墓碑缓存防止僵尸复活
4. 多播 + 单播并行发送最大化可达性
5. 只携带 Participant 级信息保持消息轻量
6. Fibonacci 堆为调度调整提供 O(1) 性能
