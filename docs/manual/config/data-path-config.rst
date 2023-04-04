.. include:: ../external-links.part.rst

.. _data_path_config:

***********************
Data path configuration
***********************

.. _`Re-transmission merging`:

.. index:: Heartbeat, Unicast, Multicast, Re-transmission merging

=======================
Re-transmission merging
=======================

A remote reader can request re-transmission whenever it receives a heartbeat and detects
missing samples. If a sample is lost on the network for many or all readers, the
next heartbeat can trigger a large number of re-transmission requests. Therefore, to avoid
re-transmitting the same sample over and over again to many different readers, the
Writer should attempt to merge these requests into a multicast re-transmission. Similarly,
while readers should try to avoid requesting re-transmission too often, in an
interoperable system, the writers should be robust against it.

On receiving a heartbeat that indicates samples are missing, a reader either:

- Schedules the second and subsequent re-transmission requests to be sent after a delay (set in:
  :ref:`Internal/NackDelay <//CycloneDDS/Domain/Internal/NackDelay>`).
- Combines it with an already scheduled request. Any samples that are received between the
  receipt of the heartbeat and the sending of the AckNack do not need to be re-transmitted.
  A writer attempts to combine re-transmission requests:

  - When another re-transmission request arrives, and while the re-transmission has not yet occurred,
    change the messages from unicast to multicast. This is particularly effective when
    bandwidth limiting causes a backlog of samples to be re-transmitted.

  - Merge the re-transmission request:
    :ref:`Internal/ReTransmitMerging <//CycloneDDS/Domain/Internal/ReTransmitMerging>`.

.. _`Re-Transmit backlogs`:

.. index:: Re-transmission backlogs, Backlogs,

========================
Re-transmission backlogs
========================

A reader can request re-transmission of many samples at once. When the writer queues
all these samples for re-transmission, it can cause a large backlog of samples. As a
result, the samples near the queue's end may be delayed so much that the reader issues
another re-transmission request.

|var-project| limits the number of samples queued for re-transmission and ignores
re-transmission requests that either causes the re-transmission queue to contain too many samples,
or take too long to process. Two settings govern the size of these queues. The limits are
applied per timed-event thread:

- The number of re-transmission messages:
  :ref:`Internal/MaxQueuedRexmitMessages <//CycloneDDS/Domain/Internal/MaxQueuedRexmitMessages>`.
- The size of the queue:
  :ref:`Internal/MaxQueuedRexmitBytes <//CycloneDDS/Domain/Internal/MaxQueuedRexmitBytes>`.
  This defaults to a setting based on the combination of the allowed transmition bandwidth,
  and the :ref:`Internal/NackDelay <//CycloneDDS/Domain/Internal/NackDelay>` setting, which
  is an approximation of the time until the next re-transmission request from the Reader.

.. _`Controlling fragmentation`:

.. index:: Fragmentation, Datagram, Packing, Header sizes

=============
Fragmentation
=============

.. todo:: does this need to be rewritten?

Samples in DDS can be arbitrarily large, and do not always fit within a single datagram.
DDSI can fragment samples so they can fit in UDP datagrams. IP has facilities to fragment
UDP datagrams into network packets. The DDSI specification (see |url::dds2.5| section 8.4.14.1.2)
describes how to send fragments (Data must only be fragmented if required). However, |var-project|
provides a fully configurable behaviour.

If the serialised form of a sample is at least the size set in:
:ref:`General/FragmentSize <//CycloneDDS/Domain/General/FragmentSize>`,
it is fragmented using DDSI fragmentation. All fragments are this exact size, except for
the last one, which may be smaller.

To reduce the number of network packets, the following are all subject to packing into datagrams
(based on various attributes such as the destination address) before sending on the network:

- control messages
- non-fragmented samples
- sample fragments

Packing allows datagram payloads of up to
:ref:`General/MaxMessageSize <//CycloneDDS/Domain/General/MaxMessageSize>`. If
the ``MaxMessageSize`` is too small to contain a datagram payload as a single unit, …

.. todo:: Need an explanation of "If the ``MaxMessageSize`` is too small to contain a datagram as a single unit, …"

.. note::
  UDP/IP header sizes are not taken into account in the maximum message size.

To stay within the maximum size that the underlying network supports, the IP layer fragments the datagram
into multiple packets.

A trade-off is that while DDSI fragments can be re-transmitted individually, the processing overhead
of DDSI fragmentation is larger than that of UDP fragmentation.


.. _`Receive processing`:

.. index:: Receive processing, Defragmentation, Receive thread, Delivery thread

==================
Receive processing
==================

Receiving of data is split into multiple threads:

- A single receive thread, which is responsible for:

  - Retrieving all incoming network packets.
  - Running the protocol state machine, which involves scheduling of AckNack and heartbeat messages.
  - Queueing of samples that must be re-transmitted.
  - Defragmenting.
  - Ordering incoming samples.

  Between the receive thread and the delivery threads are queues. To control the maximum
  queue size, set: :ref:`Internal/DeliveryQueueMaxSamples <//CycloneDDS/Domain/Internal/DeliveryQueueMaxSamples>`.
  Generally, queues do not need to be very large (unless there are very small samples in very
  large messages). The primary function is to smooth out the processing when batches of
  samples become available at once, for example following a re-transmission.

  When any of these receive buffers hit their size limit, and it concerns application data,
  the receive thread waits for the queue to shrink. However, discovery data never blocks the
  receive thread.

- A delivery thread dedicated to processing DDSI built-in data:
   - Participant discovery
   - Endpoint discovery
   - Liveliness assertions

- One or more delivery threads dedicated to the handling of application data:
   - deserialisation
   - delivery to the DCPS data reader caches

Fragmented data first enters the defragmentation stage, which is per proxy writer. The
number of samples that can be defragmented simultaneously is limited:

- Reliable data: :ref:`Internal/DefragReliableMaxSamples <//CycloneDDS/Domain/Internal/DefragReliableMaxSamples>`
- Unreliable data: :ref:`Internal/DefragUnreliableMaxSamples <//CycloneDDS/Domain/Internal/DefragUnreliableMaxSamples>`.

Samples (defragmented if necessary) received out of sequence are buffered:

- Initially per proxy writer. The size is limited to:
  :ref:`Internal/PrimaryReorderMaxSamples <//CycloneDDS/Domain/Internal/PrimaryReorderMaxSamples>`.
- Then per Reader (catching up on historical (transient-local) data). The size is limited to:
  :ref:`Internal/SecondaryReorderMaxSamples <//CycloneDDS/Domain/Internal/SecondaryReorderMaxSamples>`.

.. _`Minimising receive latency`:

.. index:: Receive latency, Receive thread

==========================
Minimising receive latency
==========================

In low-latency environments, a few microseconds can be gained by processing the
application data either:

- Directly in the receive thread.
- Synchronously, with respect to the incoming network traffic (instead of queueing it
  for asynchronous processing by a delivery thread).

This happens for data transmitted where the *max_latency* QoS is set at a configurable value,
and the *transport_priority* QoS is set to a configurable value. By default, these values
are ``inf`` and the maximum transport priority, effectively enabling synchronous delivery
for all data.

.. _`Maximum sample size`:

.. index:: Sample size, CDR, Dropping samples

===================
Maximum sample size
===================

To control the maximum size of samples that the service can process, set:
:ref:`Internal/MaxSampleSize <//CycloneDDS/Domain/Internal/MaxSampleSize>`.
The size is the size of the Common Data Representation (:term:`CDR`) serialised payload,
and the limit is the same for both built-in data and application data.

.. note::
  The (CDR) serialised payload is never larger than the in-memory representation of the data.

When transmitting, samples larger than
:ref:`Internal/MaxSampleSize <//CycloneDDS/Domain/Internal/MaxSampleSize>` are dropped
with a warning. |var-project| behaves as if the sample never existed.

Where the transmitting side completely ignores the sample, the receiving side assumes that
the sample has been correctly received and acknowledges reception to the writer, which allows
communication to continue.

When receiving, samples larger than
:ref:`Internal/MaxSampleSize <//CycloneDDS/Domain/Internal/MaxSampleSize>` are dropped as
early as possible. To prevent any resources from being claimed for longer than strictly necessary,
samples are dropped immediately following the reception of a sample or fragment of one.

.. note::
  When the receiving side drops a sample, readers receive a *sample lost* notification included
  with the next delivered sample. This notification can be easily overlooked. Therefore, the only
  reliable way of determining whether samples have been dropped or not is by checking the logs.

While dropping samples (or fragments thereof) as early as possible is beneficial from
the point of view of reducing resource usage, it can make it hard to decide whether or
not dropping a particular sample has been recorded in the log already. Under normal
operational circumstances, only a single message is recorded for each sample
dropped, but can occasionally report multiple events for the same sample.

It is possible (but not recommended) to set
:ref:`Internal/MaxSampleSize <//CycloneDDS/Domain/Internal/MaxSampleSize>`
to a very small size (to the point that the discovery data can no longer be communicated).