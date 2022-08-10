.. _`DDSI Concepts`:

######################
DDSI Concepts
######################

The DDSI standard is intimately related to the DDS 1.2 and 1.4 standards, with a clear
correspondence between the entities in DDSI and those in DCPS.  However, this
correspondence is not one-to-one.

In this section we give a high-level description of the concepts of the DDSI
specification, with hardly any reference to the specifics of the Eclipse Cyclone DDS
implementation (addressed in :ref:`Eclipse Cyclone DDS Specifics`). This division
aids readers interested in interoperability in understanding where the specification ends
and the Eclipse Cyclone DDS implementation begins.


.. _`Mapping of DCPS domains to DDSI domains`:

***************************************
Mapping of DCPS Domains to DDSI Domains
***************************************

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

*****************************************
Mapping of DCPS Entities to DDSI Entities
*****************************************

Each DCPS domain participant in a domain is mirrored in DDSI as a DDSI participant.
These DDSI participants drive the discovery of participants, readers, and writers in DDSI
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

*****************************************
Reliable Communication
*****************************************

*Best-effort* communication is simply a wrapper around UDP/IP: the packet(s) containing
a sample are sent to the addresses at which the readers reside.  No state is maintained
on the writer.  If a packet is lost, the reader will simply ignore the samples
contained in the lost packet and continue with the next one.

When *reliable* communication is used, the writer maintains a copy of the sample, in
case a reader detects it has lost packets and requests a re-transmission.  These copies
are stored in the writer history cache (or *WHC*) of the DDSI writer.  The DDSI writer
is required to periodically send *Heartbeats* to its readers to ensure that all readers
will learn of the presence of new samples in the WHC even when packets get lost.  It is
allowed to suppress these periodic Heartbeats if all samples in the WHC have
been acknowledged by all matched readers. Eclipse Cyclone DDS exploits this freedom.

If a reader receives a Heartbeat and detects it did not receive all samples, it requests
a re-transmission by sending an *AckNack* message to the writer.  The timing of this is
somewhat adjustable and it is worth remarking that a roundtrip latency longer than the
Heartbeat interval easily results in multiple re-transmit requests for a single sample.
In addition to requesting re-transmission of some samples, a reader also uses the AckNack
messages to inform the writer up to which sample it has received everything, and which
ones it has not yet received.  Whenever the writer indicates it requires a response to a
Heartbeat the readers will send an AckNack message even when no samples are missing.  In
this case, it becomes a pure acknowledgement.

The combination of these behaviours in principle allows the writer to remove old samples
from its WHC when it fills up the cache, and allows readers to reliably receive all data.  A
complication exists in the case of unresponsive readers, readers that do not respond to
a Heartbeat at all, or that for some reason fail to receive some samples despite
re-sending them.  The specification does not define how to handle these situations.  The
default behaviour of Eclipse Cyclone DDS is to never consider readers unresponsive, but it can
be configured to consider them so after a certain length of time has passed at which
point the participant containing the reader is undiscovered.

Note that while this Heartbeat/AckNack mechanism is straightforward, the
specification actually allows suppressing heartbeats, merging of AckNacks and
re-transmissions, etc.  The use of these techniques is required to allow for a performant
DDSI implementation, whilst avoiding the need for sending redundant messages.


.. _`DDSI-specific transient-local behaviour`:

***************************************
DDSI-Specific Transient-Local Behaviour
***************************************

The above describes the essentials of the mechanism used for samples of the *volatile*
durability kind, but the DCPS specification also provides *transient-local*, *transient*
and *persistent* data.  Of these, the DDSI specification at present only covers
*transient-local*, and this is the only form of durable data available when
inter-operating across vendors.

In DDSI, transient-local data is implemented using the WHC that is normally used for
reliable communication.  For transient-local data, samples are retained even when all
readers have acknowledged them. With the default history setting of ``KEEP_LAST`` with
``history_depth = 1``, this means that late-joining readers can still obtain the latest
sample for each existing instance.

Naturally, once the DCPS writer is deleted (or disappears for whatever reason), the DDSI
writer disappears as well, and with it, its history.  For this reason, transient data is
generally preferred over transient-local data.  Eclipse Cyclone DDS has a facility
for retrieving transient data from an suitably configured OpenSplice node, but does not
yet include a native service for managing transient data.


.. _`Discovery of participants & endpoints`:

***************************************
Discovery of Participants & Endpoints
***************************************

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
