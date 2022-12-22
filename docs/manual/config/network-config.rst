.. _`Network configuration`:

.. include:: ../external-links.part.rst

*********************
Network configuration
*********************

.. _`Networking interfaces`:

.. index:: Network interface

=====================
Networking interfaces
=====================

|var-project| can use multiple network interfaces simultaneously (the default is a single 
network interface). The set of enabled interfaces determines the addresses that the host 
advertises in the discovery information (see :ref:`Discovery of participants and endpoints`).

-----------------
Default behaviour
-----------------

To determine the default network interface, the eligible interfaces are ranked by quality, 
and the interface with the highest quality is selected. If there are multiple interfaces of 
the highest quality, it selects the first enumerated one. Eligible interfaces are those 
that are connected and have the correct type of address family (IPv4 or IPv6). Priority is 
determined (in decreasing priority) as follows:

- Interfaces with a non-link-local address are preferred over those with a link-local one.
- Multicast-capable (see 
  :ref:`General/Interfaces/NetworkInterface[@multicast] <//CycloneDDS/Domain/General/Interfaces/NetworkInterface[@multicast]>`).
- Non-multicast capable and not point-to-point.
- Point-to-point.
- Loopback.

If this selection procedure does not automatically return the desired interface, to override
the selection, set: :ref:`General/Interfaces <//CycloneDDS/Domain/General/Interfaces>` adding 
any of the following: 

- Name of the interface (``<NetworkInterface name='interface_name' />``). 
- IP address of the host on the desired interface (``<NetworkInterface address='128.129.0.42' />``).
- Network portion of the IP address for the host on the desired interface (``<NetworkInterface address='128.11.0.0' />``). 

.. note:: 
  An exact match on the address is always preferred, and is the only option that allows 
  selecting the desired interface when multiple addresses are tied to a single interface.

The default address family is IPv4. To change the address family to IPv6, set: 
:ref:`General/Transport <//CycloneDDS/Domain/General/Transport>` to ``udp6`` or ``tcp6``.  

.. note::
  |var-project| does not mix IPv4 and IPv6 addressing. Therefore, all DDSI participants in 
  the network must use the same addressing mode. When inter-operating, this behaviour is 
  the same. That is, it looks at either IPv4 or IPv6 addresses in the advertised address 
  information in the SPDP and SEDP discovery protocols.

IPv6 link-local addresses are considered undesirable because they must be published 
and received via the discovery mechanism (see `Discovery behaviour`). There is no way to 
determine to which interface a received link-local address is related.

If IPv6 is requested and the selected interface has a non-link-local address, |var-project| 
operates in a *global addressing* mode and will only consider discovered non-link-local 
addresses. In this mode, you can select any set of interfaces for listening to multicasts. 

.. note:: 
  This behaviour is identical to that when using IPv4, as IPv4 does not have 
  the formal notion of address scopes that IPv6 has. If only a link-local address is 
  available, |var-project| runs in a *link-local addressing* mode. In this mode, it accepts 
  any address in a discovery packet (assuming that a link-local address is valid on the selected 
  interface). To minimise the risk involved in this assumption, it only allows the selected 
  interface for listening to multicasts.

.. index:: ! Multiple network interfaces, Network interface

---------------------------
Multiple network interfaces
---------------------------

Multiple network interfaces can be used simultaneously by listing multiple 
:ref:`NetworkInterface <//CycloneDDS/Domain/General/Interfaces/NetworkInterface>` elements. 
The default behavoir still applies, but with extended network interfaces. For example, 
the SPDP packets advertise multiple addresses and sends these packets out on all interfaces. 
If link-local addresses are used, the issue with *link-local addressing* gains importance.

In a configuration with a single network interface, it is obvious which one to use for 
sending packets to a peer. When there are multiple network interfaces, it is necessary to 
establish the set of interfaces through which multicasts can be sent (these are sent 
on a specific interface). This in turn requires determining via which subset of interfaces 
a peer is reachable.

|var-project-short| checks which interfaces match the addresses advertised by a peer 
in its SPDP or SEDP messages, which assumes that:

- The peer is attached to at least one of the configured networks.
- That checking the network parts of the addresses results in a subset of the interfaces.
 
The network interfaces in this subset are the interfaces on which the peer is assumed to 
be reachable via multicast. This leaves open two classes of addresses:

- **Loopback addresses**: these are ignored unless:
  
  - The configuration has enabled only loopback interfaces.
  - No other addresses are advertised in the discovery message.
  - A non-loopback address matches that of the machine.

- **Routable addresses that do not match an interface**: these are ignored if the 
  :ref:`General/DontRoute <//CycloneDDS/Domain/General/DontRoute>` option is set, 
  otherwise it is assumed that the network stack knows how to route them, and any of 
  the interfaces may be used.

When a message needs to be sent to a set of peers, |var-project| uses the set of addresses 
spanning the set of intended recipients with the lowest cost. That is, the number of nodes 
that: 

- Receive it without having a use for it.
- Unicast vs multicast. 
- Loopback vs real network interface.
- Configured priority. 

|var-project| uses some heuristics rather than computing the optimal solution. The address 
selection can be influenced in two ways:

- By using the ``priority`` attribute, which is used as an offset in the cost calculation.  
  The default configuration gives loopback interfaces a slightly higher priority than other 
  network types.

- By setting the ``prefer_multicast`` attribute, which raises the assumed cost of a unicast 
  message.

The :ref:`General/RedundantNetworking <//CycloneDDS/Domain/General/RedundantNetworking>` 
setting forces the address selection code to consider all interfaces advertised by a peer.

.. index:: Unicast, Multicast, Partition, DCPS

.. _`Overriding addresses`:

---------------------------------------------------
Overriding addresses/interfaces for Readers/Writers
---------------------------------------------------

The :ref:`Partitioning <//CycloneDDS/Domain/Partitioning>` element in the configuration 
allows configuring :ref:`NetworkPartition <//CycloneDDS/Domain/Partitioning/NetworkPartitions>` 
elements and mapping topic/partition names to these "network partitions" using 
:ref:`PartitionMappings <//CycloneDDS/Domain/Partitioning/PartitionMappings>` elements.

Network partitions introduce alternative multicast addresses for data and/or restrict 
the set of unicast addresses (that is, interfaces). In the DDSI discovery protocol, 
a Reader can override the addresses at which it is reachable, which is a feature of the 
discovery protocol that is used to advertise alternative multicast addresses and/or 
a subset of the unicast addresses. The Writers in the network use the addresses advertised 
by the Reader rather than the default addresses advertised by the Reader's participant.

Unicast and multicast addresses in a network partition play different roles:

- The multicast addresses specify an alternative set of addresses to be used instead of the 
  participant's default. This is particularly useful to limit high-bandwidth flows to the 
  parts of a network where the data is needed (for IP/Ethernet, this assumes switches 
  that are configured to do IGMP snooping).

- The unicast addresses not only influence the set of interfaces are used for unicast, but 
  thereby also the set of interfaces that are considered for use by multicast. For example: 
  specifying a unicast address that matches network interface A, ensures all traffic to that 
  Reader uses interface A, whether unicast or multicast.

Because the typical use of unicast addresses is to force traffic onto certain interfaces, 
the configuration also allows specifying interface names (using the ``interface`` attribute).

The mapping of a data Reader or Writer to a network partition is indirect: 

#. The partition and topic are matched against a table of *partition mappings*, partition/topic 
   combinations to obtain the name of a network partition
#. The network partition name is used to find the addressing information. 

This makes it easier to map many different partition/topic combinations to the same multicast 
address without having to specify the actual multicast address many times over. If no match is 
found, the default addresses are used.

The matching sequence is in the order in which the partition mappings are specified in the 
configuration. The first matching mapping is the one that is used. The ``*`` and ``?`` 
wildcards are available for the DCPS partition/topic combination in the partition mapping.

A single Reader or Writer is associated with a set of partitions, and each partition/topic 
combination can potentially map to a different network partition. In this case, the first 
matching network partition is used. This does not affect the data the Reader receives, it 
only affects the addressing on the network.

.. _`Controlling port numbers`:

.. index:: ! Port numbers

========================
Controlling port numbers
========================

The |var-project| port numbers are configured as follows: 

.. note::
  The first two items are defined by the DDSI specification. The third item is unique to 
  |var-project| as a way of serving multiple participants by a single DDSI instance.

- Two "well-known" multicast ports: ``B`` and ``B+1``.
- Two unicast ports at which only this instance is listening: ``B+PG*PI+10`` and
  ``B+PG*PI+11``
- One unicast port per domain participant it serves, chosen by the kernel from the list of 
  anonymous ports, that is, >= 32768.

where:

- *B* is :ref:`Discovery/Ports/Base <//CycloneDDS/Domain/Discovery/Ports/Base>` (``7400``) + :ref:`Discovery/Ports/DomainGain <//CycloneDDS/Domain/Discovery/Ports/DomainGain>`
  (``250``) * :ref:`Domain[@Id] <//CycloneDDS/Domain[@Id]>`
- *PG* is :ref:`Discovery/Ports/ParticipantGain <//CycloneDDS/Domain/Discovery/Ports/ParticipantGain>` (``2``)
- *PI* is :ref:`Discovery/ParticipantIndex <//CycloneDDS/Domain/Discovery/ParticipantIndex>`

The default values (taken from the DDSI specification) are in parentheses.

.. note:: 
  This shows only a sub-set of the available parameters. The other parameters in the 
  specification have no bearing on |var-project|. However, these are configurable. For
  further information, refer to the |url::dds2.1| or |url::dds2.2| specification, section 9.6.1.

*PI* relates to having multiple processes in the same domain on a single node. Its 
configured value is either *auto*, *none* or a non-negative integer. This setting matters:

+ *none* (default): It ignores the "participant index" altogether and asks the kernel to pick 
  random ports (>= 32768). This eliminates the limit on the number of standalone deployments 
  on a single machine and works well with multicast discovery, while complying with all other 
  parts of the specification for interoperability. However, it is incompatible with unicast discovery.
+ *auto*: |var-project| polls UDP port numbers on start-up, starting with ``PI = 0``, incrementing 
  it by one each time until it finds a pair of available port numbers, or it hits the limit. 
  To limit the cost of unicast discovery, the maximum PI is set in: 
  :ref:`Discovery/MaxAutoParticipantIndex <//CycloneDDS/Domain/Discovery/MaxAutoParticipantIndex>`.
+ *non-negative integer*: It is the value of PI in the above calculations. If multiple processes 
  on a single machine are required, they need unique values for PI, and therefore for standalone 
  deployments, this alternative is of little use.

To fully control port numbers, setting (= PI) to a hard-coded value is the only possibility. 
:ref:`Discovery/ParticipantIndex <//CycloneDDS/Domain/Discovery/ParticipantIndex>` 
By defining PI, the port numbers needed for unicast discovery are fixed as well. This allows 
listing peers as IP:PORT pairs, which significantly reduces traffic.

The other non-fixed ports that are used are the per-domain participant ports, the third
item in the list. These are used only because there exist some DDSI implementations
that assume each domain participant advertises a unique port number as part of the
discovery protocol, and hence that there is never any need for including an explicit
destination participant id when intending to address a single domain participant by
using its unicast locator. |var-project| never makes this assumption, instead opting to
send a few bytes extra to ensure the contents of a message are all that is needed. With
other implementations, you will need to check.

If all DDSI implementations in the network include full addressing information in the
messages like |var-project| does, then the per-domain participant ports serve no purpose
at all. The default ``false`` setting of :ref:`Compatibility/ManySocketsMode <//CycloneDDS/Domain/Compatibility/ManySocketsMode>` disables the
creation of these ports.

This setting can have a few other side benefits, as there may be multiple
DCPS participants using the same unicast locator. This improves the chances of a single
unicast sufficing even when addressing multiple participants.

.. _`Multicasting`:

.. index:: ! Multicast

============
Multicasting
============

You can configure the extent to which :term:`multicast` is used (regular, any-source
multicast, as well as source-specific multicast):

- whether to use multicast for data communications,
- whether to use multicast for participant discovery,
- on which interfaces to listen for multicasts.

We recommend that you use multicasting. However, if there are restrictions on
the use of multicasting, or if the network reliability is dramatically different for
multicast than for unicast, disable multicast for normal communications. To force 
the use of unicast communications for everything, set: 
:ref:`General/AllowMulticast <//CycloneDDS/Domain/General/AllowMulticast>` to ``false``.

We strongly advise you to have multicast-based participant discovery enabled, which avoids
having to specify a list of nodes to contact, and reduces the network load considerably.
To allow participant discovery via multicast while disabling multicast for everything else, set:
:ref:`General/AllowMulticast <//CycloneDDS/Domain/General/AllowMulticast>` to ``spdp`` 

To disable incoming multicasts, or to control from which interfaces multicasts are to be
accepted, set: 
:ref:`General/MulticastRecvNetworkInterfaceAddresses <//CycloneDDS/Domain/General/MulticastRecvNetworkInterfaceAddresses>`
setting. The options are:
 
 - Listening on no interface
 - Preferred
 - All
 - A specific set of interfaces


.. _`TCP support`:

.. index:: ! TCP support

===========
TCP support
===========

The DDSI protocol is a protocol that provides a connectionless transport with
unreliable datagrams. However, there are times where TCP is the only
practical network transport available (for example, across a WAN). This is the reason
|var-project| can use TCP instead of UDP if needed.

The differences in the model of operation between DDSI and TCP are quite large: DDSI is
based on the notion of peers, whereas TCP communication is based on the notion of a
session that is initiated by a "client" and accepted by a "server"; therefore, TCP requires
knowledge of the servers to connect to before the DDSI discovery protocol can exchange
that information. The configuration of this is done in the same manner as for
unicast-based UDP discovery.

TCP reliability is defined in terms of these sessions, but DDSI reliability is defined
in terms of DDSI discovery and liveliness management. It is therefore possible that a
TCP connection is (forcibly) closed while the remote endpoint is still considered alive.
Following a reconnect, the samples lost when the TCP connection was closed can be
recovered via the standard DDSI reliability. This also means that the Heartbeats and
AckNacks still need to be sent over a TCP connection, and consequently that DDSI
flow-control occurs on top of TCP flow-control.

Another point worth noting is that connection establishment potentially takes a long
time, and that giving up on a transmission to a failed or no longer reachable host can
also take a long time. These long delays can be visible at the application level at
present.

.. _`TLS support`:

.. index:: ! TLS support

-----------
TLS support
-----------

The TCP mode can be used together with Thread-Local Storage (TLS) to provide mutual 
authentication and encryption. When TLS is enabled, plain TCP connections are no longer 
accepted or initiated (see :ref:`SSL <//CycloneDDS/Domain/SSL>`). 


.. _`Raw Ethernet support`:

====================
Raw ethernet support
====================

An additional option for Linux only: |var-project| can use a raw Ethernet network interface
to communicate without a configured IP stack.