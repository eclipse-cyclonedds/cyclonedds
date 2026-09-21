# 模块 5：发包路径（Send Path）设计文档

## 1. 概述

发包路径负责将用户应用数据从 `dds_write()` API 调用传送到网络 socket。它涵盖序列化、Writer History Cache 存储、RTPS 消息构建、打包批处理和网络发送。

**关键文件**：
- `src/core/ddsc/src/dds_write.c` — 用户 API 入口
- `src/core/cdr/src/dds_cdrstream_write.part.h` — CDR 编码
- `src/core/ddsi/src/ddsi_serdata.c` / `ddsi_serdata_cdr.c` — 序列化数据抽象
- `src/core/ddsi/src/ddsi_transmit.c` — RTPS 消息构建与发送
- `src/core/ddsi/src/ddsi_xmsg.c` — 消息结构与打包
- `src/core/ddsi/src/ddsi__tran.h` / `ddsi_tran.c` — 传输抽象层

## 2. 核心数据结构

### 2.1 序列化数据 (`ddsi_serdata`)

```c
struct ddsi_serdata {
  const struct ddsi_serdata_ops *ops;   // 序列化操作虚函数表
  const struct ddsi_sertype *type;      // 类型元数据
  ddsrt_atomic_uint32_t refc;           // 引用计数
  uint32_t hash;                        // key hash
  enum ddsi_serdata_kind kind;          // EMPTY / KEY / DATA / RAWCDR
  ddsi_guid_t wr_guid;                 // 写入者 GUID
  ddsrt_mtime_t twrite;                // 写入时间
  ddsrt_wctime_t timestamp;            // 源时间戳
  uint64_t statusinfo;                 // dispose/unregister 标志
  struct ddsi_serdata *loan;           // 借用引用（零拷贝）
};
```

### 2.2 序列化类型 (`ddsi_sertype`)

```c
struct ddsi_sertype {
  const struct ddsi_sertype_ops *ops;   // 类型操作虚函数表
  const struct ddsi_serdata_ops *serdata_ops;  // 数据操作虚函数表
  char *type_name;                      // 类型名称
  uint32_t flags;                       // 固定大小、无键等标志
  size_t sizeof_type;                   // 应用类型大小
  // ...
};
```

### 2.3 RTPS 消息 (`ddsi_xmsg`)

```c
struct ddsi_xmsg {
  struct ddsi_xmsgpool *pool;           // 所属消息池
  size_t maxsz;                         // 最大消息大小
  size_t sz;                            // 当前大小
  enum ddsi_xmsg_kind kind;             // CONTROL / DATA / DATA_REXMIT
  enum ddsi_xmsg_dstmode dstmode;       // UNSET / ONE / ALL / ALL_UC
  // 目标地址（union 按 dstmode 选择）
  union {
    struct { ddsi_xlocator_t loc; } one;
    struct { struct ddsi_addrset *as; } all;
  } dstaddr;
  unsigned char payload[];              // RTPS submessage 数据
};
```

### 2.4 消息包 (`ddsi_xpack`)

```c
struct ddsi_xpack {
  struct ddsi_domaingv *gv;
  uint32_t niov;                        // iovec 条目数
  ddsrt_iovec_t *iov;                   // scatter-gather 向量
  struct ddsi_xmsg *msgs[];             // 引用的 xmsg 列表
  // ...
};
```

### 2.5 目标寻址模式

```c
enum ddsi_xmsg_dstmode {
  NN_XMSG_DST_UNSET,    // 未设置
  NN_XMSG_DST_ONE,      // 单播到特定 peer
  NN_XMSG_DST_ALL,      // 多播到所有匹配的 Reader
  NN_XMSG_DST_ALL_UC    // 逐个单播到所有 Reader（无多播时）
};
```

## 3. 机制设计

### 3.1 完整发送调用链

```
应用线程调用:
  dds_write(writer_handle, data_ptr)
    │
    ├─ dds_entity_pin(writer_handle)         [获取 Writer 引用]
    ├─ dds_write_impl(wr, data, timestamp)
    │   │
    │   ├─ sertype->serdata_from_sample()    [序列化: 应用数据 → serdata]
    │   │   └─ CDR 编码: 字节序转换、对齐、嵌套类型处理
    │   │
    │   ├─ ddsi_write_sample_gc()            [核心发送逻辑]
    │   │   │
    │   │   ├─ ddsi_whc_insert(whc, serdata)  [存入 Writer History Cache]
    │   │   │   └─ 分配 seqnum, 按 QoS 策略保留或淘汰旧 sample
    │   │   │
    │   │   ├─ 构建 RTPS DATA submessage
    │   │   │   ├─ ddsi_xmsg_new()            [从消息池分配 xmsg]
    │   │   │   ├─ 添加 INFO_TS submessage     [时间戳]
    │   │   │   └─ 添加 DATA submessage        [序列化数据负载]
    │   │   │
    │   │   ├─ ddsi_xpack_addmsg(xpack, xmsg) [打包到 xpack]
    │   │   │   └─ 将 xmsg 的 payload 添加为 iovec 条目（零拷贝引用）
    │   │   │
    │   │   └─ ddsi_xpack_send(xpack)         [批量发送]
    │   │       └─ ddsi_conn_write()           [socket sendto/writev]
    │   │
    │   └─ 返回 DDS_RETCODE_OK
    │
    └─ dds_entity_unpin(writer_handle)       [释放 Writer 引用]
```

### 3.2 批处理机制

```
非批处理模式 (whc_batch = false, 默认):
  每次 dds_write() → 立即 xpack_send() → 立即到达网络

批处理模式 (whc_batch = true):
  dds_write() → xpack_addmsg() → 累积
  dds_write() → xpack_addmsg() → 累积
  ...
  xpack 满或超时 → xpack_send() → 一次性发送所有累积消息
```

### 3.3 重传目标合并

```
Reader A 请求重传 sample #42 → xmsg(dst=ONE, locA)
Reader B 请求重传 sample #42 → xmsg(dst=ONE, locB)

ddsi_xmsg_merge_rexmit_destinations_wrlock_held():
  → 合并为 xmsg(dst=ALL, {locA, locB})
  → 只发送一个 UDP 包（多播或逐一单播）
```

## 4. 设计逻辑与设计思想

### 4.1 为什么在应用线程中执行发送？

**设计哲学：最小延迟 + 可预测性**

替代方案——**专用发送线程**：
```
应用线程: dds_write() → enqueue(send_queue) → 返回
发送线程: dequeue(send_queue) → 序列化 → 构建消息 → socket 发送
```

这种方案的问题：
- **增加延迟**：队列入队 + 线程唤醒 + 上下文切换 ≈ 10~50μs 额外延迟
- **内存管理复杂**：数据必须复制到队列中（应用线程返回后可能修改原始数据）
- **排序困难**：多线程写入同一 Writer 时，队列中的排序需要额外同步

CycloneDDS 的选择——**应用线程同步发送**：
```
应用线程: dds_write() → 序列化 → WHC → xmsg → xpack → socket → 返回
```

优势：
- **零拷贝友好**：序列化时数据仍在应用线程的缓存中（cache-hot）
- **天然有序**：同一线程的多次 write 天然按调用顺序排列
- **延迟可预测**：没有队列等待、线程调度的不确定性

**底层思想**：**延迟敏感的路径不应该跨线程**。DDS 的设计初衷是实时系统通信，而实时系统最重要的指标之一是延迟的**可预测性**，而不仅仅是平均延迟。

### 4.2 为什么分离 serdata 和 sertype？

**设计哲学：类型元数据与实例数据的解耦**

`sertype` 描述"如何序列化某种类型"（不可变，进程生命周期）：
```
sertype = {
  type_name: "SensorData",
  fields: [{name: "id", type: uint32}, {name: "value", type: float64}],
  serialize: function_pointer,
  deserialize: function_pointer
}
```

`serdata` 描述"一个具体的序列化后的实例"（临时，引用计数管理）：
```
serdata = {
  type: → sertype,
  bytes: [0x01, 0x00, 0x2A, 0x00, 0x40, 0x59, ...],
  refc: 3,  // WHC + xmsg + 用户各持有一个引用
  seqnum: 42
}
```

**为什么分离？**

- **共享与独占**：一个 `sertype` 可能被数千个 `serdata` 实例共享。如果把类型信息内嵌到每个 `serdata` 中，会浪费内存
- **可替换性**：通过替换 `sertype->ops`，可以在不修改核心代码的情况下支持新的序列化格式（如 JSON、Protobuf、自定义二进制格式）
- **生命周期独立**：`sertype` 的生命周期与 Topic 绑定，`serdata` 的生命周期与消息传递绑定

**底层思想**：这是**享元模式（Flyweight Pattern）**的应用。共享不变的类型元数据，只为每个实例存储变化的部分（序列化后的字节和元数据）。

### 4.3 为什么 xmsg → xpack 两级抽象？

**设计哲学：逻辑结构与网络效率的分离**

**xmsg（逻辑消息）**回答的问题：
- "这条消息包含什么 RTPS submessage？"
- "这条消息应该发给谁？"
- "这是数据消息还是控制消息？"

**xpack（网络包）**回答的问题：
- "如何在一个 UDP 数据报中塞入尽可能多的 xmsg？"
- "如何避免复制 payload 数据？"
- "如何处理 MTU 限制？"

如果合并两者：
- 每个 DATA submessage 单独发一个 UDP 包 → 网络开销巨大（每个 UDP 包有 28 字节 IP+UDP 头）
- 或者，将多个 submessage 写入单个缓冲区 → 需要大量内存复制

两级抽象的解决方案：
- xmsg 独立构建各自的 submessage（零依赖）
- xpack 通过 **scatter-gather I/O** 将多个 xmsg 的 payload 组装成一个 UDP 包（零拷贝）

```
xpack 的 iovec 结构：
  iov[0] → RTPS Header (固定 20 字节)
  iov[1] → xmsg_1 的 INFO_TS + DATA submessage
  iov[2] → serdata_1 的 payload（直接引用序列化数据，不复制）
  iov[3] → xmsg_2 的 INFO_TS + DATA submessage
  iov[4] → serdata_2 的 payload
  ...
  ↓
  writev(socket, iov, niov)  // 一次系统调用发送整个 UDP 包
```

**底层思想**：**消除数据复制是高性能网络编程的核心原则**。scatter-gather I/O 让操作系统内核直接从分散的内存位置组装网络包，避免了用户空间的 memcpy。

### 4.4 为什么 serdata 使用引用计数？

**设计哲学：共享所有权消除复制**

一个 serdata 实例在发送路径中被多个组件同时持有：

```
serdata (refc=4)
  ├─ Writer History Cache (用于重传)
  ├─ xmsg (用于当前发送)
  ├─ SEDP (如果是 built-in topic 数据)
  └─ 用户 (如果使用 dds_writecdr，用户持有原始引用)
```

如果不用引用计数而用复制：
- WHC 保存一份副本
- xmsg 保存一份副本
- 一个 1KB 的 serdata 变成 4KB 的内存占用

引用计数方案：
- 只有一份 serdata 存在于内存中
- `ddsi_serdata_ref()` 增加引用计数（原子 +1）
- `ddsi_serdata_unref()` 减少引用计数，到 0 时释放
- 总内存占用 = 1KB + 4 × 8B（引用指针） ≈ 1KB

**底层思想**：**如果数据是不可变的，就共享它而不是复制它**。serdata 在创建后是不可变的（序列化字节不会改变），因此多方同时读取是安全的。引用计数的原子操作开销远低于 memcpy 的开销。

### 4.5 为什么有 4 种目标寻址模式？

**设计哲学：最优网络策略的编码**

4 种模式对应 4 种网络拓扑下的最优发送策略：

| 模式 | 场景 | 网络行为 |
|---|---|---|
| `DST_UNSET` | 消息刚创建，目标未确定 | 不发送 |
| `DST_ONE` | 单播重传（特定 Reader 请求） | sendto(unicast_addr) |
| `DST_ALL` | 正常数据发布（多播可用） | sendto(multicast_addr) |
| `DST_ALL_UC` | 正常数据发布（多播不可用） | 循环 sendto(每个 Reader 的 unicast_addr) |

关键优化——**重传合并**：

当多个 Reader 同时 NACK 同一个 sample 时，单独处理会产生 N 个单播重传。通过 `ddsi_xmsg_merge_rexmit_destinations_wrlock_held()`，多个 `DST_ONE` 消息可以合并为一个 `DST_ALL` 消息——只发送一次多播就能满足所有 Reader。

**底层思想**：**在消息层面编码网络策略，而不是在发送层面决定**。xmsg 创建时就确定了它的"发送意图"（单播/多播/合并），发送层只需执行意图而无需做决策。这是**策略与执行分离**的体现。

### 4.6 批处理（whc_batch）的设计权衡

```
延迟 vs 吞吐量的经典权衡：

非批处理:
  ┌──────┐  ┌──────┐  ┌──────┐
  │write1│→ │ UDP  │→ │write2│→ │ UDP  │→ ...
  └──────┘  │packet│  └──────┘  │packet│
            └──────┘            └──────┘
  延迟：最低（每次 write 立即发送）
  吞吐：较低（每个 UDP 包只含一个 sample）
  适用：实时控制、低频高价值数据

批处理:
  ┌──────┐  ┌──────┐  ┌──────┐     ┌──────────────────┐
  │write1│→ │write2│→ │write3│→ →  │ 一个 UDP packet   │→ ...
  └──────┘  └──────┘  └──────┘     │ 含 3 个 samples  │
                                    └──────────────────┘
  延迟：较高（等待批次满或超时）
  吞吐：最高（摊薄 UDP/IP 头开销）
  适用：传感器遥测、日志流、高频低价值数据
```

**底层思想**：**不存在适合所有场景的单一发送策略**。CycloneDDS 不试图用自适应算法猜测最优策略，而是暴露 `whc_batch` 标志让用户显式选择。这体现了**显式控制优于隐式魔法**的设计原则。

## 5. 与规范的关系

- **RTPS v2.5 §8.3.7**：DATA submessage 格式——xmsg 中的 DATA 构建严格遵循规范字段布局
- **RTPS v2.5 §8.3.5**：INFO_TS submessage——每个 DATA 前附带时间戳
- **RTPS v2.5 §9.4**：Message 级别的 CDR 编码——serdata 使用 CDR/XCDR 格式
- **RTPS v2.5 §8.4.6**：StatefulWriter 行为——WHC 的 sample 保留策略对应规范中的 Writer 状态机

## 6. 总结

发包路径的设计哲学可概括为**零拷贝 + 应用线程直发 + 策略编码**：
1. 应用线程同步发送消除跨线程延迟
2. serdata/sertype 分离实现享元模式
3. xmsg/xpack 两级抽象 + scatter-gather I/O 实现零拷贝
4. 引用计数共享 serdata 消除冗余复制
5. 4 种寻址模式 + 重传合并优化网络效率
6. 批处理标志提供显式的延迟/吞吐权衡
