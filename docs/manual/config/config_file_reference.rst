.. _`configuration_reference`:

****************************
Configuration File Reference
****************************

Below is the full (generated) reference of XML you can use to configure |var-project|. The title of each section is the XML XPath notation to the relevant option.

.. _`//CycloneDDS`:

//CycloneDDS
############

Children: `//CycloneDDS/Domain`_

CycloneDDS configuration


.. _`//CycloneDDS/Domain`:

//CycloneDDS/Domain
*******************

Attributes: [Id](`//CycloneDDS/Domain[@Id]`_)
Children: `//CycloneDDS/Domain/Compatibility`_, `//CycloneDDS/Domain/Discovery`_, `//CycloneDDS/Domain/General`_, `//CycloneDDS/Domain/Internal`_, `//CycloneDDS/Domain/Partitioning`_, `//CycloneDDS/Domain/SSL`_, `//CycloneDDS/Domain/Security`_, `//CycloneDDS/Domain/SharedMemory`_, `//CycloneDDS/Domain/Sizing`_, `//CycloneDDS/Domain/TCP`_, `//CycloneDDS/Domain/Threads`_, `//CycloneDDS/Domain/Tracing`_

The General element specifying Domain related settings.


.. _`//CycloneDDS/Domain[@Id]`:

//CycloneDDS/Domain[@Id]
************************

Text

Domain id this configuration applies to, or "any" if it applies to all domain ids.

The default value is: ``any``


.. _`//CycloneDDS/Domain/Compatibility`:

//CycloneDDS/Domain/Compatibility
=================================

Children: `//CycloneDDS/Domain/Compatibility/AssumeRtiHasPmdEndpoints`_, `//CycloneDDS/Domain/Compatibility/ExplicitlyPublishQosSetToDefault`_, `//CycloneDDS/Domain/Compatibility/ManySocketsMode`_, `//CycloneDDS/Domain/Compatibility/StandardsConformance`_

The Compatibility element allows you to specify various settings related to compatibility with standards and with other DDSI implementations.


.. _`//CycloneDDS/Domain/Compatibility/AssumeRtiHasPmdEndpoints`:

//CycloneDDS/Domain/Compatibility/AssumeRtiHasPmdEndpoints
----------------------------------------------------------

Boolean

This option assumes ParticipantMessageData endpoints required by the liveliness protocol are present in RTI participants even when not properly advertised by the participant discovery protocol.

The default value is: ``false``


.. _`//CycloneDDS/Domain/Compatibility/ExplicitlyPublishQosSetToDefault`:

//CycloneDDS/Domain/Compatibility/ExplicitlyPublishQosSetToDefault
------------------------------------------------------------------

Boolean

This element specifies whether QoS settings set to default values are explicitly published in the discovery protocol. Implementations are to use the default value for QoS settings not published, which allows a significant reduction of the amount of data that needs to be exchanged for the discovery protocol, but this requires all implementations to adhere to the default values specified by the specifications.

When interoperability is required with an implementation that does not follow the specifications in this regard, setting this option to true will help.

The default value is: ``false``


.. _`//CycloneDDS/Domain/Compatibility/ManySocketsMode`:

//CycloneDDS/Domain/Compatibility/ManySocketsMode
-------------------------------------------------

One of: false, true, single, none, many

This option specifies whether a network socket will be created for each domain participant on a host. The specification seems to assume that each participant has a unique address, and setting this option will ensure this to be the case. This is not the default.

Disabling it slightly improves performance and reduces network traffic somewhat. It also causes the set of port numbers needed by Cyclone DDS to become predictable, which may be useful for firewall and NAT configuration.

The default value is: ``single``


.. _`//CycloneDDS/Domain/Compatibility/StandardsConformance`:

//CycloneDDS/Domain/Compatibility/StandardsConformance
------------------------------------------------------

One of: lax, strict, pedantic

This element sets the level of standards conformance of this instance of the Cyclone DDS Service. Stricter conformance typically means less interoperability with other implementations. Currently, three modes are defined:
 * pedantic: very strictly conform to the specification, ultimately for compliance testing, but currently of little value because it adheres even to what will most likely turn out to be editing errors in the DDSI standard. Arguably, as long as no errata have been published, the current text is in effect, and that is what pedantic currently does.

 * strict: a relatively less strict view of the standard than does pedantic: it follows the established behaviour where the standard is obviously in error.

 * lax: attempt to provide the smoothest possible interoperability, anticipating future revisions of elements in the standard in areas that other implementations do not adhere to, even though there is no good reason not to.


The default value is: ``lax``


.. _`//CycloneDDS/Domain/Discovery`:

//CycloneDDS/Domain/Discovery
=============================

Children: `//CycloneDDS/Domain/Discovery/DSGracePeriod`_, `//CycloneDDS/Domain/Discovery/DefaultMulticastAddress`_, `//CycloneDDS/Domain/Discovery/EnableTopicDiscoveryEndpoints`_, `//CycloneDDS/Domain/Discovery/ExternalDomainId`_, `//CycloneDDS/Domain/Discovery/LeaseDuration`_, `//CycloneDDS/Domain/Discovery/MaxAutoParticipantIndex`_, `//CycloneDDS/Domain/Discovery/ParticipantIndex`_, `//CycloneDDS/Domain/Discovery/Peers`_, `//CycloneDDS/Domain/Discovery/Ports`_, `//CycloneDDS/Domain/Discovery/SPDPInterval`_, `//CycloneDDS/Domain/Discovery/SPDPMulticastAddress`_, `//CycloneDDS/Domain/Discovery/Tag`_

The Discovery element allows you to specify various parameters related to the discovery of peers.


.. _`//CycloneDDS/Domain/Discovery/DSGracePeriod`:

//CycloneDDS/Domain/Discovery/DSGracePeriod
-------------------------------------------

Number-with-unit

This setting controls for how long endpoints discovered via a Cloud discovery service will survive after the discovery service disappears, allowing reconnection without loss of data when the discovery service restarts (or another instance takes over).

Valid values are finite durations with an explicit unit or the keyword 'inf' for infinity. Recognised units: ns, us, ms, s, min, hr, day.

The default value is: ``30 s``


.. _`//CycloneDDS/Domain/Discovery/DefaultMulticastAddress`:

//CycloneDDS/Domain/Discovery/DefaultMulticastAddress
-----------------------------------------------------

Text

This element specifies the default multicast address for all traffic other than participant discovery packets. It defaults to Discovery/SPDPMulticastAddress.

The default value is: ``auto``


.. _`//CycloneDDS/Domain/Discovery/EnableTopicDiscoveryEndpoints`:

//CycloneDDS/Domain/Discovery/EnableTopicDiscoveryEndpoints
-----------------------------------------------------------

Boolean

This element controls whether the built-in endpoints for topic discovery are created and used to exchange topic discovery information.

The default value is: ``false``


.. _`//CycloneDDS/Domain/Discovery/ExternalDomainId`:

//CycloneDDS/Domain/Discovery/ExternalDomainId
----------------------------------------------

Text

An override for the domain id is used to discovery and determine the port number mapping. This allows the creating of multiple domains in a single process while making them appear as a single domain on the network. The value "default" disables the override.

The default value is: ``default``


.. _`//CycloneDDS/Domain/Discovery/LeaseDuration`:

//CycloneDDS/Domain/Discovery/LeaseDuration
-------------------------------------------

Number-with-unit

This setting controls the default participant lease duration.
The unit must be specified explicitly. Recognised units: ns, us, ms, s, min, hr, day.

The default value is: ``10 s``


.. _`//CycloneDDS/Domain/Discovery/MaxAutoParticipantIndex`:

//CycloneDDS/Domain/Discovery/MaxAutoParticipantIndex
-----------------------------------------------------

Integer

This element specifies the maximum DDSI participant index selected by this instance of the Cyclone DDS service if the Discovery/ParticipantIndex is "auto".

The default value is: ``9``


.. _`//CycloneDDS/Domain/Discovery/ParticipantIndex`:

//CycloneDDS/Domain/Discovery/ParticipantIndex
----------------------------------------------

Text

This element specifies the DDSI participant index used by this instance of the Cyclone DDS service for discovery purposes. Only one such participant id is used, independent of the number of actual DomainParticipants on the node. It is either:
 * auto: which will attempt to automatically determine an available participant index (see also Discovery/MaxAutoParticipantIndex), or

 * a non-negative integer less than 120, or

 * none:, which causes it to use arbitrary port numbers for unicast sockets which entirely removes the constraints on the participant index but makes unicast discovery impossible.


The default value is: ``none``


.. _`//CycloneDDS/Domain/Discovery/Peers`:

//CycloneDDS/Domain/Discovery/Peers
-----------------------------------

Children: `//CycloneDDS/Domain/Discovery/Peers/Peer`_

This element statically configures addresses for discovery.


.. _`//CycloneDDS/Domain/Discovery/Peers/Peer`:

//CycloneDDS/Domain/Discovery/Peers/Peer
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Attributes: [Address](`//CycloneDDS/Domain/Discovery/Peers/Peer[@Address]`_)

This element statically configures addresses for discovery.


.. _`//CycloneDDS/Domain/Discovery/Peers/Peer[@Address]`:

//CycloneDDS/Domain/Discovery/Peers/Peer[@Address]
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Text

This element specifies an IP address to which discovery packets must be sent, in addition to the default multicast address (see also General/AllowMulticast). Both hostnames and a numerical IP address are accepted; the hostname or IP address may be suffixed with :PORT to explicitly set the port to which it must be sent. Multiple Peers may be specified.

The default value is: ``<empty>``


.. _`//CycloneDDS/Domain/Discovery/Ports`:

//CycloneDDS/Domain/Discovery/Ports
-----------------------------------

Children: `//CycloneDDS/Domain/Discovery/Ports/Base`_, `//CycloneDDS/Domain/Discovery/Ports/DomainGain`_, `//CycloneDDS/Domain/Discovery/Ports/MulticastDataOffset`_, `//CycloneDDS/Domain/Discovery/Ports/MulticastMetaOffset`_, `//CycloneDDS/Domain/Discovery/Ports/ParticipantGain`_, `//CycloneDDS/Domain/Discovery/Ports/UnicastDataOffset`_, `//CycloneDDS/Domain/Discovery/Ports/UnicastMetaOffset`_

The Ports element specifies various parameters related to the port numbers used for discovery. These all have default values specified by the DDSI 2.1 specification and rarely need to be changed.


.. _`//CycloneDDS/Domain/Discovery/Ports/Base`:

//CycloneDDS/Domain/Discovery/Ports/Base
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Integer

This element specifies the base port number (refer to the DDSI 2.1 specification, section 9.6.1, constant PB).

The default value is: ``7400``


.. _`//CycloneDDS/Domain/Discovery/Ports/DomainGain`:

//CycloneDDS/Domain/Discovery/Ports/DomainGain
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Integer

This element specifies the domain gain, relating domain ids to sets of port numbers (refer to the DDSI 2.1 specification, section 9.6.1, constant DG).

The default value is: ``250``


.. _`//CycloneDDS/Domain/Discovery/Ports/MulticastDataOffset`:

//CycloneDDS/Domain/Discovery/Ports/MulticastDataOffset
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Integer

This element specifies the port number for multicast data traffic (refer to the DDSI 2.1 specification, section 9.6.1, constant d2).

The default value is: ``1``


.. _`//CycloneDDS/Domain/Discovery/Ports/MulticastMetaOffset`:

//CycloneDDS/Domain/Discovery/Ports/MulticastMetaOffset
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Integer

This element specifies the port number for multicast meta traffic (refer to the DDSI 2.1 specification, section 9.6.1, constant d0).

The default value is: ``0``


.. _`//CycloneDDS/Domain/Discovery/Ports/ParticipantGain`:

//CycloneDDS/Domain/Discovery/Ports/ParticipantGain
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Integer

This element specifies the participant gain, relating p0, participant index to sets of port numbers (refer to the DDSI 2.1 specification, section 9.6.1, constant PG).

The default value is: ``2``


.. _`//CycloneDDS/Domain/Discovery/Ports/UnicastDataOffset`:

//CycloneDDS/Domain/Discovery/Ports/UnicastDataOffset
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Integer

This element specifies the port number for unicast data traffic (refer to the DDSI 2.1 specification, section 9.6.1, constant d3).

The default value is: ``11``


.. _`//CycloneDDS/Domain/Discovery/Ports/UnicastMetaOffset`:

//CycloneDDS/Domain/Discovery/Ports/UnicastMetaOffset
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Integer

This element specifies the port number for unicast meta traffic (refer to the DDSI 2.1 specification, section 9.6.1, constant d1).

The default value is: ``10``


.. _`//CycloneDDS/Domain/Discovery/SPDPInterval`:

//CycloneDDS/Domain/Discovery/SPDPInterval
------------------------------------------

Number-with-unit

This element specifies the interval between spontaneous transmissions of participant discovery packets.

The unit must be specified explicitly. Recognised units: ns, us, ms, s, min, hr, day.

The default value is: ``30 s``


.. _`//CycloneDDS/Domain/Discovery/SPDPMulticastAddress`:

//CycloneDDS/Domain/Discovery/SPDPMulticastAddress
--------------------------------------------------

Text

This element specifies the multicast address used as the destination for the participant discovery packets. In IPv4 mode the default is the (standardised) 239.255.0.1, in IPv6 mode it becomes ff02::ffff:239.255.0.1, which is a non-standardised link-local multicast address.

The default value is: ``239.255.0.1``


.. _`//CycloneDDS/Domain/Discovery/Tag`:

//CycloneDDS/Domain/Discovery/Tag
---------------------------------

Text

String extension for domain id that remote participants must match to be discovered.

The default value is: ``<empty>``


.. _`//CycloneDDS/Domain/General`:

//CycloneDDS/Domain/General
===========================

Children: `//CycloneDDS/Domain/General/AllowMulticast`_, `//CycloneDDS/Domain/General/DontRoute`_, `//CycloneDDS/Domain/General/EnableMulticastLoopback`_, `//CycloneDDS/Domain/General/EntityAutoNaming`_, `//CycloneDDS/Domain/General/ExternalNetworkAddress`_, `//CycloneDDS/Domain/General/ExternalNetworkMask`_, `//CycloneDDS/Domain/General/FragmentSize`_, `//CycloneDDS/Domain/General/Interfaces`_, `//CycloneDDS/Domain/General/MaxMessageSize`_, `//CycloneDDS/Domain/General/MaxRexmitMessageSize`_, `//CycloneDDS/Domain/General/MulticastRecvNetworkInterfaceAddresses`_, `//CycloneDDS/Domain/General/MulticastTimeToLive`_, `//CycloneDDS/Domain/General/RedundantNetworking`_, `//CycloneDDS/Domain/General/Transport`_, `//CycloneDDS/Domain/General/UseIPv6`_

The General element specifies overall Cyclone DDS service settings.


.. _`//CycloneDDS/Domain/General/AllowMulticast`:

//CycloneDDS/Domain/General/AllowMulticast
------------------------------------------

One of:
* Keyword: default
* Comma-separated list of: false, spdp, asm, ssm, true

This element controls whether Cyclone DDS uses multicasts for data traffic.

It is a comma-separated list of some of the following keywords: "spdp", "asm", "ssm", or either of "false" or "true", or "default".

 * spdp: enables the use of ASM (any-source multicast) for participant discovery, joining the multicast group on the discovery socket, transmitting SPDP messages to this group, but never advertising nor using any multicast address in any discovery message, thus forcing unicast communications for all endpoint discovery and user data.

 * asm: enables the use of ASM for all traffic, including receiving SPDP but not transmitting SPDP messages via multicast

 * ssm: enables the use of SSM (source-specific multicast) for all non-SPDP traffic (if supported)



When set to "false" all multicasting is disabled. The default, "true" enables the full use of multicasts. Listening for multicasts can be controlled by General/MulticastRecvNetworkInterfaceAddresses.

"default" maps on spdp if the network is a WiFi network, on true if it is a wired network

The default value is: ``default``


.. _`//CycloneDDS/Domain/General/DontRoute`:

//CycloneDDS/Domain/General/DontRoute
-------------------------------------

Boolean

This element allows setting the SO\_DONTROUTE option for outgoing packets to bypass the local routing tables. This is generally useful only when the routing tables cannot be trusted, which is highly unusual.

The default value is: ``false``


.. _`//CycloneDDS/Domain/General/EnableMulticastLoopback`:

//CycloneDDS/Domain/General/EnableMulticastLoopback
---------------------------------------------------

Boolean

This element specifies whether Cyclone DDS allows IP multicast packets to be visible to all DDSI participants in the same node, including itself. It must be "true" for intra-node multicast communications. However, if a node runs only a single Cyclone DDS service and does not host any other DDSI-capable programs, it should be set to "false" for improved performance.

The default value is: ``true``


.. _`//CycloneDDS/Domain/General/EntityAutoNaming`:

//CycloneDDS/Domain/General/EntityAutoNaming
--------------------------------------------

Attributes: [seed](`//CycloneDDS/Domain/General/EntityAutoNaming[@seed]`_)

One of: empty, fancy

This element specifies the entity autonaming mode. By default set to 'empty' which means no name will be set (but you can still use dds\_qset\_entity\_name). When set to 'fancy' participants, publishers, subscribers, writers, and readers will get randomly generated names. An autonamed entity will share a 3-letter prefix with their parent entity.

The default value is: ``empty``


.. _`//CycloneDDS/Domain/General/EntityAutoNaming[@seed]`:

//CycloneDDS/Domain/General/EntityAutoNaming[@seed]
---------------------------------------------------

Text

Provide an initial seed for the entity naming. Your string will be hashed to provide the random state. When provided, the same sequence of names is generated every run. Creating your entities in the same order will ensure they are the same between runs. If you run multiple nodes, set this via environment variable to ensure every node generates unique names. A random starting seed is chosen when left empty, (the default). 

The default value is: ``<empty>``


.. _`//CycloneDDS/Domain/General/ExternalNetworkAddress`:

//CycloneDDS/Domain/General/ExternalNetworkAddress
--------------------------------------------------

Text

This element allows explicitly overruling the network address Cyclone DDS advertises in the discovery protocol, which by default is the address of the preferred network interface (General/NetworkInterfaceAddress), to allow Cyclone DDS to communicate across a Network Address Translation (NAT) device.

The default value is: ``auto``


.. _`//CycloneDDS/Domain/General/ExternalNetworkMask`:

//CycloneDDS/Domain/General/ExternalNetworkMask
-----------------------------------------------

Text

This element specifies the network mask of the external network address. This element is relevant only when an external network address (General/ExternalNetworkAddress) is explicitly configured. In this case locators received via the discovery protocol that are within the same external subnet (as defined by this mask) will be translated to an internal address by replacing the network portion of the external address with the corresponding portion of the preferred network interface address. This option is IPv4-only.

The default value is: ``0.0.0.0``


.. _`//CycloneDDS/Domain/General/FragmentSize`:

//CycloneDDS/Domain/General/FragmentSize
----------------------------------------

Number-with-unit

This element specifies the size of DDSI sample fragments generated by Cyclone DDS. Samples larger than FragmentSize are fragmented into fragments of FragmentSize bytes each, except the last one, which may be smaller. The DDSI spec mandates a minimum fragment size of 1025 bytes, but Cyclone DDS will do whatever size is requested, accepting fragments of which the size is at least the minimum of 1025 and FragmentSize.

The unit must be specified explicitly. Recognised units: B (bytes), kB & KiB (2^10 bytes), MB & MiB (2^20 bytes), GB & GiB (2^30 bytes).

The default value is: ``1344 B``


.. _`//CycloneDDS/Domain/General/Interfaces`:

//CycloneDDS/Domain/General/Interfaces
--------------------------------------

Children: `//CycloneDDS/Domain/General/Interfaces/NetworkInterface`_

This element specifies the network interfaces for use by Cyclone DDS. Multiple interfaces can be specified with an assigned priority. The list in use will be sorted by priority. If interfaces have an equal priority, the specification order will be preserved.


.. _`//CycloneDDS/Domain/General/Interfaces/NetworkInterface`:

//CycloneDDS/Domain/General/Interfaces/NetworkInterface
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Attributes: [address](`//CycloneDDS/Domain/General/Interfaces/NetworkInterface[@address]`_), [autodetermine](`//CycloneDDS/Domain/General/Interfaces/NetworkInterface[@autodetermine]`_), [multicast](`//CycloneDDS/Domain/General/Interfaces/NetworkInterface[@multicast]`_), [name](`//CycloneDDS/Domain/General/Interfaces/NetworkInterface[@name]`_), [prefer_multicast](`//CycloneDDS/Domain/General/Interfaces/NetworkInterface[@prefer_multicast]`_), [presence_required](`//CycloneDDS/Domain/General/Interfaces/NetworkInterface[@presence_required]`_), [priority](`//CycloneDDS/Domain/General/Interfaces/NetworkInterface[@priority]`_)

This element defines a network interface. You can set autodetermine="true" to autoselect the interface CycloneDDS considers the highest quality. If autodetermine="false" (the default), you must specify the name and/or address attribute. If you specify both, they must match the same interface.


.. _`//CycloneDDS/Domain/General/Interfaces/NetworkInterface[@address]`:

//CycloneDDS/Domain/General/Interfaces/NetworkInterface[@address]
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Text

This attribute specifies the address of the interface. With ipv4 allows  matching on the network part if the host part is set to zero. 

The default value is: ``<empty>``


.. _`//CycloneDDS/Domain/General/Interfaces/NetworkInterface[@autodetermine]`:

//CycloneDDS/Domain/General/Interfaces/NetworkInterface[@autodetermine]
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Text

If set to "true" an interface is automatically selected. Specifying a name or an address when automatic is set is considered an error.

The default value is: ``false``


.. _`//CycloneDDS/Domain/General/Interfaces/NetworkInterface[@multicast]`:

//CycloneDDS/Domain/General/Interfaces/NetworkInterface[@multicast]
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Text

This attribute specifies whether the interface should use multicast. On its default setting, 'default', it will use the value as return by the operating system. If set to 'true', the interface will be assumed to be multicast capable even when the interface flags returned by the operating system state it is not (this provides a workaround for some platforms). If set to 'false', the interface will never be used for multicast.
The default value is: ``default``


.. _`//CycloneDDS/Domain/General/Interfaces/NetworkInterface[@name]`:

//CycloneDDS/Domain/General/Interfaces/NetworkInterface[@name]
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Text

This attribute specifies the name of the interface. 

The default value is: ``<empty>``


.. _`//CycloneDDS/Domain/General/Interfaces/NetworkInterface[@prefer_multicast]`:

//CycloneDDS/Domain/General/Interfaces/NetworkInterface[@prefer_multicast]
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Boolean

When false (default), Cyclone DDS uses unicast for data whenever a single unicast suffices. Setting this to true makes it prefer multicasting data, falling back to unicast only when no multicast is available.

The default value is: ``false``


.. _`//CycloneDDS/Domain/General/Interfaces/NetworkInterface[@presence_required]`:

//CycloneDDS/Domain/General/Interfaces/NetworkInterface[@presence_required]
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Boolean

By default, all specified network interfaces must be present; if they are missing Cyclone will not start. By explicitly setting this setting for an interface, you can instruct Cyclone to ignore that interface if it is not present.

The default value is: ``true``


.. _`//CycloneDDS/Domain/General/Interfaces/NetworkInterface[@priority]`:

//CycloneDDS/Domain/General/Interfaces/NetworkInterface[@priority]
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Text

This attribute specifies the interface priority (decimal integer or default). The default value for loopback interfaces is 2, for all other interfaces it is 0.

The default value is: ``default``


.. _`//CycloneDDS/Domain/General/MaxMessageSize`:

//CycloneDDS/Domain/General/MaxMessageSize
------------------------------------------

Number-with-unit

This element specifies the maximum size of the UDP payload that Cyclone DDS will generate. Cyclone DDS will try to maintain this limit within the bounds of the DDSI specification, which means that in some cases (especially for very low values of MaxMessageSize) larger payloads may sporadically be observed (currently up to 1192 B).

On some networks it may be necessary to set this item to keep the packetsize below the MTU to prevent IP fragmentation.

The unit must be specified explicitly. Recognised units: B (bytes), kB & KiB (2^10 bytes), MB & MiB (2^20 bytes), GB & GiB (2^30 bytes).

The default value is: ``14720 B``


.. _`//CycloneDDS/Domain/General/MaxRexmitMessageSize`:

//CycloneDDS/Domain/General/MaxRexmitMessageSize
------------------------------------------------

Number-with-unit

This element specifies the maximum size of the UDP payload that Cyclone DDS will generate for a retransmit. Cyclone DDS will try to maintain this limit within the bounds of the DDSI specification, which means that in some cases (especially for very low values) larger payloads may sporadically be observed (currently up to 1192 B).

On some networks it may be necessary to set this item to keep the packetsize below the MTU to prevent IP fragmentation.

The unit must be specified explicitly. Recognised units: B (bytes), kB & KiB (2^10 bytes), MB & MiB (2^20 bytes), GB & GiB (2^30 bytes).

The default value is: ``1456 B``


.. _`//CycloneDDS/Domain/General/MulticastRecvNetworkInterfaceAddresses`:

//CycloneDDS/Domain/General/MulticastRecvNetworkInterfaceAddresses
------------------------------------------------------------------

Text

This element specifies which network interfaces Cyclone DDS listens to multicasts. The following options are available:

 * all: listen for multicasts on all multicast-capable interfaces; or

 * any: listen for multicasts on the operating system default interface; or

 * preferred: listen for multicasts on the preferred interface (General/Interface/NetworkInterface with the highest priority); or

 * none: does not listen for multicasts on any interface; or

 * a comma-separated list of network addresses: configures Cyclone DDS to listen for multicasts on all listed addresses.



If Cyclone DDS is in IPv6 mode and the address of the preferred network interface is a link-local address, "all" is treated as a synonym for "preferred" and a comma-separated list is treated as "preferred" if it contains the preferred interface and as "none" if not.

The default value is: ``preferred``


.. _`//CycloneDDS/Domain/General/MulticastTimeToLive`:

//CycloneDDS/Domain/General/MulticastTimeToLive
-----------------------------------------------

Integer

This element specifies the time-to-live setting for outgoing multicast packets.

The default value is: ``32``


.. _`//CycloneDDS/Domain/General/RedundantNetworking`:

//CycloneDDS/Domain/General/RedundantNetworking
-----------------------------------------------

Boolean

When enabled, use selected network interfaces in parallel for redundancy.

The default value is: ``false``


.. _`//CycloneDDS/Domain/General/Transport`:

//CycloneDDS/Domain/General/Transport
-------------------------------------

One of: default, udp, udp6, tcp, tcp6, raweth

This element allows selecting the transport to be used (udp, udp6, tcp, tcp6, raweth)

The default value is: ``default``


.. _`//CycloneDDS/Domain/General/UseIPv6`:

//CycloneDDS/Domain/General/UseIPv6
-----------------------------------

One of: false, true, default

Deprecated (use Transport instead)

The default value is: ``default``


.. _`//CycloneDDS/Domain/Internal`:

//CycloneDDS/Domain/Internal
============================

Children: `//CycloneDDS/Domain/Internal/AccelerateRexmitBlockSize`_, `//CycloneDDS/Domain/Internal/AckDelay`_, `//CycloneDDS/Domain/Internal/AutoReschedNackDelay`_, `//CycloneDDS/Domain/Internal/BuiltinEndpointSet`_, `//CycloneDDS/Domain/Internal/BurstSize`_, `//CycloneDDS/Domain/Internal/ControlTopic`_, `//CycloneDDS/Domain/Internal/DefragReliableMaxSamples`_, `//CycloneDDS/Domain/Internal/DefragUnreliableMaxSamples`_, `//CycloneDDS/Domain/Internal/DeliveryQueueMaxSamples`_, `//CycloneDDS/Domain/Internal/EnableExpensiveChecks`_, `//CycloneDDS/Domain/Internal/GenerateKeyhash`_, `//CycloneDDS/Domain/Internal/HeartbeatInterval`_, `//CycloneDDS/Domain/Internal/LateAckMode`_, `//CycloneDDS/Domain/Internal/LivelinessMonitoring`_, `//CycloneDDS/Domain/Internal/MaxParticipants`_, `//CycloneDDS/Domain/Internal/MaxQueuedRexmitBytes`_, `//CycloneDDS/Domain/Internal/MaxQueuedRexmitMessages`_, `//CycloneDDS/Domain/Internal/MaxSampleSize`_, `//CycloneDDS/Domain/Internal/MeasureHbToAckLatency`_, `//CycloneDDS/Domain/Internal/MonitorPort`_, `//CycloneDDS/Domain/Internal/MultipleReceiveThreads`_, `//CycloneDDS/Domain/Internal/NackDelay`_, `//CycloneDDS/Domain/Internal/PreEmptiveAckDelay`_, `//CycloneDDS/Domain/Internal/PrimaryReorderMaxSamples`_, `//CycloneDDS/Domain/Internal/PrioritizeRetransmit`_, `//CycloneDDS/Domain/Internal/RediscoveryBlacklistDuration`_, `//CycloneDDS/Domain/Internal/RetransmitMerging`_, `//CycloneDDS/Domain/Internal/RetransmitMergingPeriod`_, `//CycloneDDS/Domain/Internal/RetryOnRejectBestEffort`_, `//CycloneDDS/Domain/Internal/SPDPResponseMaxDelay`_, `//CycloneDDS/Domain/Internal/SecondaryReorderMaxSamples`_, `//CycloneDDS/Domain/Internal/SocketReceiveBufferSize`_, `//CycloneDDS/Domain/Internal/SocketSendBufferSize`_, `//CycloneDDS/Domain/Internal/SquashParticipants`_, `//CycloneDDS/Domain/Internal/SynchronousDeliveryLatencyBound`_, `//CycloneDDS/Domain/Internal/SynchronousDeliveryPriorityThreshold`_, `//CycloneDDS/Domain/Internal/Test`_, `//CycloneDDS/Domain/Internal/UnicastResponseToSPDPMessages`_, `//CycloneDDS/Domain/Internal/UseMulticastIfMreqn`_, `//CycloneDDS/Domain/Internal/Watermarks`_, `//CycloneDDS/Domain/Internal/WriterLingerDuration`_

The Internal elements deal with a variety of settings that are evolving and that are not necessarily fully supported. For the majority of the Internal settings the functionality is supported, but the right to change the way the options control the functionality is reserved. This includes renaming or moving options.


.. _`//CycloneDDS/Domain/Internal/AccelerateRexmitBlockSize`:

//CycloneDDS/Domain/Internal/AccelerateRexmitBlockSize
------------------------------------------------------

Integer

Proxy readers that are assumed to still be retrieving historical data get this many samples retransmitted when they NACK something, even if some of these samples have sequence numbers outside the set covered by the NACK.

The default value is: ``0``


.. _`//CycloneDDS/Domain/Internal/AckDelay`:

//CycloneDDS/Domain/Internal/AckDelay
-------------------------------------

Number-with-unit

This setting controls the delay between sending identical acknowledgements.

The unit must be specified explicitly. Recognised units: ns, us, ms, s, min, hr, day.

The default value is: ``10 ms``


.. _`//CycloneDDS/Domain/Internal/AutoReschedNackDelay`:

//CycloneDDS/Domain/Internal/AutoReschedNackDelay
-------------------------------------------------

Number-with-unit

This setting controls the interval with which a reader will continue NACK'ing missing samples in the absence of a response from the writer, as a protection mechanism against writers incorrectly stopping the sending of HEARTBEAT messages.

Valid values are finite durations with an explicit unit or the keyword 'inf' for infinity. Recognised units: ns, us, ms, s, min, hr, day.

The default value is: ``3 s``


.. _`//CycloneDDS/Domain/Internal/BuiltinEndpointSet`:

//CycloneDDS/Domain/Internal/BuiltinEndpointSet
-----------------------------------------------

One of: full, writers, minimal

This element controls which participants will have which built-in endpoints for the discovery and liveliness protocols. Valid values are:
 * full: all participants have all endpoints;

 * writers: all participants have the writers, but just one has the readers;

 * minimal: only one participant has built-in endpoints.


The default is writers, as this is thought to be compliant and reasonably efficient. Minimal may or may not be compliant but is most efficient, and full is inefficient but certain to be compliant.

The default value is: ``writers``


.. _`//CycloneDDS/Domain/Internal/BurstSize`:

//CycloneDDS/Domain/Internal/BurstSize
--------------------------------------

Children: `//CycloneDDS/Domain/Internal/BurstSize/MaxInitTransmit`_, `//CycloneDDS/Domain/Internal/BurstSize/MaxRexmit`_

Setting for controlling the size of transmitting bursts.


.. _`//CycloneDDS/Domain/Internal/BurstSize/MaxInitTransmit`:

//CycloneDDS/Domain/Internal/BurstSize/MaxInitTransmit
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Number-with-unit

This element specifies how much more than the (presumed or discovered) receive buffer size may be sent when transmitting a sample for the first time, expressed as a percentage; the remainder will then be handled via retransmits. Usually, the receivers can keep up with the transmitter, at least on average, so generally it is better to hope for the best and recover. Besides, the retransmits will be unicast, and so any multicast advantage will be lost as well.

The unit must be specified explicitly. Recognised units: B (bytes), kB & KiB (2^10 bytes), MB & MiB (2^20 bytes), GB & GiB (2^30 bytes).

The default value is: ``4294967295``


.. _`//CycloneDDS/Domain/Internal/BurstSize/MaxRexmit`:

//CycloneDDS/Domain/Internal/BurstSize/MaxRexmit
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Number-with-unit

This element specifies the amount of data to be retransmitted in response to one NACK.

The unit must be specified explicitly. Recognised units: B (bytes), kB & KiB (2^10 bytes), MB & MiB (2^20 bytes), GB & GiB (2^30 bytes).

The default value is: ``1 MiB``


.. _`//CycloneDDS/Domain/Internal/ControlTopic`:

//CycloneDDS/Domain/Internal/ControlTopic
-----------------------------------------

The ControlTopic element allows configured whether Cyclone DDS provides a special control interface via a predefined topic or not.


.. _`//CycloneDDS/Domain/Internal/DefragReliableMaxSamples`:

//CycloneDDS/Domain/Internal/DefragReliableMaxSamples
-----------------------------------------------------

Integer

This element sets the maximum number of samples that can be defragmented simultaneously for a reliable writer. This has to be large enough to handle retransmissions of historical data in addition to new samples.

The default value is: ``16``


.. _`//CycloneDDS/Domain/Internal/DefragUnreliableMaxSamples`:

//CycloneDDS/Domain/Internal/DefragUnreliableMaxSamples
-------------------------------------------------------

Integer

This element sets the maximum number of samples that can be defragmented simultaneously for best-effort writers.

The default value is: ``4``


.. _`//CycloneDDS/Domain/Internal/DeliveryQueueMaxSamples`:

//CycloneDDS/Domain/Internal/DeliveryQueueMaxSamples
----------------------------------------------------

Integer

This element controls the maximum size of a delivery queue, expressed in samples. Once a delivery queue is full, incoming samples destined for that queue are dropped until space becomes available again.

The default value is: ``256``


.. _`//CycloneDDS/Domain/Internal/EnableExpensiveChecks`:

//CycloneDDS/Domain/Internal/EnableExpensiveChecks
--------------------------------------------------

One of:
* Comma-separated list of: whc, rhc, xevent, all
* Or empty

This element enables expensive checks in builds with assertions enabled and is ignored otherwise. Recognised categories are:

 * whc: writer history cache checking

 * rhc: reader history cache checking

 * xevent: xevent checking

In addition, there is the keyword all that enables all checks.

The default value is: ``<empty>``


.. _`//CycloneDDS/Domain/Internal/GenerateKeyhash`:

//CycloneDDS/Domain/Internal/GenerateKeyhash
--------------------------------------------

Boolean

When true, include keyhashes in outgoing data for topics with keys.

The default value is: ``false``


.. _`//CycloneDDS/Domain/Internal/HeartbeatInterval`:

//CycloneDDS/Domain/Internal/HeartbeatInterval
----------------------------------------------

Attributes: [max](`//CycloneDDS/Domain/Internal/HeartbeatInterval[@max]`_), [min](`//CycloneDDS/Domain/Internal/HeartbeatInterval[@min]`_), [minsched](`//CycloneDDS/Domain/Internal/HeartbeatInterval[@minsched]`_)

Number-with-unit

This element allows configuring the base interval for sending writer heartbeats and the bounds within which it can vary.

Valid values are finite durations with an explicit unit or the keyword 'inf' for infinity. Recognised units: ns, us, ms, s, min, hr, day.

The default value is: ``100 ms``


.. _`//CycloneDDS/Domain/Internal/HeartbeatInterval[@max]`:

//CycloneDDS/Domain/Internal/HeartbeatInterval[@max]
----------------------------------------------------

Number-with-unit

This attribute sets the maximum interval for periodic heartbeats.

Valid values are finite durations with an explicit unit or the keyword 'inf' for infinity. Recognised units: ns, us, ms, s, min, hr, day.

The default value is: ``8 s``


.. _`//CycloneDDS/Domain/Internal/HeartbeatInterval[@min]`:

//CycloneDDS/Domain/Internal/HeartbeatInterval[@min]
----------------------------------------------------

Number-with-unit

This attribute sets the minimum interval that must have passed since the most recent heartbeat from a writer, before another asynchronous (not directly related to writing) will be sent.

Valid values are finite durations with an explicit unit or the keyword 'inf' for infinity. Recognised units: ns, us, ms, s, min, hr, day.

The default value is: ``5 ms``


.. _`//CycloneDDS/Domain/Internal/HeartbeatInterval[@minsched]`:

//CycloneDDS/Domain/Internal/HeartbeatInterval[@minsched]
---------------------------------------------------------

Number-with-unit

This attribute sets the minimum interval for periodic heartbeats. Other events may still cause heartbeats to go out.

Valid values are finite durations with an explicit unit or the keyword 'inf' for infinity. Recognised units: ns, us, ms, s, min, hr, day.

The default value is: ``20 ms``


.. _`//CycloneDDS/Domain/Internal/LateAckMode`:

//CycloneDDS/Domain/Internal/LateAckMode
----------------------------------------

Boolean

Ack a sample only when it has been delivered, instead of when committed to delivering it.

The default value is: ``false``


.. _`//CycloneDDS/Domain/Internal/LivelinessMonitoring`:

//CycloneDDS/Domain/Internal/LivelinessMonitoring
-------------------------------------------------

Attributes: [Interval](`//CycloneDDS/Domain/Internal/LivelinessMonitoring[@Interval]`_), [StackTraces](`//CycloneDDS/Domain/Internal/LivelinessMonitoring[@StackTraces]`_)

Boolean

This element controls whether or not implementation should internally monitor its own liveliness. If liveliness monitoring is enabled, stack traces can be dumped automatically when some thread appears to have stopped making progress.

The default value is: ``false``


.. _`//CycloneDDS/Domain/Internal/LivelinessMonitoring[@Interval]`:

//CycloneDDS/Domain/Internal/LivelinessMonitoring[@Interval]
------------------------------------------------------------

Number-with-unit

This element controls the interval to check whether threads have been making progress.

The unit must be specified explicitly. Recognised units: ns, us, ms, s, min, hr, day.

The default value is: ``1s``


.. _`//CycloneDDS/Domain/Internal/LivelinessMonitoring[@StackTraces]`:

//CycloneDDS/Domain/Internal/LivelinessMonitoring[@StackTraces]
---------------------------------------------------------------

Boolean

This element controls whether or not to write stack traces to the DDSI2 trace when a thread fails to make progress (on select platforms only).

The default value is: ``true``


.. _`//CycloneDDS/Domain/Internal/MaxParticipants`:

//CycloneDDS/Domain/Internal/MaxParticipants
--------------------------------------------

Integer

This elements configures the maximum number of DCPS domain participants this Cyclone DDS instance is willing to service. 0 is unlimited.

The default value is: ``0``


.. _`//CycloneDDS/Domain/Internal/MaxQueuedRexmitBytes`:

//CycloneDDS/Domain/Internal/MaxQueuedRexmitBytes
-------------------------------------------------

Number-with-unit

This setting limits the maximum number of bytes queued for retransmission. The default value of 0 is unlimited unless an AuxiliaryBandwidthLimit has been set, in which case it becomes NackDelay \* AuxiliaryBandwidthLimit. It must be large enough to contain the largest sample that may need to be retransmitted.

The unit must be specified explicitly. Recognised units: B (bytes), kB & KiB (2^10 bytes), MB & MiB (2^20 bytes), GB & GiB (2^30 bytes).

The default value is: ``512 kB``


.. _`//CycloneDDS/Domain/Internal/MaxQueuedRexmitMessages`:

//CycloneDDS/Domain/Internal/MaxQueuedRexmitMessages
----------------------------------------------------

Integer

This setting limits the maximum number of samples queued for retransmission.

The default value is: ``200``


.. _`//CycloneDDS/Domain/Internal/MaxSampleSize`:

//CycloneDDS/Domain/Internal/MaxSampleSize
------------------------------------------

Number-with-unit

This setting controls the maximum (CDR) serialised size of samples that Cyclone DDS will forward in either direction. Samples larger than this are discarded with a warning.

The unit must be specified explicitly. Recognised units: B (bytes), kB & KiB (2^10 bytes), MB & MiB (2^20 bytes), GB & GiB (2^30 bytes).

The default value is: ``2147483647 B``


.. _`//CycloneDDS/Domain/Internal/MeasureHbToAckLatency`:

//CycloneDDS/Domain/Internal/MeasureHbToAckLatency
--------------------------------------------------

Boolean

This element enables heartbeat-to-ack latency among Cyclone DDS services by prepending timestamps to Heartbeat and AckNack messages and calculating round trip times. This is non-standard behaviour. The measured latencies are quite noisy and are currently not used anywhere.

The default value is: ``false``


.. _`//CycloneDDS/Domain/Internal/MonitorPort`:

//CycloneDDS/Domain/Internal/MonitorPort
----------------------------------------

Integer

This element allows configuring a service that dumps a text description of part the internal state to TCP clients. By default (-1), this is disabled; specifying 0 means a kernel-allocated port is used; a positive number is used as the TCP port number.

The default value is: ``-1``


.. _`//CycloneDDS/Domain/Internal/MultipleReceiveThreads`:

//CycloneDDS/Domain/Internal/MultipleReceiveThreads
---------------------------------------------------

Attributes: [maxretries](`//CycloneDDS/Domain/Internal/MultipleReceiveThreads[@maxretries]`_)

One of: false, true, default

This element controls whether all traffic is handled by a single receive thread (false) or whether multiple receive threads may be used to improve latency (true). By default it is disabled on Windows because it appears that one cannot count on being able to send packets to oneself, which is necessary to stop the thread during shutdown. Currently multiple receive threads are only used for connectionless transport (e.g., UDP) and ManySocketsMode not set to single (the default).

The default value is: ``default``


.. _`//CycloneDDS/Domain/Internal/MultipleReceiveThreads[@maxretries]`:

//CycloneDDS/Domain/Internal/MultipleReceiveThreads[@maxretries]
----------------------------------------------------------------

Integer

Receive threads dedicated to a single socket can only be triggered for termination by sending a packet. Reception of any packet will do, so termination failure due to packet loss is exceedingly unlikely, but to eliminate all risks, it will retry as many times as specified by this attribute before aborting.

The default value is: ``4294967295``


.. _`//CycloneDDS/Domain/Internal/NackDelay`:

//CycloneDDS/Domain/Internal/NackDelay
--------------------------------------

Number-with-unit

This setting controls the delay between receipt of a HEARTBEAT indicating missing samples and a NACK (ignored when the HEARTBEAT requires an answer). However, no NACK is sent if a NACK had been scheduled already for a response earlier than the delay requests: then that NACK will incorporate the latest information.

The unit must be specified explicitly. Recognised units: ns, us, ms, s, min, hr, day.

The default value is: ``100 ms``


.. _`//CycloneDDS/Domain/Internal/PreEmptiveAckDelay`:

//CycloneDDS/Domain/Internal/PreEmptiveAckDelay
-----------------------------------------------

Number-with-unit

This setting controls the delay between the discovering a remote writer and sending a pre-emptive AckNack to discover the available range of data.

The unit must be specified explicitly. Recognised units: ns, us, ms, s, min, hr, day.

The default value is: ``10 ms``


.. _`//CycloneDDS/Domain/Internal/PrimaryReorderMaxSamples`:

//CycloneDDS/Domain/Internal/PrimaryReorderMaxSamples
-----------------------------------------------------

Integer

This element sets the maximum size in samples of a primary re-order administration. Each proxy writer has one primary re-order administration to buffer the packet flow in case some packets arrive out of order. Old samples are forwarded to secondary re-order administrations associated with readers needing historical data.

The default value is: ``128``


.. _`//CycloneDDS/Domain/Internal/PrioritizeRetransmit`:

//CycloneDDS/Domain/Internal/PrioritizeRetransmit
-------------------------------------------------

Boolean

This element controls whether retransmits are prioritized over new data, speeding up recovery.

The default value is: ``true``


.. _`//CycloneDDS/Domain/Internal/RediscoveryBlacklistDuration`:

//CycloneDDS/Domain/Internal/RediscoveryBlacklistDuration
---------------------------------------------------------

Attributes: [enforce](`//CycloneDDS/Domain/Internal/RediscoveryBlacklistDuration[@enforce]`_)

Number-with-unit

This element controls for how long a remote participant that was previously deleted will remain on a blacklist to prevent rediscovery, giving the software on a node time to perform any cleanup actions it needs to do. To some extent this delay is required internally by Cyclone DDS, but in the default configuration with the 'enforce' attribute set to false, Cyclone DDS will reallow rediscovery as soon as it has cleared its internal administration. Setting it to too small a value may result in the entry being pruned from the blacklist before Cyclone DDS is ready, it is therefore recommended to set it to at least several seconds.

Valid values are finite durations with an explicit unit or the keyword 'inf' for infinity. Recognised units: ns, us, ms, s, min, hr, day.

The default value is: ``0s``


.. _`//CycloneDDS/Domain/Internal/RediscoveryBlacklistDuration[@enforce]`:

//CycloneDDS/Domain/Internal/RediscoveryBlacklistDuration[@enforce]
-------------------------------------------------------------------

Boolean

This attribute controls whether the configured time during which recently deleted participants will not be rediscovered (i.e., "black listed") is enforced and following complete removal of the participant in Cyclone DDS, or whether it can be rediscovered earlier provided all traces of that participant have been removed already.

The default value is: ``false``


.. _`//CycloneDDS/Domain/Internal/RetransmitMerging`:

//CycloneDDS/Domain/Internal/RetransmitMerging
----------------------------------------------

One of: never, adaptive, always

This elements controls the addressing and timing of retransmits. Possible values are:
 * never: retransmit only to the NACK-ing reader;

 * adaptive: attempt to combine retransmits needed for reliability, but send historical (transient-local) data to the requesting reader only;

 * always: do not distinguish between different causes, always try to merge.


The default is never. See also Internal/RetransmitMergingPeriod.

The default value is: ``never``


.. _`//CycloneDDS/Domain/Internal/RetransmitMergingPeriod`:

//CycloneDDS/Domain/Internal/RetransmitMergingPeriod
----------------------------------------------------

Number-with-unit

This setting determines the time window size in which a NACK of some sample is ignored because a retransmit of that sample has been multicasted too recently. This setting has no effect on unicasted retransmits.

See also Internal/RetransmitMerging.

The unit must be specified explicitly. Recognised units: ns, us, ms, s, min, hr, day.

The default value is: ``5 ms``


.. _`//CycloneDDS/Domain/Internal/RetryOnRejectBestEffort`:

//CycloneDDS/Domain/Internal/RetryOnRejectBestEffort
----------------------------------------------------

Boolean

Whether or not to locally retry pushing a received best-effort sample into the reader caches when resource limits are reached.

The default value is: ``false``


.. _`//CycloneDDS/Domain/Internal/SPDPResponseMaxDelay`:

//CycloneDDS/Domain/Internal/SPDPResponseMaxDelay
-------------------------------------------------

Number-with-unit

Maximum pseudo-random delay in milliseconds between discovering aremote participant and responding to it.

The unit must be specified explicitly. Recognised units: ns, us, ms, s, min, hr, day.

The default value is: ``0 ms``


.. _`//CycloneDDS/Domain/Internal/SecondaryReorderMaxSamples`:

//CycloneDDS/Domain/Internal/SecondaryReorderMaxSamples
-------------------------------------------------------

Integer

This element sets the maximum size in samples of a secondary re-order administration. The secondary re-order administration is per reader needing historical data.

The default value is: ``128``


.. _`//CycloneDDS/Domain/Internal/SocketReceiveBufferSize`:

//CycloneDDS/Domain/Internal/SocketReceiveBufferSize
----------------------------------------------------

Attributes: [max](`//CycloneDDS/Domain/Internal/SocketReceiveBufferSize[@max]`_), [min](`//CycloneDDS/Domain/Internal/SocketReceiveBufferSize[@min]`_)

The settings in this element control the size of the socket receive buffers. The operating system provides some size receive buffer upon creation of the socket, this option can be used to increase the size of the buffer beyond that initially provided by the operating system. If the buffer size cannot be increased to the requested minimum size, an error is reported.

The default setting requests a buffer size of 1MiB but accepts whatever is available after that.


.. _`//CycloneDDS/Domain/Internal/SocketReceiveBufferSize[@max]`:

//CycloneDDS/Domain/Internal/SocketReceiveBufferSize[@max]
----------------------------------------------------------

Number-with-unit

This sets the size of the socket receive buffer to request, with the special value of "default" indicating that it should try to satisfy the minimum buffer size. If both are at "default", it will request 1MiB and accept anything. It is ignored if the  maximum is set to less than the minimum.

The unit must be specified explicitly. Recognised units: B (bytes), kB & KiB (2^10 bytes), MB & MiB (2^20 bytes), GB & GiB (2^30 bytes).

The default value is: ``default``


.. _`//CycloneDDS/Domain/Internal/SocketReceiveBufferSize[@min]`:

//CycloneDDS/Domain/Internal/SocketReceiveBufferSize[@min]
----------------------------------------------------------

Number-with-unit

This sets the minimum acceptable socket receive buffer size, with the special value "default" indicating that whatever is available is acceptable.

The unit must be specified explicitly. Recognised units: B (bytes), kB & KiB (2^10 bytes), MB & MiB (2^20 bytes), GB & GiB (2^30 bytes).

The default value is: ``default``


.. _`//CycloneDDS/Domain/Internal/SocketSendBufferSize`:

//CycloneDDS/Domain/Internal/SocketSendBufferSize
-------------------------------------------------

Attributes: [max](`//CycloneDDS/Domain/Internal/SocketSendBufferSize[@max]`_), [min](`//CycloneDDS/Domain/Internal/SocketSendBufferSize[@min]`_)

The settings in this element control the size of the socket send buffers. The operating system provides some size send buffer upon creation of the socket, this option can be used to increase the size of the buffer beyond that initially provided by the operating system. If the buffer size cannot be increased to the requested minimum size, an error is reported.

The default setting requires a buffer of at least 64KiB.


.. _`//CycloneDDS/Domain/Internal/SocketSendBufferSize[@max]`:

//CycloneDDS/Domain/Internal/SocketSendBufferSize[@max]
-------------------------------------------------------

Number-with-unit

This sets the size of the socket send buffer to request, with the special value of "default" indicating that it should try to satisfy the minimum buffer size. If both are at "default", it will use whatever is the system default. It is ignored if the maximum is set to less than the minimum.

The unit must be specified explicitly. Recognised units: B (bytes), kB & KiB (2^10 bytes), MB & MiB (2^20 bytes), GB & GiB (2^30 bytes).

The default value is: ``default``


.. _`//CycloneDDS/Domain/Internal/SocketSendBufferSize[@min]`:

//CycloneDDS/Domain/Internal/SocketSendBufferSize[@min]
-------------------------------------------------------

Number-with-unit

This sets the minimum acceptable socket send buffer size, with the special value "default" indicating that whatever is available is acceptable.

The unit must be specified explicitly. Recognised units: B (bytes), kB & KiB (2^10 bytes), MB & MiB (2^20 bytes), GB & GiB (2^30 bytes).

The default value is: ``64 KiB``


.. _`//CycloneDDS/Domain/Internal/SquashParticipants`:

//CycloneDDS/Domain/Internal/SquashParticipants
-----------------------------------------------

Boolean

This element controls whether Cyclone DDS advertises all the domain participants it serves in DDSI (when set to false), or rather only one domain participant (the one corresponding to the Cyclone DDS process; when set to true). In the latter case, Cyclone DDS becomes the virtual owner of all readers and writers of all domain participants, dramatically reducing discovery traffic (a similar effect can be obtained by setting Internal/BuiltinEndpointSet to "minimal" but with less loss of information).

The default value is: ``false``


.. _`//CycloneDDS/Domain/Internal/SynchronousDeliveryLatencyBound`:

//CycloneDDS/Domain/Internal/SynchronousDeliveryLatencyBound
------------------------------------------------------------

Number-with-unit

This element controls whether samples sent by a writer with QoS settings transport\_priority >= SynchronousDeliveryPriorityThreshold and a latency\_budget at most this element's value will be delivered synchronously from the "recv" thread, all others will be delivered asynchronously through delivery queues. This reduces latency at the expense of aggregate bandwidth.

Valid values are finite durations with an explicit unit or the keyword 'inf' for infinity. Recognised units: ns, us, ms, s, min, hr, day.

The default value is: ``inf``


.. _`//CycloneDDS/Domain/Internal/SynchronousDeliveryPriorityThreshold`:

//CycloneDDS/Domain/Internal/SynchronousDeliveryPriorityThreshold
-----------------------------------------------------------------

Integer

This element controls whether samples sent by a writer with QoS settings latency\_budget <= SynchronousDeliveryLatencyBound and transport\_priority greater than or equal to this element's value will be delivered synchronously from the "recv" thread, all others will be delivered asynchronously through delivery queues. This reduces latency at the expense of aggregate bandwidth.

The default value is: ``0``


.. _`//CycloneDDS/Domain/Internal/Test`:

//CycloneDDS/Domain/Internal/Test
---------------------------------

Children: `//CycloneDDS/Domain/Internal/Test/XmitLossiness`_

Testing options.


.. _`//CycloneDDS/Domain/Internal/Test/XmitLossiness`:

//CycloneDDS/Domain/Internal/Test/XmitLossiness
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Integer

This element controls the fraction of outgoing packets to drop, specified as samples per thousand.

The default value is: ``0``


.. _`//CycloneDDS/Domain/Internal/UnicastResponseToSPDPMessages`:

//CycloneDDS/Domain/Internal/UnicastResponseToSPDPMessages
----------------------------------------------------------

Boolean

This element controls whether the response to a newly discovered participant is sent as a unicasted SPDP packet instead of rescheduling the periodic multicasted one. There is no known benefit to setting this to false.

The default value is: ``true``


.. _`//CycloneDDS/Domain/Internal/UseMulticastIfMreqn`:

//CycloneDDS/Domain/Internal/UseMulticastIfMreqn
------------------------------------------------

Integer

Do not use.

The default value is: ``0``


.. _`//CycloneDDS/Domain/Internal/Watermarks`:

//CycloneDDS/Domain/Internal/Watermarks
---------------------------------------

Children: `//CycloneDDS/Domain/Internal/Watermarks/WhcAdaptive`_, `//CycloneDDS/Domain/Internal/Watermarks/WhcHigh`_, `//CycloneDDS/Domain/Internal/Watermarks/WhcHighInit`_, `//CycloneDDS/Domain/Internal/Watermarks/WhcLow`_

Watermarks for flow-control.


.. _`//CycloneDDS/Domain/Internal/Watermarks/WhcAdaptive`:

//CycloneDDS/Domain/Internal/Watermarks/WhcAdaptive
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Boolean

This element controls whether Cyclone DDS will adapt the high-water mark to current traffic conditions based on retransmit requests and transmit pressure.

The default value is: ``true``


.. _`//CycloneDDS/Domain/Internal/Watermarks/WhcHigh`:

//CycloneDDS/Domain/Internal/Watermarks/WhcHigh
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Number-with-unit

This element sets the maximum allowed high-water mark for the Cyclone DDS WHCs, expressed in bytes. A writer is suspended when the WHC reaches this size.

The unit must be specified explicitly. Recognised units: B (bytes), kB & KiB (2^10 bytes), MB & MiB (2^20 bytes), GB & GiB (2^30 bytes).

The default value is: ``500 kB``


.. _`//CycloneDDS/Domain/Internal/Watermarks/WhcHighInit`:

//CycloneDDS/Domain/Internal/Watermarks/WhcHighInit
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Number-with-unit

This element sets the initial level of the high-water mark for the Cyclone DDS WHCs, expressed in bytes.

The unit must be specified explicitly. Recognised units: B (bytes), kB & KiB (2^10 bytes), MB & MiB (2^20 bytes), GB & GiB (2^30 bytes).

The default value is: ``30 kB``


.. _`//CycloneDDS/Domain/Internal/Watermarks/WhcLow`:

//CycloneDDS/Domain/Internal/Watermarks/WhcLow
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Number-with-unit

This element sets the low-water mark for the Cyclone DDS WHCs, expressed in bytes. A suspended writer resumes transmitting when its Cyclone DDS WHC shrinks to this size.

The unit must be specified explicitly. Recognised units: B (bytes), kB & KiB (2^10 bytes), MB & MiB (2^20 bytes), GB & GiB (2^30 bytes).

The default value is: ``1 kB``


.. _`//CycloneDDS/Domain/Internal/WriterLingerDuration`:

//CycloneDDS/Domain/Internal/WriterLingerDuration
-------------------------------------------------

Number-with-unit

This setting controls the maximum duration for which actual deletion of a reliable writer with unacknowledged data in its history will be postponed to provide proper reliable transmission.
The unit must be specified explicitly. Recognised units: ns, us, ms, s, min, hr, day.

The default value is: ``1 s``


.. _`//CycloneDDS/Domain/Partitioning`:

//CycloneDDS/Domain/Partitioning
================================

Children: `//CycloneDDS/Domain/Partitioning/IgnoredPartitions`_, `//CycloneDDS/Domain/Partitioning/NetworkPartitions`_, `//CycloneDDS/Domain/Partitioning/PartitionMappings`_

The Partitioning element specifies Cyclone DDS network partitions and how DCPS partition/topic combinations are mapped onto the network partitions.


.. _`//CycloneDDS/Domain/Partitioning/IgnoredPartitions`:

//CycloneDDS/Domain/Partitioning/IgnoredPartitions
--------------------------------------------------

Children: `//CycloneDDS/Domain/Partitioning/IgnoredPartitions/IgnoredPartition`_

The IgnoredPartitions element specifies DCPS partition/topic combinations that are not distributed over the network.


.. _`//CycloneDDS/Domain/Partitioning/IgnoredPartitions/IgnoredPartition`:

//CycloneDDS/Domain/Partitioning/IgnoredPartitions/IgnoredPartition
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Attributes: [DCPSPartitionTopic](`//CycloneDDS/Domain/Partitioning/IgnoredPartitions/IgnoredPartition[@DCPSPartitionTopic]`_)

Text

This element can prevent certain combinations of DCPS partition and topic from being transmitted over the network. Cyclone DDS will completely ignore readers and writers for which all DCPS partitions as well as their topic is ignored, not even creating DDSI readers and writers to mirror the DCPS ones.

The default value is: ``<empty>``


.. _`//CycloneDDS/Domain/Partitioning/IgnoredPartitions/IgnoredPartition[@DCPSPartitionTopic]`:

//CycloneDDS/Domain/Partitioning/IgnoredPartitions/IgnoredPartition[@DCPSPartitionTopic]
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Text

This attribute specifies a partition and a topic expression, separated by a single '.', which are used to determine if a given partition and topic will be ignored or not. The expressions may use the usual wildcards '\*' and '?'. Cyclone DDS will consider a wildcard DCPS partition to match an expression if a string that satisfies both expressions exists.

The default value is: ``<empty>``


.. _`//CycloneDDS/Domain/Partitioning/NetworkPartitions`:

//CycloneDDS/Domain/Partitioning/NetworkPartitions
--------------------------------------------------

Children: `//CycloneDDS/Domain/Partitioning/NetworkPartitions/NetworkPartition`_

The NetworkPartitions element specifies the Cyclone DDS network partitions.


.. _`//CycloneDDS/Domain/Partitioning/NetworkPartitions/NetworkPartition`:

//CycloneDDS/Domain/Partitioning/NetworkPartitions/NetworkPartition
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Attributes: [Address](`//CycloneDDS/Domain/Partitioning/NetworkPartitions/NetworkPartition[@Address]`_), [Interface](`//CycloneDDS/Domain/Partitioning/NetworkPartitions/NetworkPartition[@Interface]`_), [Name](`//CycloneDDS/Domain/Partitioning/NetworkPartitions/NetworkPartition[@Name]`_)

Text

This element defines a Cyclone DDS network partition.

The default value is: ``<empty>``


.. _`//CycloneDDS/Domain/Partitioning/NetworkPartitions/NetworkPartition[@Address]`:

//CycloneDDS/Domain/Partitioning/NetworkPartitions/NetworkPartition[@Address]
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Text

This attribute specifies the addresses associated with the network partition as a comma-separated list. The addresses are typically multicast addresses. Non-multicast addresses are allowed, provided the "Interface" attribute is not used: * An address matching the address or the "external address" (see General/ExternalNetworkAddress; default is the actual address) of a configured interface results in adding the corresponding "external" address to the set of advertised unicast addresses.
 * An address corresponding to the (external) address of a configured interface, but not the address of the host itself, for example, a match when masking the addresses with the netmask for IPv4, results in adding the external address. For IPv4, this requires the host part to be all-zero.

Readers matching this network partition (cf. Partitioning/PartitionMappings) will advertise all addresses listed to the matching writers via the discovery protocol and will join the specified multicast groups. The writers will select the most suitable address from the addresses advertised by the readers.

The unicast addresses advertised by a reader are the only unicast addresses a writer will use to send data to it and are used to select the subset of network interfaces to use for transmitting multicast data with the intent of reaching it.

The default value is: ``<empty>``


.. _`//CycloneDDS/Domain/Partitioning/NetworkPartitions/NetworkPartition[@Interface]`:

//CycloneDDS/Domain/Partitioning/NetworkPartitions/NetworkPartition[@Interface]
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Text

This attribute takes a comma-separated list of interface name that the reader is willing to receive data on. This is implemented by adding the interface addresses to the set address set configured using the sibling "Address" attribute. See there for more details.

The default value is: ``<empty>``


.. _`//CycloneDDS/Domain/Partitioning/NetworkPartitions/NetworkPartition[@Name]`:

//CycloneDDS/Domain/Partitioning/NetworkPartitions/NetworkPartition[@Name]
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Text

This attribute specifies the name of this Cyclone DDS network partition. Two network partitions cannot have the same name. Partition mappings (cf. Partitioning/PartitionMappings) refer to network partitions using these names.

The default value is: ``<empty>``


.. _`//CycloneDDS/Domain/Partitioning/PartitionMappings`:

//CycloneDDS/Domain/Partitioning/PartitionMappings
--------------------------------------------------

Children: `//CycloneDDS/Domain/Partitioning/PartitionMappings/PartitionMapping`_

The PartitionMappings element specifies the mapping from DCPS partition/topic combinations to Cyclone DDS network partitions.


.. _`//CycloneDDS/Domain/Partitioning/PartitionMappings/PartitionMapping`:

//CycloneDDS/Domain/Partitioning/PartitionMappings/PartitionMapping
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Attributes: [DCPSPartitionTopic](`//CycloneDDS/Domain/Partitioning/PartitionMappings/PartitionMapping[@DCPSPartitionTopic]`_), [NetworkPartition](`//CycloneDDS/Domain/Partitioning/PartitionMappings/PartitionMapping[@NetworkPartition]`_)

Text

This element defines a mapping from a DCPS partition/topic combination to a Cyclone DDS network partition. This allows partitioning data flows by using special multicast addresses for part of the data and possibly encrypting the data flow.

The default value is: ``<empty>``


.. _`//CycloneDDS/Domain/Partitioning/PartitionMappings/PartitionMapping[@DCPSPartitionTopic]`:

//CycloneDDS/Domain/Partitioning/PartitionMappings/PartitionMapping[@DCPSPartitionTopic]
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Text

This attribute specifies a partition and a topic expression, separated by a single '.', which are used to determine if a given partition and topic maps to the Cyclone DDS network partition named by the NetworkPartition attribute in this PartitionMapping element. The expressions may use the usual wildcards '\*' and '?'. Cyclone DDS will consider a wildcard DCPS partition to match an expression if there exists a string that satisfies both expressions.

The default value is: ``<empty>``


.. _`//CycloneDDS/Domain/Partitioning/PartitionMappings/PartitionMapping[@NetworkPartition]`:

//CycloneDDS/Domain/Partitioning/PartitionMappings/PartitionMapping[@NetworkPartition]
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Text

This attribute specifies which Cyclone DDS network partition is to be used for DCPS partition/topic combinations matching the DCPSPartitionTopic attribute within this PartitionMapping element.

The default value is: ``<empty>``


.. _`//CycloneDDS/Domain/SSL`:

//CycloneDDS/Domain/SSL
=======================

Children: `//CycloneDDS/Domain/SSL/CertificateVerification`_, `//CycloneDDS/Domain/SSL/Ciphers`_, `//CycloneDDS/Domain/SSL/Enable`_, `//CycloneDDS/Domain/SSL/EntropyFile`_, `//CycloneDDS/Domain/SSL/KeyPassphrase`_, `//CycloneDDS/Domain/SSL/KeystoreFile`_, `//CycloneDDS/Domain/SSL/MinimumTLSVersion`_, `//CycloneDDS/Domain/SSL/SelfSignedCertificates`_, `//CycloneDDS/Domain/SSL/VerifyClient`_

The SSL element allows specifying various parameters related to using SSL/TLS for DDSI over TCP.


.. _`//CycloneDDS/Domain/SSL/CertificateVerification`:

//CycloneDDS/Domain/SSL/CertificateVerification
-----------------------------------------------

Boolean

If disabled this allows SSL connections to occur even if an X509 certificate fails verification.

The default value is: ``true``


.. _`//CycloneDDS/Domain/SSL/Ciphers`:

//CycloneDDS/Domain/SSL/Ciphers
-------------------------------

Text

The set of ciphers used by SSL/TLS

The default value is: ``ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH``


.. _`//CycloneDDS/Domain/SSL/Enable`:

//CycloneDDS/Domain/SSL/Enable
------------------------------

Boolean

This enables SSL/TLS for TCP.

The default value is: ``false``


.. _`//CycloneDDS/Domain/SSL/EntropyFile`:

//CycloneDDS/Domain/SSL/EntropyFile
-----------------------------------

Text

The SSL/TLS random entropy file name.

The default value is: ``<empty>``


.. _`//CycloneDDS/Domain/SSL/KeyPassphrase`:

//CycloneDDS/Domain/SSL/KeyPassphrase
-------------------------------------

Text

The SSL/TLS key pass phrase for encrypted keys.

The default value is: ``secret``


.. _`//CycloneDDS/Domain/SSL/KeystoreFile`:

//CycloneDDS/Domain/SSL/KeystoreFile
------------------------------------

Text

The SSL/TLS key and certificate store file name. The keystore must be in PEM format.

The default value is: ``keystore``


.. _`//CycloneDDS/Domain/SSL/MinimumTLSVersion`:

//CycloneDDS/Domain/SSL/MinimumTLSVersion
-----------------------------------------

Text

The minimum TLS version that may be negotiated, valid values are 1.2 and 1.3.

The default value is: ``1.3``


.. _`//CycloneDDS/Domain/SSL/SelfSignedCertificates`:

//CycloneDDS/Domain/SSL/SelfSignedCertificates
----------------------------------------------

Boolean

This enables the use of self signed X509 certificates.

The default value is: ``false``


.. _`//CycloneDDS/Domain/SSL/VerifyClient`:

//CycloneDDS/Domain/SSL/VerifyClient
------------------------------------

Boolean

This enables an SSL server to check the X509 certificate of a connecting client.

The default value is: ``true``


.. _`//CycloneDDS/Domain/Security`:

//CycloneDDS/Domain/Security
============================

Children: `//CycloneDDS/Domain/Security/AccessControl`_, `//CycloneDDS/Domain/Security/Authentication`_, `//CycloneDDS/Domain/Security/Cryptographic`_

This element is used to configure Cyclone DDS with the DDS Security specification plugins and settings.


.. _`//CycloneDDS/Domain/Security/AccessControl`:

//CycloneDDS/Domain/Security/AccessControl
------------------------------------------

Children: `//CycloneDDS/Domain/Security/AccessControl/Governance`_, `//CycloneDDS/Domain/Security/AccessControl/Library`_, `//CycloneDDS/Domain/Security/AccessControl/Permissions`_, `//CycloneDDS/Domain/Security/AccessControl/PermissionsCA`_

This element configures the Access Control plugin of the DDS Security specification.


.. _`//CycloneDDS/Domain/Security/AccessControl/Governance`:

//CycloneDDS/Domain/Security/AccessControl/Governance
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Text

URI to the shared Governance Document signed by the Permissions CA in S/MIME format

URI schemes: file, data<br>
Examples file URIs:

<Governance>file:governance.smime</Governance>

<Governance>file:/home/myuser/governance.smime</Governance><br>
<Governance><![CDATA[data:,MIME-Version: 1.0

Content-Type: multipart/signed; protocol="application/x-pkcs7-signature"; micalg="sha-256"; boundary="----F9A8A198D6F08E1285A292ADF14DD04F"

This is an S/MIME signed message 

------F9A8A198D6F08E1285A292ADF14DD04F

<?xml version="1.0" encoding="UTF-8"?>

<dds xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"

xsi:noNamespaceSchemaLocation="omg\_shared\_ca\_governance.xsd">

<domain\_access\_rules>

 . . . 

</domain\_access\_rules>

</dds>

...

------F9A8A198D6F08E1285A292ADF14DD04F

Content-Type: application/x-pkcs7-signature; name="smime.p7s"

Content-Transfer-Encoding: base64

Content-Disposition: attachment; filename="smime.p7s"

MIIDuAYJKoZIhv ...al5s=

------F9A8A198D6F08E1285A292ADF14DD04F-]]</Governance>

The default value is: ``<empty>``


.. _`//CycloneDDS/Domain/Security/AccessControl/Library`:

//CycloneDDS/Domain/Security/AccessControl/Library
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Attributes: [finalizeFunction](`//CycloneDDS/Domain/Security/AccessControl/Library[@finalizeFunction]`_), [initFunction](`//CycloneDDS/Domain/Security/AccessControl/Library[@initFunction]`_), [path](`//CycloneDDS/Domain/Security/AccessControl/Library[@path]`_)

Text

This element specifies the library to be loaded as the DDS Security Access Control plugin.

The default value is: ``<empty>``


.. _`//CycloneDDS/Domain/Security/AccessControl/Library[@finalizeFunction]`:

//CycloneDDS/Domain/Security/AccessControl/Library[@finalizeFunction]
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Text

This element names the finalization function of Access Control plugin. This function is called to let the plugin release its resources.

The default value is: ``finalize\_access\_control``


.. _`//CycloneDDS/Domain/Security/AccessControl/Library[@initFunction]`:

//CycloneDDS/Domain/Security/AccessControl/Library[@initFunction]
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Text

This element names the initialization function of Access Control plugin. This function is called after loading the plugin library for instantiation purposes. The Init function must return an object that implements the DDS Security Access Control interface.

The default value is: ``init\_access\_control``


.. _`//CycloneDDS/Domain/Security/AccessControl/Library[@path]`:

//CycloneDDS/Domain/Security/AccessControl/Library[@path]
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Text

This element points to the path of Access Control plugin library.

It can be either absolute path excluding file extension ( /usr/lib/dds\_security\_ac ) or single file without extension ( dds\_security\_ac ).

If a single file is supplied, the library is located by the current working directory, or LD\_LIBRARY\_PATH for Unix systems, and PATH for Windows systems.

The default value is: ``dds\_security\_ac``


.. _`//CycloneDDS/Domain/Security/AccessControl/Permissions`:

//CycloneDDS/Domain/Security/AccessControl/Permissions
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Text

URI to the DomainParticipant permissions document signed by the Permissions CA in S/MIME format

The permissions document specifies the permissions to be applied to a domain.<br>
Example file URIs:

<Permissions>file:permissions\_document.p7s</Permissions>

<Permissions>file:/path\_to/permissions\_document.p7s</Permissions>

Example data URI:

<Permissions><![CDATA[data:,.........]]</Permissions>

The default value is: ``<empty>``


.. _`//CycloneDDS/Domain/Security/AccessControl/PermissionsCA`:

//CycloneDDS/Domain/Security/AccessControl/PermissionsCA
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Text

URI to an X509 certificate for the PermissionsCA in PEM format.

Supported URI schemes: file, data

The file and data schemas shall refer to a X.509 v3 certificate (see X.509 v3 ITU-T Recommendation X.509 (2005) [39]) in PEM format.<br>
Examples:<br>
<PermissionsCA>file:permissions\_ca.pem</PermissionsCA>

<PermissionsCA>file:/home/myuser/permissions\_ca.pem</PermissionsCA><br>
<PermissionsCA>data:<strong>,</strong>-----BEGIN CERTIFICATE-----

MIIC3DCCAcQCCQCWE5x+Z ... PhovK0mp2ohhRLYI0ZiyYQ==

-----END CERTIFICATE-----</PermissionsCA>

The default value is: ``<empty>``


.. _`//CycloneDDS/Domain/Security/Authentication`:

//CycloneDDS/Domain/Security/Authentication
-------------------------------------------

Children: `//CycloneDDS/Domain/Security/Authentication/CRL`_, `//CycloneDDS/Domain/Security/Authentication/IdentityCA`_, `//CycloneDDS/Domain/Security/Authentication/IdentityCertificate`_, `//CycloneDDS/Domain/Security/Authentication/IncludeOptionalFields`_, `//CycloneDDS/Domain/Security/Authentication/Library`_, `//CycloneDDS/Domain/Security/Authentication/Password`_, `//CycloneDDS/Domain/Security/Authentication/PrivateKey`_, `//CycloneDDS/Domain/Security/Authentication/TrustedCADirectory`_

This element configures the Authentication plugin of the DDS Security specification.


.. _`//CycloneDDS/Domain/Security/Authentication/CRL`:

//CycloneDDS/Domain/Security/Authentication/CRL
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Text

Optional URI to load an X509 Certificate Revocation List

Supported URI schemes: file, data

Examples:

<CRL>file:crl.pem</CRL>

<CRL>data:,-----BEGIN X509 CRL-----<br>
MIIEpAIBAAKCAQEA3HIh...AOBaaqSV37XBUJg=<br>
-----END X509 CRL-----</CRL>

The default value is: ``<empty>``


.. _`//CycloneDDS/Domain/Security/Authentication/IdentityCA`:

//CycloneDDS/Domain/Security/Authentication/IdentityCA
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Text

URI to the X509 certificate [39] of the Identity CA that is the signer of Identity Certificate.

Supported URI schemes: file, data

The file and data schemas shall refer to a X.509 v3 certificate (see X.509 v3 ITU-T Recommendation X.509 (2005) [39]) in PEM format.

Examples:

<IdentityCA>file:identity\_ca.pem</IdentityCA>

<IdentityCA>data:,-----BEGIN CERTIFICATE-----<br>
MIIC3DCCAcQCCQCWE5x+Z...PhovK0mp2ohhRLYI0ZiyYQ==<br>
-----END CERTIFICATE-----</IdentityCA>

The default value is: ``<empty>``


.. _`//CycloneDDS/Domain/Security/Authentication/IdentityCertificate`:

//CycloneDDS/Domain/Security/Authentication/IdentityCertificate
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Text

An identity certificate will identify all participants in the OSPL instance.<br>The content is URI to an X509 certificate signed by the IdentityCA in PEM format containing the signed public key.

Supported URI schemes: file, data

Examples:

<IdentityCertificate>file:participant1\_identity\_cert.pem</IdentityCertificate>

<IdentityCertificate>data:,-----BEGIN CERTIFICATE-----<br>
MIIDjjCCAnYCCQDCEu9...6rmT87dhTo=<br>
-----END CERTIFICATE-----</IdentityCertificate>

The default value is: ``<empty>``


.. _`//CycloneDDS/Domain/Security/Authentication/IncludeOptionalFields`:

//CycloneDDS/Domain/Security/Authentication/IncludeOptionalFields
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Boolean

The authentication handshake tokens may contain optional fields to be included for finding interoperability problems. If this parameter is set to true the optional fields are included in the handshake token exchange.

The default value is: ``false``


.. _`//CycloneDDS/Domain/Security/Authentication/Library`:

//CycloneDDS/Domain/Security/Authentication/Library
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Attributes: [finalizeFunction](`//CycloneDDS/Domain/Security/Authentication/Library[@finalizeFunction]`_), [initFunction](`//CycloneDDS/Domain/Security/Authentication/Library[@initFunction]`_), [path](`//CycloneDDS/Domain/Security/Authentication/Library[@path]`_)

Text

This element specifies the library to be loaded as the DDS Security Access Control plugin.

The default value is: ``<empty>``


.. _`//CycloneDDS/Domain/Security/Authentication/Library[@finalizeFunction]`:

//CycloneDDS/Domain/Security/Authentication/Library[@finalizeFunction]
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Text

This element names the finalization function of the Authentication plugin. This function is called to let the plugin release its resources.

The default value is: ``finalize\_authentication``


.. _`//CycloneDDS/Domain/Security/Authentication/Library[@initFunction]`:

//CycloneDDS/Domain/Security/Authentication/Library[@initFunction]
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Text

This element names the initialization function of the Authentication plugin. This function is called after loading the plugin library for instantiation purposes. The Init function must return an object that implements the DDS Security Authentication interface.

The default value is: ``init\_authentication``


.. _`//CycloneDDS/Domain/Security/Authentication/Library[@path]`:

//CycloneDDS/Domain/Security/Authentication/Library[@path]
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Text

This element points to the path of the Authentication plugin library.

It can be either absolute path excluding file extension ( /usr/lib/dds\_security\_auth ) or single file without extension ( dds\_security\_auth ).

If a single file is supplied, the library is located by the current working directory, or LD\_LIBRARY\_PATH for Unix systems, and PATH for Windows systems.

The default value is: ``dds\_security\_auth``


.. _`//CycloneDDS/Domain/Security/Authentication/Password`:

//CycloneDDS/Domain/Security/Authentication/Password
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Text

A password is used to decrypt the private\_key.

The value of the password property shall be interpreted as the Base64 encoding of the AES-128 key that shall be used to decrypt the private\_key using AES128-CBC.

If the password property is not present, then the value supplied in the private\_key property must contain the unencrypted private key.

The default value is: ``<empty>``


.. _`//CycloneDDS/Domain/Security/Authentication/PrivateKey`:

//CycloneDDS/Domain/Security/Authentication/PrivateKey
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Text

URI to access the private Private Key for all of the participants in the OSPL federation.

Supported URI schemes: file, data

Examples:

<PrivateKey>file:identity\_ca\_private\_key.pem</PrivateKey>

<PrivateKey>data:,-----BEGIN RSA PRIVATE KEY-----<br>
MIIEpAIBAAKCAQEA3HIh...AOBaaqSV37XBUJg==<br>
-----END RSA PRIVATE KEY-----</PrivateKey>

The default value is: ``<empty>``


.. _`//CycloneDDS/Domain/Security/Authentication/TrustedCADirectory`:

//CycloneDDS/Domain/Security/Authentication/TrustedCADirectory
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Text

Trusted CA Directory which contains trusted CA certificates as separated files.

The default value is: ``<empty>``


.. _`//CycloneDDS/Domain/Security/Cryptographic`:

//CycloneDDS/Domain/Security/Cryptographic
------------------------------------------

Children: `//CycloneDDS/Domain/Security/Cryptographic/Library`_

This element configures the Cryptographic plugin of the DDS Security specification.


.. _`//CycloneDDS/Domain/Security/Cryptographic/Library`:

//CycloneDDS/Domain/Security/Cryptographic/Library
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Attributes: [finalizeFunction](`//CycloneDDS/Domain/Security/Cryptographic/Library[@finalizeFunction]`_), [initFunction](`//CycloneDDS/Domain/Security/Cryptographic/Library[@initFunction]`_), [path](`//CycloneDDS/Domain/Security/Cryptographic/Library[@path]`_)

Text

This element specifies the library to be loaded as the DDS Security Cryptographic plugin.

The default value is: ``<empty>``


.. _`//CycloneDDS/Domain/Security/Cryptographic/Library[@finalizeFunction]`:

//CycloneDDS/Domain/Security/Cryptographic/Library[@finalizeFunction]
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Text

This element names the finalization function of the Cryptographic plugin. This function is called to let the plugin release its resources.

The default value is: ``finalize\_crypto``


.. _`//CycloneDDS/Domain/Security/Cryptographic/Library[@initFunction]`:

//CycloneDDS/Domain/Security/Cryptographic/Library[@initFunction]
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Text

This element names the initialization function of the Cryptographic plugin. This function is called after loading the plugin library for instantiation purposes. The Init function must return an object that implements the DDS Security Cryptographic interface.

The default value is: ``init\_crypto``


.. _`//CycloneDDS/Domain/Security/Cryptographic/Library[@path]`:

//CycloneDDS/Domain/Security/Cryptographic/Library[@path]
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Text

This element points to the path of the Cryptographic plugin library.

It can be either absolute path excluding file extension ( /usr/lib/dds\_security\_crypto ) or single file without extension ( dds\_security\_crypto ).

If a single file is supplied, the is library located by the current working directory, or LD\_LIBRARY\_PATH for Unix systems, and PATH for Windows systems.

The default value is: ``dds\_security\_crypto``


.. _`//CycloneDDS/Domain/SharedMemory`:

//CycloneDDS/Domain/SharedMemory
================================

Children: `//CycloneDDS/Domain/SharedMemory/Enable`_, `//CycloneDDS/Domain/SharedMemory/Locator`_, `//CycloneDDS/Domain/SharedMemory/LogLevel`_, `//CycloneDDS/Domain/SharedMemory/Prefix`_

The Shared Memory element allows specifying various parameters related to using shared memory.


.. _`//CycloneDDS/Domain/SharedMemory/Enable`:

//CycloneDDS/Domain/SharedMemory/Enable
---------------------------------------

Boolean

This element allows for enabling shared memory in Cyclone DDS.

The default value is: ``false``


.. _`//CycloneDDS/Domain/SharedMemory/Locator`:

//CycloneDDS/Domain/SharedMemory/Locator
----------------------------------------

Text

Explicitly set the Iceoryx locator used by Cyclone to check whether a pair of processes is attached to the same Iceoryx shared memory.  The default is to use one of the MAC addresses of the machine, which should work well in most cases.

The default value is: ``<empty>``


.. _`//CycloneDDS/Domain/SharedMemory/LogLevel`:

//CycloneDDS/Domain/SharedMemory/LogLevel
-----------------------------------------

One of: off, fatal, error, warn, info, debug, verbose

This element decides the verbosity level of shared memory message:
 * off: no log

 * fatal: show fatal log

 * error: show error log

 * warn: show warn log

 * info: show info log

 * debug: show debug log

 * verbose: show verbose log

If you don't want to see any log from shared memory, use off to disable logging.

The default value is: ``info``


.. _`//CycloneDDS/Domain/SharedMemory/Prefix`:

//CycloneDDS/Domain/SharedMemory/Prefix
---------------------------------------

Text

Override the Iceoryx service name used by Cyclone.

The default value is: ``DDS\_CYCLONE``


.. _`//CycloneDDS/Domain/Sizing`:

//CycloneDDS/Domain/Sizing
==========================

Children: `//CycloneDDS/Domain/Sizing/ReceiveBufferChunkSize`_, `//CycloneDDS/Domain/Sizing/ReceiveBufferSize`_

The Sizing element allows you to specify various configuration settings dealing with expected system sizes, buffer sizes, &c.


.. _`//CycloneDDS/Domain/Sizing/ReceiveBufferChunkSize`:

//CycloneDDS/Domain/Sizing/ReceiveBufferChunkSize
-------------------------------------------------

Number-with-unit

This element specifies the size of one allocation unit in the receive buffer. It must be greater than the maximum packet size by a modest amount (too large packets are dropped). Each allocation is shrunk immediately after processing a message or freed straightaway.

The unit must be specified explicitly. Recognised units: B (bytes), kB & KiB (2^10 bytes), MB & MiB (2^20 bytes), GB & GiB (2^30 bytes).

The default value is: ``128 KiB``


.. _`//CycloneDDS/Domain/Sizing/ReceiveBufferSize`:

//CycloneDDS/Domain/Sizing/ReceiveBufferSize
--------------------------------------------

Number-with-unit

This element sets the size of a single receive buffer. Many receive buffers may be needed. The minimum workable size is a little larger than Sizing/ReceiveBufferChunkSize, and the value used is taken as the configured value and the actual minimum workable size.

The unit must be specified explicitly. Recognised units: B (bytes), kB & KiB (2^10 bytes), MB & MiB (2^20 bytes), GB & GiB (2^30 bytes).

The default value is: ``1 MiB``


.. _`//CycloneDDS/Domain/TCP`:

//CycloneDDS/Domain/TCP
=======================

Children: `//CycloneDDS/Domain/TCP/AlwaysUsePeeraddrForUnicast`_, `//CycloneDDS/Domain/TCP/Enable`_, `//CycloneDDS/Domain/TCP/NoDelay`_, `//CycloneDDS/Domain/TCP/Port`_, `//CycloneDDS/Domain/TCP/ReadTimeout`_, `//CycloneDDS/Domain/TCP/WriteTimeout`_

The TCP element allows you to specify various parameters related to running DDSI over TCP.


.. _`//CycloneDDS/Domain/TCP/AlwaysUsePeeraddrForUnicast`:

//CycloneDDS/Domain/TCP/AlwaysUsePeeraddrForUnicast
---------------------------------------------------

Boolean

Setting this to true means the unicast addresses in SPDP packets will be ignored, and the peer address from the TCP connection will be used instead. This may help work around incorrectly advertised addresses when using TCP.

The default value is: ``false``


.. _`//CycloneDDS/Domain/TCP/Enable`:

//CycloneDDS/Domain/TCP/Enable
------------------------------

One of: false, true, default

This element enables the optional TCP transport - deprecated, use General/Transport instead.

The default value is: ``default``


.. _`//CycloneDDS/Domain/TCP/NoDelay`:

//CycloneDDS/Domain/TCP/NoDelay
-------------------------------

Boolean

This element enables the TCP\_NODELAY socket option, preventing multiple DDSI messages from being sent in the same TCP request. Setting this option typically optimises latency over throughput.

The default value is: ``true``


.. _`//CycloneDDS/Domain/TCP/Port`:

//CycloneDDS/Domain/TCP/Port
----------------------------

Integer

This element specifies the TCP port number on which Cyclone DDS accepts connections. If the port is set, it is used in entity locators, published with DDSI discovery, dynamically allocated if zero, and disabled if -1 or not configured. If disabled other DDSI services will not be able to establish connections with the service, the service can only communicate by establishing connections to other services.

The default value is: ``-1``


.. _`//CycloneDDS/Domain/TCP/ReadTimeout`:

//CycloneDDS/Domain/TCP/ReadTimeout
-----------------------------------

Number-with-unit

This element specifies the timeout for blocking TCP read operations. If this timeout expires then the connection is closed.

The unit must be specified explicitly. Recognised units: ns, us, ms, s, min, hr, day.

The default value is: ``2 s``


.. _`//CycloneDDS/Domain/TCP/WriteTimeout`:

//CycloneDDS/Domain/TCP/WriteTimeout
------------------------------------

Number-with-unit

This element specifies the timeout for blocking TCP write operations. If this timeout expires then the connection is closed.

The unit must be specified explicitly. Recognised units: ns, us, ms, s, min, hr, day.

The default value is: ``2 s``


.. _`//CycloneDDS/Domain/Threads`:

//CycloneDDS/Domain/Threads
===========================

Children: `//CycloneDDS/Domain/Threads/Thread`_

This element is used to set thread properties.


.. _`//CycloneDDS/Domain/Threads/Thread`:

//CycloneDDS/Domain/Threads/Thread
----------------------------------

Attributes: [Name](`//CycloneDDS/Domain/Threads/Thread[@Name]`_)
Children: `//CycloneDDS/Domain/Threads/Thread/Scheduling`_, `//CycloneDDS/Domain/Threads/Thread/StackSize`_

This element is used to set thread properties.


.. _`//CycloneDDS/Domain/Threads/Thread[@Name]`:

//CycloneDDS/Domain/Threads/Thread[@Name]
-----------------------------------------

Text

The Name of the thread for which properties are being set. The following threads exist:

 * gc: garbage collector thread involved in deleting entities;

 * recv: receive thread, taking data from the network and running the protocol state machine;

 * dq.builtins: delivery thread for DDSI-builtin data, primarily for discovery;

 * lease: DDSI liveliness monitoring;

 * tev: general timed-event handling, retransmits and discovery;

 * fsm: finite state machine thread for handling security handshake;

 * xmit.CHAN: transmit thread for channel CHAN;

 * dq.CHAN: delivery thread for channel CHAN;

 * tev.CHAN: timed-event thread for channel CHAN.


The default value is: ``<empty>``


.. _`//CycloneDDS/Domain/Threads/Thread/Scheduling`:

//CycloneDDS/Domain/Threads/Thread/Scheduling
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Children: `//CycloneDDS/Domain/Threads/Thread/Scheduling/Class`_, `//CycloneDDS/Domain/Threads/Thread/Scheduling/Priority`_

This element configures the scheduling properties of the thread.


.. _`//CycloneDDS/Domain/Threads/Thread/Scheduling/Class`:

//CycloneDDS/Domain/Threads/Thread/Scheduling/Class
"""""""""""""""""""""""""""""""""""""""""""""""""""

One of: realtime, timeshare, default

This element specifies the thread scheduling class (realtime, timeshare or default). The user may need special privileges from the underlying operating system to be able to assign some of the privileged scheduling classes.

The default value is: ``default``


.. _`//CycloneDDS/Domain/Threads/Thread/Scheduling/Priority`:

//CycloneDDS/Domain/Threads/Thread/Scheduling/Priority
""""""""""""""""""""""""""""""""""""""""""""""""""""""

Text

This element specifies the thread priority (decimal integer or default). Only priorities supported by the underlying operating system can be assigned to this element. The user may need special privileges from the underlying operating system to be able to assign some of the privileged priorities.

The default value is: ``default``


.. _`//CycloneDDS/Domain/Threads/Thread/StackSize`:

//CycloneDDS/Domain/Threads/Thread/StackSize
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Number-with-unit

This element configures the stack size for this thread. The default value default leaves the stack size at the operating system default.

The unit must be specified explicitly. Recognised units: B (bytes), kB & KiB (2^10 bytes), MB & MiB (2^20 bytes), GB & GiB (2^30 bytes).

The default value is: ``default``


.. _`//CycloneDDS/Domain/Tracing`:

//CycloneDDS/Domain/Tracing
===========================

Children: `//CycloneDDS/Domain/Tracing/AppendToFile`_, `//CycloneDDS/Domain/Tracing/Category`_, `//CycloneDDS/Domain/Tracing/OutputFile`_, `//CycloneDDS/Domain/Tracing/PacketCaptureFile`_, `//CycloneDDS/Domain/Tracing/Verbosity`_

The Tracing element controls the amount and type of information that is written into the tracing log by the DDSI service. This is useful to track the DDSI service during application development.


.. _`//CycloneDDS/Domain/Tracing/AppendToFile`:

//CycloneDDS/Domain/Tracing/AppendToFile
----------------------------------------

Boolean

This option specifies whether the output should be appended to an existing log file. The default is to create a new log file each time, which is generally the best option if a detailed log is generated.

The default value is: ``false``


.. _`//CycloneDDS/Domain/Tracing/Category`:

//CycloneDDS/Domain/Tracing/Category
------------------------------------

One of:
* Comma-separated list of: fatal, error, warning, info, config, discovery, data, radmin, timing, traffic, topic, tcp, plist, whc, throttle, rhc, content, shm, trace
* Or empty

This element enables individual logging categories. These are enabled in addition to those enabled by Tracing/Verbosity. Recognised categories are:

 * fatal: all fatal errors, errors causing immediate termination

 * error: failures probably impacting correctness but not necessarily causing immediate termination

 * warning: abnormal situations that will likely not impact correctness

 * config: full dump of the configuration

 * info: general informational notices

 * discovery: all discovery activity

 * data: include data content of samples in traces

 * radmin: receive buffer administration

 * timing: periodic reporting of CPU loads per thread

 * traffic: periodic reporting of total outgoing data

 * whc: tracing of writer history cache changes

 * tcp: tracing of TCP-specific activity

 * topic: tracing of topic definitions

 * plist: tracing of discovery parameter list interpretation


In addition, there is the keyword trace that enables all but radmin, topic, plist and whc.
The categorisation of tracing output is incomplete and hence most of the verbosity levels and categories are not of much use in the current release. This is an ongoing process and here we describe the target situation rather than the current situation. Currently, the most useful is trace.

The default value is: ``<empty>``


.. _`//CycloneDDS/Domain/Tracing/OutputFile`:

//CycloneDDS/Domain/Tracing/OutputFile
--------------------------------------

Text

This option specifies where the logging is printed to. Note that stdout and stderr are treated as special values, representing "standard out" and "standard error" respectively. No file is created unless logging categories are enabled using the Tracing/Verbosity or Tracing/EnabledCategory settings.

The default value is: ``cyclonedds.log``


.. _`//CycloneDDS/Domain/Tracing/PacketCaptureFile`:

//CycloneDDS/Domain/Tracing/PacketCaptureFile
---------------------------------------------

Text

This option specifies the file to which received and sent packets will be logged in the "pcap" format suitable for analysis using common networking tools, such as WireShark. IP and UDP headers are fictitious, in particular the destination address of received packets. The TTL may be used to distinguish between sent and received packets: it is 255 for sent packets and 128 for received ones. Currently IPv4 only.

The default value is: ``<empty>``


.. _`//CycloneDDS/Domain/Tracing/Verbosity`:

//CycloneDDS/Domain/Tracing/Verbosity
-------------------------------------

One of: finest, finer, fine, config, info, warning, severe, none

This element enables standard groups of categories, based on a desired verbosity level. This is in addition to the categories enabled by the Tracing/Category setting. Recognised verbosity levels and the categories they map to are:
 * none: no Cyclone DDS log

 * severe: error and fatal

 * warning: severe + warning

 * info: warning + info

 * config: info + config

 * fine: config + discovery

 * finer: fine + traffic and timing

 * finest: finer + trace


While none prevents any message from being written to a DDSI2 log file.

The categorisation of tracing output is incomplete and hence most of the verbosity levels and categories are not of much use in the current release. This is an ongoing process and here we describe the target situation rather than the current situation. Currently, the most useful verbosity levels are config, fine and finest.

The default value is: ``none``

..
   generated from ddsi_config.h[7f55b8f40b2e7f5984106abb0470128eb3d50017] 
   generated from ddsi__cfgunits.h[bd22f0c0ed210501d0ecd3b07c992eca549ef5aa] 
   generated from ddsi__cfgelems.h[771184755c23b94599f2ffd6e8c242dcea7d2658] 
   generated from ddsi_config.c[fec4d055c2154717183efd6610d46ea48236cdea] 
   generated from _confgen.h[1b1d88a85bd851f4e87118505ded33f7b33b0435] 
   generated from _confgen.c[237308acd53897a34e8c643e16e05a61d73ffd65] 
   generated from generate_rnc.c[b50e4b7ab1d04b2bc1d361a0811247c337b74934] 
   generated from generate_md.c[789b92e422631684352909cfb8bf43f6ceb16a01] 
   generated from generate_rst.c[636ceeed42784e8508dd412b88dfd5f3b44b191b] 
   generated from generate_xsd.c[6b6818d7f17a35d56c376c04ec1410427f34c0f0] 
   generated from generate_defconfig.c[ee80ba6719e71a457a85f1a638fe52f3756916d5] 
