.. index:: Network interface, IPv4, IPv6

.. _networking_interfaces:

=====================
Networking interfaces
=====================

|var-project| can use multiple network interfaces simultaneously (the default is a single 
network interface). The set of enabled interfaces determines the addresses that the host 
advertises in the discovery information (see :ref:`discovery_participants_endpoints`).

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
  the network must use the same addressing mode. When interoperating, this behaviour is 
  the same. That is, it looks at either IPv4 or IPv6 addresses in the advertised address 
  information in the SPDP and SEDP discovery protocols.

IPv6 link-local addresses are considered undesirable because they must be published 
and received via the discovery mechanism (see :ref:`discovery_behaviour`). There is no way to 
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
The default behaviour still applies, but with extended network interfaces. For example, 
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
