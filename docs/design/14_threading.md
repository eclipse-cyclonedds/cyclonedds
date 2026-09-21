# 模块 14：线程架构设计文档

## 1. 概述

CycloneDDS 使用固定角色线程模型，每种线程承担专属职责。线程间通过 vtime（虚拟时间）安全点机制协调，确保无锁读取路径和安全的延迟释放。

**关键文件**：
- `src/core/ddsi/src/ddsi_thread.c` — 线程状态管理
- `src/core/ddsi/include/dds/ddsi/ddsi_thread.h` — 线程状态定义
- `src/core/ddsi/src/ddsi_init.c` — 线程创建
- `src/core/ddsi/src/ddsi_receive.c` — 接收线程主循环
- `src/core/ddsi/src/ddsi_gc.c` — GC 线程

## 2. 核心数据结构

### 2.1 线程状态

```c
struct ddsi_thread_state {
  ddsrt_thread_t tid;                // OS 线程 ID
  const char *name;                  // 线程名称（调试用）
  volatile uint32_t vtime;           // 虚拟时间（偶数=安全, 奇数=临界区）
  enum ddsi_thread_state_kind kind;  // LAZILY / REAL / BUILTIN
  void *arg;                         // 线程参数
  struct ddsi_domaingv *gv;          // Domain 全局变量引用
};
```

### 2.2 全局线程状态表

```c
struct ddsi_thread_states {
  uint32_t nthreads;                 // 线程总数
  struct ddsi_thread_state *ts;      // 线程状态数组
};

// 全局实例:
static struct ddsi_thread_states thread_states;
```

### 2.3 线程类型与角色

```
┌──────────────────┬───────┬───────────────────────────────────────────┐
│ 线程类型          │ 数量  │ 职责                                      │
├──────────────────┼───────┼───────────────────────────────────────────┤
│ Receive Thread   │ 1~3   │ Socket poll → RTPS 解析 → 投递到 RHC      │
│ Event Thread     │ 1     │ 定时事件: HEARTBEAT, SPDP 广播, lease 检查 │
│ GC Thread        │ 1     │ 延迟实体释放（等待安全点）                  │
│ Listener Thread  │ 0~1   │ TCP accept() 循环（仅 TCP 模式）           │
│ 应用线程          │ N     │ dds_write(), dds_read()（非 DDS 管理）     │
└──────────────────┴───────┴───────────────────────────────────────────┘
```

## 3. 机制设计

### 3.1 线程创建序列

```
ddsi_init(&gv):
  │
  ├─ 初始化阶段:
  │   ├─ ddsi_thread_states_init()        [分配线程状态数组]
  │   ├─ ddsi_xeventq_new()              [事件队列]
  │   └─ ddsi_gcreq_queue_new()          [GC 队列]
  │
  ├─ 线程创建:
  │   ├─ create_thread("gc", gc_thread_fn)
  │   │   → GC 线程立即启动，等待 gcreq
  │   │
  │   ├─ create_thread("xevent", xevent_thread_fn)
  │   │   → 事件线程启动，处理定时事件
  │   │
  │   ├─ create_thread("recv", recv_thread_fn)  × 1~3
  │   │   → Receive 线程启动，开始 poll socket
  │   │   → 数量由 MultipleReceiveThreads 配置决定
  │   │
  │   └─ [TCP 模式] create_thread("listen", listen_thread_fn)
  │       → TCP Listener 线程，accept 新连接
  │
  └─ ddsi_start():
      → 激活 Domain，开始正常运行
```

### 3.2 vtime 安全点协议

```
vtime 的工作原理:

  线程进入临界区:                    线程离开临界区:
    vtime = vtime + 1  (奇数)         vtime = vtime + 1  (偶数)
    → 表示"我可能持有共享引用"          → 表示"我不持有任何共享引用"

GC 线程等待安全点:
  gc_thread:
    while (has_pending_gcreq) {
      all_safe = true;
      for (each thread_state ts in thread_states) {
        if (ts.vtime 是奇数)           // 有线程在临界区
          all_safe = false;            // 不能释放
      }
      if (all_safe)
        process_gcreq();               // 所有线程都在安全点 → 释放
      else
        sleep_briefly();               // 等待
    }

  示意:
    Thread A: ────[安全]──[临界区]──[安全]──[安全]──[安全]────
    Thread B: ────[安全]──[安全]──[安全]──[临界区]──[安全]────
    Thread C: ────[安全]──[安全]──[安全]──[安全]──[安全]────
    GC:       ────wait────wait────wait────wait────FREE!────
                                                  ↑
                                        所有线程同时处于偶数 vtime
```

### 3.3 接收线程主循环

```
ddsi_recv_thread(arg):
  loop:
    ├─ ddsrt_select() / poll()        [等待 socket 可读]
    │
    ├─ ddsi_thread_state_awake()       [vtime += 1 → 奇数（进入临界区）]
    │
    ├─ ddsi_conn_read(conn, rbuf, ...) [从 socket 读取数据到 ring buffer]
    │
    ├─ ddsi_handle_rtps_message()      [解析 RTPS 消息]
    │   ├─ 校验 RTPS Header
    │   ├─ 解析 Submessage 序列
    │   ├─ 查找 entity_index (需要共享引用)
    │   └─ 投递到 RHC / 处理协议消息
    │
    └─ ddsi_thread_state_asleep()      [vtime += 1 → 偶数（离开临界区）]
        → 回到 loop，在 poll() 中等待下一个数据包
```

### 3.4 事件队列调度

```
xevent_thread(arg):
  loop:
    ├─ 检查 xeventq 堆顶事件的触发时间
    │
    ├─ if (now >= 堆顶事件时间):
    │   ├─ 取出事件
    │   ├─ 执行回调:
    │   │   ├─ HEARTBEAT 定时器 → ddsi_add_heartbeat()
    │   │   ├─ SPDP 广播定时器 → spdp_broadcast()
    │   │   ├─ Lease 超时检查 → ddsi_lease_check()
    │   │   ├─ Deadline 超时 → deadline_missed_cb()
    │   │   └─ ACK_NACK 延迟发送 → acknack_cb()
    │   └─ 如果是周期性事件 → 更新下次触发时间, 重新插入堆
    │
    └─ if (now < 堆顶事件时间):
        └─ ddsrt_cond_waituntil(堆顶时间)
           → 阻塞到下一个事件或被新事件唤醒
```

### 3.5 线程监控（Watchdog）

```
Event 线程定期检查:
  for (each thread_state ts) {
    if (ts.vtime 长时间未变化) {
      → 线程可能卡死（死锁或无限循环）
      → 记录警告日志
    }
  }

  检测原理:
    正常线程: vtime 每次进出临界区都 +1 → 持续递增
    卡死线程: vtime 停止变化 → 被 watchdog 发现
```

## 4. 设计逻辑与设计思想

### 4.1 为什么使用固定角色线程而非线程池？

**设计哲学：可预测延迟优于吞吐量**

线程池模型：
```
任务到达 → 入队 → 空闲线程取出 → 执行
延迟 = 排队时间 + 上下文切换 + 执行时间
问题: 排队时间不确定，在高负载时可能很长
```

固定角色线程模型：
```
数据到达 → Receive 线程直接处理 → 完成
延迟 = 执行时间（无排队、无上下文切换）
```

固定角色线程的优势：
1. **延迟确定性**：每种操作由专用线程处理，不存在排队等待
2. **缓存局部性**：Receive 线程反复处理 RTPS 数据 → 指令/数据缓存预热
3. **避免优先级反转**：高优先级任务不会被低优先级任务阻塞（因为在不同线程）
4. **简化调试**：每个线程的行为是确定性的，便于追踪问题

**底层思想**：**实时系统偏好专用线程，因为其最坏执行时间（WCET）可预测**。Linux 内核的 softirq/ksoftirqd 采用相同的设计——网络包在专用上下文中处理，而非通用工作队列。DPDK 的 run-to-completion 模型也是这个思想的极致表达。

### 4.2 为什么 Receive 线程数量是可配置的（1~3）？

**设计哲学：适应硬件拓扑**

不同的部署场景需要不同的并行度：

```
1 个 Receive 线程:
  → 所有 socket (disc_mc, disc_uc, data_mc, data_uc) 在一个线程中 poll
  → 适用: 低吞吐量、简单系统、嵌入式设备

2 个 Receive 线程:
  → Thread 1: disc_conn_mc + disc_conn_uc (Discovery)
  → Thread 2: data_conn_mc + data_conn_uc (Data)
  → 隔离: 高速数据不会延迟 Discovery 处理

3 个 Receive 线程:
  → Thread 1: disc_conn_mc + disc_conn_uc (Discovery)
  → Thread 2: data_conn_mc (Data 多播)
  → Thread 3: data_conn_uc (Data 单播)
  → 最大并行: 每种流量独立处理
```

**底层思想**：Receive 线程的数量映射到**独立 I/O 流**的数量。将 Discovery 与 Data 分离可以防止数据洪流淹没发现流量——这与 NIC 的 RSS（Receive Side Scaling）将不同流分发到不同 CPU 核心的思想一致。

### 4.3 为什么用 vtime（虚拟时间）实现线程安全点？

**设计哲学：读路径零开销**

三种并发内存回收策略的比较：

| 策略 | 读开销 | 写开销 | 复杂度 |
|------|--------|--------|--------|
| 引用计数 | 原子 inc/dec (每次) | 无 | 低 |
| Hazard Pointer | 原子 store (每次) | 扫描所有 HP | 高 |
| Epoch-based (vtime) | vtime +1 (进出临界区) | 等待所有 epoch | 中 |

vtime 的优势：
- **读操作不需要原子操作于共享缓存行**——vtime 是每线程独立的
- **没有缓存行弹跳（cache line bouncing）**——引用计数的致命缺陷
- **GC 等待的开销由 GC 线程承担**——不影响读路径

```c
// 引用计数: 每次读取都有 2 次原子操作
ref = atomic_inc(&entity->refcount);   // cache line invalidation
use(entity);
atomic_dec(&entity->refcount);          // cache line invalidation

// vtime: 每个临界区 2 次本地写
ts->vtime++;                            // 只写自己的 cache line
use(entity);
ts->vtime++;                            // 只写自己的 cache line
```

**底层思想**：这是 **RCU（Read-Copy-Update）**在用户空间的实现。Linux 内核中，RCU 用于保护频繁读取但极少修改的数据结构（路由表、文件描述符表）。CycloneDDS 的 entity_index 正是这样的场景——Receive 线程每秒查找数千次，而实体创建/删除每秒可能只有个位数。vtime 是 epoch 的轻量实现。

### 4.4 为什么 Event 线程使用定时事件队列而非轮询？

**设计哲学：工作保持调度（Work-Conserving Scheduling）**

两种定时任务处理方式的对比：

```
轮询模式:
  while (true) {
    check_all_heartbeat_timers();     // O(n)
    check_all_spdp_timers();          // O(n)
    check_all_lease_timers();         // O(n)
    sleep(1ms);                       // 忙等待
  }
  → 即使没有事件也消耗 CPU
  → 检查所有定时器的开销 O(n) 随实体数增长

事件队列模式:
  while (true) {
    next_event = heap_top(xeventq);   // O(1) 查看下一个事件
    wait_until(next_event.time);       // 精确等待，零 CPU 使用
    process(next_event);               // 只处理到期的事件
  }
  → 无事件时零 CPU 使用
  → 只处理需要处理的事件 O(1)
```

**底层思想**：事件队列本质上是一个**定时器轮（Timer Wheel）**，类似 Linux 内核的 hrtimer 框架。堆数据结构保证 O(log n) 插入和 O(1) 取最早事件。对于嵌入式系统，"无事可做时不消耗 CPU"是关键的能耗优化。

### 4.5 为什么写操作在应用线程而非专用发送线程中执行？

**设计哲学：同步快路径最小化延迟**

如果使用专用发送线程：
```
应用线程:                          发送线程:
  dds_write(data)
    → 序列化
    → 入队 ──────────────────────→ 出队
    → 返回                          → 构建 RTPS
                                    → sendto()
  延迟: 序列化 + 入队 + 上下文切换 + 出队 + 发送
        至少 2 次上下文切换 (~2-10μs × 2)
```

在应用线程中直接发送：
```
应用线程:
  dds_write(data)
    → 序列化
    → 构建 RTPS
    → sendto()
    → 返回
  延迟: 序列化 + 发送
        0 次上下文切换
```

**底层思想**：**零拷贝、零上下文切换设计**。调用线程拥有数据并直接发送，避免了线程间传递的所有开销。这与 DPDK 的 run-to-completion 模型一致——数据包的所有处理在同一个线程中完成，而非在流水线的多个线程间传递。对于实时系统，上下文切换是延迟的头号杀手。

## 5. 与规范的关系

- **RTPS v2.5 §8.4**：规范定义了 Reader/Writer 的行为但未规定线程模型
- **RTPS v2.5 §8.4.7**：Writer 行为（周期性 HEARTBEAT）→ 由 Event 线程执行
- **RTPS v2.5 §8.4.10**：Reader 行为（ACK_NACK 响应）→ 由 Receive 线程触发
- 线程架构是实现选择，非规范要求——CycloneDDS 的设计优化了延迟和可预测性

## 6. 总结

线程架构的设计哲学可概括为**固定角色 + vtime 安全点 + 事件驱动 + 同步写入**：
1. 固定角色线程提供可预测的延迟和缓存局部性
2. 可配置的 Receive 线程数（1~3）适应不同硬件拓扑
3. vtime 安全点实现读路径零开销的内存回收协调
4. 定时事件队列实现工作保持调度，无事件时零 CPU 使用
5. 写操作在应用线程执行，消除上下文切换延迟
