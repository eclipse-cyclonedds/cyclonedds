.. include:: ../external-links.part.rst
  
.. index:: Discovery behaviour

.. _discovery_behaviour:

###################
Discovery behaviour
###################

.. _proxy_participants_endpoints:

********************************
Proxy participants and endpoints
********************************

In the |url::ddsi_spec|, |var-project| is known as a *stateful* implementation. writers 
only send data to discovered readers, and readers only accept data from discovered
writers. There is one exception: the ``writer`` may choose to multicast the data so 
that any reader is able to receive it. If a reader has already discovered the writer 
but not vice-versa, it can accept the data even though the connection is not fully 
established. 

Such asymmetrical discovery can cause data to be delivered when it is not expected, 
which can also cause indefinite blocking. To avoid this, |var-project| internally 
creates a proxy for each remote participant and reader or writer. In the discovery 
process, writers are matched with proxy readers, and readers are matched with proxy 
writers, based on the topic name, type name, and the :ref:`QoS <qos_bm>` settings.

Proxies have the same natural hierarchy as 'normal' DDSI entities. Each proxy endpoint 
is owned by a proxy participant. When a proxy participant is deleted, all of its proxy 
endpoints are also deleted. Participants assert their liveliness periodically, which is 
known as *automatic* liveliness in the DCPS specification, and the only mode supported 
by |var-project|. When nothing has been heard from a participant for the lease duration 
(published by that participant in its SPDP message), the lease expires, triggering a 
clean-up.

Deleting endpoints triggers 'disposes' and 'un-registers' in the SEDP protocol. Deleting a 
participant also creates special messages that allow the peers to immediately reclaim 
resources instead of waiting for the lease to expire.

.. _sharing_discovery_info:

********************************
Sharing of discovery information
********************************

|var-project| handles any number of participants in an integrated manner, the discovery
protocol as described in :ref:`proxy_participants_endpoints` can be wasteful. It is not 
necessary for each participant in a |var-project| process to run the full discovery protocol 
for itself.

Instead of implementing the protocol as suggested by the DDSI specification, |var-project| shares
all discovery activities amongst the participants, allowing you to add participants to a
process with minimal impact on the system. 

It is also possible to have a single DDSI participant in a process regardless of the number 
of DCPS participants created by the application code, which then becomes the virtual owner 
of all the endpoints created in that one process. There is no discovery penalty for having
many participants, but any participant-based liveliness monitoring can be affected.

Because other implementations of the DDSI specification may be written on the assumption
that all participants perform their own discovery, it is possible to simulate that with
|var-project|. It will not perform the discovery for each participant independently, but it 
generates the network traffic *as if* it does. These are controlled by the 
:ref:`Internal/BuiltinEndpointSet <//CycloneDDS/Domain/Internal/BuiltinEndpointSet>` option.

By sharing the discovery information across all participants in a single node, each
new participant or endpoint is immediately aware of the existing peers and can directly 
communicate with these peers. If these peers take significant time to discover the new 
participant or endpoint, it can generate some redundant network traffic.

.. index:: ! Lingering writers

*****************
Lingering writers
*****************

When an application deletes a reliable DCPS writer, there is no guarantee that all
its readers have already acknowledged the correct receipt of all samples. |var-project| 
lets the writer (and the owning participant if necessary) linger in the system for some time, 
controlled by the :ref:`Internal/writerLingerDuration <//CycloneDDS/Domain/Internal/writerLingerDuration>` 
option. The writer is deleted when all readers have acknowledged all samples, or the
linger duration has elapsed, whichever comes first.

.. note::

  The writer linger duration setting is not applied when |var-project|
  is requested to terminate.

.. _`writer history QoS and throttling`:

.. index:: ! writer History Cache

*********************************
Writer history QoS and throttling
*********************************

The |url::ddsi_spec| relies on the writer History Cache (WHC), in which a sequence number 
uniquely identifies each sample. The WHC integrates two different indices on the samples 
published by a writer: 

- The **sequence number** index is used for re-transmitting lost samples, and is therefore needed 
  for all reliable writers (see :ref:`reliable_coms`).
   
- The **key value** index is used for retaining the current state of each instance in the WHC.
  
When a new sample overwrites the state of an instance, the key value index allows dropping 
samples from the sequence number index. For transient-local behaviour (see 
:ref:`DDSI-specific transient-local behaviour`), the key value index also allows retaining 
the current state of each instance even when all readers have acknowledged a sample.

Transient-local data always requires the key values index, and by default is also 
used for other writers that have a history setting of ``KEEP_LAST``. The advantage of an 
index on key value is that superseded samples can be dropped aggressively, instead of 
delivering them to all readers. The disadvantage is that it is somewhat more resource-intensive.

The WHC distinguishes between:

- History to be retained for existing readers (controlled by the writer's history QoS setting).
- History to be retained for late-joining readers for transient-local writers (controlled by 
  the topic's durability-service history QoS setting). 

It is therefore possible to create a writer that never overwrites samples for live readers, 
while maintaining only the most recent samples for late-joining readers. This ensures that 
the data that is available for late-joining readers is the same for transient-local and for 
transient data.

.. Index:: ! writer throttling

Writer throttling
=================

Writer throttling is based on the WHC size. The following settings control writer throttling:

When the WHC contains at least ``high`` bytes in unacknowledged samples, it stalls the 
writer until the number of bytes in unacknowledged samples drops below the value set in: 
:ref:`Internal/Watermarks/WhcLow <//CycloneDDS/Domain/Internal/Watermarks/WhcLow>`.

Based on the transmit pressure and receive re-ransmit requests, the value of ``high`` is 
dynamically adjusted between:
- :ref:`Internal/Watermarks/WhcLow <//CycloneDDS/Domain/Internal/Watermarks/WhcLow>`
- :ref:`Internal/Watermarks/WhcHigh <//CycloneDDS/Domain/Internal/Watermarks/WhcHigh>`
  
The initial value of ``high`` is set in: 
:ref:`Internal/Watermarks/WhcHighInit <//CycloneDDS/Domain/Internal/Watermarks/WhcHighInit>`.

The adaptive behavior can be disabled by setting 
:ref:`Internal/Watermarks/WhcAdaptive <//CycloneDDS/Domain/Internal/Watermarks/WhcAdaptive>` 
to ``false``.

While the adaptive behaviour generally handles a variety of fast and slow writers and
readers quite well, the introduction of a very slow reader with small buffers in an
existing network that is transmitting data at high rates can cause a sudden stop while
the new reader tries to recover the large amount of data stored in the writer, before
things can continue at a much lower rate.
