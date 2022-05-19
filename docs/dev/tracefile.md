# How to read the Cyclone DDS trace file

The "canonical" trace of Cyclone DDS is enabled by setting `Tracing/Category` to `trace` or `Tracing/Verbosity` to `finest`, the two really amount to the same thing because the verbosity settings expand to predefined sets of categories, and `finest` expands to `trace`. It usually contains far too much data and yet often still misses the bits that one is really looking for. This is quite simply the trouble with logging in general. The interpretation of this output of this logging setting is what this document tries to help with.

A general familiarity with the protocol is assumed, a slightly careless (sic!) reading of the [specification](https://www.omg.org/spec/DDSI-RTPS) should provide you with that. Reading it "slightly carelessly" works better than a detailed reading, because the former will give you a good overview of how it works, while the latter will raise more questions than it answers and besides leads you into the lovely land of underspecifications and inconsistencies.

Choosing a less verbose tracing mode simply means some data gets omitted. Therefore, knowing how to make sense of the canonical tracing mode output will also help make sense of the less verbose modes. There are also additional tracing categories that can be enabled, those are use rarely and don’t significantly disturb the interpretation of the normal modes. 

There are some general conventions that are described first, what then follows is a number of excerpts with some explanation.

Finally, if you have `less` available, that's what I would suggest using for perusing these traces. It is certainly possible that other tools support this as well or better, but I haven't found them yet.

## General conventions

The trace consists of roughly the following groups:
* the configuration options
* initialisation of the network stack
* liveliness monitoring and resource usage information
* construction of built-in topic support (this simply happens first)
* normal operation
* shutdown

During normal operation, there is the creation/deletion of entities, discovery work, flow of data and all the control messages, as well as various events related to all of that. If thread liveliness monitoring is enabled, there is also periodic tracing of thread states.

The trace is a sequence of lines, with per-thread line buffering for balancing between logging what happens concurrently and keeping some order. Each line has a standard prefix:
* time stamp (added just before appending the line to the file)
* domain id
* the (truncated) thread name, or a thread id if no name is available

After this follows free-form text. Generally this document will refer to that free-form text when it mentions “the line ...” and simply ignore the existence of the standard prefix.

## Configuration

The configuration is dumped at the very beginning, each line starting with “config:”, then giving the XML element/attribute and its value (these are quite obvious), and then a less-obvious bit between braces at the end of each line. For example, with
```
CYCLONEDDS_URI=<Gen><Net>lo0</Net></Gen>,<Disc><Part>auto</>,<Tr><C>trace</>,<Disc><Part>none</></>
```
it gives:
```
1633098067.140170 [0]    1653091: config: Domain/General/NetworkInterfaceAddress/#text: lo0 {0}
1633098067.140340 [0]    1653091: config: Domain/General/MulticastRecvNetworkInterfaceAddresses/#text: preferred {}
1633098067.140354 [0]    1653091: config: Domain/General/ExternalNetworkAddress/#text: auto {}
...
1633098067.140574 [0]    1653091: config: Domain/Discovery/ParticipantIndex/#text: none {1,3}
...
1633098067.140690 [0]    1653091: config: Domain/Tracing/Category/#text: trace {2}
```
The numbers between the braces list the configuration fragments that contained a value for the setting. An empty pair (`{}`, like for `ExternalNetworkAddress`) here thus means it is at the default. The `lo0` setting for `NetworkInterfaceAddress` comes from the first fragment, the `ParticipantIndex` was set in the 2nd fragment and then overridden in the 4th fragment and the tracing category was set in the 3rd fragment. In most cases, there’s only one source of settings and it really isn’t interesting; if there are more than 32 sources, it’ll only list the first 32.

## Initialisation

Immediately following the configuration it traces the actual starting time stamp:
```
1633098067.142632 [0]    1653091: started at 1633098067.06141453 -- 2021-10-01 16:21:07+02:00
```
and the waking up of the network stack, which really starts with enumerating the interfaces:
```
1633098067.143183 [0]    1653091: interfaces: lo0 udp/127.0.0.1(q1) en0 wireless udp/192.168.1.9(q9)
1633098067.143210 [0]    1653091: selected interfaces: lo0 (index 1)
1633098067.143215 [0]    1653091: presumed robust multicast support, use for everything
```
The interface list gives the names, IP address, the type of network interface and a magic “quality” indication. The highest quality is what it uses by default; if there are multiple at that time, it picks an “arbitrary” one, which really happens to be first. Loopback get a low number, multicast-capable ones a high one.

It then lists the interface(s) it will use, which include the interface “index” that later shows up in all kinds of settings where the internal representation of a locator is printed (e.g., in `udp/127.0.0.1:51437@1`, the `@1` refers to the interface index).

If the network supports multicast, it looks at the type of adapter to decide how reliable the multicast is on this network. A WiFi network is considered to provide “flaky” multicast good enough for initial discovery but not much else, while other network interfaces (e.g., wired networks and loopback interfaces) are presumed to have “robust” multicast. This information is used to select the mode when `General/AllowMulticast` is at the default setting of `default`.

Then follows a relatively self-explanatory bit about sockets, port numbers, addresses, joining multicast groups, and the starting of threads. At this point it also sets up the built-in topic support.

## Liveliness monitoring and resource usage

This just starts at some point, shortly after the various threads get started. It consists of two kinds of lines, one a thread reporting how much CPU time it has consumed so far:
```
1633099505.905137 [0]        tev: thread_cputime 0.000034000
1633099505.905251 [0]     recvMC: thread_cputime 0.000036000
```

The other is a periodic report from the thread liveliness monitoring thread that is created if enabled by the `Internal/LivelinessMonitoring` setting:
```
1633099636.659744 [0]  threadmon:  0(1663713):a:3f0->420 1(gc):a:210->230 2(dq.builtins):a:40->40 3(dq.user):a:0->0 5(tev):a:70->70 6(recv):a:80->80 7(recvMC):a:0->0 8(recvUC):a:0->0 9(ping):a:130->160 10(pong):a:70->80: OK
1633099636.659829 [0]  threadmon: rusage: utime 0.035621000 stime 0.052603000 maxrss 49676288 data 0 vcsw 7 ivcsw 157
```

The second line is standard `getrusage` data: user and system time, maximum RSS size and amount of private memory, and voluntary and involuntary context switches. The first line lists all of the known threads for this domain, whether they are alive or dead and the virtual clock. “OK” at the end means all threads made the expected progress. If it transitions from OK to not-OK and the additionally the `Internal/LivelinessMonitoring[@StackTraces]` setting is enabled (it is by default), it’ll dump stack traces for all these threads to the log.

The details of what constitutes progress and the exact meaning of all these numbers is outside the scope of this document.

## Built-in topic support

This is really just the creation of some special writers (this is just one of them):
```
1633098563.672793 [0]    1656550: new_local_orphan_writer(DCPSParticipant/org::eclipse::cyclonedds::builtin::DCPSParticipant)
1633098563.672821 [0]    1656550:  ref tl_meta tid f1d3ff84-43297732-862df21d-c4e57262 new 0x1094033c0 resolved state 2 refc 1
1633098563.672922 [0]    1656550: WRITER 0:0:0:100c2 QOS={user_data=0<>,topic_name="DCPSParticipant",type_name="org::eclipse::cyclonedds::builtin::DCPSParticipant",topic_data=0<>,group_data=0<>,durability=1,durability_service=0:0:1:-1:-1:-1,deadline=9223372036854775807,latency_budget=0,liveliness=0:9223372036854775807,reliability=1:100000000,lifespan=9223372036854775807,destination_order=0,history=0:1,resource_limits=-1:-1:-1,ownership=0,ownership_strength=0,presentation=1:0:0,partition={"__BUILT-IN PARTITION__"},transport_priority=0,adlink_writer_data_lifecycle=1}
1633098563.672983 [0]    1656550: match_writer_with_readers(wr 0:0:0:100c2) scanning all rds of topic DCPSParticipant
```
The “orphan” in `new_local_orphan_writer` is because these writers are not attached to any DomainParticipant, nor to a Publisher entity. They just exist without having a parent. “Orphan” doesn’t quite capture the notion of something that never had a parent to begin with, but it is close enough.

In this: `DCPSParticipant/org::eclipse::cyclonedds::builtin::DCPSParticipant` is the topic name and the type name. The next line is related to type discovery, which this document ignores (for now, anyway).

The line starting with `WRITER`, like _all_ lines starting with `WRITER`, denotes the creation of a local DDSI-level writer entity. These typically are advertised on the wire, but the orphan ones are not. These lines give the GUID and the QoS settings with all the booleans, enumerated types and durations values as their numerical value. For example, “history=0:1” means a history kind = 0 = KEEP_LAST, history depth = 1. The magic number 9223372036854775807 = 2^63-1 represents infinity. Thus, given a trace, it is straightforward to find out all the QoS settings involved.

Finally `match_writer_with_readers` is where it tries to match it with readers, but this early in the start up sequence, there cannot be any readers.

## Regular operation

Regular operation covers pretty much everything that follows. This can be subdivided in a number of different kinds of activities:
* participant creation/deletion
* topic creation/deletion
* reader/writer creation/deletion
* discovery of the above from remote participants
* transmitting application data
* receiving application data
* operation of the reliable protocol
* (and possibly some more)

Overall, the work is split between:
* receive threads (starting with `recv`)
* timed-event handling (starting with `tev`)
* delivery-queue threads (starting with `dq.`)

A receive thread handles input from one or more sockets. It traces the received packets in a decoded form, performs all protocol processing and either delivers the data directly or hands it off to a delivery queue for further processing. All built-in/discovery data is handled by `dq.builtin`.

For each received message, the header and all individual submessages are traced, with the submessage names in capitals, e.g.:
```
1633275476.905567 [0]     recvUC: HDR(110b5a6:26340b11:62ec0f10 vendor 1.16) len 64 from udp/127.0.0.1:59598
1633275476.905574 [0]     recvUC: INFODST(1108c80:a5761207:e031470)
1633275476.905589 [0]     recvUC: ACKNACK(F#2:14/0: L(:1c1 853123.561141) 110b5a6:26340b11:62ec0f10:d07 -> 1108c80:a5761207:e031470:e02 ACK1 RM1 setting-has-replied-to-hb happy-now)
1633275476.905720 [0]     recvUC: HDR(110b5a6:26340b11:62ec0f10 vendor 1.16) len 104 from udp/127.0.0.1:59598
1633275476.905727 [0]     recvUC: INFOTS(1633275476.888706001)
1633275476.905737 [0]     recvUC: DATA(110b5a6:26340b11:62ec0f10:1102 -> 0:0:0:0 #1 L(:1c1 853123.561294) => EVERYONE
1633275476.905759 [0]     recvUC: data(application, vendor 1.16): 110b5a6:26340b11:62ec0f10:1102 #1: ST0 DDSPerfRPongKS/KeyedSeq:{13,0,{}})
1633275476.905771 [0]     recvUC: HEARTBEAT(#2:1..1 110b5a6:26340b11:62ec0f10:1102 -> 0:0:0:0: 1108c80:a5761207:e031470:1007@1(sync))
```
This is the receipt of two separate packets, one consisting of an RTPS message header, an INFODST submessage and an ACKNACK one; the other consisting of a header, an INFOTS and DATA submessage carrying an application sample with its source timestamp, and finally a piggy-packed HEARTBEAT message.

The format of each submessage is roughly:
* `HDR`: RTPS message header with source GUID prefix, vendor code, packet length and source address
* `INFODST`: destination GUID prefix
* `INFOTS`: timestamp
* `DATA`, `DATAFRAG`: writer GUID, reader GUID, sequence number (in the case of a datafrag: extended with the range of fragment numbers in the submessage). If receiving this submessage results in delivering data, some additional output follows
* `GAP`: similar to `DATA` as it encodes the absence of data
* `ACKNACK`:
  * `F`: final flag set
  * `#2`: ACKNACK "count" field
  * `14/0:` "bitmap base" (i.e., it acks sequence numbers < 14 in this case), length and bitmap following the colon
  * reader GUID `->` writer GUID
  * followed by some interpretation: retransmits, how many additional message have been acknowledged, how many were dropped form the WHC, some flags
  * pre-emptive ACKNACKs are published with count = 0, bitmap base = 1 and bitmap length = 0
* `HEARTBEAT`:
  * `F`: final flag set
  * `#2`: HEARTBEAT "count" field
  * `a..b`: oldest sequence number available in WHC, highest sequence number published; b = a-1 is used to indicate an empty one
  * followed by how it treats this for the matching readers
* `NACKFRAG`, `HEARTBEATFRAG`: similar to `ACKNACK` and `HEARTBEAT`

For all of these, if a GUID is unknown, it appends `?` to the GUID and ignores the message. In some cases it will ignore messages from known GUIDs, in which case the GUID is in brackets.

### Participant creation/deletion

There is not all that much specifically for creating the participant:
```
1633099629.586785 [0]    1663713: PARTICIPANT 110d959:b723e9bf:b981aa83:1c1 QOS={user_data=31<"DDSPerf:0:54744:phantasie.local">,adlink_entity_factory=0}
```
The bulk of the output generated when creating a participant is actually the creation of readers/writers for the built-in endpoints implementing the “simple endpoint discovery protocol” (SEDP) used to advertise this participant and its endpoints to remote participants and to discover remote endpoints. Depending on the configuration settings some or all of these may be omitted.

The built-in endpoints all have fixed entity ids, defined by the spec. The tracing for creating these writers is pretty much the same as it is for the “orphan” writers for locally publishing discovery data, described above. Naturally, there is an owning participant and some bookkeeping output because of that. At the time a participant is created, participants may have been discovered already and the “match\_...” lines may result in matching the local endpoints with remote ones.
```
1633099629.586825 [0]    1663713: new_writer(guid 110d959:b723e9bf:b981aa83:100c2, (null).DCPSParticipant/ParticipantBuiltinTopicData)
1633099629.586837 [0]    1663713: ref_participant(110d959:b723e9bf:b981aa83:1c1 @ 0x109401e40 <- 110d959:b723e9bf:b981aa83:100c2 @ 0x109002a84) user 1 builtin 1
1633099629.586848 [0]    1663713:  ref tl_meta tid 4325373c-93284edd-34f7ae87-4eb99fc1 new 0x107603200 resolved state 2 refc 1
1633099629.586890 [0]    1663713: WRITER 110d959:b723e9bf:b981aa83:100c2 QOS={user_data=0<>,topic_name="DCPSParticipant",type_name="ParticipantBuiltinTopicData",topic_data=0<>,group_data=0<>,durability=1,durability_service=0:0:1:-1:-1:-1,deadline=9223372036854775807,latency_budget=0,liveliness=0:9223372036854775807,reliability=0:0,lifespan=9223372036854775807,destination_order=0,history=0:1,resource_limits=-1:-1:-1,ownership=0,ownership_strength=0,presentation=0:0:0,partition={},time_based_filter=0,transport_priority=0,type_consistency_enforcement=1:00000,adlink_reader_lifespan=0:9223372036854775807,adlink_writer_data_lifecycle=1,adlink_reader_data_lifecycle=9223372036854775807:9223372036854775807,adlink_subscription_keys=0:{}}
1633099629.586906 [0]    1663713: match_writer_with_proxy_readers(wr 110d959:b723e9bf:b981aa83:100c2) scanning proxy participants tgt=0
...
```
Once the participant entity and the local discovery endpoints have been created, the “simple participant discovery protocol” (SPDP) sample describing the participant (including advertising which discovery endpoint exist for this participant) is published:
```
1633099629.587660 [0]    1663713: write_sample 0:0:0:100c2 #1: ST0 DCPSParticipant/org::eclipse::cyclonedds::builtin::DCPSParticipant:(blob)
1633099629.587667 [0]    1663713:  => EVERYONE
1633099629.587674 [0]    1663713: spdp_write(110d959:b723e9bf:b981aa83:1c1)
1633099629.587687 [0]    1663713: spdp_write(110d959:b723e9bf:b981aa83:1c1) - internals: phantasie.local/0.8.0/Darwin/Darwin
1633099629.587769 [0]    1663713: write_sample 110d959:b723e9bf:b981aa83:100c2 #1: ST0 DCPSParticipant/ParticipantBuiltinTopicData:{user_data=31<"DDSPerf:0:54744:phantasie.local">,protocol_version=2:1,vendorid=1:16,participant_lease_duration=10000000000,participant_guid={110d959:b723e9bf:b981aa83:1c1},builtin_endpoint_set=64575,domain_id=0,default_unicast_locator={udp/127.0.0.1:51108},default_multicast_locator={udp/239.255.0.1:7401},metatraffic_unicast_locator={udp/127.0.0.1:51108},metatraffic_multicast_locator={udp/239.255.0.1:7400},adlink_participant_version_info=0:44:0:0:0:"phantasie.local/0.8.0/Darwin/Darwin",cyclone_receive_buffer_size=1048576}
...
1633099629.587895 [0]        tev: xpack_addmsg 0x108707b40 0x108703740 0(data(0:0:0:0:#0/1)): niov 0 sz 0 => now niov 3 sz 352
1633099629.587932 [0]        tev: nn_xpack_send 352: 0x108707b4c:20 0x1092035d8:36 0x1094006a4:296 [ udp/239.255.0.1:7400@1 ]
1633099629.587937 [0]        tev: traffic-xmit (1) 352
```
The discovery data is all sent via the message queue handled by the `tev` thread, and around 400 bytes is fairly typical for an SPDP message.  The `nn_xpack_send` line is the point where it actually passes a datagram to the network, it enumerates the addresses it gets sent to between the square brackets (it also tends to not use square brackets for anything else, so searching for `\[ [^]]` is a way of quickly finding points where data is actually sent out).

From then on, the SPDP message is resent periodically:
```
1633099629.693013 [0]        tev: xmit spdp 110d959:b723e9bf:b981aa83:1c1 to 0:0:0:100c7 (resched 8s)
1633099629.693032 [0]        tev: xpack_addmsg 0x108707b40 0x108703740 0(data(0:0:0:0:#0/1)): niov 0 sz 0 => now niov 3 sz 352
1633099629.693095 [0]        tev: nn_xpack_send 352: 0x108707b4c:20 0x1092035d8:36 0x1094006a4:296 [ udp/239.255.0.1:7400@1 ]
1633099629.693102 [0]        tev: traffic-xmit (1) 352
```

Deleting a participant is logged, but only once all its application readers and writers have been deleted (traces for which are described below). Furthermore the actual cleaning up is done by the `gc` thread, so the `delete_participant` here only schedules the deleting and publishes the dispose/unregister message. The message makes it out in parallel to tearing down the writer.
```
1633099642.806324 [0]    1663713: delete_participant(110d959:b723e9bf:b981aa83:1c1)
1633099642.806339 [0]         gc: gc 0x10883ed80: deleting
1633099642.806353 [0]    1663713: write_sample 0:0:0:100c2 #2: ST3 DCPSParticipant/org::eclipse::cyclonedds::builtin::DCPSParticipant:(blob)
...
1633099642.806747 [0]         gc: gc_delete_participant(0x10883de80, 110d959:b723e9bf:b981aa83:1c1)
...
1633099642.806907 [0]        tev: xpack_addmsg 0x108707b40 0x10870b940 0(data(0:0:0:0:#0/1)): niov 0 sz 0 => now niov 3 sz 96
1633099642.806910 [0]         gc: writer_set_state(110d959:b723e9bf:b981aa83:100c2) state transition 0 -> 3
...
1633099642.806963 [0]        tev: nn_xpack_send 96: 0x108707b4c:20 0x1092073b8:48 0x1079086e4:28 [ udp/239.255.0.1:7400@1 ]
1633099642.806992 [0]        tev: traffic-xmit (1) 96
```

What follows is tearing down the discovery endpoints. It simply tries to delete all possibly existing built-in endpoints, regardless of whether they actually exist. For example, if DDS Security is not used, various endpoints don't exist, and these show up in the trace as:
```
1633099642.807321 [0]         gc: delete_writer_nolinger(guid 110d959:b723e9bf:b981aa83:ff0003c2) - unknown guid
```
this is harmless.

### Topic creation/deletion

TBD

### Reader/writer creation/deletion

Readers and writers are handled analogously. While there are some differences they can mostly both be simply considered as _endpoints_. One notable difference is that a writer can have unacknowledged data in its writer history cache when the application tries to delete it, In that case, the `LingerDuration` setting applies. This shows up in the trace as some additional state transitions.

For example, the creation of a reader for a built-in topic (`DCPSPublication` in this case): these are a bit special in that they never match a remote writer, but they do always match an existing writer:
```
1633099629.589583 [0]    1663713: new_reader(guid 110d959:b723e9bf:b981aa83:a07, __BUILT-IN PARTITION__.DCPSPublication/org::eclipse::cyclonedds::builtin::DCPSPublication)
1633099629.589597 [0]    1663713: ref_participant(110d959:b723e9bf:b981aa83:1c1 @ 0x109401e40 <- 110d959:b723e9bf:b981aa83:a07 @ 0x108d02dc4) user 8 builtin 15
1633099629.589609 [0]    1663713:  ref tl_meta tid edcfae98-9540fd42-e4b8556d-5b723bb6 state 2 refc 3
1633099629.589648 [0]    1663713: READER 110d959:b723e9bf:b981aa83:a07 QOS={user_data=0<>,topic_name="DCPSPublication",type_name="org::eclipse::cyclonedds::builtin::DCPSPublication",topic_data=0<>,group_data=0<>,durability=1,durability_service=0:0:1:-1:-1:-1,deadline=9223372036854775807,latency_budget=0,liveliness=0:9223372036854775807,reliability=1:100000000,lifespan=9223372036854775807,destination_order=0,history=0:1,resource_limits=-1:-1:-1,ownership=0,presentation=1:0:0,partition={"__BUILT-IN PARTITION__"},time_based_filter=0,transport_priority=0,type_consistency_enforcement=1:00000,adlink_entity_factory=1,adlink_reader_lifespan=0:9223372036854775807,adlink_reader_data_lifecycle=9223372036854775807:9223372036854775807,adlink_subscription_keys=0:{}}
1633099629.589670 [0]    1663713: write_sample 0:0:0:4c2 #3: ST0 DCPSSubscription/org::eclipse::cyclonedds::builtin::DCPSSubscription:(blob)
1633099629.589676 [0]    1663713:  => EVERYONE
1633099629.589685 [0]    1663713: match_reader_with_proxy_writers(rd 110d959:b723e9bf:b981aa83:a07) scanning all pwrs of topic DCPSPublication
1633099629.589691 [0]    1663713: match_reader_with_writers(rd 110d959:b723e9bf:b981aa83:a07) scanning all wrs of topic DCPSPublication
1633099629.589700 [0]    1663713:   reader_add_local_connection(wr 0:0:0:3c2 rd 110d959:b723e9bf:b981aa83:a07)
1633099629.589716 [0]    1663713:   writer_add_local_connection(wr 0:0:0:3c2 rd 110d959:b723e9bf:b981aa83:a07)
```

The second line (`ref_participant`) tracks the manipulation of the participant reference count for its endpoints. The user/application-created endpoints are counted separately from the built-in endpoints used for discovery. 

The `write_sample` line is the publication of the built-in topic sample describing the new reader, whichis delivered to all local subscribers to the (in this case) `DCPSSubscription` topic. The `=> EVERYONE` is mostly uninteresting, it says the sample is delivered to all local readers.

The `match_reader_with_proxy_writers` fails to find any remote writers (necessarily, because this is a built-in topic); the `match_reader_with_writers` for one of these built-in topics invariably finds the correspond orphan writer created at the very beginning. If a matching endpoint is found (for whichever combination of remote and local endpoints) these `match_...` lines are followed by a pair of lines stating that "connections" get added.

An endpoint for an "ordinary" topic is much the same:
```
1633099629.590741 [0]    1663713: READER 110d959:b723e9bf:b981aa83:1007 QOS={user_data=0<>,topic_name="DDSPerfRPongKS",type_name="KeyedSeq",topic_data=0<>,group_data=0<>,durability=0,deadline=9223372036854775807,latency_budget=0,liveliness=0:9223372036854775807,reliability=1:10000000000,destination_order=0,history=1:1,resource_limits=10000:-1:-1,ownership=0,presentation=0:0:0,partition={"0110d959_b723e9bf_b981aa83_000001c1"},time_based_filter=0,transport_priority=0,type_consistency_enforcement=1:00000,adlink_entity_factory=1,adlink_reader_lifespan=0:9223372036854775807,adlink_reader_data_lifecycle=9223372036854775807:9223372036854775807,adlink_subscription_keys=0:{}}
1633099629.590759 [0]    1663713: write_sample 0:0:0:4c2 #5: ST0 DCPSSubscription/org::eclipse::cyclonedds::builtin::DCPSSubscription:(blob)
1633099629.590762 [0]    1663713:  => EVERYONE
1633099629.590768 [0]    1663713: match_reader_with_proxy_writers(rd 110d959:b723e9bf:b981aa83:1007) scanning all pwrs of topic DDSPerfRPongKS
1633099629.590773 [0]    1663713: match_reader_with_writers(rd 110d959:b723e9bf:b981aa83:1007) scanning all wrs of topic DDSPerfRPongKS
1633099629.590784 [0]    1663713:   reader_add_local_connection(wr 110d959:b723e9bf:b981aa83:b02 rd 110d959:b723e9bf:b981aa83:1007)
1633099629.590802 [0]    1663713:   writer_add_local_connection(wr 110d959:b723e9bf:b981aa83:b02 rd 110d959:b723e9bf:b981aa83:1007)
1633099629.590856 [0]    1663713: write_sample 110d959:b723e9bf:b981aa83:4c2 #2: ST0 DCPSSubscription/SubscriptionBuiltinTopicData:{topic_name="DDSPerfRPongKS",type_name="KeyedSeq",reliability=1:10000000000,history=1:1,resource_limits=10000:-1:-1,partition={"0110d959_b723e9bf_b981aa83_000001c1"},protocol_version=2:1,vendorid=1:16,endpoint_guid={110d959:b723e9bf:b981aa83:1007},adlink_entity_factory=1,cyclone_type_information=16<7,231,236,169,182,32,130,103,43,251,65,195,65,40,113,160>}
...
```
It adds the publication of the SEDP message to inform the peers of the existence of the new endpoint. This then gets pushed out to all readers via the `tev` thread, much like the SPDP messages. However, where the SPDP messages are always sent regardless of the existence of readers, the SEDP messages are only sent if there are matching readers. In the trace I used, there weren't any at this point in time. SEDP uses transient-local endpoints and the message here is stored in the writer history cache and will be regurgitated when a remote reader requests it. This uses the same mechanisms as regular reliable communication (i.e., `HEARTBEAT` and `ACKNACK` messages).

### Participant discovery

Received SPDP messages are queued for processing by the `dq.builtins` thread, which then either updates an existing "proxy participant" entity for that GUID, or it creates a new one. E.g. receipt of the message on one of the receive threads:
```
1633275476.877215 [0]       recv: HDR(110b5a6:26340b11:62ec0f10 vendor 1.16) len 352 from udp/127.0.0.1:59598
1633275476.877246 [0]       recv: INFOTS(1633275476.873726000)
1633275476.877263 [0]       recv: DATA(110b5a6:26340b11:62ec0f10:100c2 -> 0:0:0:0 #1)
```
followed by the processing on the `dq.builtin` thread, first what it received, like it does for any sample, followed by the trace line output on creation of a new proxy participant (when searching in the file, look for `SPDP.*NEW`):
```
1633275476.877316 [0] dq.builtin: data(builtin, vendor 1.16): 0:0:0:0 #1: ST0 /ParticipantBuiltinTopicData:{user_data=31<"DDSPerf:0:88068:phantasie.local">,protocol_version=2:1,vendorid=1:16,participant_lease_duration=10000000000,participant_guid={110b5a6:26340b11:62ec0f10:1c1},builtin_endpoint_set=64575,domain_id=0,default_unicast_locator={udp/127.0.0.1:63567},default_multicast_locator={udp/239.255.0.1:7401},metatraffic_unicast_locator={udp/127.0.0.1:63567},metatraffic_multicast_locator={udp/239.255.0.1:7400},adlink_participant_version_info=0:44:0:0:0:"phantasie.local/0.8.0/Darwin/Darwin",cyclone_receive_buffer_size=1048576}
1633275476.877389 [0] dq.builtin: SPDP ST0 110b5a6:26340b11:62ec0f10:1c1 bes fc3f NEW (0x00000000-0x0000002c-0x00000000-0x00000000-0x00000000 phantasie.local/0.8.0/Darwin/Darwin) (data udp/239.255.0.1:7401@1 udp/127.0.0.1:63567@1 meta udp/239.255.0.1:7400@1 udp/127.0.0.1:63567@1) QOS={user_data=31<"DDSPerf:0:88068:phantasie.local">}
1633275476.877412 [0] dq.builtin: lease_new(tdur 10000000000 guid 110b5a6:26340b11:62ec0f10:1c1) @ 0x10ae05370
1633275476.877422 [0] dq.builtin: lease_new(tdur 10000000000 guid 110b5a6:26340b11:62ec0f10:1c1) @ 0x10ae052c0
```
The `ST0` is the "statusinfo" field of the DDSI DATA submessage, and 0 means it is an ordinary write. Don't mind the details of the `lease_new` bits. Dealing with the various automatic and manual liveliness modes of DDS happens to result in creating two independent lease objects for each proxy participant. The automatic one gets renewed whenever a message arrives from that proxy participant, the manual one when a messages arrives that proves application activity.

The `bes` field is the "built-in endpoint set", the set of built-in endpoints used for discovery. What follows is simply the creation of the proxy readers and proxy writers specified in this set. Whereas application readers/writers look for matches based on the topic, for the built-in endpoints, it looks for specific GUIDs, hence the slightly different "scanning" note:
```
1633275476.877510 [0] dq.builtin: match_proxy_reader_with_writers(prd 110b5a6:26340b11:62ec0f10:100c7) scanning participants tgt=0
1633275476.877535 [0] dq.builtin: match_proxy_writer_with_readers(pwr 110b5a6:26340b11:62ec0f10:3c2) scanning participants tgt=3c7
1633275476.877564 [0] dq.builtin:   reader 1108c80:a5761207:e031470:3c7 init_acknack_count = 1
1633275476.877571 [0] dq.builtin:   reader_add_connection(pwr 110b5a6:26340b11:62ec0f10:3c2 rd 1108c80:a5761207:e031470:3c7)
1633275476.877592 [0] dq.builtin:   proxy_writer_add_connection(pwr 110b5a6:26340b11:62ec0f10:3c2 rd 1108c80:a5761207:e031470:3c7) - out-of-sync
1633275476.877610 [0] dq.builtin: match_proxy_reader_with_writers(prd 110b5a6:26340b11:62ec0f10:3c7) scanning participants tgt=3c2
1633275476.877639 [0] dq.builtin:   proxy_reader_add_connection(wr 1108c80:a5761207:e031470:3c2 prd 110b5a6:26340b11:62ec0f10:3c7)
1633275476.877652 [0] dq.builtin:   writer_add_connection(wr 1108c80:a5761207:e031470:3c2 prd 110b5a6:26340b11:62ec0f10:3c7) - ack seq 4
```
The "out-of-sync" and "ack seq 4" refer to the relationship state of the local reader compared to the newly discovered proxy writer, and the sequence number sent most recently by this writer (which determines the starting point for reliability for volatile readers).

Whenever a writer matches a proxy reader, it has to recompute its "address set", the set of addresses to which it needs to sent messages intended to reach all matched proxy readers:
```
1633275476.877666 [0] dq.builtin: setcover: all_addrs udp/239.255.0.1:7400@1 udp/127.0.0.1:63567@1
1633275476.877690 [0] dq.builtin: reduced nlocs=2
1633275476.877701 [0] dq.builtin: nloopback = 2, nlocs = 2, redundant_networking = 0
1633275476.877714 [0] dq.builtin: rdidx 0 lidx udp/239.255.0.1:7400@1 1 -> c
1633275476.877720 [0] dq.builtin: rdidx 0 lidx udp/127.0.0.1:63567@1 0 -> 4
1633275476.877732 [0] dq.builtin:                                                                   a
1633275476.877739 [0] dq.builtin:   loc  0 = udp/127.0.0.1:63567@1                             1 { +ul }
1633275476.877746 [0] dq.builtin:   loc  1 = udp/239.255.0.1:7400@1                            2 { +1l }
1633275476.877751 [0] dq.builtin:   best = 0
1633275476.877756 [0] dq.builtin:   simple udp/127.0.0.1:63567@1
1633275476.877771 [0] dq.builtin: rebuild_writer_addrset(1108c80:a5761207:e031470:3c2): udp/127.0.0.1:63567@1 (burst size 4294901760 rexmit 699051)
...
1633275476.878381 [0] dq.builtin: write_sample 0:0:0:100c2 #2: ST0 DCPSParticipant/org::eclipse::cyclonedds::builtin::DCPSParticipant:(blob)
1633275476.878387 [0] dq.builtin:  => EVERYONE
```
The final line (`rebuild_writer_addrset`) lists the addresses that it will use, all that precedes it is merely info for how it decided upon that set of addresses. In this case, it ends up with a single unicast address on the loopback interface because there is only one peer.

Once all the discovery-related proxy endpoints have been created, it uses one of the local orphan writers to publish a DCPSParticipant sample for any longer subscribers to that topic. This is no different than what it does for local entities.

When the remote participant is deleted, ordinarily one receives a message to this effect: a dispose+unregister of the SPDP instance:
```
1633275479.312918 [0] dq.builtin: SPDP ST3 110b5a6:26340b11:62ec0f10:1c1delete_proxy_participant_by_guid(110b5a6:26340b11:62ec0f10:1c1) - deleting
1633275479.312945 [0] dq.builtin: write_sample 0:0:0:100c2 #3: ST3 DCPSParticipant/org::eclipse::cyclonedds::builtin::DCPSParticipant:(blob)
1633275479.312958 [0] dq.builtin:  => EVERYONE
1633275479.313040 [0] dq.builtin: delete_ppt(110b5a6:26340b11:62ec0f10:1c1) - deleting dependent proxy participants
1633275479.313055 [0] dq.builtin: delete_ppt(110b5a6:26340b11:62ec0f10:1c1) - deleting endpoints
1633275479.313069 [0] dq.builtin: delete_proxy_reader (110b5a6:26340b11:62ec0f10:301c4) - deleting
1633275479.313094 [0] dq.builtin: delete_proxy_writer (110b5a6:26340b11:62ec0f10:301c3) - deleting
1633275479.313114 [0] dq.builtin: delete_proxy_reader (110b5a6:26340b11:62ec0f10:300c4) - deleting
1633275479.313146 [0] dq.builtin: delete_proxy_writer (110b5a6:26340b11:62ec0f10:300c3) - deleting
1633275479.313163 [0] dq.builtin: delete_proxy_reader (110b5a6:26340b11:62ec0f10:200c7) - deleting
1633275479.313182 [0] dq.builtin: delete_proxy_writer (110b5a6:26340b11:62ec0f10:200c2) - deleting
1633275479.313198 [0] dq.builtin: delete_proxy_reader (110b5a6:26340b11:62ec0f10:4c7) - deleting
1633275479.313213 [0] dq.builtin: delete_proxy_writer (110b5a6:26340b11:62ec0f10:4c2) - deleting
1633275479.313228 [0] dq.builtin: delete_proxy_reader (110b5a6:26340b11:62ec0f10:3c7) - deleting
1633275479.313242 [0] dq.builtin: delete_proxy_writer (110b5a6:26340b11:62ec0f10:3c2) - deleting
1633275479.313256 [0] dq.builtin: delete_proxy_reader (110b5a6:26340b11:62ec0f10:100c7) - deleting
1633275479.313271 [0] dq.builtin:  delete
```
The `ST3` means "statusinfo" 3, which translates to dispose+unregister. What then follows is the disposing+unregistering of the corresponding locally published DCPSParticipant instance and scheduling the deletion of all remaining proxy readers/writers.

Another way in which the proxy participant can be removed is because its lease expires:
```
1633276790.299371 [0]         gc: lease expired: l 0x106e052c0 guid 1107d2b:52f18fdc:c0e27016:1c1 tend 854426955565583 < now 854426956460791
1633276790.299410 [0]         gc: delete_proxy_participant_by_guid(1107d2b:52f18fdc:c0e27016:1c1) - deleting
1633276790.299526 [0]         gc: write_sample 0:0:0:100c2 #3: ST3 DCPSParticipant/org::eclipse::cyclonedds::builtin::DCPSParticipant:(blob)
1633276790.299569 [0]         gc:  => EVERYONE
1633276790.299759 [0]         gc: delete_ppt(1107d2b:52f18fdc:c0e27016:1c1) - deleting dependent proxy participants
1633276790.299796 [0]         gc: delete_ppt(1107d2b:52f18fdc:c0e27016:1c1) - deleting endpoints
```
The only difference in the procedure is the way it is triggered. Once you get to the `delete_ppt` there is no difference between the two paths.

### Topic discovery

TBD

### Endpoint discovery

Similarly to SPDP processing, all endpoint discovery processing is done by the `dq.builtin` thread.
```
1633275476.882374 [0] dq.builtin: data(builtin, vendor 1.16): 110b5a6:26340b11:62ec0f10:3c2 #3: ST0 DCPSPublication/PublicationBuiltinTopicData:{topic_name="DDSPerfRPingKS",type_name="KeyedSeq",reliability=1:10000000000,protocol_version=2:1,vendorid=1:16,endpoint_guid={110b5a6:26340b11:62ec0f10:e02},adlink_entity_factory=1,cyclone_type_information=16<7,231,236,169,182,32,130,103,43,251,65,195,65,40,113,160>}
1633275476.882422 [0] dq.builtin: SEDP ST0 110b5a6:26340b11:62ec0f10:e02 reliable volatile writer: (default).DDSPerfRPingKS/KeyedSeq type-hash 07e7eca9-b6208267-2bfb41c3-412871a0 p(open) NEW (as udp/239.255.0.1:7401@1 udp/127.0.0.1:63567@1 ssm=0) QOS={user_data=0<>,topic_name="DDSPerfRPingKS",type_name="KeyedSeq",topic_data=0<>,group_data=0<>,durability=0,durability_service=0:0:1:-1:-1:-1,deadline=9223372036854775807,latency_budget=0,liveliness=0:9223372036854775807,reliability=1:10000000000,lifespan=9223372036854775807,destination_order=0,history=0:1,resource_limits=-1:-1:-1,ownership=0,ownership_strength=0,presentation=0:0:0,partition={},transport_priority=0,adlink_entity_factory=1,adlink_writer_data_lifecycle=1,cyclone_type_information=16<7,231,236,169,182,32,130,103,43,251,65,195,65,40,113,160>}
1633275476.882438 [0] dq.builtin:  ref tl_meta tid 07e7eca9-b6208267-2bfb41c3-412871a0 add ep 110b5a6:26340b11:62ec0f10:e02 state 2 refc 11
1633275476.882454 [0] dq.builtin: write_sample 0:0:0:3c2 #8: ST0 DCPSPublication/org::eclipse::cyclonedds::builtin::DCPSPublication:(blob)
1633275476.882459 [0] dq.builtin:  => EVERYONE
1633275476.882466 [0] dq.builtin: match_proxy_writer_with_readers(pwr 110b5a6:26340b11:62ec0f10:e02) scanning all rds of topic DDSPerfRPingKS
1633275476.882484 [0] dq.builtin:   reader 1108c80:a5761207:e031470:d07 init_acknack_count = 1
1633275476.882490 [0] dq.builtin:   reader_add_connection(pwr 110b5a6:26340b11:62ec0f10:e02 rd 1108c80:a5761207:e031470:d07)
1633275476.882519 [0] dq.builtin:   proxy_writer_add_connection(pwr 110b5a6:26340b11:62ec0f10:e02 rd 1108c80:a5761207:e031470:d07) - out-of-sync
```
The thing to look for for endpoint discovery is `SEDP.*NEW`, otherwise this is just the same as the above. The same holds for the deleting of proxy endpoints:
```
1633275479.308720 [0] dq.builtin: data(builtin, vendor 1.16): 110b5a6:26340b11:62ec0f10:3c2 #7: ST3 DCPSPublication/PublicationBuiltinTopicData:{endpoint_guid={110b5a6:26340b11:62ec0f10:e02}}
1633275479.308747 [0] dq.builtin: SEDP ST3 110b5a6:26340b11:62ec0f10:e02 delete_proxy_writer (110b5a6:26340b11:62ec0f10:e02) - deleting
1633275479.308791 [0] dq.builtin: write_sample 0:0:0:3c2 #12: ST3 DCPSPublication/org::eclipse::cyclonedds::builtin::DCPSPublication:(blob)
1633275479.308848 [0] dq.builtin:  => EVERYONE
1633275479.308902 [0] dq.builtin: unref tl_meta tid 07e7eca9-b6208267-2bfb41c3-412871a0 remove ep 110b5a6:26340b11:62ec0f10:e02
1633275479.308928 [0] dq.builtin:   delete
```

### Transmitting application data

Data sent by the local process is traced in lines starting with `write_sample`, giving the writer GUID, sequence number, statusinfo (again, 0 is write, 1 is dispose, 2 is unregister and 3 is dispose+unregister), as well as the topic name, type name and content (if not disabled in the tracing categories, e.g. by adding `-content` to the `Tracing/Category` setting):
```
1633275476.888712 [0]    2123463: write_sample 1108c80:a5761207:e031470:e02 #13: ST0 DDSPerfRPingKS/KeyedSeq:{13,0,{}}
```
If there are no readers, that's pretty much the end of it; if there are remote readers the thread continues with something like:
```
1633275476.888732 [0]    2123463: xpack_addmsg 0x10a702440 0x10a701f40 0(data(1108c80:a5761207:e031470:e02:#13/1)): niov 0 sz 0 => now niov 3 sz 72
1633275476.888742 [0]    2123463: writer_hbcontrol: wr 1108c80:a5761207:e031470:e02 multicasting (rel-prd 1 seq-eq-max 0 seq 13 maxseq 12)
1633275476.888752 [0]    2123463: heartbeat(wr 1108c80:a5761207:e031470:e02) piggybacked, resched in 0.0955827 s (min-ack 12!, avail-seq 13, xmit 12)
1633275476.888759 [0]    2123463: xpack_addmsg 0x10a702440 0x10a701e40 0(control): niov 3 sz 72 => now niov 4 sz 104
1633275476.888774 [0]    2123463: nn_xpack_send 104: 0x10a70244c:20 0x10b202228:36 0x10a1010fc:16 0x10b202138:32 [ udp/127.0.0.1:63567@1 ]
1633275476.888780 [0]    2123463: traffic-xmit (1) 104
```
And it is also delivered to all matching local readers:
```
1633275476.888785 [0]    2123463:  => EVERYONE
```

The `xpack_addmsg` and `nn_xpack_send` are the same as elsewhere, it is simply constructing a message and sending it out. The `writer_hbcontrol` line concerns updating the writer heartbeat state, and in this case decides to "multicast" a heartbeat to all readers (whether it actually is a multicast at the network level is a separate matter). That heartbeat is piggybacked onto the DATA submessage. An asynchronous heartbeat publication is rescheduled to occur in about 100ms, but it may never be sent if the acknowledgements come in first (or turns out to be unnecessary because of some other reasons).

The parenthesized bits give some information on the state: the number of reliable proxy readers, the number of those that have caught up with the writer (having just published a sample, none of them have) the latest sequence number in the writer and the highest acknowledged sequence number. The `!` in the `heartbeat` line here indicates some proxy readers have yet sent an acknowledgement yet at all.

### Receiving application data

As mentioned above, any time a `DATA`, `DATAFRAG`, `GAP` or `HEARTBEAT` message arrives that makes some samples available for delivery, this samples are appended to the log in lines similar to:
```
1633275476.905759 [0]     recvUC: data(application, vendor 1.16): 110b5a6:26340b11:62ec0f10:1102 #1: ST0 DDSPerfRPongKS/KeyedSeq:{13,0,{}})
```
this may be done by the receive thread that received the message or it may be done asynchronously by one of the `dq...` threads draining a delivery queue.

### Reliable protocol

The DDSI reliable protocol operates by periodically sending `HEARTBEAT` messages from writers to readers, informing the latter of the range of available sequence numbers. In this implementation, these are in principle only sent when the writer history cache contains unacknowledged data. (There are some other cases in which they do get sent.) If the heartbeat requests a response (no "final" flag set), the readers are expected to promptly respond, else they may respond but need not. A reader is not allowed to spontaneously send acknowledgements, other than "pre-emptive" ones for establishing a reliable channel.

The acknowledgement message (ACKNACK) acknowledges all data up to, but not including, the sequence number in the message, and furthermore indicates missing samples by setting bits in a variable-length bitmap. If a bit is set, a retransmit is requested for the sample with the sequence number corresponding to that bit. If instead it is clear, nothing is reported on that sample: it may or may not have been received, and it is even allowed to state that nothing is known about a sample that hasn't even been published yet (there are some implementors that apparently didn't understand that detail). Once a sample has been acknowledged there's no going back, short of lease expiry and rediscovery.

A typical happy day scenario for receiving data for a low-rate writer works out as follows (don't mind the GUIDs, this is not from a matched pair of log files). The writer packs a sample and possibly (for a low-rate writer, typically) a heartbeat into an a single RTPS message / UDP datagram, and sends this:
```
1633275477.889111 [0]    2123463: write_sample 1108c80:a5761207:e031470:e02 #14: ST0 DDSPerfRPingKS/KeyedSeq:{14,0,{}}
1633275477.889180 [0]    2123463: xpack_addmsg 0x10a702440 0x10a701f40 0(data(1108c80:a5761207:e031470:e02:#14/1)): niov 0 sz 0 => now niov 3 sz 72
1633275477.889221 [0]    2123463: writer_hbcontrol: wr 1108c80:a5761207:e031470:e02 multicasting (rel-prd 1 seq-eq-max 1 seq 14 maxseq 13)
1633275477.889269 [0]    2123463: heartbeat(wr 1108c80:a5761207:e031470:e02) piggybacked, resched in 0.1 s (min-ack 13, avail-seq 14, xmit 13)
1633275477.889296 [0]    2123463: xpack_addmsg 0x10a702440 0x10a701e40 0(control): niov 3 sz 72 => now niov 4 sz 104
1633275477.889379 [0]    2123463: nn_xpack_send 104: 0x10a70244c:20 0x10b202228:36 0x10a1010fc:16 0x10b202138:32 [ udp/127.0.0.1:63567@1 ]
1633275477.889401 [0]    2123463: traffic-xmit (1) 104
```
the receiver picks this up, and if the heartbeat has the final flag clear (typical for a low-rate writer), it responds by sending an ACKNACK:
```
1633275476.905720 [0]     recvUC: HDR(110b5a6:26340b11:62ec0f10 vendor 1.16) len 104 from udp/127.0.0.1:59598
1633275476.905727 [0]     recvUC: INFOTS(1633275476.888706001)
1633275476.905737 [0]     recvUC: DATA(110b5a6:26340b11:62ec0f10:1102 -> 0:0:0:0 #1 L(:1c1 853123.561294) => EVERYONE
1633275476.905759 [0]     recvUC: data(application, vendor 1.16): 110b5a6:26340b11:62ec0f10:1102 #1: ST0 DDSPerfRPongKS/KeyedSeq:{13,0,{}})
1633275476.905771 [0]     recvUC: HEARTBEAT(#2:1..1 110b5a6:26340b11:62ec0f10:1102 -> 0:0:0:0: 1108c80:a5761207:e031470:1007@1(sync))
1633275476.905784 [0]        tev: acknack 1108c80:a5761207:e031470:1007 -> 110b5a6:26340b11:62ec0f10:1102: F#2:2/0:
1633275476.905789 [0]        tev: send acknack(rd 1108c80:a5761207:e031470:1007 -> pwr 110b5a6:26340b11:62ec0f10:1102)
1633275476.905795 [0]        tev: xpack_addmsg 0x10a707b40 0x10a70e440 0(control): niov 0 sz 0 => now niov 2 sz 64
1633275476.905809 [0]        tev: nn_xpack_send 64: 0x10a707b4c:20 0x10b20d618:44 [ udp/127.0.0.1:63567@1 ]
1633275476.905813 [0]        tev: traffic-xmit (1) 64
```
The ACKNACK sending by Cyclone is (currently) always performed by the timed-event thread. Note that the names of the received submessages are capitals, while the ones locally generated are in lowercase. The format is pretty much the same.

This is then received by the writer:
```
1633275477.889709 [0]     recvUC: HDR(110b5a6:26340b11:62ec0f10 vendor 1.16) len 64 from udp/127.0.0.1:59598
1633275477.889746 [0]     recvUC: INFODST(1108c80:a5761207:e031470)
1633275477.889804 [0]     recvUC: ACKNACK(F#3:15/0: L(:1c1 853124.545306) 110b5a6:26340b11:62ec0f10:d07 -> 1108c80:a5761207:e031470:e02 ACK1 RM1)
```
which updates the reader state, notes that a single sample was actually ACK'd by this message and drops a single sample from the WHC because it has now been acknowledged by all matched readers.

For a high-rate writer with small samples, the timing of the piggy-backing of heartbeats changes and the final flag is set quite often. If it ends up packing multiple samples in a larger message, the pattern shifts again and becomes more like:
```
1633337183.531911 [0]        pub: write_sample 1108f55:f920d79a:a3501f3a:f02 #568: ST0 DDSPerfRDataKS/KeyedSeq:{567,0,{}}
1633337183.531969 [0]        pub: nn_xpack_send 6624: 0x105c0214c:20 0x10670e7f8:36 0x10560bc3c:16 0x10670e708:36 0x10560bafc:16 0x10670e618:36 0x10560b9bc:16 0x10670e528:36 [...]
1633337183.531975 [0]        pub: traffic-xmit (1) 6624
1633337183.531986 [0]        pub: xpack_addmsg 0x105c02140 0x105c1ae40 0(data(1108f55:f920d79a:a3501f3a:f02:#568/1)): niov 0 sz 0 => now niov 3 sz 72
1633337183.531990 [0]        pub: writer_hbcontrol: wr 1108f55:f920d79a:a3501f3a:f02 multicasting (rel-prd 1 seq-eq-max 1 seq 568 maxseq 440)
1633337183.531995 [0]        pub: heartbeat(wr 1108f55:f920d79a:a3501f3a:f02 final) piggybacked, resched in 0.0989173 s (min-ack 440, avail-seq 568, xmit 567)
1633337183.531999 [0]        pub: xpack_addmsg 0x105c02140 0x105c13540 0(control): niov 3 sz 72 => now niov 4 sz 104
1633337183.532008 [0]        pub: write_sample 1108f55:f920d79a:a3501f3a:f02 #569: ST0 DDSPerfRDataKS/KeyedSeq:{568,0,{}}
1633337183.532018 [0]        pub: xpack_addmsg 0x105c02140 0x105c13440 0(data(1108f55:f920d79a:a3501f3a:f02:#569/1)): niov 4 sz 104 => now niov 6 sz 156
...
1633337183.534008 [0]        pub: write_sample 1108f55:f920d79a:a3501f3a:f02 #694: ST0 DDSPerfRDataKS/KeyedSeq:{693,0,{}}
1633337183.534067 [0]        pub: nn_xpack_send 6604: 0x105c0214c:20 0x106715968:36 0x105611c7c:16 0x10670e7f8:32 0x10670e708:36 0x105611b3c:16 0x10670e618:36 [...]
1633337183.534072 [0]        pub: traffic-xmit (1) 6604
```
Two things are interesting here is that the message currently being constructed is typically pushed by a failure to add another sample. That sample then immediately gets appened to a new message and a heartbeat is piggy-backed into that new message. After that many more samples get added and the cycle repeats. Squeezing a heartbeat into most every message ensures that at high data rates, recovery from lost messages is possible without waiting for a timer to expire.

When samples are lost the reader sends an ACKNACK which then causes the writer to schedule retransmissions (assuming the data is still available):
```
1633337183.527838 [0]     recvUC: ACKNACK(F#1:1/4:1110 110e5bb:792b9a18:489e3f80:3c7 -> 1108f55:f920d79a:a3501f3a:3c2 complying RX1
1633337183.527852 [0]     recvUC:  RX2
1633337183.527860 [0]     recvUC:  RX3
1633337183.527871 [0]     recvUC:  rexmit#3 maxseq:3<4<=4defer_heartbeat_to_peer: 1108f55:f920d79a:a3501f3a:3c2 -> 110e5bb:792b9a18:489e3f80:3c7 - queue for transmit
```
The `RXn` stands for retransmit sequence number n; it can also trace `RXn (merged)` which means it is *assumed* to have been taken care of by a retransmit that immediately preceded it. Another possibility is `Mn` which stands for "missing" sequence number n, and invariable results in a GAP. The `rexmit#` then gives the number of retransmitted samples, the remainder is the maximum sequence number in the reply, the highest sequence number transmitted by the writer and the highest sequence number published by it (which is not necessarily the same).
