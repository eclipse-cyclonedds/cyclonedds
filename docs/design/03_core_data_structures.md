# 模块 3：核心数据结构（Central State）设计文档

## 1. 概述

核心数据结构模块以 `struct ddsi_domaingv` 为中心，承载了一个 DDS Domain 运行时所需的全部状态。它是 CycloneDDS 的"神经中枢"——所有其他模块通过引用 `gv` 指针来访问共享资源。

**关键文件**：
- `src/core/ddsi/include/dds/ddsi/ddsi_domaingv.h` — `ddsi_domaingv` 定义（350+ 行）
- `src/core/ddsi/include/dds/ddsi/ddsi_entity_index.h` — 实体索引接口
- `src/core/ddsi/src/ddsi_entity_index.c` — GUID 哈希表实现

## 2. 核心数据结构

### 2.1 `struct ddsi_domaingv` 分区架构

该结构按功能域划分为 8 个逻辑区域：

```
ddsi_domaingv
├── 控制区    terminate, deaf, mute (volatile 标志)
├── 配置区    config, logconfig
├── 索引区    entity_index, m_tkmap
├── 事件区    xevents, gcreq_queue, leaseheap
├── 网络区    m_factory, disc_conn_mc/uc, data_conn_mc/uc, xmit_conns[4]
├── 地址区    loc_spdp_mc, loc_meta_mc/uc, loc_default_mc/uc
├── 线程区    recv_threads[3], n_recv_threads, listen_ts
├── 投递区    builtins_dqueue, user_dqueue, xmsgpool
└── 类型区    spdp_type, sedp_reader_type, sedp_writer_type, pmd_type
```

### 2.2 实体索引 (`ddsi_entity_index`)

```c
struct ddsi_entity_index {
  ddsrt_ehh_t *guid_hash;                    // GUID → 实体 的并发哈希表
  ddsrt_avl_tree_t participant_index;         // 本地 Participant 有序索引
  ddsrt_avl_tree_t proxy_participant_index;   // 远端 Proxy Participant 有序索引
};
```

### 2.3 Topic Key Map (`ddsi_tkmap`)

```c
struct ddsi_tkmap {
  struct ddsrt_chh *m_hh;    // 并发哈希表：(topic, key) → instance_id
  ddsrt_atomic_uint32_t m_count;
};
```

### 2.4 控制标志

```c
volatile int terminate;  // 非零时所有线程应退出
volatile int deaf;       // 非零时忽略所有收到的消息
volatile int mute;       // 非零时停止所有发送
```

## 3. 机制设计

### 3.1 `gv` 指针传递模式

几乎所有 DDSI 层函数都接收 `struct ddsi_domaingv *gv` 作为第一个参数：

```c
void ddsi_handle_rtps_message(struct ddsi_domaingv *gv, ...);
void ddsi_spdp_write(struct ddsi_domaingv *gv, ...);
int ddsi_whc_insert(struct ddsi_domaingv *gv, ...);
```

### 3.2 并发哈希表用于实体查找

`entity_index` 使用 `ddsrt_ehh_t`（Extensible Hopscotch Hash table）实现 GUID → 实体映射。这是一种无锁并发友好的哈希表实现。

### 3.3 双投递队列

```
接收线程 → ddsi_handle_rtps_message()
  ├─ 内置端点数据 → builtins_dqueue → 高优先级处理
  └─ 用户数据     → user_dqueue     → 正常优先级处理
```

### 3.4 Fibonacci 堆管理租约

```c
ddsrt_fibheap_t leaseheap;  // 按过期时间排序的租约堆
```

最早过期的租约在堆顶，事件线程只需检查堆顶是否过期，复杂度 O(1)。

## 4. 设计逻辑与设计思想

### 4.1 为什么用一个"上帝结构"而不是分散的全局变量？

**设计哲学：作用域封装 + 多实例支持**

将所有 Domain 状态聚合到一个结构体中，而不是分散为独立的全局变量，有三个关键原因：

**多 Domain 支持**：DDS 规范允许一个进程参与多个 Domain。如果使用全局变量，每个变量都需要用 domain_id 索引的数组来存储——这实质上就是把全局变量手动组装成了结构体。不如一开始就用结构体，每个 Domain 持有独立的 `ddsi_domaingv` 实例。

**依赖注入**：所有函数通过 `gv` 指针访问共享状态，而不是直接引用全局变量。这使得：
- 单元测试可以构造 mock 的 `gv` 对象
- 函数的依赖关系显式可见（参数列表中有 `gv` 就知道它需要 Domain 状态）
- 不存在隐式的全局状态耦合

**生命周期管理**：结构体有明确的创建（`ddsi_init`）和销毁（`ddsi_fini`）时机。全局变量的生命周期与进程绑定，无法精确控制。

**底层思想**：这是**上下文对象模式（Context Object Pattern）**的经典应用。在 C 语言中没有类和 `this` 指针，`gv` 指针就是手动实现的 `this`。面向对象语言中的类成员变量，在 C 中就是"上帝结构"的字段。

### 4.2 为什么用并发哈希表（Hopscotch Hash）而不是加锁的普通哈希表？

**设计哲学：读多写少场景的无锁优化**

GUID 查找的访问模式：
- **读**：极其频繁。每收到一个 RTPS 消息，都需要通过 GUID 查找目标 Writer/Reader。每秒可能发生数十万次。
- **写**：相对稀少。只在 Discovery（SPDP/SEDP）期间增删实体。

Hopscotch Hash 的特点：
- 读操作接近无锁（通过 cache-line 级别的 neighborhood 搜索）
- 写操作有短暂的局部锁定
- 比链表哈希表更缓存友好（数据局部性好）

**底层思想**：**数据结构的选择应该匹配访问模式**。如果读/写比为 1000:1，投资在读优化上的收益远大于写优化。Hopscotch Hash 正是为这种"读密集、写稀疏"的场景设计的。

### 4.3 为什么分离 builtins_dqueue 和 user_dqueue？

**设计哲学：控制面与数据面隔离**

这是网络系统设计中经典的**控制面/数据面分离**原则：

**控制面（builtins_dqueue）**：
- 承载 SPDP、SEDP、PMD 等发现协议消息
- 处理优先级高（发现延迟直接影响系统可用性）
- 数据量小但时效性要求高

**数据面（user_dqueue）**：
- 承载用户应用数据
- 可以容忍一定的排队延迟
- 数据量可能很大（高吞吐场景）

如果共享一个队列：
- 用户数据突发会阻塞发现消息的处理
- 一个高吞吐的 DataWriter 可能导致新 Participant 的发现延迟数秒
- 这在安全关键系统中不可接受

**底层思想**：**关键路径不共享资源**。控制面是系统的"神经系统"，数据面是"血液循环系统"。神经信号（发现消息）绝不应该被大量血液（用户数据）堵塞。

### 4.4 为什么用 volatile 标志而不是原子变量来控制 terminate/deaf/mute？

**设计哲学：简单性 + 最终一致性**

这三个控制标志（`terminate`, `deaf`, `mute`）的语义是：
- 设置后，所有线程"最终"会看到变化并响应
- 不需要精确的"看到变化的瞬间"语义
- 不需要 happens-before 关系保证

`volatile` 在这里的含义是"不要优化掉对这个变量的读取"——编译器会每次从内存读取，而不是缓存到寄存器中。对于"设置一个标志让所有线程退出"这种场景，`volatile` 已经足够。

**为什么不用原子变量？** 原子变量（`ddsrt_atomic_uint32_t`）提供内存屏障保证，但 terminate/deaf/mute 不需要这种保证。使用原子变量是过度设计——增加了 CPU 开销（内存屏障指令）却没有获得实际收益。

**底层思想**：**不要为不需要的保证买单**。`volatile` 对这个用例来说是正确且最小化的工具。这体现了 C 语言系统编程的哲学：精确选择同步原语的强度。

### 4.5 为什么用 Fibonacci 堆管理租约？

**设计哲学：最优的最小值操作复杂度**

租约管理的访问模式：
- **peek-min**（查看最早到期的租约）：极其频繁，事件线程每次循环都要检查
- **decrease-key**（收到 SPDP 后续约，延长过期时间）：频繁，每次 SPDP 消息都触发
- **insert**（新 Participant 注册租约）：稀少
- **delete-min**（租约过期，删除 Participant）：稀少

Fibonacci 堆的复杂度：
| 操作 | Fibonacci 堆 | 二叉堆 |
|---|---|---|
| peek-min | O(1) | O(1) |
| insert | O(1) 摊销 | O(log n) |
| decrease-key | O(1) 摊销 | O(log n) |
| delete-min | O(log n) 摊销 | O(log n) |

`decrease-key` 的 O(1) 是关键优势——每收到一个 SPDP 消息就要续约，在大规模系统中（1000+ Participant），这个操作的频率远高于其他操作。

**底层思想**：**数据结构的选择由"热路径"的操作频率决定**。Fibonacci 堆比二叉堆实现复杂得多，但它的 `decrease-key` 是 O(1)——在租约场景中，这正是热路径上的操作。

### 4.6 `ddsi_domaingv` 的"不变式"

`ddsi_domaingv` 在初始化完成后（`ddsi_init` 返回后）有一组不变式：

1. `entity_index` 非空且已初始化
2. 至少一个有效的 `xmit_conns[]` 连接
3. `recv_threads[]` 中至少一个线程在运行
4. `spdp_type`, `sedp_reader_type`, `sedp_writer_type` 非空
5. `terminate == 0`

这些不变式持续成立直到 `ddsi_stop()` 被调用。任何访问 `gv` 的代码都可以安全地假设这些不变式成立，无需额外检查。

**底层思想**：**不变式是并发正确性的基础**。通过在初始化阶段建立不变式，运行时代码可以省略大量防御性检查。

## 5. 与规范的关系

- **RTPS v2.5 §8.2.4**：GUID 结构（prefix + entityId）对应 `entity_index` 的键
- **RTPS v2.5 §8.5**：PDP/SEDP 的内置 sertype（`spdp_type`, `sedp_*_type`）按规范定义
- **DDS v1.4 §2.2.5**：内置主题（Built-in Topics）通过 `builtins_dqueue` 投递

## 6. 总结

核心数据结构的设计哲学可概括为**上下文封装 + 访问模式驱动的数据结构选择**：
1. `ddsi_domaingv` 作为上下文对象，替代全局变量，支持多 Domain 和依赖注入
2. 并发哈希表优化读密集的 GUID 查找
3. 控制面/数据面分离确保发现消息不被用户数据阻塞
4. Fibonacci 堆优化租约续约的热路径
5. volatile 标志用于不需要内存屏障的简单控制
