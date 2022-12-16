.. _`Network partition configuration`:

*******************************
Network Partition Configuration
*******************************

.. _`Network partition configuration overview`:

========================================
Network Partition Configuration Overview
========================================

Network partitions introduce alternative multicast addresses for data. In the DDSI
discovery protocol, a Reader can override the default address at which it is reachable,
and this feature of the discovery protocol is used for advertising alternative multicast
addresses. The DDSI Writers in the network will (also) multicast to such an alternative
multicast address when multicasting samples or control data.

The mapping of a DCPS data Reader to a network partition is indirect: first, the DCPS
partitions and topic are matched against a table of *partition mappings*,
partition/topic combinations to obtain the name of a network partition, then the network
partition name is used to find addressing information. This makes it easier to map
many different partition/topic combinations to the same multicast address without specifying
the actual multicast address many times over.

In the case no match is found, the default multicast address is used.


.. _`Matching rules`:

==============
Matching Rules
==============

The matching of a DCPS partition/topic combination proceeds in the order in which the
partition mappings are specified in the configuration. The first matching mapping is
the one that will be used. The ``*`` and ``?`` wildcards are available for the DCPS
partition/topic combination in the partition mapping.

As mentioned earlier, |var-project| can be instructed to ignore all DCPS data
Readers and Writers for certain DCPS partition/topic combinations through the use of
:ref:`Partitioning/IgnoredPartitions <//CycloneDDS/Domain/Partitioning/IgnoredPartitions>`.
The ignored partitions use the same matching rules as normal mappings, and take precedence
over the normal mappings.


.. _`Multiple matching mappings`:

==========================
Multiple Matching Mappings
==========================

A single DCPS data Reader can be associated with a set of partitions, and each
partition/topic combination can potentially map to different network partitions. In
this case, the first matching network partition will be used. This does not affect what
data the Reader will receive; it only affects the addressing on the network.
