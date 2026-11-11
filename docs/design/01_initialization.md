# 模块 1：初始化（Initialization）设计文档

## 1. 概述

初始化模块负责将 CycloneDDS 从零状态引导为可运行的 DDS 中间件实例。它涵盖三个层次：全局库初始化、Domain 创建、DDSI 运行时启动。

**关键文件**：
- `src/core/ddsc/src/dds_init.c` — 全局库初始化
- `src/core/ddsc/src/dds_domain.c` — Domain 创建与生命周期
- `src/core/ddsi/src/ddsi_init.c` — DDSI-RTPS 运行时引导

## 2. 核心数据结构

### 2.1 全局状态原子变量

```c
static ddsrt_atomic_uint32_t dds_state = DDSRT_ATOMIC_UINT32_INIT(CDDS_STATE_ZERO);

enum {
  CDDS_STATE_ZERO     = 0,  // 未初始化
  CDDS_STATE_STARTING = 1,  // 初始化进行中
  CDDS_STATE_READY    = 2,  // 就绪
  CDDS_STATE_STOPPING = 3   // 正在关闭
};
```

### 2.2 全局实体根

```c
struct dds_global {
  dds_entity m_entity;              // 根实体（所有 Domain 的父节点）
  ddsrt_avl_tree_t m_domains;       // Domain AVL 树，按 domain_id 索引
  ddsrt_mutex_t m_mutex;            // 保护 Domain 树的互斥锁
};
```

### 2.3 Domain 全局变量

```c
struct ddsi_domaingv {
  volatile int terminate, deaf, mute;
  struct ddsi_config config;
  struct ddsi_entity_index *entity_index;
  struct ddsi_xeventq *xevents;
  struct ddsi_gcreq_queue *gcreq_queue;
  // ... 400+ 成员
};
```

## 3. 机制设计

### 3.1 三层引导序列

```
dds_init()                          ← 第一层：进程级单例
  └─ dds_create_participant()       ← 触发第二层
     └─ dds_domain_create_internal()← 第二层：Domain 级
        └─ ddsi_init()              ← 第三层：DDSI-RTPS 运行时
```

**第一层 `dds_init()`**：极轻量。只初始化句柄服务器、线程状态表、全局实体根。不创建任何网络资源。

**第二层 `dds_domain_create_internal()`**：解析 XML 配置、校验参数、初始化 PSMX。按 domain_id 创建独立的运行时实例。

**第三层 `ddsi_init()`**：重量级。创建网络 socket、内置端点、投递队列、事件队列、GC 队列、接收线程。

### 3.2 原子状态机

状态转换图：

```
ZERO ──CAS──→ STARTING ──完成──→ READY ──关闭──→ STOPPING ──完成──→ ZERO
  ↑                                                              │
  └──────────────────────────────────────────────────────────────┘
```

### 3.3 惰性 Domain 创建

Domain 不是在 `dds_init()` 时创建，而是在第一个 Participant 请求该 domain_id 时才创建。多个 Domain 存储在 AVL 树中，按 domain_id 索引。

### 3.4 DDSI 运行时启动子序列

```
ddsi_init(gv)
  ├─ 传输工厂注册 (UDP/TCP/RawEth)
  ├─ ddsi_gather_network_interfaces()     [枚举网卡]
  ├─ make_uc_sockets()                    [创建单播 socket]
  ├─ 多播 socket 创建 (如果启用)
  ├─ ddsi_entity_index_new()              [GUID 哈希表]
  ├─ ddsi_xeventq_new()                   [定时事件队列]
  ├─ ddsi_gcreq_queue_new()               [GC 请求队列]
  ├─ ddsi_tkmap_new()                     [Topic Key 映射]
  ├─ 内置端点 QoS 初始化
  ├─ SPDP/SEDP/PMD 内置端点创建
  ├─ 接收线程启动 (1~3 个)
  ├─ GC 线程启动
  ├─ 事件线程启动
  └─ ddsi_start()                         [激活]
```

## 4. 设计逻辑与设计思想

### 4.1 为什么用原子状态机而不是简单互斥锁？

**设计哲学：无阻塞状态观察 + 竞态安全**

互斥锁方案的问题：
- 多线程同时调用 `dds_init()` 时，一个线程获得锁执行初始化，其他线程阻塞等待——但它们并不需要"等待"，它们需要的是"知道初始化是否已完成"
- 关闭路径 `dds_fini()` 与初始化路径 `dds_init()` 交错时，互斥锁无法表达"正在关闭中"的语义

原子状态机的优势：
- **O(1) 状态检测**：任何线程通过 `ddsrt_atomic_ld32(&dds_state)` 立即获知库状态，无需获取锁
- **语义丰富**：4 个状态精确描述库的完整生命周期阶段
- **CAS 竞争解决**：多线程同时尝试 ZERO→STARTING 转换时，只有一个成功（CAS 胜出），其他线程看到 STARTING 后等待条件变量广播

**底层思想**：这是**观察者模式在同步层面的应用**——大多数线程只需要观察状态，只有少数线程需要修改状态。原子变量是观察操作的最优解。

### 4.2 为什么 Domain 惰性创建？

**设计哲学：按需分配、零开销抽象**

DDS 规范允许一个进程参与多个 Domain（不同的通信域）。如果在 `dds_init()` 时预创建所有可能的 Domain：
- 浪费内存和线程资源（每个 Domain 至少 3 个线程 + 多个 socket）
- 无法预知用户会使用哪些 domain_id
- 配置尚不可用（XML 配置在 Domain 创建时才解析）

惰性创建的好处：
- **最小权限原则**：只为实际使用的 Domain 分配资源
- **配置隔离**：每个 Domain 可以有完全不同的网络配置、传输类型、线程数
- **引导简化**：`dds_init()` 变成毫秒级的轻量操作

**底层思想**：这是 **YAGNI（You Aren't Gonna Need It）原则在资源分配层面的体现**。不要为假设的需求预分配资源，让实际的第一次使用触发初始化。

### 4.3 为什么分离三层引导？

**设计哲学：关注点分离 + 独立生命周期**

三层各自管理不同的生命周期和资源范围：

| 层次 | 生命周期 | 资源范围 | 何时销毁 |
|---|---|---|---|
| 全局 (`dds_init`) | 进程级 | 句柄服务器、线程状态 | 最后一个 Domain 销毁后 |
| Domain | 域级 | 网络 socket、DDSI 运行时、线程池 | 最后一个 Participant 销毁后 |
| Participant | 应用级 | 内置端点、GUID、发现 | 用户显式删除 |

如果合并这三层，会产生以下问题：
- 第二个 Domain 创建时需要"部分重新初始化"——逻辑复杂
- Domain 销毁时无法判断是否应该同时销毁全局资源
- 测试困难：无法在一个进程内创建/销毁多个独立 DDS 实例

**底层思想**：这是**分层架构（Layered Architecture）**的经典应用。每一层为上一层提供服务，向下一层请求资源。层间通过清晰的接口（`ddsi_init()` / `ddsi_fini()`）交互。

### 4.4 为什么 ddsi_init 是同步阻塞的？

DDSI 运行时的初始化（创建 socket、绑定端口、启动线程）是同步阻塞的，不返回 future 或 promise。

**原因**：网络资源创建是可能失败的操作（端口占用、网卡不存在、多播不支持），必须在返回给用户之前确认所有资源就绪。如果异步化，用户在调用 `dds_write()` 时可能发现底层 socket 尚未就绪——这违反了**最小惊奇原则**。

**底层思想**：**Fail-fast 原则**。初始化阶段是唯一可以安全失败的窗口。一旦 `dds_create_participant()` 返回成功，用户可以无条件信任所有基础设施已就绪。

### 4.5 线程创建策略

接收线程数量可配置（1~3），但默认 1 个。

**设计权衡**：
- 1 线程：最简架构，无需考虑 RTPS 消息的跨线程排序
- 2 线程：分离 Discovery 和 Data 流量，避免 SPDP/SEDP 突发影响数据路径延迟
- 3 线程：混合传输场景（UDP + TCP 分离）

**底层思想**：**Amdahl 定律的务实应用**。RTPS 协议天然要求严格的序列号排序，并行化收益有限。与其增加线程数引入同步开销，不如让单线程全速运行。

## 5. 与规范的关系

- **DDS v1.4 §2.2.2**：DomainParticipant 是 DDS 实体层次的入口，对应 CycloneDDS 的惰性 Domain 创建
- **RTPS v2.5 §8.5.1**：PDP（Participant Discovery Protocol）在 `ddsi_init()` 中通过内置端点实现
- **RTPS v2.5 §9**：UDP/IP PSM 在 `ddsi_udp.c` 中实现，socket 创建发生在 `ddsi_init()` 阶段

## 6. 总结

初始化模块的设计哲学可概括为**渐进式资源获取**：
1. 先快速建立全局骨架（原子状态机 + 句柄服务器）
2. 按需创建 Domain（惰性初始化 + 配置隔离）
3. Domain 内同步完成所有网络资源获取（Fail-fast）
4. 线程数量最小化（Amdahl 定律务实应用）

这套设计在**启动速度**、**资源效率**、**错误处理清晰性**之间取得了平衡。
