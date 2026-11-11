# DDS/CycloneDDS 初始化流程详细分析

## 1. 概述

CycloneDDS 的初始化是一个**多层渐进式**的过程，从高层 DDS API 到底层网络传输，可分为四个层次：

```
┌──────────────────────────────────────────────────────────────────┐
│  用户调用                                                        │
│    dds_create_domain() / dds_create_domain_with_config()          │
├──────────────────────────────────────────────────────────────────┤
│  DDS Layer (ddsc)                                                │
│    域配置解析 → XML 解析 → DDSI 域初始化 → 参与者创建              │
├──────────────────────────────────────────────────────────────────┤
│  DDSI Layer (rtps)                                               │
│    ddsi_init() → 传输初始化 → Socket 创建 → 线程启动 → 内置端点    │
├──────────────────────────────────────────────────────────────────┤
│  DDSRT Layer (runtime)                                           │
│    Socket API → 线程 → 同步原语 → 网络接口发现                      │
└──────────────────────────────────────────────────────────────────┘
```

关键文件：
- `src/core/ddsc/src/dds_domain.c` — DDS 域创建
- `src/core/ddsi/src/ddsi_init.c` — DDSI 域初始化（核心）
- `src/core/ddsi/src/ddsi_udp.c` — UDP 传输工厂
- `src/core/ddsi/src/ddsp_schedule.c` — SPDP 调度
- `src/core/ddsi/src/ddsi_receive.c` — 接收线程
- `src/core/ddsi/src/ddsi_entity.c` — 实体管理
- `src/core/ddsi/src/ddsi_participant.c` — 参与者管理
- `src/core/ddsi/src/ddsi_builtin_endpoint.c` — 内置端点创建

---

## 2. 顶层调用入口：dds_create_domain()

### 2.1 调用链

```c
// 用户使用 DDS API
dds_create_domain(uint32_t domain_id)
  → dds_create_domain_with_config(domain_id, config)
    → create_domain(domain_id, config, implicit)
      → create_and_lock_domain(domain_id, config, implicit)
        → ddsi_config_init_default()          // 若未提供配置，使用默认
        → ddsi_config_from_xml()              // 从 XML 配置文件解析
        → ddsi_config_prep()                  // 配置校验与修正
        → ddsi_domain_new()                   // 创建 DDSI 域对象
          → ddsi_init(gv)                     // ★ 核心初始化函数 ★
          → ddsi_start(gv)                    // ★ 启动所有后台线程 ★
        → dds_participant_register()          // 注册默认参与者
        → ddsi_create_participant()           // ★ 创建 DDSI 参与者 ★
          → create_builtin_entities()         // ★ 创建 SPDP/SEDP 内置端点 ★
```

### 2.2 为什么先 init 再 start？

**设计哲学：两阶段初始化 (Two-Phase Initialization)**

```
Phase 1: ddsi_init() — 资源分配与结构构建
  → 创建所有数据结构
  → 分配内存、Socket、队列
  → 构建实体索引
  → 创建特殊类型
  → 不创建任何线程 → 无竞态条件

Phase 2: ddsi_start() — 线程启动与运行
  → 所有资源已就绪
  → 按依赖顺序启动线程
  → 如果任一线程启动失败 → 有序回滚
```

这种模式将**资源分配**与**并发执行**解耦。init 阶段可以在无竞争的环境下构建复杂的数据结构，start 阶段只需负责"打开开关"。

---

## 3. Phase 1: ddsi_init() 详细流程

`ddsi_init()` 是整个 DDS 栈的核心初始化函数（位于 `ddsi_init.c:1111-1698`）。

### 3.1 阶段总览

```
ddsi_init()
  │
  ├─ [步骤 0]  初始化基础状态
  ├─ [步骤 1]  初始化传输工厂 (UDP/TCP/RawEth)
  ├─ [步骤 2]  收集网络接口信息
  ├─ [步骤 3]  配置组播地址和接口模式
  ├─ [步骤 4]  初始化数据类型系统 (sertypes)
  ├─ [步骤 5]  初始化实体管理 (entity_index, lease)
  ├─ [步骤 6]  生成参与者 GUID
  ├─ [步骤 7]  创建单播 Socket
  ├─ [步骤 8]  创建组播 Socket
  ├─ [步骤 9]  创建发送 Socket
  ├─ [步骤 10] 加入组播组
  ├─ [步骤 11] 初始化队列 (xevents, dqueue, gc)
  └─ [步骤 12] 初始化 SPDP 调度器
```

### 3.2 步骤 0：初始化基础状态 (行 1117-1150)

```c
// 记录启动时间（用于日志中的相对时间引用）
gv->tstart = ddsrt_time_wallclock();

// 初始化参数序列化表（plist 参数 ID 到序列化函数的映射表）
ddsi_plist_init_tables();

// 清零所有连接指针（为错误处理的 goto 清理做准备）
gv->disc_conn_uc = NULL;
gv->data_conn_uc = NULL;
gv->disc_conn_mc = NULL;
gv->data_conn_mc = NULL;
for (size_t i = 0; i < MAX_XMIT_CONNS; i++)
  gv->xmit_conns[i] = NULL;

// 设置初始的 deaf/mute 状态（可用于启动时暂时屏蔽出入流量）
gv->deaf = gv->config.initial_deaf;
gv->mute = gv->config.initial_mute;
```

**为什么清零所有指针？**

CycloneDDS 使用了**goto-based 错误处理**模式。所有指针初始化为 NULL，这样在错误标签处可以直接 `if (ptr) free(ptr);`，无需区分哪些已分配哪些未分配。这是一种经典的 C 语言资源管理模式。

### 3.3 步骤 1：初始化传输工厂 (行 1152-1196)

```c
switch (gv->config.transport_selector) {
  case DDSI_TRANS_UDP:   // IPv4 UDP
    ddsi_udp_init(gv);                           // 注册 UDP 传输工厂
    gv->m_factory = ddsi_factory_find(gv, "udp"); // 解析工厂指针
    break;
  case DDSI_TRANS_TCP:   // TCP
    ddsi_tcp_init(gv);
    gv->m_factory = ddsi_factory_find(gv, "tcp");
    break;
  case DDSI_TRANS_RAWETH: // Raw Ethernet (无 IP 层)
    ddsi_raweth_init(gv);
    gv->m_factory = ddsi_factory_find(gv, "raweth");
    break;
}
gv->m_factory->m_enable = true;  // 启用传输工厂
```

**传输工厂的注册原理** (`ddsi_udp_init` 内部)：

```c
int ddsi_udp_init(struct ddsi_domaingv *gv) {
  struct ddsi_tran_factory *f = ddsrt_malloc(sizeof(*f));
  memset(f, 0, sizeof(*f));
  f->m_typename = "udp";
  f->m_supports = DDSI_LOCATOR_KIND_UDPv4 | ...;

  // 注册所有传输操作函数指针
  f->m_create_conn_fn   = ddsi_udp_create_conn;
  f->m_read_fn          = ddsi_udp_conn_read;
  f->m_write_fn         = ddsi_udp_conn_write;
  f->m_join_mc_fn       = ddsi_udp_join_mc;
  f->m_leave_mc_fn      = ddsi_udp_leave_mc;
  f->m_is_mcaddr_fn     = ddsi_udp_is_mcaddr;
  f->m_is_ssm_mcaddr_fn = ddsi_udp_is_ssm_mcaddr;
  f->m_address_from_string_fn = ddsi_udp_address_from_string;
  f->m_locator_to_string_fn   = ddsi_udp_locator_to_string;

  // 注册到全局工厂链表
  ddsi_factory_add(gv, f);
}
```

**设计模式：策略模式 + 工厂模式**。传输层的所有操作通过函数指针抽象，`ddsi_init` 只调用 `ddsi_factory_create_conn()` 等通用接口，不关心底层是 UDP、TCP 还是以太网。新增传输协议只需实现一套函数指针并注册。

### 3.4 步骤 2：收集网络接口信息 (行 1198-1203)

```c
if (!ddsi_gather_network_interfaces(gv)) {
  goto err_gather_nwif;
}
```

**`ddsi_gather_network_interfaces()` 做了什么？**

1. 调用 OS API（`getifaddrs` / `GetAdaptersAddresses` / `ioctl(SIOCGIFCONF)`）枚举所有网络接口
2. 过滤回环接口（除非特别配置）
3. 对每个接口记录：
   - `loc` — 接口 IP 地址
   - `if_index` — 接口索引
   - `allow_multicast` — 是否支持组播
   - `mc_capable` / `mc_flaky` — 组播能力检测
4. 选择"首选接口"（第一个非回环、非链路本地地址）
5. 限制最多 `MAX_XMIT_CONNS` (4) 个接口

### 3.5 步骤 3：配置组播地址和接口模式 (行 1259-1286)

```c
// 3a. 设置哪些接口参与组播接收 (recvips)
set_recvips(gv);
  // 解析 "all" / "any" / "preferred" / "none" / 具体IP列表
  // 设置 gv->recvips_mode: RECVIPS_MODE_ALL / ANY / PREFERRED / NONE / SOME

// 3b. 设置 SPDP 组播地址 (默认 239.255.0.1)
set_spdp_address(gv);
  // gv->loc_spdp_mc = 239.255.0.1:7400 (domain 0)

// 3c. 设置默认组播地址 (默认同 SPDP 地址)
set_default_mc_address(gv);
  // gv->loc_default_mc = gv->loc_spdp_mc

// 3d. 设置外部地址和掩码 (NAT 穿越)
set_ext_address_and_mask(gv);
  // gv->interfaces[i].extloc = NAT 外部 IP
```

### 3.6 步骤 4：初始化数据类型系统 (行 1288-1358)

```c
// 创建消息池（预分配的 RTPS 消息对象，避免频繁 malloc）
gv->xmsgpool = ddsi_xmsgpool_new(gv->config.protocol_version);

// 复制并调整默认参与者 QoS
gv->default_local_xqos_pp = ddsi_default_qos_participant;
gv->default_local_xqos_pp.liveliness.lease_duration = gv->config.lease_duration;

// 初始化内置端点 QoS 模板
ddsi_xqos_copy(&gv->spdp_endpoint_xqos, &ddsi_default_qos_reader);
make_builtin_endpoint_xqos(&gv->builtin_endpoint_xqos_rd, &ddsi_default_qos_reader);
make_builtin_endpoint_xqos(&gv->builtin_endpoint_xqos_wr, &ddsi_default_qos_writer);

// 添加参与者位置属性（进程名、PID、主机名）
ddsi_xqos_add_property_if_unset(&gv->default_local_xqos_pp,
    DDS_BUILTIN_TOPIC_PARTICIPANT_PROPERTY_PROCESS_NAME, procname);
ddsi_xqos_add_property_if_unset(&gv->default_local_xqos_pp,
    DDS_BUILTIN_TOPIC_PARTICIPANT_PROPERTY_PID, pid_string);
ddsi_xqos_add_property_if_unset(&gv->default_local_xqos_pp,
    DDS_BUILTIN_TOPIC_PARTICIPANT_PROPERTY_HOSTNAME, hostname);

// 创建特殊类型 (sertypes) — 用于内置端点的序列化/反序列化
make_special_types(gv);
  // gv->spdp_type          → "ParticipantBuiltinTopicData"
  // gv->sedp_reader_type   → "SubscriptionBuiltinTopicData"
  // gv->sedp_writer_type   → "PublicationBuiltinTopicData"
  // gv->pmd_type           → "ParticipantMessageData"
  // (安全、类型发现等扩展类型)

// 注册到类型哈希表
ddsi_sertype_register_locked(gv, gv->spdp_type);
ddsi_sertype_register_locked(gv, gv->sedp_reader_type);
ddsi_sertype_register_locked(gv, gv->sedp_writer_type);
ddsi_sertype_register_locked(gv, gv->pmd_type);
```

**关键概念：sertype (SerData Type)**

sertype 是 CycloneDDS 对"数据类型"的抽象。每个类型包含：
- 序列化函数指针（CDR 编码）
- 反序列化函数指针（CDR 解码）
- 键哈希函数（用于按 key 路由）
- 类型名称

内置端点（SPDP/SEDP）使用特殊的 sertype，其序列化基于 Parameter List 而非标准的 CDR stream。这是为了与 RTPS 规范兼容。

### 3.7 步骤 5：初始化实体管理 (行 1360-1368)

```c
// 初始化互斥锁
ddsrt_mutex_init(&gv->sertypes_lock);
ddsrt_mutex_init(&gv->participant_set_lock);
ddsrt_cond_init(&gv->participant_set_cond);

// 租约管理（用于检测远程参与者存活）
ddsi_lease_management_init(gv);

// 已删除参与者管理（延迟清理，等待消息发送完毕）
gv->deleted_participants = ddsi_deleted_participants_admin_new(
    &gv->logconfig, gv->config.prune_deleted_ppant.delay);

// ★ 实体索引 — 所有 DDS 实体 (Participant, Writer, Reader, Topic) 的集中索引 ★
gv->entity_index = ddsi_entity_index_new(gv);

// 实体命名相关
ddsrt_mutex_init(&gv->naming_lock);
ddsrt_prng_init(&gv->naming_rng, &gv->config.entity_naming_seed);
```

**实体索引的底层结构**：

```
gv->entity_index
  ├─ guid_hash   (DDSRT_HASH) — 按 GUID 快速查找
  ├─ participant_tree (AVL)   — 按 participant GUID 组织
  ├─ writer_tree      (AVL)   — 所有 Writer
  ├─ reader_tree      (AVL)   — 所有 Reader
  ├─ topic_tree       (AVL)   — 所有 Topic
  ├─ proxy_participant_tree (AVL) — 远端参与者
  ├─ proxy_writer_tree      (AVL) — 远端 Writer
  └─ proxy_reader_tree      (AVL) — 远端 Reader
```

实体索引是所有 DDS 操作的**单一事实来源**（Single Source of Truth）。任何实体创建/删除都通过这个索引。

### 3.8 步骤 6：生成参与者 GUID (行 1369-1402)

```c
// 生成 64-bit 唯一标识符 (IID)
uint64_t iid = ddsi_iid_gen();  // 基于进程 ID + 时间戳 + 计数器

// 混合网络接口信息（增加不同机器间的区分度）
ddsrt_md5_init(&st);
ddsrt_md5_append(&st, &iid, sizeof(iid));
for each interface:
  ddsrt_md5_append(&st, &intf->loc.kind, ...);
  ddsrt_md5_append(&st, intf->loc.address, ...);
ddsrt_md5_finish(&st, digest);

// 构建 base GUID
gv->ppguid_base.prefix.s[0] = DDSI_VENDORID_ECLIPSE.id[0];  // 厂商 ID (0x02)
gv->ppguid_base.prefix.s[1] = DDSI_VENDORID_ECLIPSE.id[1];  // 厂商 ID (0x09)
memcpy(&gv->ppguid_base.prefix.s[2], digest, ...);  // MD5 摘要的剩余 14 字节
gv->ppguid_base.entityid.u = DDSI_ENTITYID_PARTICIPANT;  // 0x00000001
```

**GUID 格式** (RTPS §2.1.1)：

```
GUID: [4字节 VendorID] + [12字节 唯一标识] + [4字节 EntityID]
示例: 0209.1a2b.3c4d.5e6f.7a8b.9c0d | 00000001 (Participant)
      ─── 厂商 ID ──┘ ──── 唯一标识 ────┘ └─ EntityID ─┘
```

每次创建参与者时，GUID base 的后 8 字节会加上一个**递增计数器**，产生"跳变序列"，避免 GUID 重复。

### 3.9 步骤 7：创建单播 Socket (行 1410-1468)

```c
// 情况 A: 指定参与索引 (如 ppid = 0)
if (gv->config.participantIndex >= 0) {
  musret = make_uc_sockets(gv, &port_disc_uc, &port_data_uc, ppid);
  // 计算公式:
  //   port_disc = 7400 + 250*domain + 10 + 2*ppid
  //   port_data = 7400 + 250*domain + 11 + 2*ppid
}

// 情况 B: 自动查找可用索引 (AUTO 模式)
else if (gv->config.participantIndex == DDSI_PARTICIPANT_INDEX_AUTO) {
  for (ppid = 0; ppid <= maxAutoParticipantIndex; ppid++) {
    musret = make_uc_sockets(gv, port_disc, port_data, ppid);
    if (success) break;  // 找到可用端口就退出
    // port 已被占用 (EADDRINUSE) → 尝试下一个
  }
  gv->config.participantIndex = ppid - 1;  // 记录找到的索引
}
```

**`make_uc_sockets()` 内部流程**：

```c
make_uc_sockets()
  │
  ├─ 计算端口号:
  │   port_disc = ddsi_get_port(DDSI_PORT_UNI_DISC, ppid)
  │   port_data = ddsi_get_port(DDSI_PORT_UNI_DATA, ppid)
  │
  ├─ 创建 disc_conn_uc:
  │   qos = { m_purpose = RECV_UC, m_interface = NULL }
  │   ddsi_factory_create_conn(&disc_conn_uc, factory, port_disc, &qos)
  │     → bind(0.0.0.0:port_disc)
  │
  ├─ 创建 data_conn_uc:
  │   如果 port_data == port_disc:
  │     data_conn_uc = disc_conn_uc  ← 复用!
  │   否则:
  │     ddsi_factory_create_conn(&data_conn_uc, factory, port_data, &qos)
  │       → bind(0.0.0.0:port_data)
  │
  └─ 设置 locator:
      ddsi_conn_locator(disc_conn_uc, &loc_meta_uc)
      ddsi_conn_locator(data_conn_uc, &loc_default_uc)
```

### 3.10 步骤 8：创建组播 Socket (行 1484-1514)

```c
// 创建组播组成员关系追踪结构
gv->mship = ddsi_new_mcgroup_membership();

// 创建组播接收 Socket
if (allow_multicast) {
  create_multicast_sockets(gv);
    │
    ├─ port = ddsi_get_port(DDSI_PORT_MULTI_DISC, 0)  // 7400 + 250*domain
    ├─ qos = { m_purpose = RECV_MC, m_diffserv = 0, m_interface = NULL }
    ├─ ddsi_factory_create_conn(&disc_conn_mc, factory, port, &qos)
    │   → SO_REUSEADDR=true, bind(0.0.0.0:port)
    │
    └─ port = ddsi_get_port(DDSI_PORT_MULTI_DATA, 0)  // 7401 + 250*domain
    └─ ddsi_factory_create_conn(&data_conn_mc, factory, port, &qos)
        → SO_REUSEADDR=true, bind(0.0.0.0:port)
}
```

### 3.11 步骤 9：创建发送 Socket (行 1546-1576)

```c
// 每个网络接口一个发送 Socket
for (int i = 0; i < gv->n_interfaces; i++) {
  const struct ddsi_tran_qos qos = {
    .m_purpose = (interfaces[i].allow_multicast
                  ? DDSI_TRAN_QOS_XMIT_MC  // 可发组播
                  : DDSI_TRAN_QOS_XMIT_UC),// 仅单播
    .m_diffserv = 0,
    .m_interface = &gv->interfaces[i]      // 绑定到特定接口
  };
  ddsi_factory_create_conn(&gv->xmit_conns[i], fact, 0/*随机端口*/, &qos);
    // → bind(intf[i].ip:random)
    // → IP_MULTICAST_IF + TTL + LOOP (如果是 XMIT_MC)

  // 构建 接口 ↔ Socket 射表
  gv->intf_xlocators[i].conn = gv->xmit_conns[i];
  gv->intf_xlocators[i].c = gv->interfaces[i].loc;
  gv->intf_xlocators[i].c.port = ddsi_conn_port(gv->xmit_conns[i]);
}
```

### 3.12 步骤 10：加入组播组 (行 1584-1586)

```c
if (gv->m_factory->m_connless && joinleave_spdp_defmcip(gv, 1) < 0)
  goto err_joinleave_spdp;
```

**内部流程**：

```c
joinleave_spdp_defmcip(gv, dojoin=1)
  │
  ├─ 检查哪些接口允许 SPDP 组播
  ├─ 构建地址集:
  │   ddsi_add_locator_to_addrset(gv, as, &gv->loc_spdp_mc)
  │   ddsi_add_locator_to_addrset(gv, as, &gv->loc_default_mc)
  │
  └─ 对每个组播地址:
      ddsi_join_mc(gv, mship, disc_conn_mc, NULL, &loc_spdp_mc)
        │
        ├─ 遍历所有允许组播的接口:
        │   joinleave_mcgroups(gv, disc_conn_mc, join=1, ..., &mc_addr)
        │     │
        │     ├─ for each interface:
        │     │   joinleave_mcgroup(conn, 1, srcloc, mcloc, interface)
        │     │     │
        │     │     └─ reg_group_membership(mship, conn, src, mc)
        │     │         → 检查是否已加入
        │     │         → 首次 → ddsi_udp_join_mc()
        │     │                    → joinleave_asm_mcgroup()
        │     │                       → setsockopt(IP_ADD_MEMBERSHIP)
        │         → 已加入 → count++
        │
        └─ ddsi_join_mc(gv, mship, data_conn_mc, NULL, &loc_default_mc)
```

**引用计数的作用**：

```
接口 eth0 加入 239.255.0.1:
  disc_conn_mc 加入 → count=1 → setsockopt(IP_ADD_MEMBERSHIP) ✓
  data_conn_mc 加入 → count=2 → 跳过 (已加入)

接口 eth0 离开 239.255.0.1:
  disc_conn_mc 离开 → count=1 → 跳过 (还有人用)
  data_conn_mc 离开 → count=0 → setsockopt(IP_DROP_MEMBERSHIP) ✓
```

### 3.13 步骤 11：初始化队列系统 (行 1588-1611)

```c
// 事件队列 — 定时任务和一次性回调 (Heartbeat, ACK, Lease 检查等)
gv->xevents = ddsi_xeventq_new(gv, max_queued_rexmit_bytes, max_queued_rexmit_msgs);

// SPDP 调度器 — 管理发现消息的发送 (广播、重传、初始 peer 连接)
gv->spdp_schedule = ddsi_spdp_scheduler_new(gv, add_localhost_to_initial_peers);

// GC 队列 — 安全延迟释放实体 (等待所有引用消失后再释放内存)
gv->gcreq_queue = ddsi_gcreq_queue_new(gv);

// 数据分发队列 — 接收线程解析后的 RTPS 消息 → Worker 线程投递给用户
gv->builtins_dqueue = ddsi_dqueue_new("builtins", gv, ..., ddsi_builtins_dqueue_handler, NULL);
gv->user_dqueue     = ddsi_dqueue_new("user",     gv, ..., ddsi_user_dqueue_handler, NULL);
```

**为什么需要两套分发队列？**

```
Discovery 数据 (SPDP/SEDP):
  → builtins_dqueue
  → 低速率、高重要性
  → 必须按序处理 (bubble 机制保证)
  → 处理完成后才能创建新的实体

User 数据:
  → user_dqueue
  → 高速率、批量处理
  → 允许部分乱序 (由 reorder admin 处理)
  → 多个 worker 线程并发消费

如果共用一条队列:
  高吞吐量用户数据可能阻塞 Discovery 处理
  → 新参与无法发现
  → 租约超时检测延迟
```

### 3.14 步骤 12：SPDP 调度器初始化 (行 1595-1597)

```c
gv->spdp_schedule = ddsi_spdp_scheduler_new(gv, add_localhost_to_initial_peers);
```

SPDP 调度器的职责：

1. **初始 SPDP 广播** — 启动后立即向组播地址发送 SPDP 消息
2. **初始 Peer 连接** — 如果配置了 `<InitialPeers>`，直接向指定地址发送 SPDP
3. **定期重传** — 每 `SPDPInterval` (默认 30 秒) 重发 SPDP
4. **Participant Index 管理** — 在 AUTO 模式下，广播时携带 participant_index

---

## 4. Phase 2: ddsi_start() — 启动后台线程

`ddsi_start()` (位于 `ddsi_init.c:1700-1746`) 按**依赖顺序**启动所有后台线程。

### 4.1 线程启动顺序

```c
ddsi_start(gv)
  │
  ├─ ddsi_gcreq_queue_start()          // 1. GC 线程
  │   → 负责安全释放不再使用的实体
  │
  ├─ ddsi_dqueue_start(gv->builtins_dqueue)  // 2. Builtins 分发线程
  │   → 处理 SPDP/SEDP 消息
  │   → 创建远端参与和端点
  │
  ├─ ddsi_dqueue_start(gv->user_dqueue)      // 3. 用户数据分发线程
  │   → 处理用户数据投递
  │
  ├─ ddsi_xeventq_start()              // 4. 事件线程 (tev)
  │   → 处理 Heartbeat, ACK, Lease 检查, 重传
  │   → 一个线程管理所有定时任务
  │
  ├─ setup_and_start_recv_threads()    // 5. 接收线程 (1~3 个)
  │   → recv: 主接收线程 (waitset 多路复用)
  │   → recvMC: 组播专用线程 (可选)
  │   → recvUC: 单播专用线程 (可选)
  │
  ├─ ddsi_create_thread("listen")      // 6. TCP 监听线程 (仅 TCP 模式)
  │   → 接受入站 TCP 连接
  │
  └─ ddsi_new_debug_monitor()          // 7. 调试监控线程 (可选)
      → HTTP 接口，用于运行时诊断
```

### 4.2 为什么这个顺序？

```
依赖图:

GC Thread ──────────────────────────┐
                                    ↓
recv Threads → dqueue (builtins) → 实体创建/删除
               dqueue (user)     → 数据投递
                                    ↓
xevent Thread ←─────────────────── 需要实体存在才能定时操作
                                    ↓
listen Thread  (TCP only)  ────→ 接受新连接 → 新实体
```

**接收线程最后启动**的原因：
- 接收线程从网络读取数据后立即放入 dqueue
- 如果 dqueue 还没启动 → 数据堆积在 socket buffer → 可能溢出丢失
- 先启动 GC → 确保有实体创建/删除的安全机制
- 再启动 dqueue → 确保接收的数据有人消费
- 再启动 xevent → 确保 Heartbeat/ACK 能正确响应

---

## 5. 参与者创建：ddsi_create_participant()

初始化完成后，需要创建**参与者 (Participant)** 才能进行 DDS 通信。

### 5.1 调用链

```c
dds_create_participant(domain_id, qos, listener)
  → find_domain(domain_id)
  → dds_participant_register(pp)            // 注册到 DDS 层
  → ddsi_create_participant(gv, qos)        // ★ 核心函数 ★
    → ddsi_new_participant(gv, ...)         // 创建参与者对象
      → ddsi_ensure_special_topic_type(...)  // 确保特殊类型已就绪
      → ddsi_builtin_topic_init_entity(...)  // 初始化内置主题
      → participant_builtin_endpoint_init(...)  // ★ 创建内置端点 ★
        → create_endpoint(SPDP Writer)
        → create_endpoint(SEDP Reader Writer)
        → create_endpoint(SEDP Subscription Writer)
        → create_endpoint(PMD Writer)
    → ddsi_spdp_write(gv, participant)      // ★ 发送第一个 SPDP 消息 ★
```

### 5.2 参与对象结构

```c
struct ddsi_participant {
  struct ddsi_entity e;           // 继承 entity (GUID, 锁, 引用计数)
  ddsi_guid_t e_guid;             // 参与者 GUID (从 ppguid_base 派生)
  dds_qos_t *xqos;                // 参与者 QoS
  struct ddsi_addrset *as_disc;   // 发现地址集 (SPDP 发送给对端的目标)
  struct ddsi_addrset *as_default; // 默认地址集 (用户数据目标)
  uint32_t refc;                  // 引用计数
};
```

**GUID 派生规则**：

```
每个新的 participant:
  guid.prefix = ppguid_base.prefix (相同的前 12 字节)
  guid.entityid.u = ppguid_base.entityid.u + participant_counter
  → 同一 domain 内同一个进程的所有 participant 共享 prefix
  → 只有 entityid 的低 16 位不同 (最多 65535 个参与者)
```

### 5.3 内置端点创建

每个参与者创建时自动创建以下内置端点：

```
本地参与者:
  ├── SPDP Writer (ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER)
  │   → 发送参与者发现消息
  │   → QoS: Best Effort + Transient Local
  │
  ├── SEDP-Builtin Readers Writer (ENTITYID_SEDP_BUILTIN_PUBLICATIONS_SECURE_WRITER)
  │   → 发布本地 Writer 的信息
  │   → QoS: Reliable + Transient Local
  │
  ├── SEDP-Builtin Subscriptions Writer (ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_WRITER)
  │   → 发布本地 Reader 的信息
  │   → QoS: Reliable + Transient Local
  │
  └── PMD Writer (ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_WRITER)
      → 发送参与者间消息 (如 liveliness, secure 消息)
      → QoS: 取决于配置

远端参与者 (通过 SPDP 发现后动态创建):
  ├── Proxy Participant (远端参与者本身)
  ├── Proxy SPDP Reader (接收远端 SPDP Writer 的消息)
  ├── Proxy SEDP Publications Reader
  └── Proxy SEDP Subscriptions Reader
```

### 5.4 第一个 SPDP 消息发送

```c
ddsi_spdp_write(gv, participant)
  │
  ├─ 构建 SPDP 消息载荷:
  │   ├── participant guid
  │   ├── qos (durability, deadline, liveliness, ...)
  │   ├── default_unicast_locator (data_conn_uc)
  │   ├── metatraffic_unicast_locator (disc_conn_uc)
  │   ├── default_multicast_locator (data_conn_mc)
  │   ├── metatraffic_multicast_locator (disc_conn_mc)
  │   ├── available_builtin_endpoints (位掩码)
  │   └── vendor / product version
  │
  ├─ 创建 xmsg:
  │   xmsg = ddsi_xmsg_new(xmsgpool, participant guid, ...)
  │   ddsi_xmsg_setdstmode(xmsg, NN_XMSG_DST_ALL)  // 发给所有订阅者
  │
  ├─ 序列化到 RTPS DATA submessage:
  │   ddsi_serdata_from_sample(serdata, spdp_type, spdp_data)
  │   ddsi_xmsg_add_blob_fragment(xmsg, serdata)
  │
  └─ 发送:
      ddsi_xpack_send(xmsg)
        → 查 addrset (组播 + 单播)
        → for each xlocator:
            ddsi_conn_write(xmit_conns[i], dst_locator, msg)
              → sendmsg()
```

### 5.5 SPDP 消息传播流程

```
本地 Participant 创建
  │
  ├─ ddsi_spdp_write()  — 第一次 SPDP 广播
  │   → 组播: 从每个接口发送 IP 组播 → 同一子网的所有 DDS 节点
  │   → 单播: 如果配置了 InitialPeers → 直接发送到指定地址
  │
  远端 DDS 节点收到 SPDP 消息
  │
  ├─ recv_thread: ddsi_conn_read() → do_packet()
  │   → handle_rtps_message()
  │     → handle_Data()
  │       → handle_SPDP()  ← SPDP 特殊处理!
  │         → 解析 ParticipantBuiltinTopicData
  │         → 放入 builtins_dqueue
  │
  ├─ builtins_dqueue handler:
  │   → ddsi_new_proxy_participant()      // 创建远端参与者代理
  │   → ddsi_lease_register()             // 注册租约监视
  │     → 如果 30 秒内没收到下一个 SPDP
  │       → 标记为死 → 触发清理
  │
  └─ SEDP 交换开始
      → 远端节点的 SEDP Writer 发送 Publication/Subscription 数据
      → 本地节点创建 ProxyReader / ProxyWriter
```

---

## 6. 创建普通 Topic/Writer/Reader

参与者和端点建立后，用户可创建业务实体。

### 6.1 调用链

```c
// 创建 Topic
dds_create_topic(participant, descriptor, name, qos, listener)
  → ddsi_new_topic(gv, participant, name, descriptor)
    → 注册到 entity_index
    → 类型匹配/创建

// 创建 Writer
dds_create_writer(participant, topic, qos, listener)
  → ddsi_create_writer(gv, participant, topic, qos)
    → 分配 EntityID
    → 构造 GUID: participant_guid_prefix + ENTITYID_WRITER_AUTO
    → 初始化 WHC (Write History Cache)
    → 注册到 entity_index
    → ddsi_serdata_new()  ← 创建序列化器
    → ddsi_builtin_topic_write()  ← 发送 SEDP 消息
      → 通知远端 "我创建了一个新的 Writer"

// 创 Reader
dds_create_reader(participant, topic, qos, listener)
  → ddsi_create_reader(gv, participant, topic, qos)
    → 分配 EntityID
    → 构造 GUID: participant_guid_prefix + ENTITYID_READER_AUTO
    → 创建 Reorder Admin (用于消息重排序)
    → 创建 Defrag Admin (用于分片重组)
    → 创建 RHC (Read History Cache)
    → 注册到 entity_index
    → ddsi_builtin_topic_write()  ← 发送 SEDP 消息
      → 通知远端 "我创建了一个新的 Reader"
```

### 6.2 匹配流程 (Matching)

SEDP 发现远端的 Writer/Reader 后触发匹配：

```
收到远端 PublicationData (Writer 信息)
  │
  ├─ 遍历本地所有 Reader:
  │   └─ ddsi_check_proxy_reader_connection(pwr, local_rd)
  │       → GUID 前缀匹配？(同 participant 的 WR-RD 不创建 proxy)
  │       → Topic 名称匹配？
  │       → 类型兼容？
  │       → QoS 兼容？
  │         → Reliability: 一端 Reliable 则全链 Reliable
  │         → History: 取最小值
  │         → Durability: 需满足接收方需求
  │
  └─ 如果匹配成功:
      创建 ProxyWriter (本地 Reader 看到远端 Writer)
      ddsi_set_proxy_writer_reader_match(pwr, rd, ...)
      → 开始发送 ACKNACK (请求历史数据或维持活跃)
```

**关键原则**：RTPS 规范规定 **Reader 端主动管理连接**。Reader 发现 Writer 后，Reader 这一端会创建 ProxyWriter 并负责发送 ACKNACK/Heartbeat 请求。Writer 端创建 ProxyReader 并负责按请求重传。

---

## 7. 完整初始化时序图

```
用户进程启动
  │
  ├─ dds_create_domain(domain_id)
  │   │
  │   ├─ [配置解析] ddsi_config_init_default()
  │   │   或 ddsi_config_from_xml(config_file)
  │   │   → 设置 domain_id, transport, QoS, 接口, 端口等
  │   │
  │   ├─ [Phase 1: ddsi_init()]
  │   │   ├─ 传输工厂初始化 (ddsi_udp_init → 注册函数指针)
  │   │   ├─ 网络接口收集 (getifaddrs → 过滤/排序)
  │   │   ├─ 组播地址配置 (239.255.0.1:7400/7401)
  │   │   ├─ 特殊类型创建 (SPDP/SEDP sertype)
  │   │   ├─ 实体索引创建 (entity_index)
  │   │   ├─ 参与者 GUID 生成 (MD5 + IID)
  │   │   ├─ 单播 Socket 创建 (disc_conn_uc, data_conn_uc)
  │   │   │   → bind(0.0.0.0:7410/7411)
  │   │   ├─ 组播 Socket 创建 (disc_conn_mc, data_conn_mc)
  │   │   │   → SO_REUSEADDR, bind(0.0.0.0:7400/7401)
  │   │   ├─ 发送 Socket 创建 (xmit_conns[0..n_interfaces])
  │   │   │   → bind(intf[i].ip:random)
  │   │   ├─ 加入组播组 (IP_ADD_MEMBERSHIP 每接口)
  │   │   ├─ 队列初始化 (xevents, dqueue, gc, spdp_schedule)
  │   │   └─ 创建租约管理、实体索引等
  │   │
  │   ├─ [Phase 2: ddsi_start()]
  │   │   ├─ GC 线程启动
  │   │   ├─ Builtins 分发线程启动
  │   │   ├─ User 分发线程启动
  │   │   ├─ 事件线程启动 (tev)
  │   │   ├─ 接收线程启动 (recv [+ recvMC, recvUC])
  │   │   ├─ TCP 监听线程启动 (仅 TCP 模式)
  │   │   └─ Debug Monitor 启动 (可选)
  │   │
  │   └─ 返回 domain handle
  │
  ├─ dds_create_participant(domain, qos)
  │   │
  │   ├─ ddsi_create_participant()
  │   │   ├─ 分配 participant GUID
  │   │   ├─ 创建内置端点 (SPDP Writer, SEDP Writer, PMD Writer)
  │   │   └─ ddsi_spdp_write() — 发送第一个 SPDP 广播
  │   │
  │   └─ 返回 participant handle
  │
  ├─ dds_create_topic(participant, "MyTopic", ...)
  │   → 注册主题到 entity_index
  │
  ├─ dds_create_writer(participant, topic, qos)
  │   ├─ 分配 Writer GUID
  │   ├─ 初始化 WHC
  │   └─ ddsi_serwriter_builtins_write() — 发送 SEDP 通知
  │
  ├─ dds_create_reader(participant, topic, qos)
  │   ├─ 分配 Reader GUID
  │   ├─ 创建 Reorder Admin, Defrag Admin, RHC
  │   └─ ddsi_sereader_builtins_write() — 发送 SEDP 通知
  │
  └─ 进入主循环:
       dds_read(reader, samples, max_count, timeout)
         → 从 RHC 读取已投递的数据
         → 或通过 dds_take() 消费 (移入 read cache)
```

---

## 8. 运行时数据流

初始化完成后，系统进入稳定运行状态：

### 8.1 接收侧数据流

```
网卡接收 RTPS 数据包
  │
  ├─ recv_thread: select/poll 检测可读
  │   │
  │   ├─ ddsi_conn_read(conn) → recvmsg() ← 原始 UDP 数据
  │   │
  │   └─ do_packet()
  │       │
  │       └─ handle_rtps_message() ← 解析 RTPS Header
  │           │
  │           └─ 遍历 submessages:
  │               ├── Data      → handle_Data()
  │               │   └── handle_SPDP() / handle_regular()
  │               │       → 放入 dqueue → 投递给 RHC
  │               ├── DataFrag  → handle_DataFrag()
  │               │   └── Defrag → 收集分片 → 完整后 → dqueue
  │               ├── Heartbeat → handle_Heartbeat()
  │               │   └── 检测缺失序列号 → 发送 NACK
  │               ├── ACKNACK   → handle_AckNack()
  │               │   └── 触发 Writer 重传
  │               ├── Gap       → handle_Gap()
  │               │   └── 跳过已丢弃的序列号
  │               ├── InfoTS    → handle_InfoTS()
  │               │   └── 设置源时间戳
  │               └── InfoSRC / InfoDST → handle_InfoSRC()
  │                   └── 设置 GUID Prefix
  │
  └─ ddsi_recv_thread 回到 select/poll
```

### 8.2 发送侧数据流

```
用户调用 dds_write(writer, sample)
  │
  ├─ ddsi_write()
  │   │
  │   ├─ 序列化: ddsi_serdata_from_sample() → CDR 编码
  │   │
  │   ├─ 构建 xmsg: ddsi_xmsg_new()
  │   │   → 添加 RTPS Header
  │   │   → 添加 DATA submessage
  │   │   → 如果需要，添加 InfoTS (源时间戳)
  │   │
  │   ├─ 目标确定:
  │   │   → 查 addrset (包含所有匹配的 ProxyReader)
  │   │   → 每个 ProxyReader → 一个 xlocator (conn + dst_locator)
  │   │
  │   └─ 发送:
  │       └─ ddsi_xpack_send(xmsg)
  │           └─ for each xlocator:
  │               ddsi_conn_write(xlocator.conn, dst, msg)
  │                 └─ sendmsg(sock, dst_locator, data)
  │                     → 内核 → UDP/IP → 网卡 → 网络
  │
  └─ 写入 WHC (Write History Cache)
      → 记录序列号，用于 Heartbeat 和重传
```

### 8.3 Heartbeat-ACKNACK 可靠性流程

```
Writer 端 (可靠模式):
  ┌──────────────────────────────────────────────┐
  │ xevent_thread 定时:                           │
  │   → 每 100ms (或 HeartbeatPeriod) 发 Heartbeat│
  │     包含: lastSeq, firstUnackedSeq           │
  │                                                │
  │   → 收到 ACKNACK:                              │
  │     如果包含 NACK bits → 重传缺失的序列号      │
  │     如果全acked → 从 WHC 中清除旧数据          │
  └──────────────────────────────────────────────┘

Reader 端:
  ┌──────────────────────────────────────────────┐
  │ 收到 Heartbeat:                               │
  │   → 检查 reorder admin 中的序列号连续性       │
  │   → 如果有缺失:                                │
  │     构建 NACK: startSeq + bits 表示缺失哪些   │
  │     发送给 Writer: ddsi_conn_write()          │
  │   → 如果没有缺失:                              │
  │     发送 ACK (all acked)                      │
  │                                                │
  │ 收到重传数据 (Data):                           │
  │   → 填补 reorder admin 中的空洞               │
  │   → 如果变成连续 → 交付给 RHC                 │
  └──────────────────────────────────────────────┘
```

---

## 9. 清理流程：dds_delete()

```c
dds_delete(entity_handle)
  │
  ├─ 如果是 Reader:
  │   └─ ddsi_delete_reader()
  │       → 发送 SEDP dispose+unregister (通知对端 "我退出了")
  │       → 注销所有 ProxyWriter 关联
  │       → 释放 RHC, Reorder Admin
  │       → ddsi_gcreq_request()  → GC 队列延迟释放
  │
  ├─ 如果是 Writer:
  │   └─ ddsi_delete_writer()
  │       → 发送 SEDP dispose+unregister
  │       → 清理所有匹配的 ProxyReader
  │       → 清理 WHC
  │       → ddsi_gcreq_request()  → GC 队列延迟释放
  │
  ├─ 如果是 Participant:
  │   └─ ddsi_delete_participant()
  │       → 先删除该 participant 下所有 Reader/Writer
  │       → 发送 SPDP dispose+unregister
  │       → 释放所有内置端点
  │       → 释放 participant 对象
  │       → ddsi_gcreq_request()  → GC 队列延迟释放
  │
  └─ 如果是 Domain:
      → ddsi_stop() — 触发所有后台线程停止
      → 清除所有参与者、实体
      → ddsi_fini() — 释放所有资源
        → 释放队列、Socket、实体索引
        → 关闭所有线程 join()
        → 释放传输工厂
        → 打印 "Finis."
```

**GC 队列的工作原理**：

```
ddsi_delete_reader()
  → ddsi_gcreq_queue_enqueue(queue, request)
    → GC 线程收到请求
    → 检查实体的引用计数:
      → 如果 refc == 0 → 立即释放
      → 如果 refc > 0  → 放入延迟队列
        → 定期检查 refc → 变为 0 时释放

这个机制解决的核心问题:
  接收线程可能正在处理一个消息链 (fragchain)，
  消息链上的 RDATA 对象引用了 Reader。
  如果此时用户删除了 Reader → 不能立即 free!
  → GC 确保所有引用消失后才真正回收
```

---

## 10. 设计哲学总结

| 设计决策 | 原因 | 类比 |
|---------|------|------|
| **收发分离的 Socket** | 解决 Windows 组播 bug + Fast-RTPS 互操作问题 | IP 路由表的入/出接口分离 |
| **两阶段初始化 (init + start)** | 避免竞态条件，先构建结构再启动线程 | 建造大楼：先建框架，再通电供水 |
| **传输工厂模式** | UDP/TCP/RawEth 共享同一套上层代码 | TCP/IP 协议栈的 socket API 抽象 |
| **预计算路由 (addrset)** | 热路径零开销（每次 write 无需路由选择） | IP 路由表的 FIB (Forwarding Information Base) |
| **两阶段 GC (delete → gcreq → free)** | 解决并发删除和引用持有冲突 | Python/Java 的引用计数+延迟收集 |
| **双分发队列 (builtins + user)** | 隔离控制面和数据面，保证发现可靠性 | 网络设备中的控制/数据平面分离 |
| **租约机制 (lease)** | 容错式存活检测，不依赖网络可达性 | OSPF Hello / TCP Keepalive |
| **引用计组成员关系** | 多个端点共享同一组播订阅，按需添加/移除 | 内核网络栈的 multicast group refcount |
