.. _`Data path configuration`:

***********************
Data Path Configuration
***********************

.. _`Retransmit merging`:

.. index:: Heartbeat, Unicast, Multicasting

=====================
Re-Transmit Merging
=====================

A remote Reader can request re-transmission whenever it receives a Heartbeat and detects
missing samples. If a sample was lost on the network for many or all Readers, the
next heartbeat would likely trigger a "storm" of re-transmission requests. Thus, the
Writer should attempt merging these requests into a multicast re-transmission to avoid
re-transmitting the same sample over & over again to many different Readers. Similarly,
while Readers should try to avoid requesting re-transmissions too often, in an
interoperable system, the Writers should be robust against it.

In |var-project|, upon receiving a Heartbeat that indicates samples are missing, a Reader
will schedule the second and following re-transmission requests to be sent after
:ref:`Internal/NackDelay <//CycloneDDS/Domain/Internal/NackDelay>` or combine it with an
already scheduled request if possible. Any samples received between receipt of the Heartbeat
and the sending of the AckNack will not need to be re-transmitted.

Secondly, a Writer attempts to combine re-transmit requests in two different ways. The
first is to change messages from unicast to multicast when another re-transmit request
arrives while the re-transmit has not yet occurred. This is particularly effective
when bandwidth limiting causes a backlog of samples to be re-transmitted. The second behaviour
can be configured using the :ref:`Internal/ReTransmitMerging <//CycloneDDS/Domain/Internal/ReTransmitMerging>` setting. Based
on this setting, a re-transmit request for a sample is either honoured unconditionally,
or it may be suppressed (or "merged") if it comes in shortly after a multicasted
re-transmission of that very sample, on the assumption that the second reader will likely
receive the re-transmit, too. The :ref:`Internal/ReTransmitMergingPeriod <//CycloneDDS/Domain/Internal/ReTransmitMergingPeriod>` controls the
length of this time window.


.. _`Re-Transmit backlogs`:

=======================
Re-Transmit Backlogs
=======================

Another issue is that a Reader can request re-transmission of many samples at once. When
the Writer queues all these samples for re-transmission, it may result in a
considerable backlog of samples to be re-transmitted. As a result, the ones near the
queue's end may be delayed so much that the Reader issues another re-transmit request.

Therefore, |var-project| limits the number of samples queued for re-transmission and
ignores (those parts of) re-transmission requests that would cause the re-transmit queue
to contain too many samples or take too much time to process. Two settings govern the size
of these queues, and the limits are applied per timed-event thread.
The first is :ref:`Internal/MaxQueuedRexmitMessages <//CycloneDDS/Domain/Internal/MaxQueuedRexmitMessages>`, which limits the number of re-transmit
messages, the second :ref:`Internal/MaxQueuedRexmitBytes <//CycloneDDS/Domain/Internal/MaxQueuedRexmitBytes>` which limits the number of bytes.
The latter defaults to a setting based on the combination of the allowed transmit
bandwidth and the :ref:`Internal/NackDelay <//CycloneDDS/Domain/Internal/NackDelay>` setting as an approximation of the likely time
until the next potential re-transmit request from the Reader.


.. _`Controlling fragmentation`:

=========================
Controlling Fragmentation
=========================

.. todo:: does this need to be rewritten?

Samples in DDS can be arbitrarily large, and will not always fit within a single
datagram. DDSI has facilities to fragment samples so they can fit in UDP datagrams, and
similarly IP has facilities to fragment UDP datagrams into network packets. The DDSI
specification states that one must not unnecessarily fragment at the DDSI level, but
|var-project| provides a fully configurable behaviour.

If the serialised form of a sample is at least :ref:`General/FragmentSize <//CycloneDDS/Domain/General/FragmentSize>`,
it will be fragmented using the DDSI fragmentation. All but the last fragment
will be this exact size; the last one may be smaller.

Control messages, non-fragmented samples, and sample fragments are all subject to
packing into datagrams before sending it out on the network, based on various attributes
such as the destination address, to reduce the number of network packets. This packing
allows datagram payloads of up to :ref:`General/MaxMessageSize <//CycloneDDS/Domain/General/MaxMessageSize>`, overshooting this size if
the set maximum is too small to contain what must be sent as a single unit.
In that case it no longer matters where the data is rejected: there is a real problem anyway.

UDP/IP header sizes are not taken into account in the maximum message size.

The IP layer then takes this UDP datagram, possibly fragmenting it into multiple packets
to stay within the maximum size the underlying network supports. A trade-off
is that while DDSI fragments can be re-transmitted individually, the processing overhead
of DDSI fragmentation is larger than that of UDP fragmentation.


.. _`Receive processing`:

==================
Receive Processing
==================

Receiving of data is split into multiple threads:

+ A single receive thread responsible for retrieving network packets and running
  the protocol state machine;
+ A delivery thread dedicated to processing DDSI built-in data: participant
  discovery, endpoint discovery, and liveliness assertions;
+ One or more delivery threads dedicated to the handling of application data:
  deserialisation and delivery to the DCPS data Reader caches.

The receive thread is responsible for retrieving all incoming network packets, running
the protocol state machine, which involves scheduling of AckNack and Heartbeat messages
and queueing of samples that must be retransmitted, and for defragmenting and ordering
incoming samples.

Fragmented data first enters the defragmentation stage, which is per proxy Writer. The
number of samples that can be defragmented simultaneously is limited for reliable data
to :ref:`Internal/DefragReliableMaxSamples <//CycloneDDS/Domain/Internal/DefragReliableMaxSamples>` and for unreliable data to
:ref:`Internal/DefragUnreliableMaxSamples <//CycloneDDS/Domain/Internal/DefragUnreliableMaxSamples>`.

Samples (defragmented if necessary) received out of sequence are buffered, primarily per
proxy Writer, but, secondarily, per Reader catching up on historical (transient-local)
data. The size of the first is limited to :ref:`Internal/PrimaryReorderMaxSamples <//CycloneDDS/Domain/Internal/PrimaryReorderMaxSamples>`, the
size of the second to :ref:`Internal/SecondaryReorderMaxSamples <//CycloneDDS/Domain/Internal/SecondaryReorderMaxSamples>`.

In between the receive thread and the delivery threads sit queues, of which the maximum
size is controlled by the :ref:`Internal/DeliveryQueueMaxSamples <//CycloneDDS/Domain/Internal/DeliveryQueueMaxSamples>` setting.  Generally there
is no need for these queues to be very large (unless one has very small samples in very
large messages), their primary function is to smooth out the processing when batches of
samples become available at once, for example following a retransmission.

When any of these receive buffers hit their size limit, and it concerns application data,
the receive thread will wait for the queue to shrink (a compromise that is the lesser
evil within the constraints of various other choices). However, discovery data will
never block the receive thread.


.. _`Minimising receive latency`:

==========================
Minimising Receive Latency
==========================

In low-latency environments, a few microseconds can be gained by processing the
application data directly in the receive thread, or synchronously with respect to the
incoming network traffic, instead of queueing it for asynchronous processing by a
delivery thread. This happens for data transmitted with the *max_latency* QoS setting at
most a configurable value and the *transport_priority* QoS setting at least a
configurable value. By default, these values are ``inf`` and the maximum transport
priority, effectively enabling synchronous delivery for all data.


.. _`Maximum sample size`:

===================
Maximum Sample Size
===================

|var-project| provides a setting, :ref:`Internal/MaxSampleSize <//CycloneDDS/Domain/Internal/MaxSampleSize>`, to control the maximum size
of samples the service is willing to process. The size is the size of the (CDR)
serialised payload, and the limit holds both for built-in data and for application data.
The (CDR) serialised payload is never larger than the in-memory representation of the
data.

On the transmitting side, samples larger than :ref:`Internal/MaxSampleSize <//CycloneDDS/Domain/Internal/MaxSampleSize>` are dropped with a
warning. |var-project| behaves as if the sample never existed.

Similarly, on the receiving side, samples larger than :ref:`Internal/MaxSampleSize <//CycloneDDS/Domain/Internal/MaxSampleSize>` are dropped as
early as possible, immediately following the reception of a sample or fragment of one,
to prevent any resources from being claimed for longer than strictly necessary. Where
the transmitting side completely ignores the sample, the receiving side pretends the
sample has been correctly received acknowledges reception to the Writer.
This allows communication to continue.

When the receiving side drops a sample, Readers will receive a *sample lost* notification with
the next delivered sample. This notification is easily missed, so ultimately the only reliable way of determining
whether samples have been dropped or not is checking the logs.

While dropping samples (or fragments thereof) as early as possible is beneficial from
the point of view of reducing resource usage, it can make it hard to decide whether or
not dropping a particular sample has been recorded in the log already. Under normal
operational circumstances, only a single message will be recorded for each sample
dropped, but it may occasionally report multiple events for the same sample.

Finally, it is technically permitted to set :ref:`Internal/MaxSampleSize <//CycloneDDS/Domain/Internal/MaxSampleSize>` to very small sizes,
even to the point that the discovery data can no longer be communicated.
The dropping of the discovery data will be reported as normal, but the utility
of such a configuration seems doubtful.
