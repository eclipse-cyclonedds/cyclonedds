# 模块 12：Listener 与 WaitSet 设计文档

## 1. 概述

Listener 和 WaitSet 是 DDS 的两种异步通知机制。Listener 提供回调式的推送通知，WaitSet 提供轮询式的拉取通知。两者共同构成了应用程序感知 DDS 事件（数据到达、匹配变化、活性变化等）的完整通道。

**关键文件**：
- `src/core/ddsc/src/dds_listener.c` — Listener 实现
- `src/core/ddsc/src/dds_waitset.c` — WaitSet 实现
- `src/core/ddsc/include/dds/ddsc/dds_public_listener.h` — Listener 公开 API
- `src/core/ddsc/src/dds__types.h` — 实体结构定义（含观察者相关字段）

## 2. 核心数据结构

### 2.1 Listener 结构

```c
struct dds_listener {
  uint32_t inherited;            // 哪些回调从父实体继承（位掩码）
  uint32_t reset_on_invoke;      // 哪些状态在回调触发后自动重置（位掩码）

  // 13 个回调函数指针 + 对应的 arg 指针:
  dds_on_data_available_fn on_data_available;
  void *on_data_available_arg;

  dds_on_publication_matched_fn on_publication_matched;
  void *on_publication_matched_arg;

  dds_on_subscription_matched_fn on_subscription_matched;
  void *on_subscription_matched_arg;

  dds_on_liveliness_changed_fn on_liveliness_changed;
  void *on_liveliness_changed_arg;

  dds_on_requested_deadline_missed_fn on_requested_deadline_missed;
  dds_on_requested_incompatible_qos_fn on_requested_incompatible_qos;
  dds_on_offered_deadline_missed_fn on_offered_deadline_missed;
  dds_on_offered_incompatible_qos_fn on_offered_incompatible_qos;
  dds_on_sample_lost_fn on_sample_lost;
  dds_on_sample_rejected_fn on_sample_rejected;
  dds_on_liveliness_lost_fn on_liveliness_lost;
  dds_on_data_on_readers_fn on_data_on_readers;
  dds_on_inconsistent_topic_fn on_inconsistent_topic;
  // ... 每个回调都有对应的 arg
};
```

### 2.2 WaitSet 结构

```c
struct dds_waitset {
  dds_entity m_entity;            // 继承自实体基类
  ddsrt_mutex_t wait_lock;        // 等待锁
  ddsrt_cond_etime_t wait_cond;   // 条件变量
  size_t nentities;               // 观察的实体数
  size_t ntriggered;              // 当前已触发的实体数
  dds_attachment *entities;       // 观察实体数组
};

struct dds_attachment {
  dds_entity *entity;             // 被观察的实体
  dds_entity_t handle;            // 实体句柄
  dds_attach_t arg;               // 用户自定义附加参数
};
```

### 2.3 实体中的观察者字段

```c
struct dds_entity {
  // ... 其他字段
  ddsrt_mutex_t m_mutex;          // 实体状态锁
  ddsrt_mutex_t m_observers_lock; // 观察者锁（独立于状态锁）
  dds_listener_t m_listener;      // 当前生效的 Listener
  uint32_t m_cb_count;            // 回调嵌套计数
  uint32_t m_cb_pending_count;    // 待处理回调计数
  dds_entity_observer *m_observers; // 观察者链表（WaitSet 等）
  status_and_enabled_t m_status;  // 状态位（原子操作）
};
```

## 3. 机制设计

### 3.1 Listener 继承解析

```
实体层次:
  Participant (listener_P)
    └─ Subscriber (listener_S)
       └─ DataReader (listener_R)

Listener 解析顺序:
  DataReader 事件 (e.g., DATA_AVAILABLE):
    1. 检查 listener_R.on_data_available → 如果非 NULL → 调用
    2. 如果 NULL → 检查 listener_S.on_data_on_readers → 如果非 NULL → 调用
    3. 如果 NULL → 检查 listener_P.on_data_on_readers → 如果非 NULL → 调用
    4. 如果全 NULL → 通知 WaitSet

  inherited 位掩码记录来源:
    listener_R.inherited & DATA_AVAILABLE_BIT == 1
      → 表示该回调是从父实体继承的
    listener_R.inherited & DATA_AVAILABLE_BIT == 0
      → 表示该回调是直接设置的
```

### 3.2 状态传播链

```
DDSI 层检测事件:
  e.g., 新 proxy_writer 匹配成功
    │
    ├─ status_cb(entity, status_id)            [DDSI → DDS 桥接]
    │
    ├─ dds_entity_status_set(entity, status)   [原子 OR 状态位]
    │   entity->m_status |= SUBSCRIPTION_MATCHED_STATUS
    │
    └─ dds_entity_observers_signal(entity)     [通知观察者]
        │
        ├─ 查找生效的 Listener:
        │   └─ dds_override_inherited_listener()
        │       → 向上遍历实体树，找到第一个设置了该回调的 Listener
        │
        ├─ if Listener 存在:
        │   ├─ 调用回调: listener.on_subscription_matched(entity, status, arg)
        │   └─ if reset_on_invoke & SUBSCRIPTION_MATCHED_BIT:
        │       → entity->m_status &= ~SUBSCRIPTION_MATCHED_STATUS  [自动清除]
        │
        └─ if Listener 不存在:
            └─ 遍历 m_observers 链表
                → 唤醒所有关联的 WaitSet: ddsrt_cond_broadcast(&ws->wait_cond)
```

### 3.3 WaitSet 等待流程

```
dds_waitset_wait(ws, triggered_array, max_count, timeout)
  │
  ├─ dds_waitset_wait_impl():
  │   ├─ lock(ws->wait_lock)
  │   │
  │   ├─ 循环:
  │   │   ├─ 遍历 ws->entities[0..nentities-1]:
  │   │   │   ├─ Condition 实体: 原子读 m_trigger 标志
  │   │   │   └─ 普通实体: 检查 (status & enabled_mask) != 0
  │   │   │
  │   │   ├─ ntriggered > 0 → break (有触发)
  │   │   ├─ timeout 到期 → break (超时)
  │   │   └─ ddsrt_cond_waituntil(&ws->wait_cond, &ws->wait_lock, abstime)
  │   │       → 阻塞等待（被 observers_signal 唤醒或超时）
  │   │
  │   ├─ 收集已触发实体到 triggered_array
  │   ├─ unlock(ws->wait_lock)
  │   └─ 返回触发数量
```

### 3.4 WaitSet 附加与分离

```
dds_waitset_attach(ws, entity):
  ├─ lock(ws->wait_lock)
  ├─ 创建 dds_attachment { entity, handle, arg }
  ├─ 添加到 ws->entities 数组
  ├─ 在 entity->m_observers 链表中注册 ws
  ├─ unlock(ws->wait_lock)
  └─ 如果 entity 已有未处理状态 → 立即触发

dds_waitset_detach(ws, entity):
  ├─ lock(ws->wait_lock)
  ├─ 从 ws->entities 移除
  ├─ 从 entity->m_observers 链表中注销
  └─ unlock(ws->wait_lock)
```

## 4. 设计逻辑与设计思想

### 4.1 为什么 Listener 使用继承模型？

**设计哲学：Convention over Configuration（约定优于配置）**

在典型 DDS 应用中，同一 Participant 下的大多数 Reader/Writer 使用相同的错误处理策略。如果每个实体都需要独立设置所有 13 个回调：

```c
// 无继承: 100 个 Reader 各设置 13 个回调 → 1300 次设置
for (int i = 0; i < 100; i++) {
  dds_set_listener(readers[i], listener);  // 必须对每个实体重复
}

// 有继承: 在 Participant 设置一次 → 所有子实体自动继承
dds_set_listener(participant, listener);   // 一次设置
// 所有 Reader/Writer 自动获得相同的错误处理回调
```

`inherited` 位掩码精确跟踪每个回调的来源。当子实体覆盖某个回调时，只覆盖那一个，其余仍继承父实体。

**底层思想**：这是 DDS 规范 §2.2.4 中定义的 Listener 继承语义的忠实实现。继承模型遵循**原型链（Prototype Chain）**的思想——与 JavaScript 的原型继承相似，查找沿着实体层次向上走，在第一个定义点停止。

### 4.2 为什么实体有两把锁（m_mutex 和 m_observers_lock）？

**设计哲学：切分锁以打破循环依赖**

如果只有一把锁：

```
场景: Listener 回调中调用 dds_read()

Thread A:
  lock(entity->m_mutex)              // 状态变更
  → call listener.on_data_available() // 回调
    → dds_read(entity)               // 用户代码
      → lock(entity->m_mutex)        // 死锁!
```

两把锁的分工：
- `m_mutex`：保护实体状态（QoS、内部数据、生命周期状态）
- `m_observers_lock`：保护观察者列表（Listener 设置、WaitSet 注册）

```
分离后:
  Thread A:
    lock(m_observers_lock)            // 只锁观察者
    → call listener.on_data_available()
      → dds_read(entity)
        → lock(m_mutex)              // 锁实体状态 → 不冲突 ✓
```

**底层思想**：**Lock Splitting（锁分割）**是并发编程中打破循环等待的标准技术。与 Java ConcurrentHashMap 的分段锁（Segment Lock）同源——将一把大锁拆成多把不重叠的小锁，消除竞争路径。关键在于两把锁保护的数据集合**完全不相交**。

### 4.3 为什么有 reset_on_invoke 机制？

**设计哲学：读取与清除的原子性**

考虑没有自动重置的场景：

```
Thread A (Listener 回调):                 Thread B (DDSI):
  callback(status)                          
  // 用户处理 status                      status_set(NEW_EVENT)  // 新事件到达
  dds_reset_status(entity, MATCHED)       // 用户清除状态
  // → 错误! Thread B 的新事件也被清除了!
```

`reset_on_invoke` 在回调调用的**同一个原子操作**中清除状态：

```
dds_entity_observers_signal():
  status = atomic_read(m_status);           // 读取当前状态
  call listener(entity, status);            // 传递给用户
  atomic_and(&m_status, ~reset_on_invoke);  // 原子清除已通知的状态位
  // 在 call 和 clear 之间新到达的事件会被保留
```

**底层思想**：这是**Compare-And-Swap（CAS）语义在应用层的投影**。"读取事件并原子标记为已处理"等价于硬件级的 test-and-clear 指令。确保事件**恰好被处理一次**——不丢失、不重复。

### 4.4 为什么 WaitSet 使用条件变量而非信号量或事件对象？

**设计哲学：电平触发优于边沿触发**

三种等待机制的比较：

| 机制 | 触发模式 | 多实体等待 | 虚假唤醒安全 |
|------|---------|-----------|------------|
| 信号量 | 边沿触发（计数） | 需要多个信号量 | 不安全（计数可能不准确） |
| 事件对象 | 边沿触发 | WaitForMultipleObjects (Windows) | 平台相关 |
| 条件变量 + 谓词 | 电平触发 | 唤醒后重新检查所有实体 | 安全（重新检查谓词） |

WaitSet 需要的语义是**"等待直到任意一个被观察实体有未处理事件"**。这是天然的电平触发语义：

```c
// 条件变量的经典模式:
while (!(triggered = check_all_entities(ws))) {
  cond_wait(&ws->wait_cond, &ws->wait_lock);  // 虚假唤醒 → 重新检查
}
```

信号量的问题：如果两个事件在同一个唤醒窗口内到达，信号量计数为 2，但 WaitSet 只需要返回一次（包含两个触发实体）。计数语义与 WaitSet 的"快照"语义不匹配。

**底层思想**：**Monitor 模式（Hoare/Mesa Monitor）**——条件变量 + 互斥锁 + 谓词循环是等待复合条件的经典解法。Mesa 语义（while 循环检查，而非 if）天然容忍虚假唤醒，是最健壮的等待范式。

### 4.5 为什么 Listener 回调优先于 WaitSet 通知？

**设计哲学：推送优先于轮询**

当同一实体同时设置了 Listener 和 WaitSet 时，DDS 规范定义了明确的优先级：

```
事件到达:
  if (entity 有 Listener 回调) → 调用 Listener, WaitSet 不触发
  else → 触发 WaitSet
```

为什么 Listener 优先？

1. **延迟**：Listener 在事件产生的线程中直接调用 → 零额外延迟。WaitSet 需要唤醒等待线程 → 至少一次上下文切换。

2. **确定性**：Listener 回调的执行时机是确定的（事件产生时立即执行）。WaitSet 的处理时机取决于等待线程何时被调度。

3. **避免双重处理**：如果同时触发两者，用户可能处理同一事件两次。

```
Listener = 推送（Push）模型:
  事件 → 直接调用用户代码 → 处理完成
  延迟: ~0 (内联调用)

WaitSet = 拉取（Pull）模型:
  事件 → 设置状态位 → 唤醒等待线程 → 线程醒来 → 检查状态 → 处理
  延迟: 上下文切换时间 (~1-10μs)
```

**底层思想**：**Push vs Pull 是分布式系统通知的基本二分法**。Push（回调/Listener）适合低延迟场景，Pull（WaitSet/epoll）适合高吞吐场景。DDS 同时提供两种模型，并通过优先级规则避免冲突。这与 Linux 中的 signal handler（push）vs epoll_wait（pull）的关系完全对应。

### 4.6 为什么 WaitSet 维护触发和未触发实体的分区？

**设计哲学：避免重复检查 + 减少唤醒开销**

WaitSet 的 `entities` 数组结构：

```
entities[0 .. ntriggered-1]    → 已触发实体
entities[ntriggered .. nentities-1] → 未触发实体
```

这种设计带来三个优势：

1. **快速返回**：`dds_waitset_wait()` 直接返回 `entities[0..ntriggered-1]`，无需遍历
2. **减少检查**：下次等待时先检查已触发实体是否仍触发，避免全量扫描
3. **原地交换**：触发/未触发状态变化只需交换两个元素，O(1) 操作

```c
// 实体从未触发变为触发:
dds_attachment tmp = ws->entities[i];           // i 在未触发区域
ws->entities[i] = ws->entities[ws->ntriggered]; // 未触发区最后元素移到位置 i
ws->entities[ws->ntriggered++] = tmp;           // 原 i 移到触发区末尾
```

**底层思想**：**数组的分区不变量（Partition Invariant）**——类似于快速排序的分区策略。维护"左侧满足条件 P，右侧不满足"的不变量，使得查询和更新都可以高效进行。

## 5. 与规范的关系

- **DDS v1.4 §2.2.4**：Listener 接口定义和继承规则
- **DDS v1.4 §2.2.4.3**：StatusCondition 和 WaitSet 语义
- **DDS v1.4 §2.2.1.2**：Listener 回调优先级（Listener 优先于 WaitSet）
- **DDS v1.4 Table 2-16**：13 种 Listener 回调与对应的 Communication Status

## 6. 总结

Listener 与 WaitSet 的设计哲学可概括为**继承简化 + 锁分割 + 原子通知 + 双模型共存**：
1. Listener 继承模型遵循"约定优于配置"，一次设置覆盖整个实体层次
2. 双锁设计（m_mutex + m_observers_lock）打破回调中的循环依赖
3. reset_on_invoke 实现事件读取与清除的原子语义
4. WaitSet 使用条件变量实现电平触发的多实体等待
5. Listener 优先于 WaitSet，Push 模型服务于低延迟场景
6. WaitSet 的分区数组结构优化触发检查和状态更新性能
