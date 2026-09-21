# 模块 7：可靠性协议（Reliability）设计文档

## 1. 概述

可靠性模块实现 RTPS v2.5 §8.4 定义的 Behavior Module，通过 HEARTBEAT/ACK_NACK/GAP/NACKFRAG 子消息实现 Writer 与 Reader 之间的可靠数据交付。它是 DDS RELIABLE QoS 的底层引擎。

**关键文件**：
- `src/core/ddsi/src/ddsi_acknack.c` — ACK_NACK 调度与发送
- `src/core/ddsi/src/ddsi__acknack.h` — ACK_NACK 声明
- `src/core/ddsi/src/ddsi_whc.c` — Writer History Cache（默认实现）
- `src/core/ddsi/src/ddsi__whc.h` — WHC 接口定义
- `src/core/ddsi/src/ddsi_transmit.c` — HEARTBEAT 构建与重传

## 2. 核心数据结构

### 2.1 Writer History Cache 接口

```c
struct ddsi_whc_ops {
  ddsi_whc_insert_t insert;                       // 插入新 sample
  ddsi_whc_remove_acked_messages_t remove_acked;  // 移除已确认 sample
  ddsi_whc_borrow_sample_t borrow_sample;         // 借出 sample 用于重传
  ddsi_whc_return_sample_t return_sample;          // 归还借出的 sample
  ddsi_whc_sample_iter_t sample_iter;              // 遍历 sample
  ddsi_whc_get_state_t get_state;                  // 查询 WHC 状态
  ddsi_whc_free_t free;                            // 释放 WHC
};

struct ddsi_whc {
  const struct ddsi_whc_ops *ops;   // 操作虚函数表
};
```

### 2.2 WHC 默认实现的核心状态

```c
struct whc_impl {
  struct ddsi_whc common;
  uint32_t hdepth;                  // HISTORY depth (KEEP_LAST N)
  uint32_t tldepth;                 // Transient-Local depth
  uint32_t idxdepth;                // max(hdepth, tldepth)
  ddsi_seqno_t max_drop_seq;        // 可安全丢弃的最大 seqnum
  ddsi_seqno_t seq_size;            // 当前缓存的 sample 数
  size_t unacked_bytes;             // 未确认的字节总数
  // 索引结构
  struct whc_intvtree sampleivtree; // 按 seqnum 的区间树
  struct whc_idxnode *idx;          // 按 instance 的哈希索引
};
```

### 2.3 ACK_NACK 调度结果

```c
enum ddsi_add_acknack_result {
  AANR_SILENT_ACK,       // 抑制：近期已发过 ACK，无需再发
  AANR_NACK,             // 发送 NACK：Reader 有缺失的 sample
  AANR_SUPPRESSED_NACK   // 发送 ACK（不含 NACK）：虽有缺口但暂不请求
};
```

### 2.4 HEARTBEAT 控制

```c
struct ddsi_hbcontrol {
  ddsrt_mtime_t t_of_last_write;     // 上次写入时间
  ddsrt_mtime_t t_of_last_hb;       // 上次 HEARTBEAT 时间
  ddsrt_mtime_t t_of_last_ackhb;    // 上次需要 ACK 的 HEARTBEAT 时间
  uint32_t hbs_since_last_write;     // 自上次写入以来的 HB 数
  uint32_t last_packetid;            // 上次数据包 ID
};
```

## 3. 机制设计

### 3.1 Writer 侧可靠性状态机

```
Writer 侧:

  写入新 sample:
    ddsi_whc_insert(seqnum, serdata)
      → sample 存入 WHC
      → 分配新的 seqnum
      → 构建 DATA submessage → 发送

  周期性 HEARTBEAT:
    ddsi_add_heartbeat()
      → 构建 HEARTBEAT(firstSN, lastSN, count)
      → firstSN = WHC 中最小的 seqnum
      → lastSN = 最后写入的 seqnum
      → 发送到所有匹配的 Reader

  收到 ACK_NACK:
    handle_AckNack(readerGUID, base, bitmap)
      → base: Reader 确认收到 base-1 之前的所有 sample
      → bitmap: base 之后哪些 seqnum 缺失
      ├─ 更新 per-reader 确认状态
      ├─ ddsi_whc_remove_acked_messages(min_acked_seq)
      │   → 移除所有 Reader 都已确认的 sample
      └─ 对 bitmap 中的缺失 seqnum:
         ├─ ddsi_whc_borrow_sample(seqnum)
         └─ 构建重传 DATA → 发送
```

### 3.2 Reader 侧可靠性状态机

```
Reader 侧:

  收到 DATA(seqnum):
    → 正常: seqnum == expected_seq → 投递, expected_seq++
    → 乱序: seqnum > expected_seq → 存入 reorder 缓冲
    → 重复: seqnum < expected_seq → 丢弃

  收到 HEARTBEAT(firstSN, lastSN, count):
    → 比较 expected_seq 与 [firstSN, lastSN] 范围
    → 如果 expected_seq < firstSN:
       → Reader 落后太多，缺失的 sample 已被 Writer 丢弃 → sample_lost
    → 如果有缺口 (expected_seq < lastSN 且未全部收到):
       → ddsi_sched_acknack_if_needed()
         → 决策: AANR_NACK / AANR_SILENT_ACK / AANR_SUPPRESSED_NACK
    → 构建 ACK_NACK(base, bitmap) → 发送

  收到 GAP(firstSN, lastSN):
    → 标记 [firstSN, lastSN] 范围的 sample 为"不可用"
    → 跳过这些 seqnum，继续等待后续 sample
```

### 3.3 自适应 HEARTBEAT 调度

```
初始阶段 (Writer 刚创建):
  → 积极发送 HEARTBEAT（短间隔）
  → 目的: 让 Reader 尽快同步

稳态阶段 (所有 Reader 已同步):
  → 懒惰发送 HEARTBEAT（长间隔或仅在写入新数据后发送）
  → 目的: 减少控制流量

丢包阶段 (收到 NACK):
  → 加速 HEARTBEAT 频率
  → 立即重传丢失的 sample
  → 目的: 快速恢复
```

### 3.4 重传合并

```
Reader_A NACK seqnum {5, 7}
Reader_B NACK seqnum {5, 8}
                    ↓
合并: seqnum {5} 只重传一次（多播）
      seqnum {7} 重传给 Reader_A（单播）
      seqnum {8} 重传给 Reader_B（单播）
```

## 4. 设计逻辑与设计思想

### 4.1 为什么 WHC 设计为接口（ops 虚函数表）？

**设计哲学：策略与机制分离**

DDS 规范定义了两种历史策略（DDS v1.4 §2.2.3.18）：
- **KEEP_LAST(N)**：只保留每个 instance 最后 N 个 sample
- **KEEP_ALL**：保留所有 sample 直到被确认

这两种策略对 WHC 的内部数据结构要求完全不同：

**KEEP_LAST(N)** 需要：
- 每个 instance 的循环数组（O(1) 覆盖旧 sample）
- instance 索引（快速定位特定 key 的 sample）

**KEEP_ALL** 需要：
- 简单的 seqnum 有序链表（不按 instance 分组）
- 无需 instance 索引（不覆盖旧 sample）

通过接口抽象，两种策略可以有完全不同的内部实现，而可靠性引擎（HEARTBEAT/ACK_NACK 逻辑）不需要知道 WHC 内部是如何组织的。

**底层思想**：这是**策略模式（Strategy Pattern）**在 C 语言中的实现。可靠性引擎是**机制**（如何发送 HEARTBEAT、如何处理 ACK_NACK），WHC 是**策略**（保留哪些 sample、何时淘汰）。分离机制与策略使两者可以独立演化。

### 4.2 为什么有三种 ACK_NACK 结果（SILENT/NACK/SUPPRESSED）？

**设计哲学：减少反馈风暴**

考虑 1 个 Writer 对 100 个 Reader 发送数据，丢失了 1 个包。如果每个 Reader 都立即发送 NACK：

```
Writer 收到 100 个 NACK → 重传 100 次同一个 sample → 网络风暴
```

三种结果的作用：

**AANR_SILENT_ACK**（静默确认）：
- "我近期刚发过 ACK，不需要再发"
- 防止同一个 Reader 在短时间内发送多个冗余 ACK
- **机制**：设置最小 ACK 间隔，在间隔内不发送

**AANR_NACK**（发送 NACK）：
- "我确实缺少 sample，需要重传"
- 只有在确认缺失且未被抑制时才触发

**AANR_SUPPRESSED_NACK**（抑制 NACK，发送纯 ACK）：
- "我知道缺少 sample，但暂时不请求重传"
- 场景：刚收到 HEARTBEAT，缺失的 sample 可能正在传输途中
- 等待一小段时间，如果 sample 到达就不需要 NACK

**底层思想**：这是**拥塞控制思维在可靠性协议中的应用**。TCP 用 AIMD（加性增乘性减）控制发送速率，RTPS 用 ACK 抑制控制反馈速率。核心原则相同：**避免正反馈循环导致的网络崩溃**。

### 4.3 为什么 ACK_NACK 只确认"已投递"而非"已接收"？

```c
// ddsi_acknack.c 中的关键逻辑:
// base = next_deliv_seq()  ← 下一个待投递的 seqnum
// 而不是 base = max_received_seq + 1
```

**设计哲学：端到端确认 > 跳级确认**

"已接收"（recv 线程收到 UDP 包）和"已投递"（sample 进入 RHC 且经过重排序）是两个不同的事件。

如果基于"已接收"确认：
- seqnum 3 和 5 已接收，seqnum 4 未收到
- ACK base=6 → Writer 认为 3,4,5 都收到了 → Writer 丢弃 seqnum 4
- 但 seqnum 4 实际上从未到达 Reader → **永久丢失**

如果基于"已投递"确认：
- seqnum 3 已投递，seqnum 4 未收到，seqnum 5 在 reorder 缓冲中
- ACK base=4 → Writer 知道 4 未收到 → 重传 seqnum 4
- seqnum 4 到达后，3,4,5 按序投递 → **无丢失**

**底层思想**：**只确认你已经安全处理的数据**。这与 TCP 的累积确认（cumulative ACK）原理相同——ACK 号表示"此号之前的所有数据已安全接收"。

### 4.4 HEARTBEAT 的自适应调度为什么重要？

**设计哲学：控制流量与恢复速度的动态平衡**

固定间隔 HEARTBEAT 的问题：
- 间隔太短 → 网络中充斥控制消息，浪费带宽
- 间隔太长 → 丢包后恢复缓慢，Reader 长时间等待重传

CycloneDDS 的自适应策略：

**Phase 1：新 Writer**
- 高频 HEARTBEAT（e.g., 100ms 间隔）
- 目的：让新连接的 Reader 快速发现 Writer 的 seqnum 范围并同步

**Phase 2：稳态**
- 低频 HEARTBEAT（e.g., 与 lease 周期挂钩）
- 或 piggyback：在 DATA submessage 后附带 HEARTBEAT
- 目的：最小化控制流量

**Phase 3：丢包恢复**
- 收到 NACK 后立即加速 HEARTBEAT
- 在确认所有 Reader 同步后回退到稳态
- 目的：快速恢复而不是等待下一次定期 HEARTBEAT

**底层思想**：这是**自适应系统设计**——根据系统状态（健康/恢复中/初始化）动态调整行为参数。类似于 TCP 的慢启动/拥塞避免/快速恢复三态切换。

### 4.5 WHC 的水位线（watermark）机制

```c
whc_lowwater_mark = 100KB    // 低水位
whc_highwater_mark = 500KB   // 高水位
```

**设计哲学：背压（Backpressure）的实现**

当 Writer 写入速度远超网络发送速度或 Reader 确认速度时，WHC 不断增长。

**高水位触发**：WHC 中未确认的数据超过 `highwater_mark` → Writer 开始阻塞 `dds_write()` → 应用程序被反压

**低水位恢复**：当 Reader 确认足够多的 sample，WHC 降到 `lowwater_mark` 以下 → Writer 解除阻塞

**为什么两个水位而不是一个阈值？**

单一阈值会导致**振荡**：
```
WHC 到达阈值 → 阻塞 → 数据被确认 → WHC 降到阈值以下 → 解除阻塞
→ Writer 立即写入 → WHC 再次到达阈值 → 阻塞 → ... （高频振荡）
```

双水位消除振荡：
```
WHC 到达高水位 → 阻塞
→ 数据被确认 → WHC 持续下降
→ WHC 降到低水位 → 解除阻塞
→ Writer 有足够的缓冲空间写入，不会立即再次触发高水位
```

**底层思想**：**迟滞控制（Hysteresis）**。与物理学中的迟滞环相同，引入两个阈值消除系统在单一阈值附近的高频振荡。这在恒温器、网络拥塞控制、内存管理中广泛使用。

### 4.6 为什么 WHC 用区间树（Interval Tree）索引？

**设计哲学：优化 ACK_NACK 处理的批量操作**

ACK_NACK 消息中的 `base + bitmap` 表示一个序列号区间中哪些 sample 缺失。处理 ACK_NACK 需要两类操作：

1. **移除已确认的 sample**：`remove_acked_messages(min_seq)` → 移除 seqnum < min_seq 的所有 sample
2. **查找待重传的 sample**：`borrow_sample(seqnum)` → 按 seqnum 定位

区间树（Interval Tree）提供：
- **范围删除 O(k + log n)**：一次操作移除一个连续区间的 sample
- **点查询 O(log n)**：按 seqnum 定位特定 sample
- **范围查询 O(k + log n)**：查找某区间内的所有 sample（用于 bitmap 处理）

**底层思想**：**数据结构为热操作优化**。WHC 的热操作是"批量移除"和"按 seqnum 查找"，区间树正好为这两种操作提供最优复杂度。

## 5. 与规范的关系

- **RTPS v2.5 §8.4.6**：StatefulWriter 行为——对应 Writer 侧状态机
- **RTPS v2.5 §8.4.10**：StatefulReader 行为——对应 Reader 侧状态机
- **RTPS v2.5 §8.3.5.5**：HEARTBEAT submessage 格式
- **RTPS v2.5 §8.3.5.4**：ACK_NACK submessage 格式（base + bitmap）
- **RTPS v2.5 §8.7.1**：RELIABILITY QoS 到 RTPS 行为的映射
- **DDS v1.4 §2.2.3.14**：RELIABILITY QoS（BEST_EFFORT/RELIABLE）

## 6. 总结

可靠性模块的设计哲学可概括为**策略-机制分离 + 反馈控制 + 迟滞稳定**：
1. WHC 接口将保留策略（KEEP_LAST/KEEP_ALL）与可靠性机制（HEARTBEAT/ACK_NACK）解耦
2. 三种 ACK_NACK 结果防止反馈风暴
3. 基于"已投递"的确认确保端到端可靠性
4. 自适应 HEARTBEAT 在控制流量与恢复速度之间动态平衡
5. 双水位线通过迟滞控制消除背压振荡
6. 区间树为批量移除和点查询提供最优复杂度
