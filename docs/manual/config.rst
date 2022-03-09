###########################################################
A guide to the configuration options of Eclipse Cyclone DDS
###########################################################

This document attempts to provide background information that will help in adjusting the
configuration of Eclipse Cyclone DDS when the default settings do not give the desired behavior.
A full listing of all settings is out of scope for this document, but can be extracted
from the sources.


.. _`DDSI Concepts`:

DDSI Concepts
*************

The DDSI standard is intimately related to the DDS 1.2 and 1.4 standards, with a clear
correspondence between the entities in DDSI and those in DCPS.  However, this
correspondence is not one-to-one.

In this section we give a high-level description of the concepts of the DDSI
specification, with hardly any reference to the specifics of the Eclipse Cyclone DDS
implementation, which are addressed in subsequent sections. This division was chosen to
aid readers interested in interoperability to understand where the specification ends
and the Eclipse Cyclone DDS implementation begins.


.. _`Mapping of DCPS domains to DDSI domains`:

Mapping of DCPS domains to DDSI domains
=======================================

In DCPS, a domain is uniquely identified by a non-negative integer, the domain id.  In
the UDP/IP mapping, this domain id is mapped to port numbers to be used for
communicating with the peer nodes — these port numbers are particularly important for
the discovery protocol — and this mapping of domain ids to UDP/IP port numbers ensures
that accidental cross-domain communication is impossible with the default mapping.

DDSI does not communicate the DCPS port number in the discovery protocol; it assumes
that each domain id maps to a unique port number.  While it is unusual to change the
mapping, the specification requires this to be possible, and this means that two
different DCPS domain ids can be mapped to a single DDSI domain.


.. _`Mapping of DCPS entities to DDSI entities`:

Mapping of DCPS entities to DDSI entities
=========================================

Each DCPS domain participant in a domain is mirrored in DDSI as a DDSI participant.
These DDSI participants drive the discovery of participants, readers and writers in DDSI
via the discovery protocols.  By default, each DDSI participant has a unique address on
the network in the form of its own UDP/IP socket with a unique port number.

Any data reader or data writer created by a DCPS domain participant is mirrored in DDSI
as a DDSI reader or writer.  In this translation, some of the structure of the DCPS
domain is obscured because the standardized parts of DDSI have no knowledge of DCPS
Subscribers and Publishers.  Instead, each DDSI reader is the combination of the
corresponding DCPS data reader and the DCPS subscriber it belongs to; similarly, each
DDSI writer is a combination of the corresponding DCPS data writer and DCPS publisher.
This corresponds to the way the standardized DCPS built-in topics describe the DCPS data
readers and data writers, as there are no standardized built-in topics for describing
the DCPS subscribers and publishers either.  Implementations can (and do) offer
additional built-in topics for describing these entities and include them in the
discovery, but these are non-standard extensions.

In addition to the application-created readers and writers (referred to as *endpoints*),
DDSI participants have a number of DDSI built-in endpoints used for discovery and
liveliness checking/asserting.  The most important ones are those absolutely required
for discovery: readers and writers for the discovery data concerning DDSI participants,
DDSI readers and DDSI writers.  Some other ones exist as well, and a DDSI implementation
can leave out some of these if it has no use for them.  For example, if a participant
has no writers, it doesn’t strictly need the DDSI built-in endpoints for describing
writers, nor the DDSI built-in endpoint for learning of readers of other participants.


.. _`Reliable communication`:

Reliable communication
======================

*Best-effort* communication is simply a wrapper around UDP/IP: the packet(s) containing
a sample are sent to the addresses at which the readers reside.  No state is maintained
on the writer.  If a packet is lost, the reader will simply ignore the whatever samples
were contained in the lost packet and continue with the next one.

When *reliable* communication is used, the writer does maintain a copy of the sample, in
case a reader detects it has lost packets and requests a retransmission.  These copies
are stored in the writer history cache (or *WHC*) of the DDSI writer.  The DDSI writer
is required to periodically send *Heartbeats* to its readers to ensure that all readers
will learn of the presence of new samples in the WHC even when packets get lost.  It is
allowed to suppress these periodic Heartbeats if there is all samples in the WHC have
been acknowledged by all matched readers and the Eclipse Cyclone DDS exploits this freedom.

If a reader receives a Heartbeat and detects it did not receive all samples, it requests
a retransmission by sending an *AckNack* message to the writer.  The timing of this is
somewhat adjustable and it is worth remarking that a roundtrip latency longer than the
Heartbeat interval easily results in multiple retransmit requests for a single sample.
In addition to requesting retransmission of some samples, a reader also uses the AckNack
messages to inform the writer up to what sample it has received everything, and which
ones it has not yet received.  Whenever the writer indicates it requires a response to a
Heartbeat the readers will send an AckNack message even when no samples are missing.  In
this case, it becomes a pure acknowledgement.

The combination of these behaviours in principle allows the writer to remove old samples
from its WHC when it fills up too far, and allows readers to always receive all data.  A
complication exists in the case of unresponsive readers, readers that do not respond to
a Heartbeat at all, or that for some reason fail to receive some samples despite
resending it.  The specification leaves the way these get treated unspecified.  The
default behaviour of Eclipse Cyclone DDS is to never consider readers unresponsive, but it can
be configured to consider them so after a certain length of time has passed at which
point the participant containing the reader is undiscovered.

Note that while this Heartbeat/AckNack mechanism is very straightforward, the
specification actually allows suppressing heartbeats, merging of AckNacks and
retransmissions, etc.  The use of these techniques is required to allow for a performant
DDSI implementation, whilst avoiding the need for sending redundant messages.


.. _`DDSI-specific transient-local behaviour`:

DDSI-specific transient-local behaviour
=======================================

The above describes the essentials of the mechanism used for samples of the *volatile*
durability kind, but the DCPS specification also provides *transient-local*, *transient*
and *persistent* data.  Of these, the DDSI specification at present only covers
*transient-local*, and this is the only form of durable data available when
interoperating across vendors.

In DDSI, transient-local data is implemented using the WHC that is normally used for
reliable communication.  For transient-local data, samples are retained even when all
readers have acknowledged them. With the default history setting of ``KEEP_LAST`` with
``history_depth = 1``, this means that late-joining readers can still obtain the latest
sample for each existing instance.

Naturally, once the DCPS writer is deleted (or disappears for whatever reason), the DDSI
writer disappears as well, and with it, its history.  For this reason, transient data is
generally much to be preferred over transient-local data.  Eclipse Cyclone DDS has a facility
for retrieving transient data from an suitably configured OpenSplice node, but does not
yet include a native service for managing transient data.


.. _`Discovery of participants & endpoints`:

Discovery of participants & endpoints
=====================================

DDSI participants discover each other by means of the *Simple Participant Discovery
Protocol* or *SPDP* for short.  This protocol is based on periodically sending a message
containing the specifics of the participant to a set of known addresses.  By default,
this is a standardised multicast address (``239.255.0.1``; the port number is derived
from the domain id) that all DDSI implementations listen to.

Particularly important in the SPDP message are the unicast and multicast addresses at
which the participant can be reached.  Typically, each participant has a unique unicast
address, which in practice means all participants on a node all have a different UDP/IP
port number in their unicast address.  In a multicast-capable network, it doesn’t matter
what the actual address (including port number) is, because all participants will learn
them through these SPDP messages.

The protocol does allow for unicast-based discovery, which requires listing the
addresses of machines where participants may be located and ensuring each participant
uses one of a small set of port numbers.  Because of this, some of the port numbers are
derived not only from the domain id, but also from a *participant index*, which is a
small non-negative integer, unique to a participant within a node.  (Eclipse Cyclone DDS adds an
indirection and uses at most one participant index for a domain for each process,
regardless of how many DCPS participants are created by the process.)

Once two participants have discovered each other and both have matched the DDSI built-in
endpoints their peer is advertising in the SPDP message, the *Simple Endpoint Discovery
Protocol* or *SEDP* takes over, exchanging information on the DCPS data readers and data
writers (and for Eclipse Cyclone DDS, also publishers, subscribers and topics in a manner
compatible with OpenSplice) in the two participants.

The SEDP data is handled as reliable, transient-local data.  Therefore, the SEDP writers
send Heartbeats, the SEDP readers detect they have not yet received all samples and send
AckNacks requesting retransmissions, the writer responds to these and eventually
receives a pure acknowledgement informing it that the reader has now received the
complete set.

Note that the discovery process necessarily creates a burst of traffic each time a
participant is added to the system: *all* existing participants respond to the SPDP
message, following which all start exchanging SEDP data.

  
.. _`Eclipse Cyclone DDS specifics`:

Eclipse Cyclone DDS specifics
*****************************

.. _`Discovery behaviour`:

Discovery behaviour
===================

.. _`Proxy participants and endpoints`:

Proxy participants and endpoints
--------------------------------

Eclipse Cyclone DDS is what the DDSI specification calls a *stateful* implementation.  Writers
only send data to discovered readers and readers only accept data from discovered
writers.  (There is one exception: the writer may choose to multicast the data, and
anyone listening will be able to receive it, if a reader has already discovered the
writer but not vice-versa; it may accept the data even though the connection is not
fully established yet.  At present, not only can such asymmetrical discovery cause data
to be delivered when it was perhaps not expected, it can also cause indefinite blocking
if the situation persists for a long time.)  Consequently, for each remote participant
and reader or writer, Eclipse Cyclone DDS internally creates a proxy participant, proxy reader
or proxy writer.  In the discovery process, writers are matched with proxy readers, and
readers are matched with proxy writers, based on the topic and type names and the QoS
settings.

Proxies have the same natural hierarchy that ‘normal’ DDSI entities have: each proxy
endpoint is owned by some proxy participant, and once the proxy participant is deleted,
all of its proxy endpoints are deleted as well.  Participants assert their liveliness
periodically (called *automatic* liveliness in the DCPS specification and the only mode
currently supported by Eclipse Cyclone DDS), and when nothing has been heard from a participant
for the lease duration published by that participant in its SPDP message, the lease
becomes expired triggering a clean-up.

Under normal circumstances, deleting endpoints simply triggers disposes and unregisters
in SEDP protocol, and, similarly, deleting a participant also creates special messages
that allow the peers to immediately reclaim resources instead of waiting for the lease
to expire.


.. _`Sharing of discovery information`:

Sharing of discovery information
--------------------------------

As Eclipse Cyclone DDS handles any number of participants in an integrated manner, the discovery
protocol as sketched earlier is rather wasteful: there is no need for each individual
participant in a Eclipse Cyclone DDS process to run the full discovery protocol for itself.

Instead of implementing the protocol as suggested by the standard, Eclipse Cyclone DDS shares
all discovery activities amongst the participants, allowing one to add participants on a
process with only a minimal impact on the system.  It is even possible to have only a
single DDSI participant in a process regardless of the number of DCPS participants
created by the application code in that process, which then becomes the virtual owner of
all the endpoints created in that one process.  (See `Combining multiple
participants`_.)  In this latter mode, there is no discovery penalty at all for having
many participants, but evidently, any participant-based liveliness monitoring will be
affected.

Because other implementations of the DDSI specification may be written on the assumption
that all participants perform their own discovery, it is possible to simulate that with
Eclipse Cyclone DDS.  It will not actually perform the discovery for each participant
independently, but it will generate the network traffic *as if* it does.  These are
controlled by the ``Internal/BuiltinEndpointSet`` and
``Internal/ConservativeBuiltinReaderStartup`` options.  However, please note that at the
time of writing, we are not aware of any DDSI implementation requiring the use of these
settings.)

By sharing the discovery information across all participants in a single node, each
new participant or endpoint is immediately aware of the existing peers and will
immediately try to communicate with these peers.  This may generate some
redundant network traffic if these peers take a significant amount of time for
discovering this new participant or endpoint.


.. _`Lingering writers`:

Lingering writers
-----------------

When an application deletes a reliable DCPS data writer, there is no guarantee that all
its readers have already acknowledged the correct receipt of all samples.  In such a
case, Eclipse Cyclone DDS lets the writer (and the owning participant if necessary) linger in
the system for some time, controlled by the ``Internal/WriterLingerDuration`` option.
The writer is deleted when all samples have been acknowledged by all readers or the
linger duration has elapsed, whichever comes first.

Note that the writer linger duration setting is currently not applied when Eclipse Cyclone DDS
is requested to terminate.


.. _`Start-up mode`:

Start-up mode
-------------

A similar issue exists when starting Eclipse Cyclone DDS: DDSI discovery takes time, and when
data is written immediately after the first participant was created, it is likely that
the discovery process hasn’t completed yet and some remote readers have not yet been
discovered.  This would cause the writers to throw away samples for lack of interest,
even though matching readers already existed at the time of starting.  For best-effort
writers, this is perhaps surprising but still acceptable; for reliable writers, however,
it would be very counter-intuitive.

Hence the existence of the so-called *start-up mode*, during which all volatile reliable
writers are treated as-if they are transient-local writers.  Transient-local data is
meant to ensure samples are available to late-joining readers, the start-up mode uses
this same mechanism to ensure late-discovered readers will also receive the data.  This
treatment of volatile data as-if it were transient-local happens internally and is
invisible to the outside world, other than the availability of some samples that would
not otherwise be available.

Once initial discovery has been completed, any new local writers can be matched locally
against already existing readers, and consequently keeps any new samples published in a
writer history cache because these existing readers have not acknowledged them yet.
Hence why this mode is tied to the start-up of the DDSI stack, rather than to that of an
individual writer.

Unfortunately it is impossible to detect with certainty when the initial discovery
process has been completed and therefore the duration of this start-up mode is
controlled by an option: ``General/StartupModeDuration``.

While in general this start-up mode is beneficial, it is not always so.  There are two
downsides: the first is that during the start-up period, the writer history caches can
grow significantly larger than one would normally expect; the second is that it does
mean large amounts of historical data may be transferred to readers discovered
relatively late in the process.


.. _`Writer history QoS and throttling`:

Writer history QoS and throttling
=================================

The DDSI specification heavily relies on the notion of a writer history cache (WHC)
within which a sequence number uniquely identifies each sample.  This WHC integrates two
different indices on the samples published by a writer: one is on sequence number, used
for retransmitting lost samples, and one is on key value and is used for retaining the
current state of each instance in the WHC.

The index on key value allows dropping samples from the index on sequence number when
the state of an instance is overwritten by a new sample.  For transient-local, it
conversely (also) allows retaining the current state of each instance even when all
readers have acknowledged a sample.

The index on sequence number is required for retransmitting old data, and is therefore
needed for all reliable writers.  The index on key values is always needed for
transient-local data, and will be default also be used for other writers using a history
setting of ``KEEP_LAST``.  (The ``Internal/AggressiveKeepLastWhc`` setting controls this
behaviour.)  The advantage of an index on key value in such a case is that superseded
samples can be dropped aggressively, instead of having to deliver them to all readers;
the disadvantage is that it is somewhat more resource-intensive.

The WHC distinguishes between history to be retained for existing readers (controlled by
the writer’s history QoS setting) and the history to be retained for late-joining
readers for transient-local writers (controlled by the topic’s durability-service
history QoS setting).  This makes it possible to create a writer that never overwrites
samples for live readers while maintaining only the most recent samples for late-joining
readers.  Moreover, it ensures that the data that is available for late-joining readers
is the same for transient-local and for transient data.

Writer throttling is based on the WHC size using a simple controller.  Once the WHC
contains at least *high* bytes in unacknowledged samples, it stalls the writer until the
number of bytes in unacknowledged samples drops below ``Internal/Watermarks/WhcLow``.
The value of *high* is dynamically adjusted between ``Internal/Watermarks/WhcLow`` and
``Internal/Watermarks/WhcHigh`` based on transmit pressure and receive retransmit
requests. The initial value of *high* is ``Internal/Watermarks/WhcHighInit`` and the
adaptive behavior can be disabled by setting ``Internal/Watermarks/WhcAdaptive`` to
false.

While the adaptive behaviour generally handles a variety of fast and slow writers and
readers quite well, the introduction of a very slow reader with small buffers in an
existing network that is transmitting data at high rates can cause a sudden stop while
the new reader tries to recover the large amount of data stored in the writer, before
things can continue at a much lower rate.


.. _`Network and discovery configuration`:

Network and discovery configuration
***********************************

.. _`Networking interfaces`:

Networking interfaces
=====================

Eclipse Cyclone DDS uses a single network interface, the *preferred* interface, for transmitting
its multicast packets and advertises only the address corresponding to this interface in
the DDSI discovery protocol.

To determine the default network interface, the eligible interfaces are ranked by
quality and then selects the interface with the highest quality.  If multiple interfaces
are of the highest quality, it will select the first enumerated one.  Eligible
interfaces are those that are up and have the right kind of address family (IPv4 or
IPv6).  Priority is then determined as follows:

+ interfaces with a non-link-local address are preferred over those with
  a link-local one;
+ multicast-capable is preferred (see also ``General/Interfaces/NetworkInterface[@multicast]``), or if
  none is available
+ non-multicast capable but neither point-to-point, or if none is available
+ point-to-point, or if none is available
+ loopback

If this procedure doesn’t select the desired interface automatically, it can be
overridden by setting ``General/Interfaces`` by adding the interface(s) either by name of the
interface (<NetworkInterface name='interface_name' />), the IP address of the host
on the desired interface (<NetworkInterface address='128.129.0.42' />), or the network portion
of the IP address of the host on the desired interface (<NetworkInterface address='128.11.0.0' />).
An exact match on the address is always preferred and is the only option that allows selecting
the desired one when multiple addresses are tied to a single interface.

The default address family is IPv4, setting General/UseIPv6 will change this to IPv6.
Currently, Eclipse Cyclone DDS does not mix IPv4 and IPv6 addressing.  Consequently, all DDSI
participants in the network must use the same addressing mode.  When interoperating,
this behaviour is the same, i.e., it will look at either IPv4 or IPv6 addresses in the
advertised address information in the SPDP and SEDP discovery protocols.

IPv6 link-local addresses are considered undesirable because they need to be published
and received via the discovery mechanism, but there is in general no way to determine to
which interface a received link-local address is related.

If IPv6 is requested and the preferred interface has a non-link-local address, Cyclone
DDS will operate in a *global addressing* mode and will only consider discovered
non-link-local addresses.  In this mode, one can select any set of interface for
listening to multicasts.  Note that this behaviour is essentially identical to that when
using IPv4, as IPv4 does not have the formal notion of address scopes that IPv6 has.  If
instead only a link-local address is available, Eclipse Cyclone DDS will run in a *link-local
addressing* mode.  In this mode it will accept any address in a discovery packet,
assuming that a link-local address is valid on the preferred interface.  To minimise the
risk involved in this assumption, it only allows the preferred interface for listening
to multicasts.

When a remote participant publishes multiple addresses in its SPDP message (or in SEDP
messages, for that matter), it will select a single address to use for communicating
with that participant. The address chosen is the first eligible one on the same network
as the locally chosen interface, else one that is on a network corresponding to any of
the other local interfaces, and finally simply the first one.  Eligibility is determined
in the same way as for network interfaces.


.. _`Multicasting`:

Multicasting
------------

Eclipse Cyclone DDS allows configuring to what extent multicast (the regular, any-source
multicast as well as source-specific multicast) is to be used:

+ whether to use multicast for data communications,
+ whether to use multicast for participant discovery,
+ on which interfaces to listen for multicasts.

It is advised to allow multicasting to be used.  However, if there are restrictions on
the use of multicasting, or if the network reliability is dramatically different for
multicast than for unicast, it may be attractive to disable multicast for normal
communications.  In this case, setting ``General/AllowMulticast`` to ``false`` will
force the use of unicast communications for everything.

If at all possible, it is strongly advised to leave multicast-based participant
discovery enabled, because that avoids having to specify a list of nodes to contact, and
it furthermore reduces the network load considerably.  Setting
``General/AllowMulticast`` to ``spdp`` will allow participant discovery via multicast
while disabling multicast for everything else.

To disable incoming multicasts, or to control from which interfaces multicasts are to be
accepted, one can use the ``General/MulticastRecvInterfaceAddresses`` setting.  This
allows listening on no interface, the preferred, all or a specific set of interfaces.


.. _`TCP support`:

TCP support
-----------

The DDSI protocol is really a protocol designed for a transport providing
connectionless, unreliable datagrams.  However, there are times where TCP is the only
practical network transport available (for example, across a WAN).  Because of this,
Eclipse Cyclone DDS can use TCP instead of UDP.

The differences in the model of operation between DDSI and TCP are quite large: DDSI is
based on the notion of peers, whereas TCP communication is based on the notion of a
session that is initiated by a ‘client’ and accepted by a ‘server’, and so TCP requires
knowledge of the servers to connect to before the DDSI discovery protocol can exchange
that information.  The configuration of this is done in the same manner as for
unicast-based UDP discovery.

TCP reliability is defined in terms of these sessions, but DDSI reliability is defined
in terms of DDSI discovery and liveliness management.  It is therefore possible that a
TCP connection is (forcibly) closed while the remote endpoint is still considered alive.
Following a reconnect the samples lost when the TCP connection was closed can be
recovered via the normal DDSI reliability.  This also means that the Heartbeats and
AckNacks still need to be sent over a TCP connection, and consequently that DDSI
flow-control occurs on top of TCP flow-control.

Another point worth noting is that connection establishment takes a potentially long
time, and that giving up on a transmission to a failed or no-longer reachable host can
also take a long time. These long delays can be visible at the application level at
present.

.. _`TLS support`:

TLS support
...........

The TCP mode can be used in conjunction with TLS to provide mutual authentication and
encryption.  When TLS is enabled, plain TCP connections are no longer accepted or
initiated.


.. _`Raw Ethernet support`:

Raw Ethernet support
--------------------

As an additional option, on Linux, Eclipse Cyclone DDS can use a raw Ethernet network interface
to communicate without a configured IP stack.


.. _`Discovery configuration`:

Discovery configuration
-----------------------

.. _`Discovery addresses`:

Discovery addresses
...................

The DDSI discovery protocols, SPDP for the domain participants and SEDP for their
endpoints, usually operate well without any explicit configuration.  Indeed, the SEDP
protocol never requires any configuration.

The SPDP protocol periodically sends, for each domain participant, an SPDP sample to a
set of addresses, which by default contains just the multicast address, which is
standardised for IPv4 (``239.255.0.1``) but not for IPv6 (it uses
``ff02::ffff:239.255.0.1``).  The actual address can be overridden using the
``Discovery/SPDPMulticastAddress`` setting, which requires a valid multicast address.

In addition (or as an alternative) to the multicast-based discovery, any number of
unicast addresses can be configured as addresses to be contacted by specifying peers in
the ``Discovery/Peers`` section.  Each time an SPDP message is sent, it is sent to all
of these addresses.

Default behaviour is to include each IP address several times in the set (for
participant indices 0 through ``MaxAutoParticipantIndex``, each time with a different
UDP port number (corresponding to another participant index), allowing at least several
applications to be present on these hosts.

Obviously, configuring a number of peers in this way causes a large burst of packets
to be sent each time an SPDP message is sent out, and each local DDSI participant
causes a burst of its own. Most of the participant indices will not actually be use,
making this rather wasteful behaviour.

To avoid sending large numbers of packets to each host, differing only in port number,
it is also possible to add a port number to the IP address, formatted as IP:PORT, but
this requires manually calculating the port number.  In practice it also requires fixing
the participant index using ``Discovery/ParticipantIndex`` (see the description of ‘PI’
in `Controlling port numbers`_) to ensure that the configured port number indeed
corresponds to the port number the remote DDSI implementation is listening on, and
therefore is really attractive only when it is known that there is but a single DDSI
process on that node.


.. _`Asymmetrical discovery`:

Asymmetrical discovery
......................

On reception of an SPDP packet, the addresses advertised in the packet are added to the
set of addresses to which SPDP packets are sent periodically, allowing asymmetrical
discovery.  In an extreme example, if SPDP multicasting is disabled entirely, host A has
the address of host B in its peer list and host B has an empty peer list, then B will
eventually discover A because of an SPDP message sent by A, at which point it adds A’s
address to its own set and starts sending its own SPDP message to A, allowing A to
discover B.  This takes a bit longer than normal multicast based discovery, though, and
risks writers being blocked by unresponsive readers.


.. _`Timing of SPDP packets`:

Timing of SPDP packets
......................

The interval with which the SPDP packets are transmitted is configurable as well, using
the Discovery/SPDPInterval setting.  A longer interval reduces the network load, but
also increases the time discovery takes, especially in the face of temporary network
disconnections.


.. _`Endpoint discovery`:

Endpoint discovery
..................

Although the SEDP protocol never requires any configuration, network partitioning does
interact with it: so-called ‘ignored partitions’ can be used to instruct Eclipse Cyclone DDS to
completely ignore certain DCPS topic and partition combinations, which will prevent data
for these topic/partition combinations from being forwarded to and from the network.


.. _`Combining multiple participants`:

Combining multiple participants
===============================

If a single process creates multiple participants, these are faithfully mirrored in DDSI
participants and so a single process can appear as if it is a large system with many
participants.  The ``Internal/SquashParticipants`` option can be used to simulate the
existence of only one participant, which owns all endpoints on that node.  This reduces
the background messages because far fewer liveliness assertions need to be sent, but
there are some downsides.

Firstly, the liveliness monitoring features that are related to domain participants will
be affected if multiple DCPS domain participants are combined into a single DDSI domain
participant.  For the ‘automatic’ liveliness setting, this is not an issue.

Secondly, this option makes it impossible for tooling to show the actual system
topology.

Thirdly, the QoS of this sole participant is simply that of the first participant
created in the process.  In particular, no matter what other participants specify as
their ‘user data’, it will not be visible on remote nodes.

There is an alternative that sits between squashing participants and normal operation,
and that is setting ``Internal/BuiltinEndpointSet`` to ``minimal``. In the default
setting, each DDSI participant handled has its own writers for built-in topics and
publishes discovery data on its own entities, but when set to ‘minimal’, only the first
participant has these writers and publishes data on all entities. This is not fully
compatible with other implementations as it means endpoint discovery data can be
received for a participant that has not yet been discovered.


.. _`Controlling port numbers`:

Controlling port numbers
========================

The port numbers used by by Eclipse Cyclone DDS are determined as follows, where the first two
items are given by the DDSI specification and the third is unique to Eclipse Cyclone DDS as a
way of serving multiple participants by a single DDSI instance:

+ 2 ‘well-known’ multicast ports: ``B`` and ``B+1``
+ 2 unicast ports at which only this instance is listening: ``B+PG*PI+10`` and
  ``B+PG*PI+11``
+ 1 unicast port per domain participant it serves, chosen by the kernel
  from the anonymous ports, *i.e.* >= 32768

where:

+ *B* is ``Discovery/Ports/Base`` (``7400``) + ``Discovery/Ports/DomainGain``
  (``250``) * ``Domain/Id``
+ *PG* is ``Discovery/Ports/ParticipantGain`` (``2``)
+ *PI* is ``Discovery/ParticipantIndex``

The default values, taken from the DDSI specification, are in parentheses.  There are
actually even more parameters, here simply turned into constants as there is absolutely
no point in ever changing these values; however, they *are* configurable and the
interested reader is referred to the DDSI 2.1 or 2.2 specification, section 9.6.1.

PI is the most interesting, as it relates to having multiple processes in the same
domain on a single node. Its configured value is either *auto*, *none* or a non-negative
integer.  This setting matters:

+ When it is *auto*, Eclipse Cyclone DDS probes UDP port numbers on
  start-up, starting with PI = 0, incrementing it by one each time until it finds a pair
  of available port numbers, or it hits the limit.  The maximum PI it will ever choose
  is ``Discovery/MaxAutoParticipantIndex`` as a way of limiting the cost of unicast
  discovery.
+ When it is *none* (which is the default) it simply ignores the ‘participant index’
  altogether and asks the kernel to pick random ports (>= 32768).  This eliminates
  the limit on the number of standalone deployments on a single machine and works
  just fine with multicast discovery while complying with all other parts of the
  specification for interoperability. However, it is incompatible with unicast discovery.
+ When it is a non-negative integer, it is simply the value of PI in the above
  calculations.  If multiple processes on a single machine are needed, they will need
  unique values for PI, and so for standalone deployments this particular alternative is
  hardly useful.

Clearly, to fully control port numbers, setting ``Discovery/ParticipantIndex`` (= PI) to
a hard-coded value is the only possibility.  By fixing PI, the port numbers needed for
unicast discovery are fixed as well.  This allows listing peers as IP:PORT pairs,
significantly reducing traffic, as explained in the preceding subsection.

The other non-fixed ports that are used are the per-domain participant ports, the third
item in the list.  These are used only because there exist some DDSI implementations
that assume each domain participant advertises a unique port number as part of the
discovery protocol, and hence that there is never any need for including an explicit
destination participant id when intending to address a single domain participant by
using its unicast locator.  Eclipse Cyclone DDS never makes this assumption, instead opting to
send a few bytes extra to ensure the contents of a message are all that is needed.  With
other implementations, you will need to check.

If all DDSI implementations in the network include full addressing information in the
messages like Eclipse Cyclone DDS does, then the per-domain participant ports serve no purpose
at all.  The default ``false`` setting of ``Compatibility/ManySocketsMode`` disables the
creation of these ports.

This setting can have a few other side benefits as well, as there will may be multiple
DCPS participants using the same unicast locator.  This improves the chances of a single
unicast sufficing even when addressing a multiple participants.


.. _`Data path configuration`:

Data path configuration
***********************

.. _`Retransmit merging`:

Retransmit merging
==================

A remote reader can request retransmissions whenever it receives a Heartbeat and detects
samples are missing.  If a sample was lost on the network for many or all readers, the
next heartbeat is likely to trigger a ‘storm’ of retransmission requests.  Thus, the
writer should attempt merging these requests into a multicast retransmission, to avoid
retransmitting the same sample over & over again to many different readers.  Similarly,
while readers should try to avoid requesting retransmissions too often, in an
interoperable system the writers should be robust against it.

In Eclipse Cyclone DDS, upon receiving a Heartbeat that indicates samples are missing, a reader
will schedule the second and following retransmission requests to be sent after
``Internal/NackDelay`` or combine it with an already scheduled request if possible.  Any
samples received in between receipt of the Heartbeat and the sending of the AckNack will
not need to be retransmitted.

Secondly, a writer attempts to combine retransmit requests in two different ways.  The
first is to change messages from unicast to multicast when another retransmit request
arrives while the retransmit has not yet taken place.  This is particularly effective
when bandwidth limiting causes a backlog of samples to be retransmitted.  The behaviour
of the second can be configured using the ``Internal/RetransmitMerging`` setting.  Based
on this setting, a retransmit request for a sample is either honoured unconditionally,
or it may be suppressed (or ‘merged’) if it comes in shortly after a multicasted
retransmission of that very sample, on the assumption that the second reader will likely
receive the retransmit, too.  The ``Internal/RetransmitMergingPeriod`` controls the
length of this time window.


.. _`Retransmit backlogs`:

Retransmit backlogs
===================

Another issue is that a reader can request retransmission of many samples at once.  When
the writer simply queues all these samples for retransmission, it may well result in a
huge backlog of samples to be retransmitted.  As a result, the ones near the end of the
queue may be delayed by so much that the reader issues another retransmit request.

Therefore, Eclipse Cyclone DDS limits the number of samples queued for retransmission and
ignores (those parts of) retransmission requests that would cause the retransmit queue
to contain too many samples or take too much time to process. There are two settings
governing the size of these queues, and the limits are applied per timed-event thread.
The first is ``Internal/MaxQueuedRexmitMessages``, which limits the number of retransmit
messages, the second ``Internal/MaxQueuedRexmitBytes`` which limits the number of bytes.
The latter defaults to a setting based on the combination of the allowed transmit
bandwidth and the ``Internal/NackDelay`` setting, as an approximation of the likely time
until the next potential retransmit request from the reader.


.. _`Controlling fragmentation`:

Controlling fragmentation
=========================

Samples in DDS can be arbitrarily large, and will not always fit within a single
datagram.  DDSI has facilities to fragment samples so they can fit in UDP datagrams, and
similarly IP has facilities to fragment UDP datagrams to into network packets.  The DDSI
specification states that one must not unnecessarily fragment at the DDSI level, but
Eclipse Cyclone DDS simply provides a fully configurable behaviour.

If the serialised form of a sample is at least ``Internal/FragmentSize``,
it will be fragmented using the DDSI fragmentation. All but the last fragment
will be exactly this size; the last one may be smaller.

Control messages, non-fragmented samples, and sample fragments are all subject to
packing into datagrams before sending it out on the network, based on various attributes
such as the destination address, to reduce the number of network packets.  This packing
allows datagram payloads of up to ``Internal/MaxMessageSize``, overshooting this size if
the set maximum is too small to contain what must be sent as a single unit.  Note that
in this case, there is a real problem anyway, and it no longer matters where the data is
rejected, if it is rejected at all.  UDP/IP header sizes are not taken into account in
this maximum message size.

The IP layer then takes this UDP datagram, possibly fragmenting it into multiple packets
to stay within the maximum size the underlying network supports.  A trade-off to be made
is that while DDSI fragments can be retransmitted individually, the processing overhead
of DDSI fragmentation is larger than that of UDP fragmentation.


.. _`Receive processing`:

Receive processing
==================

Receiving of data is split into multiple threads:

+ A single receive thread responsible for retrieving network packets and running
  the protocol state machine;
+ A delivery thread dedicated to processing DDSI built-in data: participant
  discovery, endpoint discovery and liveliness assertions;
+ One or more delivery threads dedicated to the handling of application data:
  deserialisation and delivery to the DCPS data reader caches.

The receive thread is responsible for retrieving all incoming network packets, running
the protocol state machine, which involves scheduling of AckNack and Heartbeat messages
and queueing of samples that must be retransmitted, and for defragmenting and ordering
incoming samples.

Fragmented data first enters the defragmentation stage, which is per proxy writer.  The
number of samples that can be defragmented simultaneously is limited, for reliable data
to ``Internal/DefragReliableMaxSamples`` and for unreliable data to
``Internal/DefragUnreliableMaxSamples``.

Samples (defragmented if necessary) received out of sequence are buffered, primarily per
proxy writer, but, secondarily, per reader catching up on historical (transient-local)
data.  The size of the first is limited to ``Internal/PrimaryReorderMaxSamples``, the
size of the second to ``Internal/SecondaryReorderMaxSamples``.
   
In between the receive thread and the delivery threads sit queues, of which the maximum
size is controlled by the ``Internal/DeliveryQueueMaxSamples`` setting.  Generally there
is no need for these queues to be very large (unless one has very small samples in very
large messages), their primary function is to smooth out the processing when batches of
samples become available at once, for example following a retransmission.

When any of these receive buffers hit their size limit and it concerns application data,
the receive thread of will wait for the queue to shrink (a compromise that is the lesser
evil within the constraints of various other choices).  However, discovery data will
never block the receive thread.


.. _`Minimising receive latency`:

Minimising receive latency
==========================

In low-latency environments, a few microseconds can be gained by processing the
application data directly in the receive thread, or synchronously with respect to the
incoming network traffic, instead of queueing it for asynchronous processing by a
delivery thread. This happens for data transmitted with the *max_latency* QoS setting at
most a configurable value and the *transport_priority* QoS setting at least a
configurable value. By default, these values are ``inf`` and the maximum transport
priority, effectively enabling synchronous delivery for all data.


.. _`Maximum sample size`:

Maximum sample size
===================

Eclipse Cyclone DDS provides a setting, ``Internal/MaxSampleSize``, to control the maximum size
of samples that the service is willing to process. The size is the size of the (CDR)
serialised payload, and the limit holds both for built-in data and for application data.
The (CDR) serialised payload is never larger than the in-memory representation of the
data.

On the transmitting side, samples larger than ``MaxSampleSize`` are dropped with a
warning in the.  Eclipse Cyclone DDS behaves as if the sample never existed.

Similarly, on the receiving side, samples large than ``MaxSampleSize`` are dropped as
early as possible, immediately following the reception of a sample or fragment of one,
to prevent any resources from being claimed for longer than strictly necessary.  Where
the transmitting side completely ignores the sample, the receiving side pretends the
sample has been correctly received and, at the acknowledges reception to the writer.
This allows communication to continue.

When the receiving side drops a sample, readers will get a *sample lost* notification at
the next sample that does get delivered to those readers.  This condition means that
again checking the info log is ultimately the only truly reliable way of determining
whether samples have been dropped or not.

While dropping samples (or fragments thereof) as early as possible is beneficial from
the point of view of reducing resource usage, it can make it hard to decide whether or
not dropping a particular sample has been recorded in the log already.  Under normal
operational circumstances, only a single message will be recorded for each sample
dropped, but it may on occasion report multiple events for the same sample.

Finally, it is technically allowed to set ``MaxSampleSize`` to very small sizes,
even to the point that the discovery data can’t be communicated anymore.
The dropping of the discovery data will be duly reported, but the usefulness
of such a configuration seems doubtful.


.. _`Network partition configuration`:

Network partition configuration
*******************************

.. _`Network partition configuration overview`:

Network partition configuration overview
========================================

Network partitions introduce alternative multicast addresses for data.  In the DDSI
discovery protocol, a reader can override the default address at which it is reachable,
and this feature of the discovery protocol is used to advertise alternative multicast
addresses. The DDSI writers in the network will (also) multicast to such an alternative
multicast address when multicasting samples or control data.

The mapping of a DCPS data reader to a network partition is indirect: first the DCPS
partitions and topic are matched against a table of *partition mappings*,
partition/topic combinations to obtain the name of a network partition, then the network
partition name is used to find a addressing information..  This makes it easier to map
many different partition/topic combinations to the same multicast address without having
to specify the actual multicast address many times over.

If no match is found, the default multicast address is used.


.. _`Matching rules`:

Matching rules
==============

Matching of a DCPS partition/topic combination proceeds in the order in which the
partition mappings are specified in the configuration.  The first matching mapping is
the one that will be used. The ``*`` and ``?`` wildcards are available for the DCPS
partition/topic combination in the partition mapping.

As mentioned earlier, Eclipse Cyclone DDS can be instructed to ignore all DCPS data
readers and writers for certain DCPS partition/topic combinations through the use of
*IgnoredPartitions*.  The ignored partitions use the same matching rules as normal
mappings, and take precedence over the normal mappings.


.. _`Multiple matching mappings`:

Multiple matching mappings
==========================

A single DCPS data reader can be associated with a set of partitions, and each
partition/topic combination can potentially map to a different network partitions. In
this case, the first matching network partition will be used. This does not affect what
data the reader will receive; it only affects the addressing on the network.


.. _`Thread configuration`:

Thread configuration
********************

Eclipse Cyclone DDS creates a number of threads and each of these threads has a number of
properties that can be controlled individually.  The properties that can be controlled
are:

+ stack size,
+ scheduling class, and
+ scheduling priority.

The threads are named and the attribute ``Threads/Thread[@name]`` is used to set the
properties by thread name.  Any subset of threads can be given special properties;
anything not specified explicitly is left at the default value.

The following threads exist:

+ *gc*: garbage collector, which sleeps until garbage collection is requested for an
  entity, at which point it starts monitoring the state of Eclipse Cyclone DDS, pushing the
  entity through whatever state transitions are needed once it is safe to do so, ending
  with the freeing of the memory.
+ *recv*: accepts incoming network packets from all sockets/ports, performs all protocol
  processing, queues (nearly) all protocol messages sent in response for handling by the
  timed-event thread, queues for delivery or, in special cases, delivers it directly to
  the data readers.
+ *dq.builtins*: processes all discovery data coming in from the network.
+ *lease*: performs internal liveliness monitoring of Eclipse Cyclone DDS.
+ *tev*: timed-event handling, used for all kinds of things, such as: periodic
  transmission of participant discovery and liveliness messages, transmission of control
  messages for reliable writers and readers (except those that have their own
  timed-event thread), retransmitting of reliable data on request (except those that
  have their own timed-event thread), and handling of start-up mode to normal mode
  transition.

and, for each defined channel:

+ *dq.channel-name*: deserialisation and asynchronous delivery of all user data.
+ *tev.channel-name*: channel-specific ‘timed-event’ handling: transmission of control
  messages for reliable writers and readers and retransmission of data on request.
  Channel-specific threads exist only if the configuration includes an element for it or
  if an auxiliary bandwidth limit is set for the channel.

When no channels are explicitly defined, there is one channel named *user*.


.. _`Reporting and tracing`:

Reporting and tracing
*********************

Eclipse Cyclone DDS can produce highly detailed traces of all traffic and internal activities.
It enables individual categories of information, as well as having a simple verbosity
level that enables fixed sets of categories.

The categorisation of tracing output is incomplete and hence most of the verbosity
levels and categories are not of much use in the current release.  This is an ongoing
process and here we describe the target situation rather than the current situation.

All *fatal* and *error* messages are written both to the trace and to the
``cyclonedds-error.log`` file; similarly all ‘warning’ messages are written to the trace
and the ``cyclonedds-info.log`` file.

The Tracing element has the following sub elements:

+ *Verbosity*:
  selects a tracing level by enabled a pre-defined set of categories. The
  list below gives the known tracing levels, and the categories they enable:

  - *none*
  - *severe*: ‘error’ and ‘fatal’
  - *warning*, *info*: severe + ‘warning’
  - *config*: info + ‘config’
  - *fine*: config + ‘discovery’
  - *finer*: fine + ‘traffic’, ‘timing’ and ‘info’
  - *finest*: fine + ‘trace’

+ *EnableCategory*:
  a comma-separated list of keywords, each keyword enabling
  individual categories. The following keywords are recognised:

  - *fatal*: all fatal errors, errors causing immediate termination
  - *error*: failures probably impacting correctness but not necessarily causing
    immediate termination.
  - *warning*: abnormal situations that will likely not impact correctness.
  - *config*: full dump of the configuration
  - *info*: general informational notices
  - *discovery*: all discovery activity
  - *data*: include data content of samples in traces
  - *timing*: periodic reporting of CPU loads per thread
  - *traffic*: periodic reporting of total outgoing data
  - *tcp*: connection and connection cache management for the TCP support
  - *throttle*: throttling events where the writer stalls because its WHC hit the
    high-water mark
  - *topic*: detailed information on topic interpretation (in particular topic keys)
  - *plist*: dumping of parameter lists encountered in discovery and inline QoS
  - *radmin*: receive buffer administration
  - *whc*: very detailed tracing of WHC content management

In addition, the keyword *trace* enables everything from *fatal* to *throttle*. The
*topic* and *plist* ones are useful only for particular classes of discovery failures;
and *radmin* and *whc* only help in analyzing the detailed behaviour of those two
components and produce very large amounts of output.

+ *OutputFile*: the file to write the trace to
+ *AppendToFile*: boolean, set to ``true`` to append to the trace instead of replacing the
  file.

Currently, the useful verbosity settings are *config*, *fine* and *finest*.

*Config* writes the full configuration to the trace file as well as any warnings or
errors, which can be a good way to verify everything is configured and behaving as
expected.

*Fine* additionally includes full discovery information in the trace, but nothing
related to application data or protocol activities. If a system has a stable topology,
this will therefore typically result in a moderate size trace.

*Finest* provides a detailed trace of everything that occurs and is an
indispensable source of information when analysing problems; however,
it also requires a significant amount of time and results in huge log files.

Whether these logging levels are set using the verbosity level or by enabling the
corresponding categories is immaterial.


.. _`Compatibility and conformance`:

Compatibility and conformance
*****************************

.. _`Conformance modes`:

Conformance modes
=================

Eclipse Cyclone DDS operates in one of three modes: *pedantic*, *strict* and *lax*; the mode is
configured using the ``Compatibility/StandardsConformance`` setting.  The default is
*lax*.

The first, *pedantic* mode, is of such limited utility that it will be removed.

The second mode, *strict*, attempts to follow the *intent* of the specification while
staying close to the letter of it. Recent developments at the OMG have resolved these
issues and this mode is no longer of any value.

The default mode, *lax*, attempts to work around (most of) the deviations of other
implementations, and generally provides good interoperability without any further
settings.  In lax mode, the Eclipse Cyclone DDS not only accepts some invalid messages, it will
even transmit them.  The consequences for interoperability of not doing this are simply
too severe.  It should be noted that if one configures two Eclipse Cyclone DDS processes with
different compliancy modes, the one in the stricter mode will complain about messages
sent by the one in the less strict mode.


.. _`Compatibility issues with RTI`:

Compatibility issues with RTI
-----------------------------

In *lax* mode, there should be no major issues with most topic types when working across
a network, but within a single host there used to be an issue with the way RTI DDS uses,
or attempts to use, its shared memory transport to communicate with peers even when they
clearly advertises only UDP/IP addresses.  The result is an inability to reliably
establish bidirectional communication between the two.

Disposing data may also cause problems, as RTI DDS leaves out the serialised key value
and instead expects the reader to rely on an embedded hash of the key value.  In the
strict modes, Eclipse Cyclone DDS requires a proper key value to be supplied; in the relaxed
mode, it is willing to accept key hash, provided it is of a form that contains the key
values in an unmangled form.

If an RTI DDS data writer disposes an instance with a key of which the serialised
representation may be larger than 16 bytes, this problem is likely to occur. In
practice, the most likely cause is using a key as string, either unbounded, or with a
maximum length larger than 11 bytes. See the DDSI specification for details.

In *strict* mode, there is interoperation with RTI DDS, but at the cost of incredibly
high CPU and network load, caused by a Heartbeats and AckNacks going back-and-forth
between a reliable RTI DDS data writer and a reliable Eclipse Cyclone DDS data reader. The
problem is that once Eclipse Cyclone DDS informs the RTI writer that it has received all data
(using a valid AckNack message), the RTI writer immediately publishes a message listing
the range of available sequence numbers and requesting an acknowledgement, which becomes
an endless loop.

There is furthermore also a difference of interpretation of the meaning of the
‘autodispose_unregistered_instances’ QoS on the writer.  Eclipse Cyclone DDS aligns with
OpenSplice.
