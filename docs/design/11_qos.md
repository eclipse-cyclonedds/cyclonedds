# 模块 11：QoS 系统设计文档

## 1. 概述

QoS（Quality of Service）系统管理 DDS 实体的 22 个服务质量策略。它负责 QoS 的存储、校验、默认值补充、匹配兼容性判断，以及 QoS 变更时的通知传播。

**关键文件**：
- `src/core/ddsi/include/dds/ddsi/ddsi_xqos.h` — QoS 数据结构定义
- `src/core/ddsc/src/dds_qos.c` — DDS 层 QoS 管理 API
- `src/core/ddsi/src/ddsi_qosmatch.c` — QoS 匹配逻辑

## 2. 核心数据结构

### 2.1 QoS 主结构

```c
typedef struct dds_qos {
  uint64_t present;                              // 哪些 QoS 已设置（位掩码）
  uint64_t aliased;                              // 哪些字符串是别名（非独立分配）

  dds_reliability_qospolicy_t reliability;       // BEST_EFFORT / RELIABLE + max_blocking_time
  dds_durability_qospolicy_t durability;         // VOLATILE / TRANSIENT_LOCAL / TRANSIENT / PERSISTENT
  dds_deadline_qospolicy_t deadline;             // 数据更新截止时间
  dds_latency_budget_qospolicy_t latency_budget; // 延迟预算
  dds_liveliness_qospolicy_t liveliness;         // AUTOMATIC / MANUAL_BY_PARTICIPANT / MANUAL_BY_TOPIC
  dds_ownership_qospolicy_t ownership;           // SHARED / EXCLUSIVE
  dds_ownership_strength_qospolicy_t ownership_strength;  // EXCLUSIVE 模式下的优先级
  dds_history_qospolicy_t history;               // KEEP_LAST(depth) / KEEP_ALL
  dds_resource_limits_qospolicy_t resource_limits;  // max_samples / max_instances / max_samples_per_instance
  dds_partition_qospolicy_t partition;            // 分区名列表（支持通配符）
  dds_presentation_qospolicy_t presentation;     // access_scope / coherent / ordered
  dds_destination_order_qospolicy_t destination_order;  // BY_RECEPTION / BY_SOURCE
  dds_durability_service_qospolicy_t durability_service; // 持久化服务配置
  dds_transport_priority_qospolicy_t transport_priority; // 传输优先级
  dds_lifespan_qospolicy_t lifespan;             // 样本生存时间
  dds_userdata_qospolicy_t user_data;            // 用户自定义元数据
  dds_topicdata_qospolicy_t topic_data;          // Topic 元数据
  dds_groupdata_qospolicy_t group_data;          // Publisher/Subscriber 元数据
  dds_type_consistency_enforcement_qospolicy_t type_consistency;  // 类型一致性策略
  dds_data_representation_qospolicy_t data_representation;  // CDR / XCDR / XCDR2
  dds_entity_factory_qospolicy_t entity_factory; // 自动使能
  // ... 更多
} dds_qos_t;
```

### 2.2 QoS 掩码定义

```c
// 每个 QoS 策略对应一个位
#define DDSI_QP_RELIABILITY          (1u << 0)
#define DDSI_QP_DURABILITY           (1u << 1)
#define DDSI_QP_DEADLINE             (1u << 2)
#define DDSI_QP_LATENCY_BUDGET       (1u << 3)
// ... 共 22+ 个位

// RxO (Requested-Offered) QoS：参与匹配的子集
#define DDSI_QP_RXO_MASK  (DDSI_QP_DURABILITY | DDSI_QP_RELIABILITY | ...)

// 可变更 QoS：创建后可修改的子集
#define DDSI_QP_CHANGEABLE_MASK  (DDSI_QP_DEADLINE | DDSI_QP_PARTITION | DDSI_QP_OWNERSHIP_STRENGTH | ...)
```

## 3. 机制设计

### 3.1 QoS 生命周期

```
创建阶段:
  dds_create_qos() → 分配空 dds_qos_t, present = 0
  dds_qset_reliability(qos, RELIABLE, ...) → present |= DDSI_QP_RELIABILITY
  dds_qset_durability(qos, TRANSIENT_LOCAL) → present |= DDSI_QP_DURABILITY
  // 用户只设置需要的 QoS，其余保持未设置

应用阶段:
  dds_create_writer(participant, topic, qos, listener)
    → ddsi_xqos_mergein_missing(user_qos, topic_qos)     // 用 Topic QoS 补充未设置的
    → ddsi_xqos_mergein_missing(merged_qos, default_qos) // 用全局默认补充剩余
    → 校验: ddsi_xqos_valid(merged_qos)                  // 合法性检查
    → 存储到 entity->m_qos

运行时变更:
  dds_set_qos(entity, new_qos)
    → 检查 new_qos 中的策略是否在 CHANGEABLE_MASK 中
    → 调用 deriver->set_qos() 执行类型特定变更
    → 触发 SEDP 重新发布（通知远端 QoS 变化）
```

### 3.2 QoS 合并（三级继承）

```
最终 QoS = 用户设置 ← Topic QoS ← 全局默认

示例:
  用户设置:  { reliability = RELIABLE }        present = 0x01
  Topic QoS: { deadline = 100ms, history = KEEP_LAST(5) } present = 0x0C
  默认 QoS:  { reliability = BEST_EFFORT, durability = VOLATILE, ... } present = 0xFF

  合并后:    { reliability = RELIABLE,        ← 用户设置优先
               deadline = 100ms,              ← Topic QoS 补充
               history = KEEP_LAST(5),        ← Topic QoS 补充
               durability = VOLATILE,         ← 默认值补充
               ... }
```

### 3.3 QoS Delta 编码（用于 SEDP）

```c
uint64_t ddsi_xqos_delta(const dds_qos_t *a, const dds_qos_t *b, uint64_t mask);
// 返回: a 和 b 中不同的 QoS 位掩码
// mask: 只比较这些 QoS（通常传 ~0ULL 比较所有）
```

## 4. 设计逻辑与设计思想

### 4.1 为什么用 `present` 位掩码而不是为每个 QoS 设 `is_set` 标志？

**设计哲学：批量操作 + 空间效率**

如果每个 QoS 策略有独立的 `bool is_set` 标志：
```c
struct dds_qos {
  bool reliability_is_set;
  dds_reliability_qospolicy_t reliability;
  bool durability_is_set;
  dds_durability_qospolicy_t durability;
  // ... 22 × (1 byte bool + padding) → 44+ 额外字节
};
```

用 `uint64_t present` 位掩码：
```c
struct dds_qos {
  uint64_t present;  // 8 字节覆盖所有 22 个 QoS
};
```

优势不仅是空间节约，更在于**批量操作效率**：

```c
// 一次操作检查多个 QoS 是否设置
if (qos->present & DDSI_QP_RXO_MASK) { ... }  // 一条 AND 指令

// 合并未设置的 QoS
qos->present |= src->present & ~qos->present;  // 一条指令完成所有合并

// Delta 编码
delta = (a->present ^ b->present) & mask;       // 一条 XOR + AND
```

如果用 bool 标志，每种操作都需要 22 次条件判断。

**底层思想**：**位运算是 C 语言中最优雅的批量操作原语**。一个 64 位整数可以表示 64 个独立的二值状态，且所有集合操作（并集、交集、差集、补集）都是单条指令。

### 4.2 为什么区分 RxO 和非 RxO QoS？

**设计哲学：兼容性是协商的，策略是本地的**

DDS 规范将 QoS 分为两类：

**RxO（Requested-Offered）QoS** — 必须兼容才能匹配：
- Reader **请求**（Request）一个服务质量水平
- Writer **提供**（Offer）一个服务质量水平
- 只有 Offer ≥ Request 时才兼容

这些 QoS 影响**两端的通信行为**：如果 Reader 要求 RELIABLE 但 Writer 只能 BEST_EFFORT，数据交付保证不一致——匹配无意义。

**非 RxO QoS** — 各端独立设置：
- HISTORY：Writer 保留最后 5 个 sample，Reader 保留最后 10 个——完全独立
- RESOURCE_LIMITS：各端根据自己的内存限制设定
- TRANSPORT_PRIORITY：各端根据自己的网络策略设定

**底层思想**：**分布式系统中的协商 vs 自治**。RxO QoS 是需要**双方协商**才有意义的属性（像合同条款）。非 RxO QoS 是各方**自主决定**的属性（像内部管理规定）。将两者区分开来，避免不必要的匹配失败。

### 4.3 为什么 QoS 有可变更/不可变更之分？

**设计哲学：契约稳定性**

不可变更的 QoS（创建后不能修改）：
- RELIABILITY、DURABILITY、OWNERSHIP、HISTORY、...
- 原因：这些 QoS 影响底层协议行为（HEARTBEAT/ACK_NACK、WHC 策略等）。运行时修改需要重建整个通信链路，成本极高

可变更的 QoS：
- DEADLINE、PARTITION、OWNERSHIP_STRENGTH、TRANSPORT_PRIORITY
- 原因：这些 QoS 的变更不需要重建通信链路——只需通知对端

```
PARTITION 变更的处理:
  dds_set_qos(writer, new_qos_with_different_partition)
    → 取消与旧 Partition 中 Reader 的匹配
    → 与新 Partition 中的 Reader 重新匹配
    → 通过 SEDP 通知远端
    → 无需重建 socket 或 WHC
```

**底层思想**：**系统契约有不同的变更成本**。不可变更的 QoS 是"宪法级"契约（修改成本极高），可变更的 QoS 是"政策级"契约（修改成本低）。设计应该反映变更成本的差异。

### 4.4 为什么 QoS 有三级继承（用户 ← Topic ← 默认）？

**设计哲学：减少配置冗余 + 集中管理**

一个系统中可能有 100 个 Writer，它们大多使用相同的 QoS。如果每个 Writer 都要完整设置所有 22 个 QoS：
- 大量重复代码
- 修改全局策略需要改 100 处

三级继承的意义：
- **默认值**：为 90% 的场景提供合理配置（零配置可用）
- **Topic QoS**：同一 Topic 的所有 Writer/Reader 共享的策略（集中管理）
- **用户设置**：特定 Writer/Reader 的个性化配置（覆盖上层）

```
例: 100 个 Writer，99 个用默认 QoS，1 个需要 RELIABLE
  → 99 个: dds_create_writer(pp, topic, NULL, NULL)   // QoS=NULL → 用默认
  → 1 个:  dds_create_writer(pp, topic, reliable_qos, NULL)  // 只设置 reliability
```

**底层思想**：**配置继承是 DRY（Don't Repeat Yourself）原则在运行时参数中的应用**。

### 4.5 `aliased` 字段解决什么问题？

**设计哲学：安全的浅拷贝优化**

QoS 结构中有多个字符串指针（topic_name、type_name、partition names）。在 QoS 合并和传递过程中，频繁的字符串复制（`strdup`）开销不小。

`aliased` 位掩码标记哪些字符串是**别名**（指向其他 QoS 结构的字符串，未独立分配）。

```c
// 浅拷贝（aliased）:
dst->topic_data.value = src->topic_data.value;  // 只复制指针
dst->aliased |= DDSI_QP_TOPIC_DATA;

// 深拷贝时跳过 aliased 字段:
if (!(qos->aliased & DDSI_QP_TOPIC_DATA))
  free(qos->topic_data.value);  // 只释放独立分配的内存
```

**底层思想**：**写时复制（Copy-on-Write）的简化版**。大多数场景下 QoS 只读不写，浅拷贝足够。`aliased` 标记确保 `free()` 时不会释放不属于自己的内存。

## 5. 与规范的关系

- **DDS v1.4 §2.2.3**：22 个 QoS 策略的完整定义
- **DDS v1.4 §2.2.3.x**：每个策略的 RxO 规则和可变更性
- **DDS v1.4 Table 2-22**：QoS 适用性表（哪些实体支持哪些 QoS）
- **RTPS v2.5 §8.7**：QoS 到 RTPS 行为的映射

## 6. 总结

QoS 系统的设计哲学可概括为**位掩码批量操作 + 协商与自治分离 + 三级继承**：
1. `present` 位掩码实现单指令批量 QoS 操作
2. RxO/非 RxO 分类区分协商型和自治型策略
3. 可变更/不可变更分类反映变更成本差异
4. 三级继承（用户 ← Topic ← 默认）消除配置冗余
5. `aliased` 字段实现安全的浅拷贝优化
