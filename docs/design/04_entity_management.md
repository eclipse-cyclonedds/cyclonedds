# 模块 4：实体管理（Entity Management）设计文档

## 1. 概述

实体管理模块实现 DDS 规范定义的实体层次结构（Domain → Participant → Publisher/Subscriber → Writer/Reader → Topic），并负责实体的创建、引用计数、生命周期管理和安全删除。

**关键文件**：
- `src/core/ddsc/src/dds_entity.c` — 实体生命周期（1840 行）
- `src/core/ddsc/src/dds__types.h` — 实体结构定义
- `src/core/ddsc/src/dds__entity.h` — 内部实体 API
- `src/core/ddsc/src/dds_handles.c` — 句柄分配与引用计数

## 2. 核心数据结构

### 2.1 实体基类

```c
struct dds_entity {
  // 身份与层次
  struct dds_handle_link m_hdllink;    // 句柄 + 引用计数 + 标志
  dds_entity_kind_t m_kind;            // PARTICIPANT/WRITER/READER/TOPIC/...
  struct dds_entity *m_parent;         // 父实体指针
  ddsrt_avl_tree_t m_children;         // 子实体 AVL 树
  struct dds_domain *m_domain;         // 所属 Domain
  dds_qos_t *m_qos;                   // QoS 策略集
  ddsi_guid_t m_guid;                 // RTPS GUID
  dds_instance_handle_t m_iid;        // 实例 ID（AVL 树的键）

  // 同步机制（双锁设计）
  ddsrt_mutex_t m_mutex;              // 保护实体状态 + 子节点
  ddsrt_cond_t m_cond;                // 状态变化条件变量
  ddsrt_mutex_t m_observers_lock;     // 保护 Listener + WaitSet 观察者
  ddsrt_cond_t m_observers_cond;      // 观察者变化条件变量

  // 状态与回调
  status_and_enabled_t m_status;       // 原子状态位 + 启用掩码
  uint32_t m_cb_pending_count;         // 正在执行的回调计数
  dds_listener_t m_listener;           // 监听器回调集
  dds_entity_observer *m_observers;    // 观察者链表（WaitSet 等）

  // 标志
  uint32_t m_flags;                    // DDS_ENTITY_ENABLED | DDS_ENTITY_IMPLICIT | ...
};
```

### 2.2 虚函数表（Entity Deriver）

```c
typedef struct dds_entity_deriver {
  void (*interrupt)(struct dds_entity *e);              // 中断阻塞操作
  void (*close)(struct dds_entity *e);                  // 关闭前清理
  dds_return_t (*delete)(struct dds_entity *e);         // 类型特定删除
  dds_return_t (*set_qos)(struct dds_entity *e, const dds_qos_t *qos, bool enabled);
  dds_return_t (*validate_status)(uint32_t mask);       // 校验状态掩码合法性
  void (*invoke_cbs_for_pending_events)(struct dds_entity *e, uint32_t status);
} dds_entity_deriver;
```

### 2.3 句柄系统

```c
struct dds_handle_link {
  int32_t hdl;                         // 32 位伪随机句柄（用户可见）
  ddsrt_atomic_voidp_t cnt_flags;      // 原子引用计数 + 标志位
};

// cnt_flags 位布局:
// [0:5]   pin count (6 bits, max 63)
// [6:11]  ref count (6 bits, max 63)
// [12+]   flags: IMPLICIT, CLOSING, DELETE_DEFERRED, ALLOW_CHILDREN
```

### 2.4 实体层次

```
CycloneDDS (root entity, kind=DDS_KIND_CYCLONEDDS)
  └─ Domain (kind=DDS_KIND_DOMAIN)
     └─ Participant (kind=DDS_KIND_PARTICIPANT)
        ├─ Publisher (kind=DDS_KIND_PUBLISHER, 可隐式创建)
        │   └─ DataWriter (kind=DDS_KIND_WRITER)
        ├─ Subscriber (kind=DDS_KIND_SUBSCRIBER, 可隐式创建)
        │   └─ DataReader (kind=DDS_KIND_READER)
        │      ├─ ReadCondition
        │      └─ QueryCondition
        ├─ Topic (kind=DDS_KIND_TOPIC)
        └─ WaitSet (kind=DDS_KIND_WAITSET)
```

## 3. 机制设计

### 3.1 三阶段删除

```
阶段 1: Pin & Close
  dds_entity_pin_for_delete()
    ├─ 原子设置 HDL_FLAG_CLOSING
    ├─ 如果其他线程已在删除 → 返回 DELETE_DEFERRED
    └─ 获得 pin → 进入阶段 2

阶段 2: Interrupt & Wait
  really_delete_pinned_closed_locked()
    ├─ deriver->interrupt()          [中断阻塞的 dds_read 等]
    ├─ while (m_cb_pending_count > 0) wait  [等待回调完成]
    ├─ 清空 listener                  [阻止新回调触发]
    ├─ dds_handle_close_wait()       [等待所有 pin 释放]
    └─ deriver->close()              [类型特定关闭]

阶段 3: Delete Children & Free
    ├─ delete_children()             [递归删除子实体]
    │   └─ 按类型排序：先 Writer/Reader, 后 Topic
    ├─ deriver->delete()             [释放类型特定资源]
    └─ dds_entity_final_deinit()     [释放基类资源]
```

### 3.2 锁序协议

```
允许的锁获取顺序：
  parent.m_mutex → child.m_mutex          (层次递降)
  {pub,sub}.m_mutex → topic.m_mutex       (Topic 横向依赖)
  {rd,wr}.m_mutex → topic.m_mutex         (Topic 横向依赖)
  entity.m_mutex → entity.m_observers_lock (同实体内)
  任意 → waitset.wait_lock                (WaitSet 独立)

禁止:
  child.m_mutex → parent.m_mutex          (反向层次)
  m_observers_lock → m_mutex              (锁序反转)
```

### 3.3 隐式实体自动创建/销毁

```c
dds_create_writer(participant, topic, qos, listener)
  → 检查 participant 下是否有 Publisher
  → 没有 → 隐式创建 Publisher (flag: DDS_ENTITY_IMPLICIT)
  → 在 Publisher 下创建 Writer

// 删除 Writer 时:
dds_delete(writer)
  → Writer 删除
  → 检查 Publisher 的子节点数
  → 如果 Publisher 是 IMPLICIT 且无子节点 → 自动删除 Publisher
```

## 4. 设计逻辑与设计思想

### 4.1 为什么需要双锁设计（m_mutex + m_observers_lock）？

**设计哲学：消除回调重入死锁**

考虑这个场景：
```
Thread A:
  lock(reader.m_mutex)
  → 检测到数据到达
  → 触发 on_data_available 回调
  → 用户在回调中调用 dds_get_qos(reader)
  → dds_get_qos 尝试 lock(reader.m_mutex) → 死锁！
```

双锁解决方案：
- `m_mutex` 保护**结构性状态**（子节点、QoS、GUID 等）
- `m_observers_lock` 保护**通知机制**（Listener 回调、WaitSet 观察者）

回调触发的代码路径只持有 `m_observers_lock`，用户在回调中调用的 API（如 `dds_get_qos`）获取 `m_mutex`——两个锁不冲突。

**底层思想**：**锁应该保护数据，而不是操作**。`m_mutex` 保护"实体是什么"（结构），`m_observers_lock` 保护"谁在关注实体的变化"（通知）。这两个关注点天然独立，用两把锁表达这种独立性。

### 4.2 为什么用 AVL 树存储子实体？

**设计哲学：迭代安全性 + O(log n) 查找**

替代方案的问题：

**链表**：O(n) 查找；迭代时删除节点需要特殊处理（前驱指针失效）

**哈希表**：无序，无法按创建顺序迭代；迭代时删除可能导致 rehash

**AVL 树**（按 `m_iid` 排序）：
- **O(log n) 按 ID 查找**：需要查找特定子实体时高效
- **安全迭代删除**：AVL 的游标（cursor）通过 `m_iid` 定位而不是指针。删除当前节点后，`find_succ(deleted_iid)` 可以安全找到下一个节点
- **有序性**：按 `m_iid`（单调递增）遍历等价于按创建顺序遍历

**底层思想**：**数据结构的选择应该服务于最危险的操作**。子实体删除时的安全迭代是最容易出 bug 的操作（`delete_children()` 在递归删除时修改树结构）。AVL 树的游标稳定性为这个危险操作提供了结构性保障。

### 4.3 为什么用虚函数表（Deriver Pattern）？

**设计哲学：C 语言中的多态——开放/封闭原则**

DDS 规范定义了多种实体类型，每种的行为不同：
- Writer 删除时需要发送 dispose 通知
- Reader 删除时需要清理 RHC
- Topic 删除时需要检查是否有 Writer/Reader 还在引用
- Participant 删除时需要发送 SPDP bye-bye

如果用 `switch (entity->m_kind)` 来处理：
```c
void delete_entity(dds_entity *e) {
  switch (e->m_kind) {
    case WRITER: ... break;
    case READER: ... break;
    case TOPIC: ... break;
    // 每加一种实体类型，都要修改这个 switch
  }
}
```

这违反了**开放/封闭原则**（对扩展开放，对修改封闭）。新增实体类型需要修改核心删除逻辑。

虚函数表方案：
```c
e->m_deriver->delete(e);  // 自动分派到类型特定实现
```

新增实体类型只需定义新的 `dds_entity_deriver` 结构体，无需修改 `dds_entity.c`。

**底层思想**：CycloneDDS 在 C 语言中实现了**对象继承和多态**。`dds_entity` 是基类，各实体类型结构体（如 `dds_writer`）将 `dds_entity` 作为第一个成员实现"继承"，`dds_entity_deriver` 是虚函数表。这是 C 语言面向对象编程的经典范式。

### 4.4 为什么三阶段删除？

**设计哲学：渐进式安全拆除**

一阶段删除（立即释放）的问题：
- Thread A 正在 `dds_read()` 阻塞等待数据
- Thread B 调用 `dds_delete(reader)` 释放 Reader
- Thread A 被唤醒后访问已释放的 Reader → Use-After-Free

三阶段的每一阶段解决一个具体的安全问题：

**阶段 1（Pin & Close）**：**声明删除意图**
- 原子设置 CLOSING 标志 → 新的 API 调用立即返回错误
- Pin 计数 → 确保只有一个线程执行删除
- 解决问题：**并发删除竞争**

**阶段 2（Interrupt & Wait）**：**等待所有使用者退出**
- `interrupt()` → 唤醒所有阻塞在实体上的线程（如 `dds_read` 阻塞）
- `wait(cb_pending_count == 0)` → 等待所有正在执行的 Listener 回调完成
- `handle_close_wait()` → 等待所有持有 pin 的线程释放
- 解决问题：**Use-After-Free**

**阶段 3（Delete Children & Free）**：**安全释放资源**
- 先删除 Writer/Reader，后删除 Topic（因为 Writer/Reader 引用 Topic）
- 类型特定的资源释放（网络连接、缓冲区等）
- 解决问题：**资源泄漏 + 依赖顺序**

**底层思想**：这是**两阶段关闭协议（Two-Phase Shutdown）的扩展版**。在分布式系统中，安全关闭需要先声明意图（prepare），等待所有参与方确认（commit），然后执行（execute）。三阶段删除将 prepare 细分为"标记"和"等待"，因为在并发环境中，标记后仍需等待"正在飞行中"的操作完成。

### 4.5 为什么使用伪随机句柄？

**设计哲学：安全性 + 鲁棒性**

句柄（`dds_entity_t`）是用户与 CycloneDDS 交互的唯一标识符。设计选择：

**顺序句柄的问题**：
- **信息泄露**：句柄值 42 暗示系统创建了 42 个实体——攻击者可推断系统规模
- **重用风险**：实体删除后，句柄 42 可能被分配给新实体。持有旧句柄的代码可能误操作新实体
- **可预测性**：攻击者可以猜测有效句柄并尝试操作

**伪随机句柄的优势**：
- **不可预测**：无法从一个句柄推断其他有效句柄
- **误用检测**：旧句柄被重用为新实体的概率极低（32 位随机空间 → ~1/2^31 碰撞概率）
- **无信息泄露**：句柄值不反映系统内部状态

**底层思想**：**句柄是 capability token，不是数组索引**。这与操作系统的文件描述符设计不同（fd 是顺序的）。CycloneDDS 选择牺牲"句柄的人类可读性"换取"安全性和鲁棒性"。

### 4.6 为什么 cnt_flags 用原子 CAS 而不是锁？

**设计哲学：pin/unpin 是热路径，不能有锁竞争**

每次 API 调用（`dds_write`, `dds_read`, `dds_get_qos`, ...）都需要先 pin 实体（增加 pin count），完成后 unpin。这是**极热路径**——每秒可能发生百万次。

使用互斥锁保护 pin count 意味着每次 API 调用都有锁竞争。在多线程写入同一个 DataWriter 的场景中，锁竞争会成为瓶颈。

CAS（Compare-And-Swap）方案：
```c
do {
  old = atomic_load(&cnt_flags);
  new = old + 1;  // increment pin count
} while (!atomic_cas(&cnt_flags, old, new));
```

CAS 是乐观的：假设没有竞争，如果有竞争就重试。在低竞争场景下（99%+ 的情况），CAS 第一次就成功，开销仅为一条原子指令。

**底层思想**：**锁是悲观的，CAS 是乐观的。热路径应该用乐观策略。**

### 4.7 为什么支持隐式 Publisher/Subscriber？

**设计哲学：API 易用性 > 规范纯洁性**

DDS 规范定义了严格的层次：Writer 必须属于 Publisher，Reader 必须属于 Subscriber。但在大多数实际应用中，用户只需要一个默认的 Publisher/Subscriber，不需要分组管理。

```c
// 规范要求的冗长代码：
pub = dds_create_publisher(participant, NULL, NULL);
wr = dds_create_writer(pub, topic, NULL, NULL);

// CycloneDDS 允许的简洁代码：
wr = dds_create_writer(participant, topic, NULL, NULL);
// 自动隐式创建 Publisher
```

隐式实体有 `DDS_ENTITY_IMPLICIT` 标志，当最后一个子实体删除时自动销毁。

**底层思想**：**API 应该让简单的事情简单，让复杂的事情可能**。不强制用户理解 Publisher/Subscriber 的概念就能完成基本的 pub/sub。需要分组管理时，仍然可以显式创建。

## 5. 与规范的关系

- **DDS v1.4 §2.2.2.1**：Entity 基类层次（DomainEntity → Entity）对应 `dds_entity` 继承链
- **DDS v1.4 §2.2.2.2.1**：DomainParticipant 作为 Entity Factory 对应 Participant 的子实体创建
- **DDS v1.4 §2.2.2.4**：Publisher/Subscriber 的可选性对应隐式创建机制
- **DDS v1.4 §2.2.1.1**：Entity 的 QoS、Listener、StatusCondition 对应 `dds_entity` 的字段

## 6. 总结

实体管理的设计哲学可概括为**C 语言面向对象 + 并发安全**：
1. 双锁设计消除回调重入死锁
2. AVL 树为迭代删除提供结构性安全保障
3. 虚函数表实现开放/封闭原则
4. 三阶段删除确保并发环境下的安全拆除
5. CAS 原子引用计数避免热路径锁竞争
6. 伪随机句柄提供安全性和鲁棒性
7. 隐式实体简化 API 使用
