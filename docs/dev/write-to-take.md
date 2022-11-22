## Write-to-take with memory allocations

Path traversed by a sample, skipping some trivial functions and functions that are simply
"more of the same", as well as with a bit of license in the "local subscription bypass"
(skips straight from `ddsi_write_sample_gc` to `rhc_store`, which is a bit of a lie when you
look carefully at the code).  This is annotated with memory allocation activity.

Currently in default configuration, everything from `dds_write` to `sendmsg` happens on
the application thread.  With current "asynchronous" mode, outgoing packets get queued and
are transmitted on a separate thread.

    APPLICATION
      |
      v
    dds_write
      |
      |  serdata: from_sample
      |    - allocates "serdata" and serializes into it
      |  key to instance_handle mapping (1)
      |    allocates for a unique key
      |    - tkmap_instance (entry in table)
      |    - "untyped serdata" for insertion in table
      |    - possibly resizes hash table
      |    (if no readers, undoes the above)
      |
      v
    ddsi_write_sample_gc
      |         \
      |          \ (local subscriptiosn bypass)
      |           \
      |            \
      |             \
      |              \
      |               |
      |  ddsi_whc_insert (for reliable data)
      |    - allocates whc_node (2)
      |    - inserts in seq# hash (which may grow hash table)
      |    - adds to seq# interval tree (which may require an interval tree node)
      |    - may push out old samples, which may cause frees
      |    - if keyed topic: insert in index on instance_handle
      |      - may grow hash table (1)
      |               |
      v               |
    transmit_sample   |
      |               |
      |  allocate and initialise new "xmsg" (3)
      |    - large samples: needs many, for DATAFRAG and HEARTBEATFRAG
      |    - serialised data integrated by-reference
      |    - may also need one for a HEARTBEAT
      |               |
      |  xpack_add called for each "xmsg" to combine into RTPS messages
      |    - fills a (lazily allocated per writer) scatter/gather list
      |    - hands off the packet for synchronous/asynchronous publication
      |      when full/flushed
      |               |
    ddsi_xpack_send.. | ......+ (*current* asynchronous mode)
      |               |       |
      |               |       v
      |               |     ddsi_xpack_sendq_thread
      |               |       |
      |               |       |
      v               |       v
    ddsi_xpack_send_real    ddsi_xpack_send_real
      |               |       |
      |    - transmits the packet using sendmsg()
      |    - releases record of samples just sent to track the highest seq#
      |      that was actually transmitted, which may release samples if
      |      these were ACK'd already (unlikely, but possible) (3)
      |               |       |
      |               |       |
      v               |       v
    sendmsg           |     sendmsg
      .               |
      .               |
      .               |
    [network]         | (local subscriptions bypass)
      .               |
      .               |
      .               |
    ddsi_rmsg_new     |
      |               |
      |  ensure receive buffer space is available (5)
      |    - may allocate new buffers: data is left in these buffers while
      |      defragmenting or dealing with samples received out-of-order
      |    - these buffers are huge in the default config to reduce number of allocations
      v               |
    recvmsg           |
      |               |
      |               |
      v               |
    do_packet/handle_submsg_sequence (5)
      |               |
      |  typically allocates memory, typically contiguous with received datagram by
      |  bumping a pointer
      |    - COW receiver state on state changes
      |               |
      |  DATA/DATAFRAG/GAP:
      |    - allocate message defragmenting state
      |    - allocate message reordering state
      |    - (typically GAP doesn't require the above)
      |    - may result in delivering data or discarding fragments, which may free memory
      |               |
      |  ACKNACK:     |
      |    - may drop messages from WHC, freeing (2):
      |      - whc_node, interval tree entry, index entries, possibly serdata
      |      - possible "keyless serdata" and instance_handle index entry
      |  ACKNACK/NACKFRAG:
      |    - possibly queues retransmits, GAPs and HEARTBEATs
      |      - allocates "xmsg"s (like data path) (3)
      |      - allocates queue entries (4)
      |      - freed upon sending
      |               |
      |  HEARTBEAT:   |
      |    - may result in delivering data or discarding fragments, which may free memory
      |               |
      |               |
      |  Note: asynchronous delivery queues samples ready for delivery; the
      |  matching delivery thread then calls deliver_user_data_synchronously
      |  to deliver the data (no allocations needed for enqueuing)
      |               |
      v               |
    deliver_user_data_synchronously
      |               |
      |  serdata: from_ser
      |    - allocates a "serdata" and, depending on the implementation,
      |      validates the serialized data and stores it (e.g., the C version),
      |      deserialises it immediately (e.g., the C++ version), or leaves
      |      if in the receive buffers (incrementing refcounts; not done currently,
      |      probably not a wise choice either)
      |               |
      |  frees receive buffer claims after having created the "serdata" (5)
      |  typical synchronous delivery path without message loss:
      |    - resets receive buffer allocator pointer to what it was prior to processing
      |      datagram, re-using the memory for the next packet
      |    - (but typical is not so interesting in a worst-case analysis ...)
      |               |
      |  key to instance_handle mapping (1)
      |    allocates for a unique key
      |    - tkmap_instance (entry in table)
      |    - "untyped serdata" for insertion in table
      |    - possibly resizes hash table
      |    (if no readers, undoes the above)
      |               |
      |              /
      |             /
      |            /
      |           /
      |          / (local subscriptions bypass)
      v         /
    rhc_store (once per reader)
      |
      |  - allocates new "instance" if instance handle not yet present in its hash table
      |    may grow instance hash table
      |  - allocates new sample (typically, though never for KEEP_LAST with depth 1 nor
      |    for pushing old samples out of the history
      |  - may free serdatas that were pushed out of the history
      |    this won't affect the instance_handle mappings nor the "untyped serdata"
      |    because overwriting data in the history doesn't change the set of keys
      |  - may require insertion of a "registration" in a hash table, which in turn
      |    may require allocating/growing the hash table (the "registration" itself
      |    is not allocated)
      |
      v
    dds_take/dds_read
      |
      |  - serdata: to_sample, deserialises into application representation
      |  dds_take:
      |  - frees the "serdata" if the refcount drops to zero
      |  - removes the sample from the reader history (possibly the instance as well)
      |    which may involve
      |  - freeing memory allocated for the instance handle mapping and the "untyped
      |    serdata" (1)
      |
      v
    APPLICATION

There are a number of points worth noting in the above:

* Cyclone defers freeing memory in some cases, relying on a garbage collector, but this
  garbage collector is one in the sense of the garbage trucks that drive through the
  streets collecting the garbage that has been put on the sidewalk, rather than the
  stop-the-world/"thief in the night" model used in Java, C#, Haskell, &c.

  The deferring is so that some data can be used without having to do reference counting
  for dynamic references caused by using some data for a very short period of time, as
  well as, to some extent, to not incur the cost of freeing at the point of use.  This is
  currently used for:

  * mapping entries for key value to instance handle
  * all DDSI-level entities (writers, readers, participants, topics and their "proxy"
    variants for remote ones)
  * hash table buckets for the concurrent hash tables used to index the above
  * active manual-by-participant lease objects in proxy participants

  Freeing these requires enqueueing them for the garbage collector; that in turn is
  currently implemented by allocating a "gcreq" queue entry.

* If one only uses keyless topics (like ROS 2 in its current version) for each topic there
  is at most a single "instance" and consequently, at most a single instance handle and
  mapping entry at any one time.  For administrating these instance handles, the
  implementation reduces the sample to its key value and erases the topic from it (the
  "untyped serdata" in the above).  This way, if different topics from the same underlying
  type implementation have the same key value, they may get the same instance handle and
  the same mapping entry.

* The allocations of `whc_node` (marked `(2)`) for tracking individual samples in the
  writer history cache potentially happen at very high rates (> 1M/s for throughput tests
  with small samples) and I have seen the allocator becomes a bottleneck.  Caching is the
  current trick to speed this up; the cache today is bounded by what can reasonably be
  expected to be needed.

  The interval tree is allocated/freed without caching, because there is far less churn
  for those (e.g., for a simple queue, there is only one interval needed).

  The number of samples in the writer history cache is, of course, bounded by history
  settings, and so these could just as easily be pre-allocated.  Even better is to use the
  fact that the WHC has been abstracted in the code.  A simple pre-allocated circular
  array is sufficient for a implementing a queue with limited depth, and so using a
  different implementation is altogether more sensible.

* The "xmsg" (marked `(3)`) that represent RTPS submessages (or more precisely: groups of
  submessages that must be kept together, like an INFO_TS and the DATA it applies to) are
  cached for the same reason the WHC entries are cached.  Unlike the latter, the lifetime
  of the "xmsg" is the actual sending of data.  The number of "xmsg" that can be packed
  into a message is bounded, and certainly for the data path these can be preallocated.
  Currently they are cached.

  For generating responses to ACKNACKs and HEARTBEATs and queueing them, maintaining a
  pool with a sensible policy in case the pool is too small should be possible: not
  sending a message in response because no "xmsg" is available is equivalent to it getting
  lost on the network, and that is supported.  The "sensible" part of the policy is that
  it may be better to prioritise some types over others, e.g., perhaps it would be better
  to prioritise acknowledgements over retransmits.  That's something worth investigating.

* Responses to received RTPS messages are not sent by the same thread, but rather by a
  separate thread (marked `(4)`).  Retransmits (for example) are generated and queued by
  the thread handling the incoming packet, but (for example) ACKNACKs are generated by
  rescheduling a pre-existing event that then generates an "xmsg" in that separate thread
  and transmits it similar to regular data transmission.

  Pre-allocating the queue entries (or better yet: turning the queue into a circular
  array) has similar considerations to pre-allocating the "xmsg".

* Receive buffers are troublesome (marked `(5)`): to support platforms without
  scatter/gather support in the kernel (are there still any left?) it allocates large
  chunks of memory so it can accept a full UDP datagram in it with some room to spare.
  That room to spare is then used to store data generated as a consequence of interpreting
  the packet: receiver state, sample information, administration for tracking fragments or
  samples that were received out of order.  Data is the longest lived of all this, and so
  releasing the memory happens when all data in the buffer has been delivered (or
  discarded).

  If fragmented data is received or data is received out-of-order, that means the packets
  can hang around for a while.  The benefit (and primary reason why I made this experiment
  way back when) is that there is no copying of anything until the received data is turned
  into "serdata"s by `from_ser`.  Even at that stage, it is still possible to not copy the
  data but instead leave it in the receive buffer, but I have never actually tried that.

  The allocator used within the buffers is a simple bump allocator with the optimisation
  that it resets completely if no part of the packet remains live after processing all its
  submessages, and so for non-fragmented data with negligible packet loss, you're
  basically never allocating memory.  Conversely, when data is fragmented or there is
  significant packet loss, it becomes a bit of a mess.

  Eliminating the allocating and freeing of these buffers needs some thought ... quite
  possibly the best way is to assume scatter/gather support and accept an additional copy
  if the data can't be delivered to the reader history immediately.

* The reader history cache is really vastly overcomplicated for many simlpe use cases.
  Similarly to the writer history cache, it can be replaced by an alternate
  implementation, and that's probably the most straightforward path to eliminating
  allocations.  Alternatively, one could pre-allocate instances and samples.

  The "registrations" that track which writer is writing which instances do not incur any
  allocations unless there are multiple writers writing the same instance.  Cyclone
  follows the DDS spec in treating topics that have no key fields as topics having a
  single instance, and so multiple publishers for the same topic will cause these to be
  tracked.  That means typical queuing patterns result in as many "registrations" for a
  topic as there are writers for that topic.

  It may be a good idea to add a setting to avoid tracking registrations.  Note that the
  DDSI spec already describes a model wherein there are no registrations for keyless
  topics.