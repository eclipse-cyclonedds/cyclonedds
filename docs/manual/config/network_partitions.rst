.. index:: 
  single: Addresses; Unicast
  single: Addresses; Multicast
  single: Partition
  single: Network partition
  single: DCPS
  single: Unicast 
  single: Multicast

.. _network_partitions:

-------------------
Network partitions
-------------------

The :ref:`Partitioning <//CycloneDDS/Domain/Partitioning>` element in the configuration 
allows configuring :ref:`NetworkPartition <//CycloneDDS/Domain/Partitioning/NetworkPartitions>` 
elements and mapping topic/partition names to these "network partitions" using 
:ref:`PartitionMappings <//CycloneDDS/Domain/Partitioning/PartitionMappings>` elements.

Network partitions introduce alternative multicast addresses for data and/or restrict 
the set of unicast addresses (that is, interfaces). In the DDSI discovery protocol, 
a reader can override the addresses at which it is reachable, which is a feature of the 
discovery protocol that is used to advertise alternative multicast addresses and/or 
a subset of the unicast addresses. The writers in the network use the addresses advertised 
by the reader rather than the default addresses advertised by the reader's participant.

Unicast and multicast addresses in a network partition play different roles:

- The multicast addresses specify an alternative set of addresses to be used instead of the 
  participant's default. This is particularly useful to limit high-bandwidth flows to the 
  parts of a network where the data is needed (for IP/Ethernet, this assumes switches 
  that are configured to do IGMP snooping).

- The unicast addresses not only influence the set of interfaces are used for unicast, but 
  thereby also the set of interfaces that are considered for use by multicast. For example: 
  specifying a unicast address that matches network interface A, ensures all traffic to that 
  reader uses interface A, whether unicast or multicast.

The typical use of unicast addresses is to force traffic onto certain interfaces, 
the configuration also allows specifying interface names (using the ``interface`` attribute).

The mapping of a data reader or writer to a network partition is indirect: 

#. The partition and topic are matched against a table of *partition mappings*, partition/topic 
   combinations to obtain the name of a network partition
#. The network partition name is used to find the addressing information. 

This makes it easier to map many different partition/topic combinations to the same multicast 
address without having to specify the actual multicast address many times over. If no match is 
found, the default addresses are used.

The matching sequence is in the order in which the partition mappings are specified in the 
configuration. The first matching mapping is the one that is used. The ``*`` and ``?`` 
wildcards are available for the DCPS partition/topic combination in the partition mapping.

A single reader or writer is associated with a set of partitions, and each partition/topic 
combination can potentially map to a different network partition. In this case, the first 
matching network partition is used. This does not affect the data the reader receives, it 
only affects the addressing on the network.
