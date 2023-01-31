.. index:: ! Multicast.. 

.. _multicasting_bm:

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

.. important::
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
