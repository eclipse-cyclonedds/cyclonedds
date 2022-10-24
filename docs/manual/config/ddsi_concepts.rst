.. _`DDSI Concepts`:

#############
DDSI Concepts
#############

The DDSI standard is intimately related to the DDS 1.2 and 1.4 standards, with a clear
correspondence between the entities in DDSI and those in DCPS. However, this
correspondence is not one-to-one.

In this section we give a high-level description of the concepts of the DDSI
specification, with hardly any reference to the specifics of the |var-project|
implementation (addressed in :ref:`cyclonedds_specifics`). This division
helps readers interested in interoperability to understand where the specification ends
and the |var-project| implementation begins.


.. _`Mapping of DCPS domains to DDSI domains`:

***************************************
Mapping of DCPS Domains to DDSI Domains
***************************************

In DCPS, a domain is uniquely identified by a non-negative integer, the domain id. In
the UDP/IP mapping, this domain id is mapped to port numbers to be used for
communicating with the peer nodes. These port numbers are of significance for
the discovery protocol. This mapping of domain ids to UDP/IP port numbers ensures
that accidental cross-domain communication is impossible with the default mapping.

DDSI does not communicate the DCPS port number in the discovery protocol; it assumes
that each domain id maps to a unique port number. While it is unusual to change the
mapping, the specification requires this to be possible, and this means that two
different DCPS domain ids can be mapped to a single DDSI domain.


.. _`Mapping of DCPS entities to DDSI entities`:

*****************************************
Mapping of DCPS Entities to DDSI Entities
*****************************************

Each DCPS domain participant in a domain is mirrored in DDSI as a DDSI participant.
These DDSI participants drive the discovery of participants, Readers, and Writers in DDSI
via the discovery protocols. By default, each DDSI participant has a unique address on
the network in the form of its own UDP/IP socket with a unique port number.

Any DataReader or DataWriter created by a DCPS domain participant is mirrored in DDSI
as a DDSI Reader or Writer. In this translation, some of the structure of the DCPS
domain is obscured because the standardized parts of DDSI do not know DCPS
Subscribers and Publishers. Instead, each DDSI Reader is the combination of the
corresponding DCPS DataReader and the DCPS Subscriber it belongs to; similarly, each
DDSI Writer is a combination of the corresponding DCPS DataWriter and DCPS Publisher.
This corresponds to how the standardized DCPS built-in topics describe the DCPS
DataReader and DataWriter, as there are no standardized built-in topics for describing
the DCPS Subscribers and Publishers. Implementations can (and do) offer
additional built-in topics representing these entities and include them in the
discovery, but these are non-standard extensions.

In addition to the application-created Readers and Writers (referred to as *endpoints*),
DDSI participants have several DDSI built-in endpoints used for discovery and
liveliness checking/asserting. The most important ones are those required for discovery:
Readers and Writers for the discovery data concerning DDSI participants, DDSI Readers,
and DDSI Writers. Some other ones exist as well, and a DDSI implementation can leave
out some of these if it has no use. For example, if a participant does not have
Writers, it doesn't strictly need the DDSI built-in endpoints for describing Writers,
nor the DDSI built-in endpoint for learning of Readers of other participants.


.. _`Reliable communication`:

*****************************************
Reliable Communication
*****************************************

*Best-effort* communication is simply a wrapper around UDP/IP: the packet(s) containing
a sample are sent to the addresses where the Readers reside. No state is maintained
on the Writer. If a packet is lost, the Reader will ignore the samples contained in the 
lost packet and continue with the next one.

When *reliable* communication is used, the Writer maintains a copy of the sample in
case a Reader detects it has lost packets and requests a re-transmission. These copies
are stored in the Writer history cache (or *WHC*) of the DDSI Writer. The DDSI Writer
must periodically send *Heartbeats* to its Readers to ensure that all Readers
will learn of the presence of new samples in the WHC, even when packets get lost. It can 
suppress these periodic Heartbeats if all matched Readers have acknowledged all samples 
in the WHC. |var-project-short| exploits this freedom.

If a Reader receives a Heartbeat and detects it did not receive all samples, it requests 
a re-transmission by sending an *AckNack* message to the Writer. This timing is somewhat 
adjustable, and it is worth remarking that a roundtrip latency longer than the Heartbeat 
interval quickly results in multiple re-transmit requests for a single sample.
In addition to requesting the re-transmission of some samples, a Reader also uses the 
AckNack messages to inform the Writer of samples received and not received. Whenever the 
Writer indicates it requires a response to a Heartbeat, the Readers will send an AckNack 
message even when no samples are missing. In this case, it simply becomes acknowledgment.

In principle, combining these behaviours allows the Writer to remove old samples from its 
WHC when it fills up the cache, enabling Readers to receive all data reliably. A complication 
exists in the case of unresponsive Readers that do not respond to a Heartbeat or fail to 
receive some samples despite resending them. The specification does not define how to 
handle these situations. The default behaviour of |var-project-short| is never to consider 
Readers unresponsive. However, you can configure it after a certain length of time to 
consider them when the participant containing the Reader is undiscovered.

.. note::

    Although the Heartbeat/AckNack mechanism is straightforward, the specification
    allows suppressing heartbeats, merging AckNacks and re-transmissions, etc. These techniques
    are required to allow for a performant DDSI implementation while avoiding the need for
    sending redundant messages.


.. _`DDSI-specific transient-local behaviour`:

***************************************
DDSI-Specific Transient-Local Behaviour
***************************************

The above describes the essentials of the mechanism used for samples of the *volatile*
durability kind, but the DCPS specification also provides *transient-local*, *transient*,
and *persistent* data. Of these, the DDSI specification currently only covers
*transient-local*, and this is the only form of durable data available when
inter-operating across vendors.

In DDSI, transient-local data is implemented using the WHC that is normally used for
reliable communication. For transient-local data, samples are retained even when all
eaders have acknowledged them. With the default history setting of ``KEEP_LAST`` with
``history_depth = 1``, this means that late-joining Readers can still obtain the latest
sample for each existing instance.

Once the DCPS Writer is deleted (or disappears for whatever reason), the DDSI
Writer and its history are also lost. For this reason, transient data is
typically preferred over transient-local data. |var-project| has a facility
for retrieving transient data from a suitably configured OpenSplice node, but does not
yet include a native service for managing transient data.


.. _`Discovery of participants & endpoints`:

***************************************
Discovery of Participants & Endpoints
***************************************

DDSI participants discover each other using the *Simple Participant Discovery
Protocol* or *SPDP* for short. This protocol is based on periodically sending a message
containing the specifics of the participant to a set of known addresses. By default
this address is a standardised multicast address (``239.255.0.1``; the port number is derived
from the domain id) that all DDSI implementations listen to.

In the SPDP message, the unicast and multicast addresses are particularly important,
as that's where the participant can be reached. Typically, each participant has a
unique unicast address, which means all participants on a node have a different UDP/IP
port number in their unicast address. The actual address (including port number) in a
multicast-capable network is unimportant because all participants will learn them
through these SPDP messages.

The protocol does allow for unicast-based discovery, which requires listing the
addresses of machines where participants may be located and ensuring each participant
uses one of a small set of port numbers.  Because of this, some of the port numbers are
derived not only from the domain id, but also from a *participant index*, which is a
small non-negative integer, unique to a participant within a node. (|var-project| adds an
indirection and uses at most one participant index for a domain for each process,
regardless of how many DCPS participants are created by the process.)

Once two participants have discovered each other and both have matched the DDSI built-in
endpoints their peer is advertising in the SPDP message, the *Simple Endpoint Discovery
Protocol* or *SEDP* takes over, exchanging information on the DCPS Readers and writers
(and for |var-project|, also publishers, subscribers and topics in a manner
compatible with OpenSplice) in the two participants.

The SEDP data is handled as reliable, transient-local data. Therefore, the SEDP Writers
send Heartbeats, the SEDP Readers detect they have not yet received all samples and send
AckNacks requesting retransmissions, the Writer responds to these and eventually
receives a pure acknowledgement informing it that the Reader has now received the
complete set.

.. note::

    The discovery process creates a burst of traffic each time a participant is
    added to the system: *all* existing participants respond to the SPDP message, which all
    start exchanging SEDP data.