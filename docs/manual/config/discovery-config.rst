.. _discovery_config:

#######################
Discovery configuration
#######################

.. index:: IPv4, IPv6

.. _discovery_addresses:

Discovery addresses
===================

The DDSI discovery protocols: 

- Simple Participant Discovery Protocol (SPDP) for the *domain participants* (usually 
  operates well without any explicit configuration). 
- Simple Endpoint Discovery Protocol (SEDP) for their *endpoints* (never requires 
  configuration, see :ref:`endpoint_discovery`).

For each domain participant, the SPDP protocol periodically sends an SPDP sample to a
set of addresses (the default only contains the multicast address):

- IPv4 (``239.255.0.1``) 
- IPv6 (``ff02::ffff:239.255.0.1``)

To override the address, set the: 
:ref:`Discovery/SPDPMulticastAddress <//CycloneDDS/Domain/Discovery/SPDPMulticastAddress>`
(requires a valid multicast address).

In addition (or as an alternative) to the multicast-based discovery, any number of unicast addresses can 
be configured as 'addresses to be contacted', by specifying peers in: 
:ref:`Discovery/Peers <//CycloneDDS/Domain/Discovery/Peers>`. Each time an 
SPDP message is sent, it is sent to all of these addresses.

The default behaviour is to include each IP address several times in the set of addresses
(for participant indices 0 through 
:ref:`Discovery/MaxAutoParticipantIndex <//CycloneDDS/Domain/Discovery/MaxAutoParticipantIndex>`).
Each IP address then has a different UDP port number, each corresponding to a participant index. 
Configuring several peers in this way causes a large burst of packets to be sent each 
time an SPDP message is sent out, and each local DDSI participant causes a burst of 
its own messages. Because most participant indices are not used, this is wasteful behaviour and is 
only attractive when it is known that there is a single DDSI process on that node.

.. todo:: clarify the above section.

To avoid sending large numbers of packets to each host (that differ only in port number),
add the port number to the IP address (formatted as ``IP:PORT``). This requires manually 
calculating the port number.

To ensure that the configured port number corresponds to the port number that the remote 
DDSI implementation is listening on, also edit the participant index by setting: 
:ref:`Discovery/ParticipantIndex <//CycloneDDS/Domain/Discovery/ParticipantIndex>` 
(see the description of "PI" in :ref:`port_numbers`).

.. index:: Asymmetrical discovery

Asymmetrical discovery
======================

On receipt of an SPDP packet, the addresses in the packet are added to the set of 
addresses to which SPDP packets are periodically sent. For example:

If SPDP multicasting is disabled entirely: 

- Host A has the address of host B in its peer list.
- Host B has an empty peer list.
 
B eventually discovers A because of an SPDP message sent by A, at which point it 
adds A's address to its own set and starts sending its SPDP message to A, therefore 
allowing A to discover B. This takes longer than normal multicast based discovery, 
and risks Writers being blocked by unresponsive Readers.

.. index:: Timing of SPDP,

Timing of SPDP packets
======================

To configure the interval with which the SPDP packets are transmitted, set 
:ref:`Discovery/SPDPInterval <//CycloneDDS/Domain/Discovery/SPDPInterval>`. 

.. note::
  A longer interval reduces the network load, but also increases the time discovery takes
  (especially in the face of temporary network disconnections).

.. index:: Partition, Ignored partitions, Endpoint discovery

.. _endpoint_discovery:

Endpoint discovery
==================

Although the SEDP protocol never requires any configuration, network partitioning does
interact with it. 

To completely ignore specific DCPS topics and partition combinations, set the 
:ref:`Partitioning/IgnoredPartitions <//CycloneDDS/Domain/Partitioning/IgnoredPartitions>`.
This option prevents data for these topic/partition combinations from being forwarded to 
and from the network.
