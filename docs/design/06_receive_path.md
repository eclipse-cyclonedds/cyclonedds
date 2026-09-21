# 模块 6：收包路径（Receive Path）设计文档

## 1. 概述

收包路径负责从网络 socket 接收 RTPS 数据包，解析协议头和子消息，投递到 Reader History Cache，最终通过 `dds_read()`/`dds_take()` 提供给用户应用。

**关键文件**：
- `src/core/ddsi/src/ddsi_receive.c` — 收包主逻辑（16K+ 行，最大单文件）
- `src/core/ddsi/src/ddsi__receive.h` — 收包声明
- `src/core/ddsi/src/ddsi_rhc.c` — Reader History Cache
- `src/core/ddsc/src/dds_read.c` — 用户读取 API

## 2. 核心数据结构

### 2.1 接收缓冲池 (`rbufpool`)

```c
struct ddsi_rbufpool {
  ddsrt_mutex_t lock;
  struct ddsi_rbuf *current;      // 当前活跃的 ring buffer
  struct ddsi_rbuf *freelist;     // 空闲 buffer 链表
  uint32_t rbuf_size;             // 每个 buffer 大小（默认 ~2MB）
};

struct ddsi_rbuf {
  ddsrt_atomic_uint32_t n_live_rmsg_chunks;  // 活跃消息块计数
  uint32_t size;                  // buffer 总大小
  uint32_t max_rmsg_size;         // 单条消息最大大小
  unsigned char *freeptr;         // 下一个可用位置
  struct ddsi_rbufpool *rbufpool; // 所属池
  unsigned char payload[];        // 实际数据空间
};
```

### 2.2 接收消息 (`ddsi_rmsg`)

```c
struct ddsi_rmsg {
  struct ddsi_rbuf *rbuf;           // 所属 ring buffer
  ddsrt_atomic_uint32_t refcount;   // 引用计数
  uint32_t size;                    // 消息大小
  unsigned char payload[];          // RTPS 消息字节
};
```

### 2.3 投递队列 (`ddsi_dqueue`)

```c
struct ddsi_dqueue {
  ddsrt_mutex_t lock;
  ddsrt_cond_t cond;
  struct ddsi_dqueue_bubble *head, *tail;  // 待投递消息链表
  uint32_t max_samples;                     // 最大积压量
};
```

### 2.4 重排序缓冲区 (`ddsi_reorder`)

```c
struct ddsi_reorder {
  ddsrt_avl_tree_t sampleivtree;    // 按 seqnum 排序的样本区间树
  ddsi_seqno_t next_seq;            // 期望的下一个序列号
  enum ddsi_reorder_mode mode;      // NORMAL / BIDIR（双向）
  uint32_t max_samples;             // 最大缓存量
};
```

### 2.5 碎片重组 (`ddsi_defrag`)

```c
struct ddsi_defrag {
  ddsrt_avl_tree_t fragtree;        // 按 seqnum 排序的碎片树
  uint32_t max_fragments;           // 最大碎片缓存数
  uint32_t n_fragments;             // 当前碎片数
  enum ddsi_defrag_drop_mode drop_mode;  // OLDEST / LATEST
};
```

## 3. 机制设计

### 3.1 完整接收调用链

```
Receive 线程:
  ddsi_recv_thread()
    │
    ├─ 循环: socket poll (select/epoll/waitset)
    │   └─ 有数据到达:
    │       ├─ ddsi_rmsg_new(rbufpool)        [从 ring buffer 分配空间]
    │       ├─ recvfrom(socket, rmsg->payload) [读取 UDP 包]
    │       └─ ddsi_handle_rtps_message()      [处理 RTPS 消息]
    │
    └─ ddsi_handle_rtps_message(gv, rmsg)
        │
        ├─ 校验 RTPS Header
        │   ├─ magic: "RTPS" (4 bytes)
        │   ├─ version: 2.x
        │   ├─ vendor_id: 识别发送方厂商
        │   └─ guid_prefix: 发送方 Participant GUID 前缀
        │
        ├─ 遍历 Submessages:
        │   ├─ INFO_SRC → 更新源地址上下文
        │   ├─ INFO_DST → 更新目标 GUID 前缀
        │   ├─ INFO_TS  → 记录时间戳
        │   │
        │   ├─ DATA → handle_Data()
        │   │   ├─ 查找目标 proxy_writer (by writer GUID)
        │   │   ├─ ddsi_serdata_from_ser_iov()  [反序列化为 serdata]
        │   │   ├─ ddsi_defrag_rsample()         [碎片重组，如果需要]
        │   │   ├─ ddsi_reorder_rsample()        [重排序]
        │   │   └─ 投递:
        │   │       ├─ 同步投递 → deliver_user_data_synchronously()
        │   │       └─ 异步投递 → ddsi_dqueue_enqueue()
        │   │
        │   ├─ HEARTBEAT → handle_Heartbeat()
        │   │   └─ 比较 seqnum → 触发 ACK_NACK（如果有缺口）
        │   │
        │   ├─ ACK_NACK → handle_AckNack()
        │   │   └─ 更新 WHC → 触发重传（如果 Reader 缺少 sample）
        │   │
        │   ├─ GAP → handle_Gap()
        │   │   └─ 标记序列号范围为"不可用"
        │   │
        │   ├─ NACKFRAG → handle_NackFrag()
        │   │   └─ 重传指定碎片
        │   │
        │   └─ 其他: PAD, INFO_REPLY, ...
        │
        └─ ddsi_rmsg_commit(rmsg)  [释放 ring buffer 空间]

投递到用户:
  ddsi_rhc_store()                   [存入 Reader History Cache]
    ├─ 去重 (by seqnum)
    ├─ 状态跟踪 (NOT_READ → READ)
    └─ 通知 Listener / WaitSet

应用线程:
  dds_read() / dds_take()
    └─ dds_read_collect_sample()
       └─ ddsi_serdata_to_sample()   [反序列化为应用数据]
```

### 3.2 同步投递 vs 异步投递

```
同步投递 (deliver_synchronously = true):
  Recv 线程直接调用 RHC insert → 零额外延迟
  风险: RHC 操作耗时会阻塞 Recv 线程处理后续包

异步投递 (deliver_synchronously = false):
  Recv 线程将 sample 放入 dqueue → dqueue 线程处理投递
  优势: Recv 线程不被 RHC 操作阻塞
  代价: 队列调度延迟
```

### 3.3 碎片重组 + 重排序流水线

```
网络包到达（可能乱序、可能分片）:
  seqnum=5 frag=2/3   ──┐
  seqnum=3 complete    ──┤
  seqnum=5 frag=1/3   ──┤→ defrag → reorder → 按序投递
  seqnum=4 complete    ──┤     ↓        ↓
  seqnum=5 frag=3/3   ──┘   重组完整  排序后释放
                              sample    连续区间
```

## 4. 设计逻辑与设计思想

### 4.1 为什么用 Ring Buffer 而不是 malloc/free？

**设计哲学：确定性内存分配 + 缓存友好**

每收到一个 UDP 包（可能每秒数十万次），都需要一块内存来存放数据。

**malloc/free 方案**：
- 每次 `malloc(packet_size)` → `free(packet)` 
- 问题：内存碎片化、分配器锁竞争、不确定的延迟（worst-case 可能触发 sbrk/mmap）

**Ring Buffer 方案**：
- 预分配 ~2MB 连续内存
- 新消息从 `freeptr` 开始写入，`freeptr` 向前推进
- 旧消息通过引用计数释放，当整个 rbuf 的所有消息都释放后，rbuf 回到 freelist
- 分配操作是 O(1) 的指针递增

**底层思想**：**实时系统的内存分配必须是 O(1) 且无碎片的**。Ring Buffer 通过预分配和线性推进实现了确定性分配。这与 Linux 内核的 `sk_buff` 分配策略（预分配 slab cache）异曲同工。

### 4.2 为什么 Recv 线程数量是 1~3 而不是更多？

**设计哲学：RTPS 排序语义限制了并行度**

RTPS 协议要求：
- 来自同一 Writer 的 DATA 必须按 seqnum 顺序处理
- HEARTBEAT 和 ACK_NACK 必须与 DATA 在同一上下文中处理（它们共享 seqnum 状态）

如果 10 个线程并行处理来自同一 Writer 的消息：
- 线程 A 处理 seqnum=5，线程 B 处理 seqnum=3
- 线程 B 先完成，投递 seqnum=3 到 RHC
- 线程 A 后完成，投递 seqnum=5 到 RHC
- 但 seqnum=4 还没收到 → RHC 中出现"缺口"，触发不必要的 NACK

**CycloneDDS 的选择**：
- 1 线程：大多数场景最优。简单、无同步开销、天然有序
- 2 线程：分离 Discovery（SPDP/SEDP）和用户数据。Discovery 突发不影响数据延迟
- 3 线程：混合传输（UDP + TCP 独立线程）

**底层思想**：**不要让并行化损害正确性**。RTPS 的序列化语义天然限制了有效的并行度。增加线程不仅无益，还会引入同步开销和排序问题。这是 **Amdahl 定律的协议级应用**——可并行化的部分很小。

### 4.3 为什么分离 defrag 和 reorder？

**设计哲学：关注点分离——物理重组 vs 逻辑排序**

**defrag（碎片重组）**解决的问题：
- 一个逻辑 sample 可能因为超过 MTU 而被分成多个 DATAFRAG submessage
- defrag 收集同一 seqnum 的所有碎片，组装成完整的 sample
- 这是**物理层面**的问题（网络 MTU 限制）

**reorder（重排序）**解决的问题：
- 完整的 sample 可能因为网络路径不同而乱序到达
- reorder 按 seqnum 排序，只释放连续的区间 [next_seq, next_seq+N]
- 这是**逻辑层面**的问题（有序交付保证）

如果合并两者：
- 碎片状态和排序状态纠缠在一起
- 一个 seqnum 的碎片尚未收齐时，不知道它在排序中的位置
- 测试复杂：无法独立测试碎片重组和排序

**底层思想**：**流水线设计（Pipeline Pattern）**。defrag 是流水线的第一级（物理重组），reorder 是第二级（逻辑排序）。每一级独立工作、独立测试、独立优化。

### 4.4 为什么 builtins_dqueue 和 user_dqueue 分离？

**设计哲学：控制面不被数据面阻塞**

重复模块 3 的观点，但从收包路径的角度补充：

在收包路径中，分离的具体意义：
- 内置端点（SPDP/SEDP）消息进入 `builtins_dqueue`，由专门的投递线程处理
- 用户数据消息进入 `user_dqueue`，由另一个投递线程处理

即使用户数据突发导致 `user_dqueue` 积压，SPDP/SEDP 消息仍然能够及时处理——新 Participant 的发现不会因为数据洪流而延迟。

**底层思想**：**优先级反转是分布式系统的头号杀手之一**。通过物理隔离（不同的队列和线程），确保高优先级的控制面流量绝不与低优先级的数据面流量竞争资源。

### 4.5 为什么 rmsg 使用引用计数？

**设计哲学：延迟释放 + 零拷贝投递**

一个 rmsg（接收到的 RTPS 消息）可能包含多个 submessage，每个 submessage 可能投递到不同的 Reader。在异步投递模式下：

```
rmsg (refc=3)
  ├─ submessage[0] (DATA for topic_A) → 进入 user_dqueue → Reader_A
  ├─ submessage[1] (DATA for topic_B) → 进入 user_dqueue → Reader_B
  └─ submessage[2] (HEARTBEAT)        → 已处理完毕
```

rmsg 的内存不能在 recv 线程处理完后立即释放——dqueue 线程可能还在引用 submessage 中的数据。引用计数确保 rmsg 在所有使用者完成后才释放。

**底层思想**：**零拷贝要求共享所有权**。如果不用引用计数，就必须在投递到 dqueue 之前复制 submessage 数据——这违背了 ring buffer 设计的初衷。

### 4.6 为什么 ddsi_receive.c 有 16K+ 行？

这不是设计缺陷，而是**RTPS 协议复杂性的忠实反映**。

RTPS 定义了 13 种 submessage 类型，每种都有独特的处理逻辑：
- DATA/DATAFRAG 需要反序列化、碎片重组、重排序、投递
- HEARTBEAT/ACK_NACK 需要可靠性状态机更新
- GAP/NACKFRAG 需要缺口处理和碎片重传

每种 submessage 还要处理：安全模式、不同供应商的兼容性、边界条件（过期消息、重复消息、恶意消息）。

**底层思想**：**复杂性必须存在于某处**。CycloneDDS 选择将所有接收逻辑集中在一个文件中，而不是分散到 13 个文件——因为 submessage 处理之间共享大量上下文（源地址、GUID 前缀、时间戳）。分散会导致大量的上下文传递代码。

## 5. 与规范的关系

- **RTPS v2.5 §8.3**：所有 Submessage 类型的解析严格遵循规范格式
- **RTPS v2.5 §8.4**：Behavior Module 的 Stateful Reader/Writer 状态机在收包路径中实现
- **RTPS v2.5 §8.3.7.2**：DATA submessage 的 serializedPayload 字段对应 serdata 反序列化
- **DDS v1.4 §2.2.2.5.4**：`dds_read()` / `dds_take()` 的语义（read 不改变状态，take 移除 sample）

## 6. 总结

收包路径的设计哲学可概括为**确定性分配 + 流水线处理 + 控制面隔离**：
1. Ring Buffer 提供 O(1) 确定性内存分配
2. 1~3 recv 线程尊重 RTPS 的排序语义
3. defrag → reorder 流水线分离物理重组与逻辑排序
4. 双投递队列隔离控制面与数据面
5. rmsg 引用计数支持零拷贝异步投递
6. 集中式 16K 行处理逻辑反映协议的内在复杂性
