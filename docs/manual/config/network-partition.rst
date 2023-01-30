.. index:: Network partition, Partition, Multicast, DCPS

.. _network_partition_config:

*******************************
Network partition configuration
*******************************

Network partitions have alternative multicast addresses for data that is transmitted via 
readers and writers that are mapped to the topic under that partition. 

In the DDSI discovery protocol, a reader can override the default address at which it is 
reachable. This feature of the discovery protocol is used for advertising alternative 
multicast addresses. The DDSI writers in the network multicast to such an alternative 
multicast address when multicasting samples or control data.

The mapping of a :term:`DCPS` data reader to a network partition is indirect: 

#. To obtain the network partition name, the DCPS partitions and topic are matched against 
   a table of *partition mappings* (partition/topic combinations). 
#. The network partition name is used to find addressing information. This makes it easier to map
   many different partition/topic combinations to the same multicast address without specifying
   the actual multicast address many times over.

If no match is found, the default multicast address is used.

.. index:: Matching rules, DCPS, Ignored partitions

.. _matching_rules:

==============
Matching rules
==============

The matching of a DCPS partition/topic combination proceeds in the order in which the
partition mappings are specified in the configuration. The first matching mapping is
the one that is used. The ``*`` and ``?`` wildcards are available for the DCPS
partition/topic combination in the partition mapping.

|var-project| can be instructed to ignore all DCPS data readers and writers for certain 
DCPS partition/topic combinations through the use of 
:ref:`Partitioning/IgnoredPartitions <//CycloneDDS/Domain/Partitioning/IgnoredPartitions>`
(see :ref:`Endpoint discovery <Endpoint discovery>`). The ignored partitions use the same 
matching rules as normal mappings, and take precedence over the normal mappings.

.. _multiple_matching_mappings:

==========================
Multiple matching mappings
==========================

A single DCPS data reader can be associated with a set of partitions, and each partition/topic 
combination can potentially map to different network partitions. The first matching network 
partition is used. 

.. note::
    This does not affect the data that the reader receives. It only affects the addressing on the network.
