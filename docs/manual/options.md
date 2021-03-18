# //CycloneDDS
Children: [Domain](#cycloneddsdomain)

CycloneDDS configuration


## //CycloneDDS/Domain
Attributes: [Id](#cycloneddsdomainid)
Children: [Compatibility](#cycloneddsdomaincompatibility), [Discovery](#cycloneddsdomaindiscovery), [General](#cycloneddsdomaingeneral), [Internal](#cycloneddsdomaininternal), [Partitioning](#cycloneddsdomainpartitioning), [SSL](#cycloneddsdomainssl), [Security](#cycloneddsdomainsecurity), [Sizing](#cycloneddsdomainsizing), [TCP](#cycloneddsdomaintcp), [Threads](#cycloneddsdomainthreads), [Tracing](#cycloneddsdomaintracing)

The General element specifying Domain related settings.


## //CycloneDDS/Domain[@Id]
Text

Domain id this configuration applies to, or "any" if it applies to all domain ids.

The default value is: "any".


### //CycloneDDS/Domain/Compatibility
Children: [AssumeRtiHasPmdEndpoints](#cycloneddsdomaincompatibilityassumertihaspmdendpoints), [ExplicitlyPublishQosSetToDefault](#cycloneddsdomaincompatibilityexplicitlypublishqossettodefault), [ManySocketsMode](#cycloneddsdomaincompatibilitymanysocketsmode), [StandardsConformance](#cycloneddsdomaincompatibilitystandardsconformance)

The Compatibility elements allows specifying various settings related to compatability with standards and with other DDSI implementations.


#### //CycloneDDS/Domain/Compatibility/AssumeRtiHasPmdEndpoints
Boolean

This option assumes ParticipantMessageData endpoints required by the liveliness protocol are present in RTI participants even when not properly advertised by the participant discovery protocol.

The default value is: "false".


#### //CycloneDDS/Domain/Compatibility/ExplicitlyPublishQosSetToDefault
Boolean

This element specifies whether QoS settings set to default values are explicitly published in the discovery protocol. Implementations are to use the default value for QoS settings not published, which allows a significant reduction of the amount of data that needs to be exchanged for the discovery protocol, but this requires all implementations to adhere to the default values specified by the specifications.

When interoperability is required with an implementation that does not follow the specifications in this regard, setting this option to true will help.

The default value is: "false".


#### //CycloneDDS/Domain/Compatibility/ManySocketsMode
One of: false, true, single, none, many

This option specifies whether a network socket will be created for each domain participant on a host. The specification seems to assume that each participant has a unique address, and setting this option will ensure this to be the case. This is not the default.

Disabling it slightly improves performance and reduces network traffic somewhat. It also causes the set of port numbers needed by Cyclone DDS to become predictable, which may be useful for firewall and NAT configuration.

The default value is: "single".


#### //CycloneDDS/Domain/Compatibility/StandardsConformance
One of: lax, strict, pedantic

This element sets the level of standards conformance of this instance of the Cyclone DDS Service. Stricter conformance typically means less interoperability with other implementations. Currently three modes are defined:
 * pedantic: very strictly conform to the specification, ultimately for compliancy testing, but currently of little value because it adheres even to what will most likely turn out to be editing errors in the DDSI standard. Arguably, as long as no errata have been published it is the current text that is in effect, and that is what pedantic currently does.

 * strict: a slightly less strict view of the standard than does pedantic: it follows the established behaviour where the standard is obviously in error.

 * lax: attempt to provide the smoothest possible interoperability, anticipating future revisions of elements in the standard in areas that other implementations do not adhere to, even though there is no good reason not to.

The default value is: "lax".


### //CycloneDDS/Domain/Discovery
Children: [DSGracePeriod](#cycloneddsdomaindiscoverydsgraceperiod), [DefaultMulticastAddress](#cycloneddsdomaindiscoverydefaultmulticastaddress), [ExternalDomainId](#cycloneddsdomaindiscoveryexternaldomainid), [MaxAutoParticipantIndex](#cycloneddsdomaindiscoverymaxautoparticipantindex), [ParticipantIndex](#cycloneddsdomaindiscoveryparticipantindex), [Peers](#cycloneddsdomaindiscoverypeers), [Ports](#cycloneddsdomaindiscoveryports), [SPDPInterval](#cycloneddsdomaindiscoveryspdpinterval), [SPDPMulticastAddress](#cycloneddsdomaindiscoveryspdpmulticastaddress), [Tag](#cycloneddsdomaindiscoverytag)

The Discovery element allows specifying various parameters related to the discovery of peers.


#### //CycloneDDS/Domain/Discovery/DSGracePeriod
Number-with-unit

This setting controls for how long endpoints discovered via a Cloud discovery service will survive after the discovery service disappeared, allowing reconnect without loss of data when the discovery service restarts (or another instance takes over).

Valid values are finite durations with an explicit unit or the keyword 'inf' for infinity. Recognised units: ns, us, ms, s, min, hr, day.

The default value is: "30 s".


#### //CycloneDDS/Domain/Discovery/DefaultMulticastAddress
Text

This element specifies the default multicast address for all traffic other than participant discovery packets. It defaults to Discovery/SPDPMulticastAddress.

The default value is: "auto".


#### //CycloneDDS/Domain/Discovery/ExternalDomainId
Text

An override for the domain id, to be used in discovery and for determining the port number mapping. This allows creating multiple domains in a single process while making them appear as a single domain on the network. The value "default" disables the override.

The default value is: "default".


#### //CycloneDDS/Domain/Discovery/MaxAutoParticipantIndex
Integer

This element specifies the maximum DDSI participant index selected by this instance of the Cyclone DDS service if the Discovery/ParticipantIndex is "auto".

The default value is: "9".


#### //CycloneDDS/Domain/Discovery/ParticipantIndex
Text

This element specifies the DDSI participant index used by this instance of the Cyclone DDS service for discovery purposes. Only one such participant id is used, independent of the number of actual DomainParticipants on the node. It is either:
 * auto: which will attempt to automatically determine an available participant index (see also Discovery/MaxAutoParticipantIndex), or

 * a non-negative integer less than 120, or

 * none:, which causes it to use arbitrary port numbers for unicast sockets which entirely removes the constraints on the participant index but makes unicast discovery impossible.

The default is auto. The participant index is part of the port number calculation and if predictable port numbers are needed and fixing the participant index has no adverse effects, it is recommended that the second be option be used.

The default value is: "none".


#### //CycloneDDS/Domain/Discovery/Peers
Children: [Group](#cycloneddsdomaindiscoverypeersgroup), [Peer](#cycloneddsdomaindiscoverypeerspeer)

This element statically configures addresses for discovery.


##### //CycloneDDS/Domain/Discovery/Peers/Group
Children: [Peer](#cycloneddsdomaindiscoverypeersgrouppeer)

This element statically configures a fault tolerant group of addresses for discovery. Each member of the group is tried in sequence until one succeeds.


###### //CycloneDDS/Domain/Discovery/Peers/Group/Peer
Attributes: [Address](#cycloneddsdomaindiscoverypeersgrouppeeraddress)

This element statically configures an addresses for discovery.


###### //CycloneDDS/Domain/Discovery/Peers/Group/Peer[@Address]
Text

This element specifies an IP address to which discovery packets must be sent, in addition to the default multicast address (see also General/AllowMulticast). Both a hostnames and a numerical IP address is accepted; the hostname or IP address may be suffixed with :PORT to explicitly set the port to which it must be sent. Multiple Peers may be specified.

The default value is: "".


##### //CycloneDDS/Domain/Discovery/Peers/Peer
Attributes: [Address](#cycloneddsdomaindiscoverypeersgrouppeeraddress)

This element statically configures an addresses for discovery.


##### //CycloneDDS/Domain/Discovery/Peers/Group/Peer[@Address]
Text

This element specifies an IP address to which discovery packets must be sent, in addition to the default multicast address (see also General/AllowMulticast). Both a hostnames and a numerical IP address is accepted; the hostname or IP address may be suffixed with :PORT to explicitly set the port to which it must be sent. Multiple Peers may be specified.

The default value is: "".


#### //CycloneDDS/Domain/Discovery/Ports
Children: [Base](#cycloneddsdomaindiscoveryportsbase), [DomainGain](#cycloneddsdomaindiscoveryportsdomaingain), [MulticastDataOffset](#cycloneddsdomaindiscoveryportsmulticastdataoffset), [MulticastMetaOffset](#cycloneddsdomaindiscoveryportsmulticastmetaoffset), [ParticipantGain](#cycloneddsdomaindiscoveryportsparticipantgain), [UnicastDataOffset](#cycloneddsdomaindiscoveryportsunicastdataoffset), [UnicastMetaOffset](#cycloneddsdomaindiscoveryportsunicastmetaoffset)

The Ports element allows specifying various parameters related to the port numbers used for discovery. These all have default values specified by the DDSI 2.1 specification and rarely need to be changed.


##### //CycloneDDS/Domain/Discovery/Ports/Base
Integer

This element specifies the base port number (refer to the DDSI 2.1 specification, section 9.6.1, constant PB).

The default value is: "7400".


##### //CycloneDDS/Domain/Discovery/Ports/DomainGain
Integer

This element specifies the domain gain, relating domain ids to sets of port numbers (refer to the DDSI 2.1 specification, section 9.6.1, constant DG).

The default value is: "250".


##### //CycloneDDS/Domain/Discovery/Ports/MulticastDataOffset
Integer

This element specifies the port number for multicast data traffic (refer to the DDSI 2.1 specification, section 9.6.1, constant d2).

The default value is: "1".


##### //CycloneDDS/Domain/Discovery/Ports/MulticastMetaOffset
Integer

This element specifies the port number for multicast meta traffic (refer to the DDSI 2.1 specification, section 9.6.1, constant d0).

The default value is: "0".


##### //CycloneDDS/Domain/Discovery/Ports/ParticipantGain
Integer

This element specifies the participant gain, relating p0, participant index to sets of port numbers (refer to the DDSI 2.1 specification, section 9.6.1, constant PG).

The default value is: "2".


##### //CycloneDDS/Domain/Discovery/Ports/UnicastDataOffset
Integer

This element specifies the port number for unicast data traffic (refer to the DDSI 2.1 specification, section 9.6.1, constant d3).

The default value is: "11".


##### //CycloneDDS/Domain/Discovery/Ports/UnicastMetaOffset
Integer

This element specifies the port number for unicast meta traffic (refer to the DDSI 2.1 specification, section 9.6.1, constant d1).

The default value is: "10".


#### //CycloneDDS/Domain/Discovery/SPDPInterval
Number-with-unit

This element specifies the interval between spontaneous transmissions of participant discovery packets.

The unit must be specified explicitly. Recognised units: ns, us, ms, s, min, hr, day.

The default value is: "30 s".


#### //CycloneDDS/Domain/Discovery/SPDPMulticastAddress
Text

This element specifies the multicast address that is used as the destination for the participant discovery packets. In IPv4 mode the default is the (standardised) 239.255.0.1, in IPv6 mode it becomes ff02::ffff:239.255.0.1, which is a non-standardised link-local multicast address.

The default value is: "239.255.0.1".


#### //CycloneDDS/Domain/Discovery/Tag
Text

String extension for domain id that remote participants must match to be discovered.

The default value is: "".


### //CycloneDDS/Domain/General
Children: [AllowMulticast](#cycloneddsdomaingeneralallowmulticast), [DontRoute](#cycloneddsdomaingeneraldontroute), [EnableMulticastLoopback](#cycloneddsdomaingeneralenablemulticastloopback), [ExternalNetworkAddress](#cycloneddsdomaingeneralexternalnetworkaddress), [ExternalNetworkMask](#cycloneddsdomaingeneralexternalnetworkmask), [FragmentSize](#cycloneddsdomaingeneralfragmentsize), [MaxMessageSize](#cycloneddsdomaingeneralmaxmessagesize), [MaxRexmitMessageSize](#cycloneddsdomaingeneralmaxrexmitmessagesize), [MulticastRecvNetworkInterfaceAddresses](#cycloneddsdomaingeneralmulticastrecvnetworkinterfaceaddresses), [MulticastTimeToLive](#cycloneddsdomaingeneralmulticasttimetolive), [NetworkInterfaceAddress](#cycloneddsdomaingeneralnetworkinterfaceaddress), [PreferMulticast](#cycloneddsdomaingeneralprefermulticast), [Transport](#cycloneddsdomaingeneraltransport), [UseIPv6](#cycloneddsdomaingeneraluseipv)

The General element specifies overall Cyclone DDS service settings.


#### //CycloneDDS/Domain/General/AllowMulticast
One of:
* Keyword: default
* Comma-separated list of: false, spdp, asm, ssm, true

This element controls whether Cyclone DDS uses multicasts for data traffic.

It is a comma-separated list of some of the following keywords: "spdp", "asm", "ssm", or either of "false" or "true", or "default".

 * spdp: enables the use of ASM (any-source multicast) for participant discovery, joining the multicast group on the discovery socket, transmitting SPDP messages to this group, but never advertising nor using any multicast address in any discovery message, thus forcing unicast communications for all endpoint discovery and user data.

 * asm: enables the use of ASM for all traffic, including receiving SPDP but not transmitting SPDP messages via multicast

 * ssm: enables the use of SSM (source-specific multicast) for all non-SPDP traffic (if supported)


When set to "false" all multicasting is disabled. The default, "true" enables full use of multicasts. Listening for multicasts can be controlled by General/MulticastRecvNetworkInterfaceAddresses.

"default" maps on spdp if the network is a WiFi network, on true if it is a wired network

The default value is: "default".


#### //CycloneDDS/Domain/General/DontRoute
Boolean

This element allows setting the SO\_DONTROUTE option for outgoing packets, to bypass the local routing tables. This is generally useful only when the routing tables cannot be trusted, which is highly unusual.

The default value is: "false".


#### //CycloneDDS/Domain/General/EnableMulticastLoopback
Boolean

This element specifies whether Cyclone DDS allows IP multicast packets to be visible to all DDSI participants in the same node, including itself. It must be "true" for intra-node multicast communications, but if a node runs only a single Cyclone DDS service and does not host any other DDSI-capable programs, it should be set to "false" for improved performance.

The default value is: "true".


#### //CycloneDDS/Domain/General/ExternalNetworkAddress
Text

This element allows explicitly overruling the network address Cyclone DDS advertises in the discovery protocol, which by default is the address of the preferred network interface (General/NetworkInterfaceAddress), to allow Cyclone DDS to communicate across a Network Address Translation (NAT) device.

The default value is: "auto".


#### //CycloneDDS/Domain/General/ExternalNetworkMask
Text

This element specifies the network mask of the external network address. This element is relevant only when an external network address (General/ExternalNetworkAddress) is explicitly configured. In this case locators received via the discovery protocol that are within the same external subnet (as defined by this mask) will be translated to an internal address by replacing the network portion of the external address with the corresponding portion of the preferred network interface address. This option is IPv4-only.

The default value is: "0.0.0.0".


#### //CycloneDDS/Domain/General/FragmentSize
Number-with-unit

This element specifies the size of DDSI sample fragments generated by Cyclone DDS. Samples larger than FragmentSize are fragmented into fragments of FragmentSize bytes each, except the last one, which may be smaller. The DDSI spec mandates a minimum fragment size of 1025 bytes, but Cyclone DDS will do whatever size is requested, accepting fragments of which the size is at least the minimum of 1025 and FragmentSize.

The unit must be specified explicitly. Recognised units: B (bytes), kB & KiB (2^10 bytes), MB & MiB (2^20 bytes), GB & GiB (2^30 bytes).

The default value is: "1344 B".


#### //CycloneDDS/Domain/General/MaxMessageSize
Number-with-unit

This element specifies the maximum size of the UDP payload that Cyclone DDS will generate. Cyclone DDS will try to maintain this limit within the bounds of the DDSI specification, which means that in some cases (especially for very low values of MaxMessageSize) larger payloads may sporadically be observed (currently up to 1192 B).

On some networks it may be necessary to set this item to keep the packetsize below the MTU to prevent IP fragmentation.

The unit must be specified explicitly. Recognised units: B (bytes), kB & KiB (2^10 bytes), MB & MiB (2^20 bytes), GB & GiB (2^30 bytes).

The default value is: "14720 B".


#### //CycloneDDS/Domain/General/MaxRexmitMessageSize
Number-with-unit

This element specifies the maximum size of the UDP payload that Cyclone DDS will generate for a retransmit. Cyclone DDS will try to maintain this limit within the bounds of the DDSI specification, which means that in some cases (especially for very low values) larger payloads may sporadically be observed (currently up to 1192 B).

On some networks it may be necessary to set this item to keep the packetsize below the MTU to prevent IP fragmentation.

The unit must be specified explicitly. Recognised units: B (bytes), kB & KiB (2^10 bytes), MB & MiB (2^20 bytes), GB & GiB (2^30 bytes).

The default value is: "1456 B".


#### //CycloneDDS/Domain/General/MulticastRecvNetworkInterfaceAddresses
Text

This element specifies on which network interfaces Cyclone DDS listens to multicasts. The following options are available:

 * all: listen for multicasts on all multicast-capable interfaces; or

 * any: listen for multicasts on the operating system default interface; or

 * preferred: listen for multicasts on the preferred interface (General/NetworkInterfaceAddress); or

 * none: does not listen for multicasts on any interface; or

 * a comma-separated list of network addresses: configures Cyclone DDS to listen for multicasts on all of the listed addresses.


If Cyclone DDS is in IPv6 mode and the address of the preferred network interface is a link-local address, "all" is treated as a synonym for "preferred" and a comma-separated list is treated as "preferred" if it contains the preferred interface and as "none" if not.

The default value is: "preferred".


#### //CycloneDDS/Domain/General/MulticastTimeToLive
Integer

This element specifies the time-to-live setting for outgoing multicast packets.

The default value is: "32".


#### //CycloneDDS/Domain/General/NetworkInterfaceAddress
Text

This element specifies the preferred network interface for use by Cyclone DDS. The preferred network interface determines the IP address that Cyclone DDS advertises in the discovery protocol (but see also General/ExternalNetworkAddress), and is also the only interface over which multicasts are transmitted. The interface can be identified by its IP address, network interface name or network portion of the address. If the value "auto" is entered here, Cyclone DDS will select what it considers the most suitable interface.

The default value is: "auto".


#### //CycloneDDS/Domain/General/PreferMulticast
Boolean

When false (default) Cyclone DDS uses unicast for data whenever there a single unicast suffices. Setting this to true makes it prefer multicasting data, falling back to unicast only when no multicast address is available.

The default value is: "false".


#### //CycloneDDS/Domain/General/Transport
One of: default, udp, udp6, tcp, tcp6, raweth

This element allows selecting the transport to be used (udp, udp6, tcp, tcp6, raweth)

The default value is: "default".


#### //CycloneDDS/Domain/General/UseIPv6
One of: false, true, default

Deprecated (use Transport instead)

The default value is: "default".


### //CycloneDDS/Domain/Internal
Children: [AccelerateRexmitBlockSize](#cycloneddsdomaininternalacceleraterexmitblocksize), [AckDelay](#cycloneddsdomaininternalackdelay), [AssumeMulticastCapable](#cycloneddsdomaininternalassumemulticastcapable), [AutoReschedNackDelay](#cycloneddsdomaininternalautoreschednackdelay), [BuiltinEndpointSet](#cycloneddsdomaininternalbuiltinendpointset), [BurstSize](#cycloneddsdomaininternalburstsize), [ControlTopic](#cycloneddsdomaininternalcontroltopic), [DDSI2DirectMaxThreads](#cycloneddsdomaininternalddsidirectmaxthreads), [DefragReliableMaxSamples](#cycloneddsdomaininternaldefragreliablemaxsamples), [DefragUnreliableMaxSamples](#cycloneddsdomaininternaldefragunreliablemaxsamples), [DeliveryQueueMaxSamples](#cycloneddsdomaininternaldeliveryqueuemaxsamples), [EnableExpensiveChecks](#cycloneddsdomaininternalenableexpensivechecks), [GenerateKeyhash](#cycloneddsdomaininternalgeneratekeyhash), [HeartbeatInterval](#cycloneddsdomaininternalheartbeatinterval), [LateAckMode](#cycloneddsdomaininternallateackmode), [LeaseDuration](#cycloneddsdomaininternalleaseduration), [LivelinessMonitoring](#cycloneddsdomaininternallivelinessmonitoring), [MaxParticipants](#cycloneddsdomaininternalmaxparticipants), [MaxQueuedRexmitBytes](#cycloneddsdomaininternalmaxqueuedrexmitbytes), [MaxQueuedRexmitMessages](#cycloneddsdomaininternalmaxqueuedrexmitmessages), [MaxSampleSize](#cycloneddsdomaininternalmaxsamplesize), [MeasureHbToAckLatency](#cycloneddsdomaininternalmeasurehbtoacklatency), [MinimumSocketReceiveBufferSize](#cycloneddsdomaininternalminimumsocketreceivebuffersize), [MinimumSocketSendBufferSize](#cycloneddsdomaininternalminimumsocketsendbuffersize), [MonitorPort](#cycloneddsdomaininternalmonitorport), [MultipleReceiveThreads](#cycloneddsdomaininternalmultiplereceivethreads), [NackDelay](#cycloneddsdomaininternalnackdelay), [PreEmptiveAckDelay](#cycloneddsdomaininternalpreemptiveackdelay), [PrimaryReorderMaxSamples](#cycloneddsdomaininternalprimaryreordermaxsamples), [PrioritizeRetransmit](#cycloneddsdomaininternalprioritizeretransmit), [RediscoveryBlacklistDuration](#cycloneddsdomaininternalrediscoveryblacklistduration), [RetransmitMerging](#cycloneddsdomaininternalretransmitmerging), [RetransmitMergingPeriod](#cycloneddsdomaininternalretransmitmergingperiod), [RetryOnRejectBestEffort](#cycloneddsdomaininternalretryonrejectbesteffort), [SPDPResponseMaxDelay](#cycloneddsdomaininternalspdpresponsemaxdelay), [ScheduleTimeRounding](#cycloneddsdomaininternalscheduletimerounding), [SecondaryReorderMaxSamples](#cycloneddsdomaininternalsecondaryreordermaxsamples), [SendAsync](#cycloneddsdomaininternalsendasync), [SquashParticipants](#cycloneddsdomaininternalsquashparticipants), [SynchronousDeliveryLatencyBound](#cycloneddsdomaininternalsynchronousdeliverylatencybound), [SynchronousDeliveryPriorityThreshold](#cycloneddsdomaininternalsynchronousdeliveryprioritythreshold), [Test](#cycloneddsdomaininternaltest), [UnicastResponseToSPDPMessages](#cycloneddsdomaininternalunicastresponsetospdpmessages), [UseMulticastIfMreqn](#cycloneddsdomaininternalusemulticastifmreqn), [Watermarks](#cycloneddsdomaininternalwatermarks), [WriteBatch](#cycloneddsdomaininternalwritebatch), [WriterLingerDuration](#cycloneddsdomaininternalwriterlingerduration)

The Internal elements deal with a variety of settings that evolving and that are not necessarily fully supported. For the vast majority of the Internal settings, the functionality per-se is supported, but the right to change the way the options control the functionality is reserved. This includes renaming or moving options.


#### //CycloneDDS/Domain/Internal/AccelerateRexmitBlockSize
Integer

Proxy readers that are assumed to sill be retrieving historical data get this many samples retransmitted when they NACK something, even if some of these samples have sequence numbers outside the set covered by the NACK.

The default value is: "0".


#### //CycloneDDS/Domain/Internal/AckDelay
Number-with-unit

This setting controls the delay between sending identical acknowledgements.

The unit must be specified explicitly. Recognised units: ns, us, ms, s, min, hr, day.

The default value is: "10 ms".


#### //CycloneDDS/Domain/Internal/AssumeMulticastCapable
Text

This element controls which network interfaces are assumed to be capable of multicasting even when the interface flags returned by the operating system state it is not (this provides a workaround for some platforms). It is a comma-separated lists of patterns (with ? and \* wildcards) against which the interface names are matched.

The default value is: "".


#### //CycloneDDS/Domain/Internal/AutoReschedNackDelay
Number-with-unit

This setting controls the interval with which a reader will continue NACK'ing missing samples in the absence of a response from the writer, as a protection mechanism against writers incorrectly stopping the sending of HEARTBEAT messages.

Valid values are finite durations with an explicit unit or the keyword 'inf' for infinity. Recognised units: ns, us, ms, s, min, hr, day.

The default value is: "1 s".


#### //CycloneDDS/Domain/Internal/BuiltinEndpointSet
One of: full, writers, minimal

This element controls which participants will have which built-in endpoints for the discovery and liveliness protocols. Valid values are:
 * full: all participants have all endpoints;

 * writers: all participants have the writers, but just one has the readers;

 * minimal: only one participant has built-in endpoints.

The default is writers, as this is thought to be compliant and reasonably efficient. Minimal may or may not be compliant but is most efficient, and full is inefficient but certain to be compliant. See also Internal/ConservativeBuiltinReaderStartup.

The default value is: "writers".


#### //CycloneDDS/Domain/Internal/BurstSize
Children: [MaxInitTransmit](#cycloneddsdomaininternalburstsizemaxinittransmit), [MaxRexmit](#cycloneddsdomaininternalburstsizemaxrexmit)

Setting for controlling the size of transmit bursts.


##### //CycloneDDS/Domain/Internal/BurstSize/MaxInitTransmit
Number-with-unit

This element specifies how much more than the (presumed or discovered) receive buffer size may be sent when transmitting a sample for the first time, expressed as a percentage; the remainder will then be handled via retransmits. Usually the receivers can keep up with transmitter, at least on average, and so generally it is better to hope for the best and recover. Besides, the retransmits will be unicast, and so any multicast advantage will be lost as well.

The unit must be specified explicitly. Recognised units: B (bytes), kB & KiB (2^10 bytes), MB & MiB (2^20 bytes), GB & GiB (2^30 bytes).

The default value is: "4294967295".


##### //CycloneDDS/Domain/Internal/BurstSize/MaxRexmit
Number-with-unit

This element specifies the amount of data to be retransmitted in response to one NACK.

The unit must be specified explicitly. Recognised units: B (bytes), kB & KiB (2^10 bytes), MB & MiB (2^20 bytes), GB & GiB (2^30 bytes).

The default value is: "1 MiB".


#### //CycloneDDS/Domain/Internal/ControlTopic
The ControlTopic element allows configured whether Cyclone DDS provides a special control interface via a predefined topic or not.


#### //CycloneDDS/Domain/Internal/DDSI2DirectMaxThreads
Integer

This element sets the maximum number of extra threads for an experimental, undocumented and unsupported direct mode.

The default value is: "1".


#### //CycloneDDS/Domain/Internal/DefragReliableMaxSamples
Integer

This element sets the maximum number of samples that can be defragmented simultaneously for a reliable writer. This has to be large enough to handle retransmissions of historical data in addition to new samples.

The default value is: "16".


#### //CycloneDDS/Domain/Internal/DefragUnreliableMaxSamples
Integer

This element sets the maximum number of samples that can be defragmented simultaneously for a best-effort writers.

The default value is: "4".


#### //CycloneDDS/Domain/Internal/DeliveryQueueMaxSamples
Integer

This element controls the maximum size of a delivery queue, expressed in samples. Once a delivery queue is full, incoming samples destined for that queue are dropped until space becomes available again.

The default value is: "256".


#### //CycloneDDS/Domain/Internal/EnableExpensiveChecks
One of:
* Comma-separated list of: whc, rhc, xevent, all
* Or empty

This element enables expensive checks in builds with assertions enabled and is ignored otherwise. Recognised categories are:

 * whc: writer history cache checking

 * rhc: reader history cache checking

 * xevent: xevent checking

In addition, there is the keyword all that enables all checks.

The default value is: "".


#### //CycloneDDS/Domain/Internal/GenerateKeyhash
Boolean

When true, include keyhashes in outgoing data for topics with keys.

The default value is: "false".


#### //CycloneDDS/Domain/Internal/HeartbeatInterval
Attributes: [max](#cycloneddsdomaininternalheartbeatintervalmax), [min](#cycloneddsdomaininternalheartbeatintervalmin), [minsched](#cycloneddsdomaininternalheartbeatintervalminsched)

Number-with-unit

This element allows configuring the base interval for sending writer heartbeats and the bounds within which it can vary.

Valid values are finite durations with an explicit unit or the keyword 'inf' for infinity. Recognised units: ns, us, ms, s, min, hr, day.

The default value is: "100 ms".


#### //CycloneDDS/Domain/Internal/HeartbeatInterval[@max]
Number-with-unit

This attribute sets the maximum interval for periodic heartbeats.

Valid values are finite durations with an explicit unit or the keyword 'inf' for infinity. Recognised units: ns, us, ms, s, min, hr, day.

The default value is: "8 s".


#### //CycloneDDS/Domain/Internal/HeartbeatInterval[@min]
Number-with-unit

This attribute sets the minimum interval that must have passed since the most recent heartbeat from a writer, before another asynchronous (not directly related to writing) will be sent.

Valid values are finite durations with an explicit unit or the keyword 'inf' for infinity. Recognised units: ns, us, ms, s, min, hr, day.

The default value is: "5 ms".


#### //CycloneDDS/Domain/Internal/HeartbeatInterval[@minsched]
Number-with-unit

This attribute sets the minimum interval for periodic heartbeats. Other events may still cause heartbeats to go out.

Valid values are finite durations with an explicit unit or the keyword 'inf' for infinity. Recognised units: ns, us, ms, s, min, hr, day.

The default value is: "20 ms".


#### //CycloneDDS/Domain/Internal/LateAckMode
Boolean

Ack a sample only when it has been delivered, instead of when committed to delivering it.

The default value is: "false".


#### //CycloneDDS/Domain/Internal/LeaseDuration
Number-with-unit

This setting controls the default participant lease duration.
The unit must be specified explicitly. Recognised units: ns, us, ms, s, min, hr, day.

The default value is: "10 s".


#### //CycloneDDS/Domain/Internal/LivelinessMonitoring
Attributes: [Interval](#cycloneddsdomaininternallivelinessmonitoringinterval), [StackTraces](#cycloneddsdomaininternallivelinessmonitoringstacktraces)

Boolean

This element controls whether or not implementation should internally monitor its own liveliness. If liveliness monitoring is enabled, stack traces can be dumped automatically when some thread appears to have stopped making progress.

The default value is: "false".


#### //CycloneDDS/Domain/Internal/LivelinessMonitoring[@Interval]
Number-with-unit

This element controls the interval at which to check whether threads have been making progress.

The unit must be specified explicitly. Recognised units: ns, us, ms, s, min, hr, day.

The default value is: "1s".


#### //CycloneDDS/Domain/Internal/LivelinessMonitoring[@StackTraces]
Boolean

This element controls whether or not to write stack traces to the DDSI2 trace when a thread fails to make progress (on select platforms only).

The default value is: "true".


#### //CycloneDDS/Domain/Internal/MaxParticipants
Integer

This elements configures the maximum number of DCPS domain participants this Cyclone DDS instance is willing to service. 0 is unlimited.

The default value is: "0".


#### //CycloneDDS/Domain/Internal/MaxQueuedRexmitBytes
Number-with-unit

This setting limits the maximum number of bytes queued for retransmission. The default value of 0 is unlimited unless an AuxiliaryBandwidthLimit has been set, in which case it becomes NackDelay \* AuxiliaryBandwidthLimit. It must be large enough to contain the largest sample that may need to be retransmitted.

The unit must be specified explicitly. Recognised units: B (bytes), kB & KiB (2^10 bytes), MB & MiB (2^20 bytes), GB & GiB (2^30 bytes).

The default value is: "512 kB".


#### //CycloneDDS/Domain/Internal/MaxQueuedRexmitMessages
Integer

This settings limits the maximum number of samples queued for retransmission.

The default value is: "200".


#### //CycloneDDS/Domain/Internal/MaxSampleSize
Number-with-unit

This setting controls the maximum (CDR) serialised size of samples that Cyclone DDS will forward in either direction. Samples larger than this are discarded with a warning.

The unit must be specified explicitly. Recognised units: B (bytes), kB & KiB (2^10 bytes), MB & MiB (2^20 bytes), GB & GiB (2^30 bytes).

The default value is: "2147483647 B".


#### //CycloneDDS/Domain/Internal/MeasureHbToAckLatency
Boolean

This element enables heartbeat-to-ack latency among Cyclone DDS services by prepending timestamps to Heartbeat and AckNack messages and calculating round trip times. This is non-standard behaviour. The measured latencies are quite noisy and are currently not used anywhere.

The default value is: "false".


#### //CycloneDDS/Domain/Internal/MinimumSocketReceiveBufferSize
Number-with-unit

This setting controls the minimum size of socket receive buffers. The operating system provides some size receive buffer upon creation of the socket, this option can be used to increase the size of the buffer beyond that initially provided by the operating system. If the buffer size cannot be increased to the specified size, an error is reported.

The default setting is the word "default", which means Cyclone DDS will attempt to increase the buffer size to 1MB, but will silently accept a smaller buffer should that attempt fail.

The unit must be specified explicitly. Recognised units: B (bytes), kB & KiB (2^10 bytes), MB & MiB (2^20 bytes), GB & GiB (2^30 bytes).

The default value is: "default".


#### //CycloneDDS/Domain/Internal/MinimumSocketSendBufferSize
Number-with-unit

This setting controls the minimum size of socket send buffers. This setting can only increase the size of the send buffer, if the operating system by default creates a larger buffer, it is left unchanged.

The unit must be specified explicitly. Recognised units: B (bytes), kB & KiB (2^10 bytes), MB & MiB (2^20 bytes), GB & GiB (2^30 bytes).

The default value is: "64 KiB".


#### //CycloneDDS/Domain/Internal/MonitorPort
Integer

This element allows configuring a service that dumps a text description of part the internal state to TCP clients. By default (-1), this is disabled; specifying 0 means a kernel-allocated port is used; a positive number is used as the TCP port number.

The default value is: "-1".


#### //CycloneDDS/Domain/Internal/MultipleReceiveThreads
Attributes: [maxretries](#cycloneddsdomaininternalmultiplereceivethreadsmaxretries)

One of: false, true, default

This element controls whether all traffic is handled by a single receive thread (false) or whether multiple receive threads may be used to improve latency (true). By default it is disabled on Windows because it appears that one cannot count on being able to send packets to oneself, which is necessary to stop the thread during shutdown. Currently multiple receive threads are only used for connectionless transport (e.g., UDP) and ManySocketsMode not set to single (the default).

The default value is: "default".


#### //CycloneDDS/Domain/Internal/MultipleReceiveThreads[@maxretries]
Integer

Receive threads dedicated to a single socket can only be triggered for termination by sending a packet. Reception of any packet will do, so termination failure due to packet loss is exceedingly unlikely, but to eliminate all risks, it will retry as many times as specified by this attribute before aborting.

The default value is: "4294967295".


#### //CycloneDDS/Domain/Internal/NackDelay
Number-with-unit

This setting controls the delay between receipt of a HEARTBEAT indicating missing samples and a NACK (ignored when the HEARTBEAT requires an answer). However, no NACK is sent if a NACK had been scheduled already for a response earlier than the delay requests: then that NACK will incorporate the latest information.

The unit must be specified explicitly. Recognised units: ns, us, ms, s, min, hr, day.

The default value is: "100 ms".


#### //CycloneDDS/Domain/Internal/PreEmptiveAckDelay
Number-with-unit

This setting controls the delay between the discovering a remote writer and sending a pre-emptive AckNack to discover the range of data available.

The unit must be specified explicitly. Recognised units: ns, us, ms, s, min, hr, day.

The default value is: "10 ms".


#### //CycloneDDS/Domain/Internal/PrimaryReorderMaxSamples
Integer

This element sets the maximum size in samples of a primary re-order administration. Each proxy writer has one primary re-order administration to buffer the packet flow in case some packets arrive out of order. Old samples are forwarded to secondary re-order administrations associated with readers in need of historical data.

The default value is: "128".


#### //CycloneDDS/Domain/Internal/PrioritizeRetransmit
Boolean

This element controls whether retransmits are prioritized over new data, speeding up recovery.

The default value is: "true".


#### //CycloneDDS/Domain/Internal/RediscoveryBlacklistDuration
Attributes: [enforce](#cycloneddsdomaininternalrediscoveryblacklistdurationenforce)

Number-with-unit

This element controls for how long a remote participant that was previously deleted will remain on a blacklist to prevent rediscovery, giving the software on a node time to perform any cleanup actions it needs to do. To some extent this delay is required internally by Cyclone DDS, but in the default configuration with the 'enforce' attribute set to false, Cyclone DDS will reallow rediscovery as soon as it has cleared its internal administration. Setting it to too small a value may result in the entry being pruned from the blacklist before Cyclone DDS is ready, it is therefore recommended to set it to at least several seconds.

Valid values are finite durations with an explicit unit or the keyword 'inf' for infinity. Recognised units: ns, us, ms, s, min, hr, day.

The default value is: "0s".


#### //CycloneDDS/Domain/Internal/RediscoveryBlacklistDuration[@enforce]
Boolean

This attribute controls whether the configured time during which recently deleted participants will not be rediscovered (i.e., "black listed") is enforced and following complete removal of the participant in Cyclone DDS, or whether it can be rediscovered earlier provided all traces of that participant have been removed already.

The default value is: "false".


#### //CycloneDDS/Domain/Internal/RetransmitMerging
One of: never, adaptive, always

This elements controls the addressing and timing of retransmits. Possible values are:
 * never: retransmit only to the NACK-ing reader;

 * adaptive: attempt to combine retransmits needed for reliability, but send historical (transient-local) data to the requesting reader only;

 * always: do not distinguish between different causes, always try to merge.

The default is never. See also Internal/RetransmitMergingPeriod.

The default value is: "never".


#### //CycloneDDS/Domain/Internal/RetransmitMergingPeriod
Number-with-unit

This setting determines the size of the time window in which a NACK of some sample is ignored because a retransmit of that sample has been multicasted too recently. This setting has no effect on unicasted retransmits.

See also Internal/RetransmitMerging.

The unit must be specified explicitly. Recognised units: ns, us, ms, s, min, hr, day.

The default value is: "5 ms".


#### //CycloneDDS/Domain/Internal/RetryOnRejectBestEffort
Boolean

Whether or not to locally retry pushing a received best-effort sample into the reader caches when resource limits are reached.

The default value is: "false".


#### //CycloneDDS/Domain/Internal/SPDPResponseMaxDelay
Number-with-unit

Maximum pseudo-random delay in milliseconds between discovering aremote participant and responding to it.

The unit must be specified explicitly. Recognised units: ns, us, ms, s, min, hr, day.

The default value is: "0 ms".


#### //CycloneDDS/Domain/Internal/ScheduleTimeRounding
Number-with-unit

This setting allows the timing of scheduled events to be rounded up so that more events can be handled in a single cycle of the event queue. The default is 0 and causes no rounding at all, i.e. are scheduled exactly, whereas a value of 10ms would mean that events are rounded up to the nearest 10 milliseconds.

The unit must be specified explicitly. Recognised units: ns, us, ms, s, min, hr, day.

The default value is: "0 ms".


#### //CycloneDDS/Domain/Internal/SecondaryReorderMaxSamples
Integer

This element sets the maximum size in samples of a secondary re-order administration. The secondary re-order administration is per reader in need of historical data.

The default value is: "128".


#### //CycloneDDS/Domain/Internal/SendAsync
Boolean

This element controls whether the actual sending of packets occurs on the same thread that prepares them, or is done asynchronously by another thread.

The default value is: "false".


#### //CycloneDDS/Domain/Internal/SquashParticipants
Boolean

This element controls whether Cyclone DDS advertises all the domain participants it serves in DDSI (when set to false), or rather only one domain participant (the one corresponding to the Cyclone DDS process; when set to true). In the latter case Cyclone DDS becomes the virtual owner of all readers and writers of all domain participants, dramatically reducing discovery traffic (a similar effect can be obtained by setting Internal/BuiltinEndpointSet to "minimal" but with less loss of information).

The default value is: "false".


#### //CycloneDDS/Domain/Internal/SynchronousDeliveryLatencyBound
Number-with-unit

This element controls whether samples sent by a writer with QoS settings transport\_priority >= SynchronousDeliveryPriorityThreshold and a latency\_budget at most this element's value will be delivered synchronously from the "recv" thread, all others will be delivered asynchronously through delivery queues. This reduces latency at the expense of aggregate bandwidth.

Valid values are finite durations with an explicit unit or the keyword 'inf' for infinity. Recognised units: ns, us, ms, s, min, hr, day.

The default value is: "inf".


#### //CycloneDDS/Domain/Internal/SynchronousDeliveryPriorityThreshold
Integer

This element controls whether samples sent by a writer with QoS settings latency\_budget <= SynchronousDeliveryLatencyBound and transport\_priority greater than or equal to this element's value will be delivered synchronously from the "recv" thread, all others will be delivered asynchronously through delivery queues. This reduces latency at the expense of aggregate bandwidth.

The default value is: "0".


#### //CycloneDDS/Domain/Internal/Test
Children: [XmitLossiness](#cycloneddsdomaininternaltestxmitlossiness)

Testing options.


##### //CycloneDDS/Domain/Internal/Test/XmitLossiness
Integer

This element controls the fraction of outgoing packets to drop, specified as samples per thousand.

The default value is: "0".


#### //CycloneDDS/Domain/Internal/UnicastResponseToSPDPMessages
Boolean

This element controls whether the response to a newly discovered participant is sent as a unicasted SPDP packet, instead of rescheduling the periodic multicasted one. There is no known benefit to setting this to false.

The default value is: "true".


#### //CycloneDDS/Domain/Internal/UseMulticastIfMreqn
Integer

Do not use.

The default value is: "0".


#### //CycloneDDS/Domain/Internal/Watermarks
Children: [WhcAdaptive](#cycloneddsdomaininternalwatermarkswhcadaptive), [WhcHigh](#cycloneddsdomaininternalwatermarkswhchigh), [WhcHighInit](#cycloneddsdomaininternalwatermarkswhchighinit), [WhcLow](#cycloneddsdomaininternalwatermarkswhclow)

Watermarks for flow-control.


##### //CycloneDDS/Domain/Internal/Watermarks/WhcAdaptive
Boolean

This element controls whether Cyclone DDS will adapt the high-water mark to current traffic conditions, based on retransmit requests and transmit pressure.

The default value is: "true".


##### //CycloneDDS/Domain/Internal/Watermarks/WhcHigh
Number-with-unit

This element sets the maximum allowed high-water mark for the Cyclone DDS WHCs, expressed in bytes. A writer is suspended when the WHC reaches this size.

The unit must be specified explicitly. Recognised units: B (bytes), kB & KiB (2^10 bytes), MB & MiB (2^20 bytes), GB & GiB (2^30 bytes).

The default value is: "500 kB".


##### //CycloneDDS/Domain/Internal/Watermarks/WhcHighInit
Number-with-unit

This element sets the initial level of the high-water mark for the Cyclone DDS WHCs, expressed in bytes.

The unit must be specified explicitly. Recognised units: B (bytes), kB & KiB (2^10 bytes), MB & MiB (2^20 bytes), GB & GiB (2^30 bytes).

The default value is: "30 kB".


##### //CycloneDDS/Domain/Internal/Watermarks/WhcLow
Number-with-unit

This element sets the low-water mark for the Cyclone DDS WHCs, expressed in bytes. A suspended writer resumes transmitting when its Cyclone DDS WHC shrinks to this size.

The unit must be specified explicitly. Recognised units: B (bytes), kB & KiB (2^10 bytes), MB & MiB (2^20 bytes), GB & GiB (2^30 bytes).

The default value is: "1 kB".


#### //CycloneDDS/Domain/Internal/WriteBatch
Boolean

This element enables the batching of write operations. By default each write operation writes through the write cache and out onto the transport. Enabling write batching causes multiple small write operations to be aggregated within the write cache into a single larger write. This gives greater throughput at the expense of latency. Currently there is no mechanism for the write cache to automatically flush itself, so that if write batching is enabled, the application may have to use the dds\_write\_flush function to ensure that all samples are written.

The default value is: "false".


#### //CycloneDDS/Domain/Internal/WriterLingerDuration
Number-with-unit

This setting controls the maximum duration for which actual deletion of a reliable writer with unacknowledged data in its history will be postponed to provide proper reliable transmission.
The unit must be specified explicitly. Recognised units: ns, us, ms, s, min, hr, day.

The default value is: "1 s".


### //CycloneDDS/Domain/Partitioning
Children: [IgnoredPartitions](#cycloneddsdomainpartitioningignoredpartitions), [NetworkPartitions](#cycloneddsdomainpartitioningnetworkpartitions), [PartitionMappings](#cycloneddsdomainpartitioningpartitionmappings)

The Partitioning element specifies Cyclone DDS network partitions and how DCPS partition/topic combinations are mapped onto the network partitions.


#### //CycloneDDS/Domain/Partitioning/IgnoredPartitions
Children: [IgnoredPartition](#cycloneddsdomainpartitioningignoredpartitionsignoredpartition)

The IgnoredPartitions element specifies DCPS partition/topic combinations that are not distributed over the network.


##### //CycloneDDS/Domain/Partitioning/IgnoredPartitions/IgnoredPartition
Attributes: [DCPSPartitionTopic](#cycloneddsdomainpartitioningignoredpartitionsignoredpartitiondcpspartitiontopic)

Text

This element can be used to prevent certain combinations of DCPS partition and topic from being transmitted over the network. Cyclone DDS will complete ignore readers and writers for which all DCPS partitions as well as their topic is ignored, not even creating DDSI readers and writers to mirror the DCPS ones.

The default value is: "".


##### //CycloneDDS/Domain/Partitioning/IgnoredPartitions/IgnoredPartition[@DCPSPartitionTopic]
Text

This attribute specifies a partition and a topic expression, separated by a single '.', that are used to determine if a given partition and topic will be ignored or not. The expressions may use the usual wildcards '\*' and '?'. Cyclone DDS will consider an wildcard DCPS partition to match an expression iff there exists a string that satisfies both expressions.

The default value is: "".


#### //CycloneDDS/Domain/Partitioning/NetworkPartitions
Children: [NetworkPartition](#cycloneddsdomainpartitioningnetworkpartitionsnetworkpartition)

The NetworkPartitions element specifies the Cyclone DDS network partitions.


##### //CycloneDDS/Domain/Partitioning/NetworkPartitions/NetworkPartition
Attributes: [Address](#cycloneddsdomainpartitioningnetworkpartitionsnetworkpartitionaddress), [Name](#cycloneddsdomainpartitioningnetworkpartitionsnetworkpartitionname)

Text

This element defines a Cyclone DDS network partition.

The default value is: "".


##### //CycloneDDS/Domain/Partitioning/NetworkPartitions/NetworkPartition[@Address]
Text

This attribute specifies the multicast addresses associated with the network partition as a comma-separated list. Readers matching this network partition (cf. Partitioning/PartitionMappings) will listen for multicasts on all of these addresses and advertise them in the discovery protocol. The writers will select the most suitable address from the addresses advertised by the readers.

The default value is: "".


##### //CycloneDDS/Domain/Partitioning/NetworkPartitions/NetworkPartition[@Name]
Text

This attribute specifies the name of this Cyclone DDS network partition. Two network partitions cannot have the same name.

The default value is: "".


#### //CycloneDDS/Domain/Partitioning/PartitionMappings
Children: [PartitionMapping](#cycloneddsdomainpartitioningpartitionmappingspartitionmapping)

The PartitionMappings element specifies the mapping from DCPS partition/topic combinations to Cyclone DDS network partitions.


##### //CycloneDDS/Domain/Partitioning/PartitionMappings/PartitionMapping
Attributes: [DCPSPartitionTopic](#cycloneddsdomainpartitioningpartitionmappingspartitionmappingdcpspartitiontopic), [NetworkPartition](#cycloneddsdomainpartitioningpartitionmappingspartitionmappingnetworkpartition)

Text

This element defines a mapping from a DCPS partition/topic combination to a Cyclone DDS network partition. This allows partitioning data flows by using special multicast addresses for part of the data and possibly also encrypting the data flow.

The default value is: "".


##### //CycloneDDS/Domain/Partitioning/PartitionMappings/PartitionMapping[@DCPSPartitionTopic]
Text

This attribute specifies a partition and a topic expression, separated by a single '.', that are used to determine if a given partition and topic maps to the Cyclone DDS network partition named by the NetworkPartition attribute in this PartitionMapping element. The expressions may use the usual wildcards '\*' and '?'. Cyclone DDS will consider a wildcard DCPS partition to match an expression if there exists a string that satisfies both expressions.

The default value is: "".


##### //CycloneDDS/Domain/Partitioning/PartitionMappings/PartitionMapping[@NetworkPartition]
Text

This attribute specifies which Cyclone DDS network partition is to be used for DCPS partition/topic combinations matching the DCPSPartitionTopic attribute within this PartitionMapping element.

The default value is: "".


### //CycloneDDS/Domain/SSL
Children: [CertificateVerification](#cycloneddsdomainsslcertificateverification), [Ciphers](#cycloneddsdomainsslciphers), [Enable](#cycloneddsdomainsslenable), [EntropyFile](#cycloneddsdomainsslentropyfile), [KeyPassphrase](#cycloneddsdomainsslkeypassphrase), [KeystoreFile](#cycloneddsdomainsslkeystorefile), [MinimumTLSVersion](#cycloneddsdomainsslminimumtlsversion), [SelfSignedCertificates](#cycloneddsdomainsslselfsignedcertificates), [VerifyClient](#cycloneddsdomainsslverifyclient)

The SSL element allows specifying various parameters related to using SSL/TLS for DDSI over TCP.


#### //CycloneDDS/Domain/SSL/CertificateVerification
Boolean

If disabled this allows SSL connections to occur even if an X509 certificate fails verification.

The default value is: "true".


#### //CycloneDDS/Domain/SSL/Ciphers
Text

The set of ciphers used by SSL/TLS

The default value is: "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH".


#### //CycloneDDS/Domain/SSL/Enable
Boolean

This enables SSL/TLS for TCP.

The default value is: "false".


#### //CycloneDDS/Domain/SSL/EntropyFile
Text

The SSL/TLS random entropy file name.

The default value is: "".


#### //CycloneDDS/Domain/SSL/KeyPassphrase
Text

The SSL/TLS key pass phrase for encrypted keys.

The default value is: "secret".


#### //CycloneDDS/Domain/SSL/KeystoreFile
Text

The SSL/TLS key and certificate store file name. The keystore must be in PEM format.

The default value is: "keystore".


#### //CycloneDDS/Domain/SSL/MinimumTLSVersion
Text

The minimum TLS version that may be negotiated, valid values are 1.2 and 1.3.

The default value is: "1.3".


#### //CycloneDDS/Domain/SSL/SelfSignedCertificates
Boolean

This enables the use of self signed X509 certificates.

The default value is: "false".


#### //CycloneDDS/Domain/SSL/VerifyClient
Boolean

This enables an SSL server checking the X509 certificate of a connecting client.

The default value is: "true".


### //CycloneDDS/Domain/Security
Children: [AccessControl](#cycloneddsdomainsecurityaccesscontrol), [Authentication](#cycloneddsdomainsecurityauthentication), [Cryptographic](#cycloneddsdomainsecuritycryptographic)

This element is used to configure Cyclone DDS with the DDS Security specification plugins and settings.


#### //CycloneDDS/Domain/Security/AccessControl
Children: [Governance](#cycloneddsdomainsecurityaccesscontrolgovernance), [Library](#cycloneddsdomainsecurityaccesscontrollibrary), [Permissions](#cycloneddsdomainsecurityaccesscontrolpermissions), [PermissionsCA](#cycloneddsdomainsecurityaccesscontrolpermissionsca)

This element configures the Access Control plugin of the DDS Security specification.


##### //CycloneDDS/Domain/Security/AccessControl/Governance
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

The default value is: "".


##### //CycloneDDS/Domain/Security/AccessControl/Library
Attributes: [finalizeFunction](#cycloneddsdomainsecurityaccesscontrollibraryfinalizefunction), [initFunction](#cycloneddsdomainsecurityaccesscontrollibraryinitfunction), [path](#cycloneddsdomainsecurityaccesscontrollibrarypath)

Text

This element specifies the library to be loaded as the DDS Security Access Control plugin.

The default value is: "".


##### //CycloneDDS/Domain/Security/AccessControl/Library[@finalizeFunction]
Text

This element names the finalization function of Access Control plugin. This function is called to let the plugin release its resources.

The default value is: "finalize\_access\_control".


##### //CycloneDDS/Domain/Security/AccessControl/Library[@initFunction]
Text

This element names the initialization function of Access Control plugin. This function is called after loading the plugin library for instantiation purposes. Init function must return an object that implements DDS Security Access Control interface.

The default value is: "init\_access\_control".


##### //CycloneDDS/Domain/Security/AccessControl/Library[@path]
Text

This element points to the path of Access Control plugin library.

It can be either absolute path excluding file extension ( /usr/lib/dds\_security\_ac ) or single file without extension ( dds\_security\_ac ).

If single file is supplied, the library located by way of the current working directory, or LD\_LIBRARY\_PATH for Unix systems, and PATH for Windows systems.

The default value is: "dds\_security\_ac".


##### //CycloneDDS/Domain/Security/AccessControl/Permissions
Text

URI to the DomainParticipant permissions document signed by the Permissions CA in S/MIME format

The permissions document specifies the permissions to be applied to a domain.<br>
Example file URIs:

<Permissions>file:permissions\_document.p7s</Permissions>

<Permissions>file:/path\_to/permissions\_document.p7s</Permissions>

Example data URI:

<Permissions><![CDATA[data:,.........]]</Permissions>

The default value is: "".


##### //CycloneDDS/Domain/Security/AccessControl/PermissionsCA
Text

URI to a X509 certificate for the PermissionsCA in PEM format.

Supported URI schemes: file, data

The file and data schemas shall refer to a X.509 v3 certificate (see X.509 v3 ITU-T Recommendation X.509 (2005) [39]) in PEM format.<br>
Examples:<br>
<PermissionsCA>file:permissions\_ca.pem</PermissionsCA>

<PermissionsCA>file:/home/myuser/permissions\_ca.pem</PermissionsCA><br>
<PermissionsCA>data:<strong>,</strong>-----BEGIN CERTIFICATE-----

MIIC3DCCAcQCCQCWE5x+Z ... PhovK0mp2ohhRLYI0ZiyYQ==

-----END CERTIFICATE-----</PermissionsCA>

The default value is: "".


#### //CycloneDDS/Domain/Security/Authentication
Children: [IdentityCA](#cycloneddsdomainsecurityauthenticationidentityca), [IdentityCertificate](#cycloneddsdomainsecurityauthenticationidentitycertificate), [IncludeOptionalFields](#cycloneddsdomainsecurityauthenticationincludeoptionalfields), [Library](#cycloneddsdomainsecurityauthenticationlibrary), [Password](#cycloneddsdomainsecurityauthenticationpassword), [PrivateKey](#cycloneddsdomainsecurityauthenticationprivatekey), [TrustedCADirectory](#cycloneddsdomainsecurityauthenticationtrustedcadirectory)

This element configures the Authentication plugin of the DDS Security specification.


##### //CycloneDDS/Domain/Security/Authentication/IdentityCA
Text

URI to the X509 certificate [39] of the Identity CA that is the signer of Identity Certificate.

Supported URI schemes: file, data

The file and data schemas shall refer to a X.509 v3 certificate (see X.509 v3 ITU-T Recommendation X.509 (2005) [39]) in PEM format.

Examples:

<IdentityCA>file:identity\_ca.pem</IdentityCA>

<IdentityCA>data:,-----BEGIN CERTIFICATE-----<br>
MIIC3DCCAcQCCQCWE5x+Z...PhovK0mp2ohhRLYI0ZiyYQ==<br>
-----END CERTIFICATE-----</IdentityCA>

The default value is: "".


##### //CycloneDDS/Domain/Security/Authentication/IdentityCertificate
Text

Identity certificate that will be used for identifying all participants in the OSPL instance.<br>The content is URI to a X509 certificate signed by the IdentityCA in PEM format containing the signed public key.

Supported URI schemes: file, data

Examples:

<IdentityCertificate>file:participant1\_identity\_cert.pem</IdentityCertificate>

<IdentityCertificate>data:,-----BEGIN CERTIFICATE-----<br>
MIIDjjCCAnYCCQDCEu9...6rmT87dhTo=<br>
-----END CERTIFICATE-----</IdentityCertificate>

The default value is: "".


##### //CycloneDDS/Domain/Security/Authentication/IncludeOptionalFields
Boolean

The authentication handshake tokens may contain optional fields to be included for finding interoperability problems. If this parameter is set to true the optional fields are included in the handshake token exchange.

The default value is: "false".


##### //CycloneDDS/Domain/Security/Authentication/Library
Attributes: [finalizeFunction](#cycloneddsdomainsecurityauthenticationlibraryfinalizefunction), [initFunction](#cycloneddsdomainsecurityauthenticationlibraryinitfunction), [path](#cycloneddsdomainsecurityauthenticationlibrarypath)

Text

This element specifies the library to be loaded as the DDS Security Access Control plugin.

The default value is: "".


##### //CycloneDDS/Domain/Security/Authentication/Library[@finalizeFunction]
Text

This element names the finalization function of Authentication plugin. This function is called to let the plugin release its resources.

The default value is: "finalize\_authentication".


##### //CycloneDDS/Domain/Security/Authentication/Library[@initFunction]
Text

This element names the initialization function of Authentication plugin. This function is called after loading the plugin library for instantiation purposes. Init function must return an object that implements DDS Security Authentication interface.

The default value is: "init\_authentication".


##### //CycloneDDS/Domain/Security/Authentication/Library[@path]
Text

This element points to the path of Authentication plugin library.

It can be either absolute path excluding file extension ( /usr/lib/dds\_security\_auth ) or single file without extension ( dds\_security\_auth ).

If single file is supplied, the library located by way of the current working directory, or LD\_LIBRARY\_PATH for Unix systems, and PATH for Windows system.

The default value is: "dds\_security\_auth".


##### //CycloneDDS/Domain/Security/Authentication/Password
Text

A password used to decrypt the private\_key.

The value of the password property shall be interpreted as the Base64 encoding of the AES-128 key that shall be used to decrypt the private\_key using AES128-CBC.

If the password property is not present, then the value supplied in the private\_key property must contain the unencrypted private key.

The default value is: "".


##### //CycloneDDS/Domain/Security/Authentication/PrivateKey
Text

URI to access the private Private Key for all of the participants in the OSPL federation.

Supported URI schemes: file, data

Examples:

<PrivateKey>file:identity\_ca\_private\_key.pem</PrivateKey>

<PrivateKey>data:,-----BEGIN RSA PRIVATE KEY-----<br>
MIIEpAIBAAKCAQEA3HIh...AOBaaqSV37XBUJg==<br>
-----END RSA PRIVATE KEY-----</PrivateKey>

The default value is: "".


##### //CycloneDDS/Domain/Security/Authentication/TrustedCADirectory
Text

Trusted CA Directory which contains trusted CA certificates as separated files.

The default value is: "".


#### //CycloneDDS/Domain/Security/Cryptographic
Children: [Library](#cycloneddsdomainsecuritycryptographiclibrary)

This element configures the Cryptographic plugin of the DDS Security specification.


##### //CycloneDDS/Domain/Security/Cryptographic/Library
Attributes: [finalizeFunction](#cycloneddsdomainsecuritycryptographiclibraryfinalizefunction), [initFunction](#cycloneddsdomainsecuritycryptographiclibraryinitfunction), [path](#cycloneddsdomainsecuritycryptographiclibrarypath)

Text

This element specifies the library to be loaded as the DDS Security Cryptographic plugin.

The default value is: "".


##### //CycloneDDS/Domain/Security/Cryptographic/Library[@finalizeFunction]
Text

This element names the finalization function of Cryptographic plugin. This function is called to let the plugin release its resources.

The default value is: "finalize\_crypto".


##### //CycloneDDS/Domain/Security/Cryptographic/Library[@initFunction]
Text

This element names the initialization function of Cryptographic plugin. This function is called after loading the plugin library for instantiation purposes. Init function must return an object that implements DDS Security Cryptographic interface.

The default value is: "init\_crypto".


##### //CycloneDDS/Domain/Security/Cryptographic/Library[@path]
Text

This element points to the path of Cryptographic plugin library.

It can be either absolute path excluding file extension ( /usr/lib/dds\_security\_crypto ) or single file without extension ( dds\_security\_crypto ).

If single file is supplied, the library located by way of the current working directory, or LD\_LIBRARY\_PATH for Unix systems, and PATH for Windows systems.

The default value is: "dds\_security\_crypto".


### //CycloneDDS/Domain/Sizing
Children: [ReceiveBufferChunkSize](#cycloneddsdomainsizingreceivebufferchunksize), [ReceiveBufferSize](#cycloneddsdomainsizingreceivebuffersize)

The Sizing element specifies a variety of configuration settings dealing with expected system sizes, buffer sizes, &c.


#### //CycloneDDS/Domain/Sizing/ReceiveBufferChunkSize
Number-with-unit

This element specifies the size of one allocation unit in the receive buffer. Must be greater than the maximum packet size by a modest amount (too large packets are dropped). Each allocation is shrunk immediately after processing a message, or freed straightaway.

The unit must be specified explicitly. Recognised units: B (bytes), kB & KiB (2^10 bytes), MB & MiB (2^20 bytes), GB & GiB (2^30 bytes).

The default value is: "128 KiB".


#### //CycloneDDS/Domain/Sizing/ReceiveBufferSize
Number-with-unit

This element sets the size of a single receive buffer. Many receive buffers may be needed. The minimum workable size a little bit larger than Sizing/ReceiveBufferChunkSize, and the value used is taken as the configured value and the actual minimum workable size.

The unit must be specified explicitly. Recognised units: B (bytes), kB & KiB (2^10 bytes), MB & MiB (2^20 bytes), GB & GiB (2^30 bytes).

The default value is: "1 MiB".


### //CycloneDDS/Domain/TCP
Children: [AlwaysUsePeeraddrForUnicast](#cycloneddsdomaintcpalwaysusepeeraddrforunicast), [Enable](#cycloneddsdomaintcpenable), [NoDelay](#cycloneddsdomaintcpnodelay), [Port](#cycloneddsdomaintcpport), [ReadTimeout](#cycloneddsdomaintcpreadtimeout), [WriteTimeout](#cycloneddsdomaintcpwritetimeout)

The TCP element allows specifying various parameters related to running DDSI over TCP.


#### //CycloneDDS/Domain/TCP/AlwaysUsePeeraddrForUnicast
Boolean

Setting this to true means the unicast addresses in SPDP packets will be ignored and the peer address from the TCP connection will be used instead. This may help work around incorrectly advertised addresses when using TCP.

The default value is: "false".


#### //CycloneDDS/Domain/TCP/Enable
One of: false, true, default

This element enables the optional TCP transport - deprecated, use General/Transport instead.

The default value is: "default".


#### //CycloneDDS/Domain/TCP/NoDelay
Boolean

This element enables the TCP\_NODELAY socket option, preventing multiple DDSI messages being sent in the same TCP request. Setting this option typically optimises latency over throughput.

The default value is: "true".


#### //CycloneDDS/Domain/TCP/Port
Integer

This element specifies the TCP port number on which Cyclone DDS accepts connections. If the port is set it is used in entity locators, published with DDSI discovery. Dynamically allocated if zero. Disabled if -1 or not configured. If disabled other DDSI services will not be able to establish connections with the service, the service can only communicate by establishing connections to other services.

The default value is: "-1".


#### //CycloneDDS/Domain/TCP/ReadTimeout
Number-with-unit

This element specifies the timeout for blocking TCP read operations. If this timeout expires then the connection is closed.

The unit must be specified explicitly. Recognised units: ns, us, ms, s, min, hr, day.

The default value is: "2 s".


#### //CycloneDDS/Domain/TCP/WriteTimeout
Number-with-unit

This element specifies the timeout for blocking TCP write operations. If this timeout expires then the connection is closed.

The unit must be specified explicitly. Recognised units: ns, us, ms, s, min, hr, day.

The default value is: "2 s".


### //CycloneDDS/Domain/Threads
Children: [Thread](#cycloneddsdomainthreadsthread)

This element is used to set thread properties.


#### //CycloneDDS/Domain/Threads/Thread
Attributes: [Name](#cycloneddsdomainthreadsthreadname)
Children: [Scheduling](#cycloneddsdomainthreadsthreadscheduling), [StackSize](#cycloneddsdomainthreadsthreadstacksize)

This element is used to set thread properties.


#### //CycloneDDS/Domain/Threads/Thread[@Name]
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

The default value is: "".


##### //CycloneDDS/Domain/Threads/Thread/Scheduling
Children: [Class](#cycloneddsdomainthreadsthreadschedulingclass), [Priority](#cycloneddsdomainthreadsthreadschedulingpriority)

This element configures the scheduling properties of the thread.


###### //CycloneDDS/Domain/Threads/Thread/Scheduling/Class
One of: realtime, timeshare, default

This element specifies the thread scheduling class (realtime, timeshare or default). The user may need special privileges from the underlying operating system to be able to assign some of the privileged scheduling classes.

The default value is: "default".


###### //CycloneDDS/Domain/Threads/Thread/Scheduling/Priority
Text

This element specifies the thread priority (decimal integer or default). Only priorities that are supported by the underlying operating system can be assigned to this element. The user may need special privileges from the underlying operating system to be able to assign some of the privileged priorities.

The default value is: "default".


##### //CycloneDDS/Domain/Threads/Thread/StackSize
Number-with-unit

This element configures the stack size for this thread. The default value default leaves the stack size at the operating system default.

The unit must be specified explicitly. Recognised units: B (bytes), kB & KiB (2^10 bytes), MB & MiB (2^20 bytes), GB & GiB (2^30 bytes).

The default value is: "default".


### //CycloneDDS/Domain/Tracing
Children: [AppendToFile](#cycloneddsdomaintracingappendtofile), [Category](#cycloneddsdomaintracingcategory), [OutputFile](#cycloneddsdomaintracingoutputfile), [PacketCaptureFile](#cycloneddsdomaintracingpacketcapturefile), [Verbosity](#cycloneddsdomaintracingverbosity)

The Tracing element controls the amount and type of information that is written into the tracing log by the DDSI service. This is useful to track the DDSI service during application development.


#### //CycloneDDS/Domain/Tracing/AppendToFile
Boolean

This option specifies whether the output is to be appended to an existing log file. The default is to create a new log file each time, which is generally the best option if a detailed log is generated.

The default value is: "false".


#### //CycloneDDS/Domain/Tracing/Category
One of:
* Comma-separated list of: fatal, error, warning, info, config, discovery, data, radmin, timing, traffic, topic, tcp, plist, whc, throttle, rhc, content, trace
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

The default value is: "".


#### //CycloneDDS/Domain/Tracing/OutputFile
Text

This option specifies where the logging is printed to. Note that stdout and stderr are treated as special values, representing "standard out" and "standard error" respectively. No file is created unless logging categories are enabled using the Tracing/Verbosity or Tracing/EnabledCategory settings.

The default value is: "cyclonedds.log".


#### //CycloneDDS/Domain/Tracing/PacketCaptureFile
Text

This option specifies the file to which received and sent packets will be logged in the "pcap" format suitable for analysis using common networking tools, such as WireShark. IP and UDP headers are fictitious, in particular the destination address of received packets. The TTL may be used to distinguish between sent and received packets: it is 255 for sent packets and 128 for received ones. Currently IPv4 only.

The default value is: "".


#### //CycloneDDS/Domain/Tracing/Verbosity
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

The default value is: "none".
