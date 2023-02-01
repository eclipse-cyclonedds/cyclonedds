.. index::
    single: Discovery; DDSI Participants and endpoints
    single: Endpoints; DDSI discovery
    single: Participants; DDSI discovery
    single: DDSI; Discovery of participants and endpoints

.. _discovery_participants_endpoints:

********************************************
Discovery of DDSI participants and endpoints
********************************************

DDSI participants discover each other using the *Simple Participant Discovery
Protocol* (SPDP). This protocol periodically sends a message containing the specifics 
of the participant to a set of known addresses. By default, this address is a 
standardised multicast address (``239.255.0.1`` where the port number is derived
from the domain ID) that all DDSI implementations listen to.

In the SPDP message, the unicast and multicast addresses are important, as that is where 
the participant can be reached. Typically, each participant has a unique unicast address, 
which means all participants on a node have a different UDP/IP port number in their unicast 
address. The actual address (including port number) in a multicast-capable network is 
unimportant because all participants learn them through the SPDP messages.

The SPDP protocol allows for unicast-based discovery. This requires listing the
addresses of machines where participants are located, and ensuring that each participant
uses one of a small set of port numbers. Because of this, some port numbers are
derived from the domain ID and a *participant index*, which is a small non-negative integer, 
unique to a participant within a node. |var-project| adds an indirection and uses one 
participant index for a domain for each process, regardless of how many DCPS participants 
are created by the process.

When two participants have discovered each other and both have matched the DDSI built-in
endpoints that their peer is advertising in the SPDP message, the *Simple Endpoint Discovery
Protocol* (SEDP) takes over. The SEDP exchanges information about the DCPS Readers and 
writers with the two participants. For |var-project|, SEDP also exchanges information about 
publishers, subscribers and topics in a manner compatible with OpenSplice.

The SEDP data is handled as reliable (see :ref:`reliable_coms`), transient-local data 
(see :ref:`DDSI-specific transient-local behaviour`). Therefore, the SEDP Writers
send Heartbeats. If the SEDP Readers detect they have not yet received all samples and send
AckNacks requesting re-transmissions, the Writer responds to these and eventually
receives a pure acknowledgement informing it that the reader has now received the
complete set.

.. note::

    The discovery process creates a burst of traffic each time a participant is
    added to the system: **all** existing participants respond to the SPDP message, which all
    start exchanging SEDP data.
