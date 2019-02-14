/*
 * Copyright(c) 2006 to 2018 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <assert.h>
#include <limits.h>
#include <string.h>

#include "os/os.h"

#include "ddsi/q_config.h"
#include "ddsi/q_log.h"
#include "util/ut_avl.h"
#include "ddsi/q_unused.h"
#include "ddsi/q_misc.h"
#include "ddsi/q_addrset.h"
#include "ddsi/q_nwif.h"
#include "ddsi/q_error.h"

#include "util/ut_xmlparser.h"
#include "util/ut_expand_envvars.h"

#include "ddsc/ddsc_project.h"

#define WARN_DEPRECATED_ALIAS 1
#define WARN_DEPRECATED_UNIT 1
#define MAX_PATH_DEPTH 10 /* max nesting level of configuration elements */

struct cfgelem;
struct cfgst;

typedef int(*init_fun_t) (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem);
typedef int(*update_fun_t) (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, int first, const char *value);
typedef void(*free_fun_t) (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem);
typedef void(*print_fun_t) (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, int is_default);

#ifdef DDSI_INCLUDE_SECURITY
struct q_security_plugins q_security_plugin = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
#endif

struct unit {
    const char *name;
    int64_t multiplier;
};

struct cfgelem {
    const char *name;
    const struct cfgelem *children;
    const struct cfgelem *attributes;
    int multiplicity;
    const char *defvalue; /* NULL -> no default */
    int relative_offset;
    int elem_offset;
    init_fun_t init;
    update_fun_t update;
    free_fun_t free;
    print_fun_t print;
    const char *description;
};

struct cfgst_nodekey {
    const struct cfgelem *e;
};

struct cfgst_node {
    ut_avlNode_t avlnode;
    struct cfgst_nodekey key;
    int count;
    int failed;
    int is_default;
};

struct cfgst {
    ut_avlTree_t found;
    struct config *cfg;
    /* error flag set so that we can continue parsing for some errors and still fail properly */
    int error;

    /* path_depth, isattr and path together control the formatting of
    error messages by cfg_error() */
    int path_depth;
    int isattr[MAX_PATH_DEPTH];
    const struct cfgelem *path[MAX_PATH_DEPTH];
    void *parent[MAX_PATH_DEPTH];
};

/* "trace" is special: it enables (nearly) everything */
static const char *logcat_names[] = {
    "fatal", "error", "warning", "info", "config", "discovery", "data", "radmin", "timing", "traffic", "topic", "tcp", "plist", "whc", "throttle", "rhc", "trace", NULL
};
static const uint32_t logcat_codes[] = {
    DDS_LC_FATAL, DDS_LC_ERROR, DDS_LC_WARNING, DDS_LC_INFO, DDS_LC_CONFIG, DDS_LC_DISCOVERY, DDS_LC_DATA, DDS_LC_RADMIN, DDS_LC_TIMING, DDS_LC_TRAFFIC, DDS_LC_TOPIC, DDS_LC_TCP, DDS_LC_PLIST, DDS_LC_WHC, DDS_LC_THROTTLE, DDS_LC_RHC, DDS_LC_ALL
};

/* We want the tracing/verbosity settings to be fixed while parsing
the configuration, so we update this variable instead. */
static unsigned enabled_logcats;

static int cfgst_node_cmp(const void *va, const void *vb);
static const ut_avlTreedef_t cfgst_found_treedef =
UT_AVL_TREEDEF_INITIALIZER(offsetof(struct cfgst_node, avlnode), offsetof(struct cfgst_node, key), cfgst_node_cmp, 0);

#define DU(fname) static int uf_##fname (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, int first, const char *value)
#define PF(fname) static void pf_##fname (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, int is_default)
#define DUPF(fname) DU(fname) ; PF(fname)
PF(nop);
DUPF(networkAddress);
DUPF(networkAddresses);
DU(ipv4);
DUPF(allow_multicast);
DUPF(boolean);
DU(boolean_default);
#if 0
PF(boolean_default);
#endif
DUPF(negated_boolean);
DUPF(string);
DU(tracingOutputFileName);
DU(verbosity);
DUPF(logcat);
DUPF(float);
DUPF(int);
DUPF(uint);
DUPF(int32);
#if 0
DUPF(uint32);
#endif
DU(natint);
DU(natint_255);
DUPF(participantIndex);
DU(port);
DU(dyn_port);
DUPF(memsize);
DU(duration_inf);
DU(duration_ms_1hr);
DU(duration_ms_1s);
DU(duration_us_1s);
PF(duration);
DUPF(standards_conformance);
DUPF(besmode);
DUPF(retransmit_merging);
DUPF(sched_prio_class);
DUPF(sched_class);
DUPF(maybe_memsize);
DUPF(maybe_int32);
#ifdef DDSI_INCLUDE_ENCRYPTION
DUPF(cipher);
#endif
#ifdef DDSI_INCLUDE_BANDWIDTH_LIMITING
DUPF(bandwidth);
#endif
DUPF(domainId);
DUPF(durability_cdr);
DUPF(transport_selector);
DUPF(many_sockets_mode);
DU(deaf_mute);
#ifdef DDSI_INCLUDE_SSL
DUPF(min_tls_version);
#endif
#undef DUPF
#undef DU
#undef PF

#define DF(fname) static void fname (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem)
DF(ff_free);
DF(ff_networkAddresses);
#undef DF

#define DI(fname) static int fname (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem)
#ifdef DDSI_INCLUDE_NETWORK_CHANNELS
DI(if_channel);
#endif /* DDSI_INCLUDE_NETWORK_CHANNELS */
#ifdef DDSI_INCLUDE_ENCRYPTION
DI(if_security_profile);
#endif
#ifdef DDSI_INCLUDE_NETWORK_PARTITIONS
DI(if_network_partition);
DI(if_ignored_partition);
DI(if_partition_mapping);
#endif
DI(if_peer);
DI(if_thread_properties);
#undef DI

#define CO(name) ((int) offsetof (struct config, name))
#define ABSOFF(name) 0, CO (name)
#define RELOFF(parent,name) 1, ((int) offsetof (struct parent, name))
#define NODATA 1, NULL, 0, 0, 0, 0, 0, 0
#define END_MARKER { NULL, NULL, NULL, NODATA, NULL }
#define WILDCARD { "*", NULL, NULL, NODATA, NULL }
#define LEAF(name) name, NULL, NULL
#define DEPRECATED_LEAF(name) "|" name, NULL, NULL
#define LEAF_W_ATTRS(name, attrs) name, NULL, attrs
#define GROUP(name, children) name, children, NULL, 1, NULL, 0, 0, 0, 0, 0, 0
#define GROUP_W_ATTRS(name, children, attrs) name, children, attrs, 1, NULL, 0, 0, 0, 0, 0, 0
#define MGROUP(name, children, attrs) name, children, attrs
#define ATTR(name) name, NULL, NULL
#define DEPRECATED_ATTR(name) "|" name, NULL, NULL
/* MOVED: whereto must be a path relative to DDSI2Service, may not be used in/for lists and only for elements, may not be chained */
#define MOVED(name, whereto) ">" name, NULL, NULL, 0, whereto, 0, 0, 0, 0, 0, 0, NULL
static const struct cfgelem timestamp_cfgattrs[] = {
    { ATTR("absolute"), 1, "false", ABSOFF(tracingRelativeTimestamps), 0, uf_negated_boolean, 0, pf_negated_boolean,
    "<p>This option has no effect</p>" },
    END_MARKER
};

static const struct cfgelem general_cfgelems[] = {
    { LEAF("NetworkInterfaceAddress"), 1, "auto", ABSOFF(networkAddressString), 0, uf_networkAddress, ff_free, pf_networkAddress,
    "<p>This element specifies the preferred network interface for use by DDSI2E. The preferred network interface determines the IP address that DDSI2E advertises in the discovery protocol (but see also General/ExternalNetworkAddress), and is also the only interface over which multicasts are transmitted. The interface can be identified by its IP address, network interface name or network portion of the address. If the value \"auto\" is entered here, DDSI2E will select what it considers the most suitable interface.</p>" },
    { LEAF("MulticastRecvNetworkInterfaceAddresses"), 1, "preferred", ABSOFF(networkRecvAddressStrings), 0, uf_networkAddresses, ff_networkAddresses, pf_networkAddresses,
    "<p>This element specifies on which network interfaces DDSI2E listens to multicasts. The following options are available:</p>\n\
<ul>\n\
<li><i>all</i>: listen for multicasts on all multicast-capable interfaces; or</li>\n\
<li><i>any</i>: listen for multicasts on the operating system default interface; or</li>\n\
<li><i>preferred</i>: listen for multicasts on the preferred interface (General/NetworkInterfaceAddress); or</li>\n\
<li><i>none</i>: does not listen for multicasts on any interface; or</li>\n\
<li>a comma-separated list of network addresses: configures DDSI2E to listen for multicasts on all of the listed addresses.</li>\n\
</ul>\n\
<p>If DDSI2E is in IPv6 mode and the address of the preferred network interface is a link-local address, \"all\" is treated as a synonym for \"preferred\" and a comma-separated list is treated as \"preferred\" if it contains the preferred interface and as \"none\" if not.</p>" },
{ LEAF("ExternalNetworkAddress"), 1, "auto", ABSOFF(externalAddressString), 0, uf_networkAddress, ff_free, pf_networkAddress,
"<p>This element allows explicitly overruling the network address DDSI2E advertises in the discovery protocol, which by default is the address of the preferred network interface (General/NetworkInterfaceAddress), to allow DDSI2E to communicate across a Network Address Translation (NAT) device.</p>" },
{ LEAF("ExternalNetworkMask"), 1, "0.0.0.0", ABSOFF(externalMaskString), 0, uf_string, ff_free, pf_string,
"<p>This element specifies the network mask of the external network address. This element is relevant only when an external network address (General/ExternalNetworkAddress) is explicitly configured. In this case locators received via the discovery protocol that are within the same external subnet (as defined by this mask) will be translated to an internal address by replacing the network portion of the external address with the corresponding portion of the preferred network interface address. This option is IPv4-only.</p>" },
{ LEAF("AllowMulticast"), 1, "true", ABSOFF(allowMulticast), 0, uf_allow_multicast, 0, pf_allow_multicast,
"<p>This element controls whether DDSI2E uses multicasts for data traffic.</p>\n\
<p>It is a comma-separated list of some of the following keywords: \"spdp\", \"asm\", \"ssm\", or either of \"false\" or \"true\".</p>\n\
<ul>\n\
<li><i>spdp</i>: enables the use of ASM (any-source multicast) for participant discovery</li>\n\
<li><i>asm</i>: enables the use of ASM for all traffic (including SPDP)</li>\n\
<li><i>ssm</i>: enables the use of SSM (source-specific multicast) for all non-SPDP traffic (if supported)</li>\n\
</ul>\n\
<p>When set to \"false\" all multicasting is disabled. The default, \"true\" enables full use of multicasts. Listening for multicasts can be controlled by General/MulticastRecvNetworkInterfaceAddresses.</p>" },
{ LEAF("MulticastTimeToLive"), 1, "32", ABSOFF(multicast_ttl), 0, uf_natint_255, 0, pf_int,
"<p>This element specifies the time-to-live setting for outgoing multicast packets.</p>" },
{ LEAF("DontRoute"), 1, "false", ABSOFF(dontRoute), 0, uf_boolean, 0, pf_boolean,
"<p>This element allows setting the SO_DONTROUTE option for outgoing packets, to bypass the local routing tables. This is generally useful only when the routing tables cannot be trusted, which is highly unusual.</p>" },
{ LEAF ("UseIPv6"), 1, "default", ABSOFF (compat_use_ipv6), 0, uf_boolean_default, 0, pf_nop,
"<p>Deprecated (use Transport instead)</p>" },
{ LEAF ("Transport"), 1, "default", ABSOFF (transport_selector), 0, uf_transport_selector, 0, pf_transport_selector,
"<p>This element allows selecting the transport to be used (udp, udp6, tcp, tcp6, raweth)</p>" },
{ LEAF("EnableMulticastLoopback"), 1, "true", ABSOFF(enableMulticastLoopback), 0, uf_boolean, 0, pf_boolean,
"<p>This element specifies whether DDSI2E allows IP multicast packets to be visible to all DDSI participants in the same node, including itself. It must be \"true\" for intra-node multicast communications, but if a node runs only a single DDSI2E service and does not host any other DDSI-capable programs, it should be set to \"false\" for improved performance.</p>" },
{ DEPRECATED_LEAF("EnableLoopback"), 1, "false", ABSOFF(enableLoopback), 0, uf_boolean, 0, pf_boolean,
"<p>This element specifies whether DDSI packets are visible to all DDSI participants in the same process. It must be \"true\" for intra-process communications, i.e. a reader and writer communicating in the same address space. If enabled and using multicast then EnableMulticastLoopback must also be enabled.</p>" },
{ LEAF("StartupModeDuration"), 1, "2 s", ABSOFF(startup_mode_duration), 0, uf_duration_ms_1hr, 0, pf_duration,
"<p>This element specifies how long the DDSI2E remains in its \"startup\" mode. While in \"startup\" mode all volatile reliable data published on the local node is retained as-if it were transient-local data, allowing existing readers on remote nodes to obtain the data even though discovering them takes some time. Best-effort data by definition need not arrive, and transient and persistent data are covered by the durability service.</p>\n\
<p>Once the system is stable, DDSI2E keeps track of the existence of remote readers whether or not matching writers exist locally, avoiding this discovery delay and ensuring this is merely a node startup issue.</p>\n\
<p>Setting General/StartupModeDuration to 0s will disable it.</p>" },
{ LEAF("StartupModeCoversTransient"), 1, "true", ABSOFF(startup_mode_full), 0, uf_boolean, 0, pf_boolean,
"<p>This element configures whether startup-mode should also cover transient and persistent data, for configurations where the durability service does not take care of it. Configurations without defined merge policies best leave this enabled.</p>" },
{ LEAF("MaxMessageSize"), 1, "4096 B", ABSOFF(max_msg_size), 0, uf_memsize, 0, pf_memsize,
"<p>This element specifies the maximum size of the UDP payload that DDSI2E will generate. DDSI2E will try to maintain this limit within the bounds of the DDSI specification, which means that in some cases (especially for very low values of MaxMessageSize) larger payloads may sporadically be observed (currently up to 1192 B).</p>\n\
<p>On some networks it may be necessary to set this item to keep the packetsize below the MTU to prevent IP fragmentation. In those cases, it is generally advisable to also consider reducing Internal/FragmentSize.</p>" },
{ LEAF("FragmentSize"), 1, "1280 B", ABSOFF(fragment_size), 0, uf_memsize, 0, pf_memsize,
"<p>This element specifies the size of DDSI sample fragments generated by DDSI2E. Samples larger than FragmentSize are fragmented into fragments of FragmentSize bytes each, except the last one, which may be smaller. The DDSI spec mandates a minimum fragment size of 1025 bytes, but DDSI2E will do whatever size is requested, accepting fragments of which the size is at least the minimum of 1025 and FragmentSize.</p>" },
END_MARKER
};

#ifdef DDSI_INCLUDE_ENCRYPTION
static const struct cfgelem securityprofile_cfgattrs[] = {
    { ATTR("Name"), 1, NULL, RELOFF(config_securityprofile_listelem, name), 0, uf_string, ff_free, pf_string,
    "<p>This attribute specifies the name of this DDSI2E security profile. Two security profiles cannot have the same name.</p>" },
    { ATTR("Cipher"), 1, "null", RELOFF(config_securityprofile_listelem, cipher), 0, uf_cipher, 0, pf_cipher,
    "<p>This attribute specifies the cipher to be used for encrypting traffic over network partitions secured by this security profile. The possible ciphers are:</p>\n\
<ul><li><i>aes128</i>: AES with a 128-bit key;</li>\n\
<li><i>aes192</i>: AES with a 192-bit key;</li>\n\
<li><i>aes256</i>: AES with a 256-bit key;</li>\n\
<li><i>blowfish</i>: the Blowfish cipher with a 128 bit key;</li>\n\
<li><i>null</i>: no encryption;</li></ul>\n\
<p>SHA1 is used on conjunction with all ciphers except \"null\" to ensure data integrity.</p>" },
{ ATTR("CipherKey"), 1, "", RELOFF(config_securityprofile_listelem, key), 0, uf_string, ff_free, pf_key,
"<p>The CipherKey attribute is used to define the secret key required by the cipher selected using the Cipher attribute. The value can be a URI referencing an external file containing the secret key, or the secret key can be defined in-place as a string value.</p>\n\
<p>The key must be specified as a hexadecimal string with each character representing 4 bits of the key. E.g., 1ABC represents the 16-bit key 0001 1010 1011 1100. The key should not follow a well-known pattern and must exactly match the key length of the selected cipher.</p>\n\
<p>A malformed key will cause the security profile to be marked as invalid, and disable all network partitions secured by the (invalid) security profile to prevent information leaks.</p>\n\
<p>As all DDS applications require read access to the XML configuration file, for security reasons it is recommended to store the secret key in an external file in the file system, referenced by its URI. The file should be protected against read and write access from other users on the host.</p>" },
END_MARKER
};

static const struct cfgelem security_cfgelems[] = {
    { LEAF_W_ATTRS("SecurityProfile", securityprofile_cfgattrs), 0, 0, ABSOFF(securityProfiles), if_security_profile, 0, 0, 0,
    "<p>This element defines a DDSI2E security profile.</p>" },
    END_MARKER
};
#endif /* DDSI_INCLUDE_ENCRYPTION */

#ifdef DDSI_INCLUDE_NETWORK_PARTITIONS
static const struct cfgelem networkpartition_cfgattrs[] = {
    { ATTR("Name"), 1, NULL, RELOFF(config_networkpartition_listelem, name), 0, uf_string, ff_free, pf_string,
    "<p>This attribute specifies the name of this DDSI2E network partition. Two network partitions cannot have the same name.</p>" },
    { ATTR("Address"), 1, NULL, RELOFF(config_networkpartition_listelem, address_string), 0, uf_string, ff_free, pf_string,
    "<p>This attribute specifies the multicast addresses associated with the network partition as a comma-separated list. Readers matching this network partition (cf. Partitioning/PartitionMappings) will listen for multicasts on all of these addresses and advertise them in the discovery protocol. The writers will select the most suitable address from the addresses advertised by the readers.</p>" },
    { ATTR("Connected"), 1, "true", RELOFF(config_networkpartition_listelem, connected), 0, uf_boolean, 0, pf_boolean,
    "<p>This attribute is a placeholder.</p>" },
#ifdef DDSI_INCLUDE_ENCRYPTION
    { ATTR("SecurityProfile"), 1, "null", RELOFF(config_networkpartition_listelem, profileName), 0, uf_string, ff_free, pf_string,
    "<p>This attribute selects the DDSI2E security profile for encrypting the traffic mapped to this DDSI2E network partition. The default \"null\" means the network partition is unsecured; any other name refers to a security profile defined using the Security/SecurityProfile elements.</p>" },
#endif /* DDSI_INCLUDE_ENCRYPTION */
    END_MARKER
};

static const struct cfgelem networkpartitions_cfgelems[] = {
    { LEAF_W_ATTRS("NetworkPartition", networkpartition_cfgattrs), 0, 0, ABSOFF(networkPartitions), if_network_partition, 0, 0, 0,
    "<p>This element defines a DDSI2E network partition.</p>" },
    END_MARKER
};

static const struct cfgelem ignoredpartitions_cfgattrs[] = {
    { ATTR("DCPSPartitionTopic"), 1, NULL, RELOFF(config_ignoredpartition_listelem, DCPSPartitionTopic), 0, uf_string, ff_free, pf_string,
    "<p>This attribute specifies a partition and a topic expression, separated by a single '.', that are used to determine if a given partition and topic will be ignored or not. The expressions may use the usual wildcards '*' and '?'. DDSI2E will consider an wildcard DCPS partition to match an expression iff there exists a string that satisfies both expressions.</p>" },
    END_MARKER
};

static const struct cfgelem ignoredpartitions_cfgelems[] = {
    { LEAF_W_ATTRS("IgnoredPartition", ignoredpartitions_cfgattrs), 0, 0, ABSOFF(ignoredPartitions), if_ignored_partition, 0, 0, 0,
    "<p>This element can be used to prevent certain combinations of DCPS partition and topic from being transmitted over the network. DDSI2E will complete ignore readers and writers for which all DCPS partitions as well as their topic is ignored, not even creating DDSI readers and writers to mirror the DCPS ones.</p>" },
    END_MARKER
};

static const struct cfgelem partitionmappings_cfgattrs[] = {
    { ATTR("NetworkPartition"), 1, NULL, RELOFF(config_partitionmapping_listelem, networkPartition), 0, uf_string, ff_free, pf_string,
    "<p>This attribute specifies which DDSI2E network partition is to be used for DCPS partition/topic combinations matching the DCPSPartitionTopic attribute within this PartitionMapping element.</p>" },
    { ATTR("DCPSPartitionTopic"), 1, NULL, RELOFF(config_partitionmapping_listelem, DCPSPartitionTopic), 0, uf_string, ff_free, pf_string,
    "<p>This attribute specifies a partition and a topic expression, separated by a single '.', that are used to determine if a given partition and topic maps to the DDSI2E network partition named by the NetworkPartition attribute in this PartitionMapping element. The expressions may use the usual wildcards '*' and '?'. DDSI2E will consider a wildcard DCPS partition to match an expression if there exists a string that satisfies both expressions.</p>" },
    END_MARKER
};

static const struct cfgelem partitionmappings_cfgelems[] = {
    { LEAF_W_ATTRS("PartitionMapping", partitionmappings_cfgattrs), 0, 0, ABSOFF(partitionMappings), if_partition_mapping, 0, 0, 0,
    "<p>This element defines a mapping from a DCPS partition/topic combination to a DDSI2E network partition. This allows partitioning data flows by using special multicast addresses for part of the data and possibly also encrypting the data flow.</p>" },
    END_MARKER
};

static const struct cfgelem partitioning_cfgelems[] = {
    { GROUP("NetworkPartitions", networkpartitions_cfgelems),
    "<p>The NetworkPartitions element specifies the DDSI2E network partitions.</p>" },
    { GROUP("IgnoredPartitions", ignoredpartitions_cfgelems),
    "<p>The IgnoredPartitions element specifies DCPS partition/topic combinations that are not distributed over the network.</p>" },
    { GROUP("PartitionMappings", partitionmappings_cfgelems),
    "<p>The PartitionMappings element specifies the mapping from DCPS partition/topic combinations to DDSI2E network partitions.</p>" },
    END_MARKER
};
#endif /* DDSI_INCLUDE_NETWORK_PARTITIONS */

#ifdef DDSI_INCLUDE_NETWORK_CHANNELS
static const struct cfgelem channel_cfgelems[] = {
#ifdef DDSI_INCLUDE_BANDWIDTH_LIMITING
    { LEAF("DataBandwidthLimit"), 1, "inf", RELOFF(config_channel_listelem, data_bandwidth_limit), 0, uf_bandwidth, 0, pf_bandwidth,
    "<p>This element specifies the maximum transmit rate of new samples and directly related data, for this channel. Bandwidth limiting uses a leaky bucket scheme. The default value \"inf\" means DDSI2E imposes no limitation, the underlying operating system and hardware will likely limit the maimum transmit rate.</p>" },
    { LEAF("AuxiliaryBandwidthLimit"), 1, "inf", RELOFF(config_channel_listelem, auxiliary_bandwidth_limit), 0, uf_bandwidth, 0, pf_bandwidth,
    "<p>This element specifies the maximum transmit rate of auxiliary traffic on this channel (e.g. retransmits, heartbeats, etc). Bandwidth limiting uses a leaky bucket scheme. The default value \"inf\" means DDSI2E imposes no limitation, the underlying operating system and hardware will likely limit the maimum transmit rate.</p>" },
#endif
    { LEAF("DiffServField"), 1, "0", RELOFF(config_channel_listelem, diffserv_field), 0, uf_natint, 0, pf_int,
    "<p>This element describes the DiffServ setting the channel will apply to the networking messages. This parameter determines the value of the diffserv field of the IP version 4 packets sent on this channel which allows QoS setting to be applied to the network traffic send on this channel.<br/>\n\
Windows platform support for setting the diffserv field is dependent on the OS version.<br/>\n\
For Windows versions XP SP2 and 2003 to use the diffserv field the following parameter should be added to the register:<br/><br>\n\
HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\TcpIp\\Parameters\\DisableUserTOSSetting<br/><br/>\n\
The type of this parameter is a DWORD and its value should be set to 0 to allow setting of the diffserv field.<br/><br/>\n\
For Windows version 7 or higher a new API (qWAVE) has been introduced. For these platforms the specified diffserv value is mapped to one of the support traffic types.\n\
The mapping is as follows: 1-8 background traffic; 9-40 excellent traffic; 41-55 audio/video traffic; 56 voice traffic; 57-63 control traffic.\n\
When an application is run without Administrative priveleges then only the diffserv value of 0, 8, 40 or 56 is allowed.</p>" },
END_MARKER
};

static const struct cfgelem channel_cfgattrs[] = {
    { ATTR("Name"), 1, NULL, RELOFF(config_channel_listelem, name), 0, uf_string, ff_free, pf_string,
    "<p>This attribute specifies name of this channel. The name should uniquely identify the channel.</p>" },
    { ATTR("TransportPriority"), 1, "0", RELOFF(config_channel_listelem, priority), 0, uf_natint, 0, pf_int,
    "<p>This attribute sets the transport priority threshold for the channel. Each DCPS data writer has a \"transport_priority\" QoS and this QoS is used to select a channel for use by this writer. The selected channel is the one with the largest threshold not greater than the writer's transport priority, and if no such channel exists, the channel with the lowest threshold.</p>" },
    END_MARKER
};

static const struct cfgelem channels_cfgelems[] = {
    { MGROUP("Channel", channel_cfgelems, channel_cfgattrs), 42, 0, ABSOFF(channels), if_channel, 0, 0, 0,
    "<p>This element defines a channel.</p>" },
    END_MARKER
};
#endif /* DDSI_INCLUDE_NETWORK_CHANNELS */

static const struct cfgelem thread_properties_sched_cfgelems[] = {
    { LEAF("Class"), 1, "default", RELOFF(config_thread_properties_listelem, sched_class), 0, uf_sched_class, 0, pf_sched_class,
    "<p>This element specifies the thread scheduling class (<i>realtime</i>, <i>timeshare</i> or <i>default</i>). The user may need special privileges from the underlying operating system to be able to assign some of the privileged scheduling classes.</p>" },
    { LEAF("Priority"), 1, "default", RELOFF(config_thread_properties_listelem, sched_priority), 0, uf_maybe_int32, 0, pf_maybe_int32,
    "<p>This element specifies the thread priority (decimal integer or <i>default</i>). Only priorities that are supported by the underlying operating system can be assigned to this element. The user may need special privileges from the underlying operating system to be able to assign some of the privileged priorities.</p>" },
    END_MARKER
};

static const struct cfgelem thread_properties_cfgattrs[] = {
    { ATTR("Name"), 1, NULL, RELOFF(config_thread_properties_listelem, name), 0, uf_string, ff_free, pf_string,
    "<p>The Name of the thread for which properties are being set. The following threads exist:</p>\n\
<ul><li><i>gc</i>: garbage collector thread involved in deleting entities;</li>\n\
<li><i>recv</i>: receive thread, taking data from the network and running the protocol state machine;</li>\n\
<li><i>dq.builtins</i>: delivery thread for DDSI-builtin data, primarily for discovery;</li>\n\
<li><i>lease</i>: DDSI liveliness monitoring;</li>\n\
<li><i>tev</i>: general timed-event handling, retransmits and discovery;</li>\n\
<li><i>xmit.CHAN</i>: transmit thread for channel CHAN;</li>\n\
<li><i>dq.CHAN</i>: delivery thread for channel CHAN;</li>\n\
<li><i>tev.CHAN</i>: timed-even thread for channel CHAN.</li></ul>" },
END_MARKER
};

static const struct cfgelem thread_properties_cfgelems[] = {
    { GROUP("Scheduling", thread_properties_sched_cfgelems),
    "<p>This element configures the scheduling properties of the thread.</p>" },
    { LEAF("StackSize"), 1, "default", RELOFF(config_thread_properties_listelem, stack_size), 0, uf_maybe_memsize, 0, pf_maybe_memsize,
    "<p>This element configures the stack size for this thread. The default value <i>default</i> leaves the stack size at the operating system default.</p>" },
    END_MARKER
};

static const struct cfgelem threads_cfgelems[] = {
    { MGROUP("Thread", thread_properties_cfgelems, thread_properties_cfgattrs), 1000, 0, ABSOFF(thread_properties), if_thread_properties, 0, 0, 0,
    "<p>This element is used to set thread properties.</p>" },
    END_MARKER
};

static const struct cfgelem compatibility_cfgelems[] = {
    { LEAF("StandardsConformance"), 1, "lax", ABSOFF(standards_conformance), 0, uf_standards_conformance, 0, pf_standards_conformance,
    "<p>This element sets the level of standards conformance of this instance of the DDSI2E Service. Stricter conformance typically means less interoperability with other implementations. Currently three modes are defined:</p>\n\
<ul><li><i>pedantic</i>: very strictly conform to the specification, ultimately for compliancy testing, but currently of little value because it adheres even to what will most likely turn out to be editing errors in the DDSI standard. Arguably, as long as no errata have been published it is the current text that is in effect, and that is what pedantic currently does.</li>\n\
<li><i>strict</i>: a slightly less strict view of the standard than does pedantic: it follows the established behaviour where the standard is obviously in error.</li>\n\
<li><i>lax</i>: attempt to provide the smoothest possible interoperability, anticipating future revisions of elements in the standard in areas that other implementations do not adhere to, even though there is no good reason not to.</li></ul>\n\
<p>The default setting is \"lax\".</p>" },
{ LEAF("ExplicitlyPublishQosSetToDefault"), 1, "false", ABSOFF(explicitly_publish_qos_set_to_default), 0, uf_boolean, 0, pf_boolean,
"<p>This element specifies whether QoS settings set to default values are explicitly published in the discovery protocol. Implementations are to use the default value for QoS settings not published, which allows a significant reduction of the amount of data that needs to be exchanged for the discovery protocol, but this requires all implementations to adhere to the default values specified by the specifications.</p>\n\
<p>When interoperability is required with an implementation that does not follow the specifications in this regard, setting this option to true will help.</p>" },
{ LEAF ("ManySocketsMode"), 1, "single", ABSOFF (many_sockets_mode), 0, uf_many_sockets_mode, 0, pf_many_sockets_mode,
"<p>This option specifies whether a network socket will be created for each domain participant on a host. The specification seems to assume that each participant has a unique address, and setting this option will ensure this to be the case. This is not the defeault.</p>\n\
<p>Disabling it slightly improves performance and reduces network traffic somewhat. It also causes the set of port numbers needed by DDSI2E to become predictable, which may be useful for firewall and NAT configuration.</p>" },
{ LEAF("ArrivalOfDataAssertsPpAndEpLiveliness"), 1, "true", ABSOFF(arrival_of_data_asserts_pp_and_ep_liveliness), 0, uf_boolean, 0, pf_boolean,
"<p>When set to true, arrival of a message from a peer asserts liveliness of that peer. When set to false, only SPDP and explicit lease renewals have this effect.</p>" },
{ LEAF("AckNackNumbitsEmptySet"), 1, "0", ABSOFF(acknack_numbits_emptyset), 0, uf_natint, 0, pf_int,
"<p>This element governs the representation of an acknowledgement message that does not also negatively-acknowledge some samples. If set to 0, the generated acknowledgements have an invalid form and will be reject by the strict and pedantic conformance modes, but several other implementation require this setting for smooth interoperation.</p>\n\
<p>If set to 1, all acknowledgements sent by DDSI2E adhere the form of acknowledgement messages allowed by the standard, but this causes problems when interoperating with these other implementations. The strict and pedantic standards conformance modes always overrule an AckNackNumbitsEmptySet=0 to prevent the transmitting of invalid messages.</p>" },
{ LEAF("RespondToRtiInitZeroAckWithInvalidHeartbeat"), 1, "false", ABSOFF(respond_to_rti_init_zero_ack_with_invalid_heartbeat), 0, uf_boolean, 0, pf_boolean,
"<p>This element allows a closer mimicking of the behaviour of some other DDSI implementations, albeit at the cost of generating even more invalid messages. Setting it to true ensures a Heartbeat can be sent at any time when a remote node requests one, setting it to false delays it until a valid one can be sent.</p>\n\
<p>The latter is fully compliant with the specification, and no adverse effects have been observed. It is the default.</p>" },
{ LEAF("AssumeRtiHasPmdEndpoints"), 1, "false", ABSOFF(assume_rti_has_pmd_endpoints), 0, uf_boolean, 0, pf_boolean,
"<p>This option assumes ParticipantMessageData endpoints required by the liveliness protocol are present in RTI participants even when not properly advertised by the participant discovery protocol.</p>" },
END_MARKER
};

static const struct cfgelem unsupp_test_cfgelems[] = {
    { LEAF("XmitLossiness"), 1, "0", ABSOFF(xmit_lossiness), 0, uf_int, 0, pf_int,
    "<p>This element controls the fraction of outgoing packets to drop, specified as samples per thousand.</p>" },
    END_MARKER
};

static const struct cfgelem unsupp_watermarks_cfgelems[] = {
    { LEAF("WhcLow"), 1, "1 kB", ABSOFF(whc_lowwater_mark), 0, uf_memsize, 0, pf_memsize,
    "<p>This element sets the low-water mark for the DDSI2E WHCs, expressed in bytes. A suspended writer resumes transmitting when its DDSI2E WHC shrinks to this size.</p>" },
    { LEAF("WhcHigh"), 1, "100 kB", ABSOFF(whc_highwater_mark), 0, uf_memsize, 0, pf_memsize,
    "<p>This element sets the maximum allowed high-water mark for the DDSI2E WHCs, expressed in bytes. A writer is suspended when the WHC reaches this size.</p>" },
    { LEAF("WhcHighInit"), 1, "30 kB", ABSOFF(whc_init_highwater_mark), 0, uf_maybe_memsize, 0, pf_maybe_memsize,
    "<p>This element sets the initial level of the high-water mark for the DDSI2E WHCs, expressed in bytes.</p>" },
    { LEAF("WhcAdaptive|WhcAdaptative"), 1, "true", ABSOFF(whc_adaptive), 0, uf_boolean, 0, pf_boolean,
    "<p>This element controls whether DDSI2E will adapt the high-water mark to current traffic conditions, based on retransmit requests and transmit pressure.</p>" },
    END_MARKER
};

static const struct cfgelem control_topic_cfgattrs[] = {
    { DEPRECATED_ATTR("Enable"), 1, "false", ABSOFF(enable_control_topic), 0, uf_boolean, 0, pf_boolean,
      "<p>This element controls whether DDSI2E should create a topic to control DDSI2E's behaviour dynamically.<p>"
    },
    { DEPRECATED_ATTR("InitialReset"), 1, "inf", ABSOFF(initial_deaf_mute_reset), 0, uf_duration_inf, 0, pf_duration,
      "<p>This element controls after how much time an initial deaf/mute state will automatically reset.<p>"
    },
    END_MARKER
};

static const struct cfgelem control_topic_cfgelems[] = {
    { DEPRECATED_LEAF ("Deaf"), 1, "false", ABSOFF (initial_deaf), 0, uf_deaf_mute, 0, pf_boolean,
    "<p>This element controls whether DDSI2E defaults to deaf mode or to normal mode. This controls both the initial behaviour and what behaviour it auto-reverts to.</p>" },
    { DEPRECATED_LEAF ("Mute"), 1, "false", ABSOFF (initial_mute), 0, uf_deaf_mute, 0, pf_boolean,
    "<p>This element controls whether DDSI2E defaults to mute mode or to normal mode. This controls both the initial behaviour and what behaviour it auto-reverts to.</p>" },
    END_MARKER
};

static const struct cfgelem rediscovery_blacklist_duration_attrs[] = {
    { ATTR("enforce"), 1, "false", ABSOFF(prune_deleted_ppant.enforce_delay), 0, uf_boolean, 0, pf_boolean,
    "<p>This attribute controls whether the configured time during which recently deleted participants will not be rediscovered (i.e., \"black listed\") is enforced and following complete removal of the participant in DDSI2E, or whether it can be rediscovered earlier provided all traces of that participant have been removed already.</p>" },
    END_MARKER
};

static const struct cfgelem heartbeat_interval_attrs[] = {
    { ATTR ("min"), 1, "5 ms", ABSOFF (const_hb_intv_min), 0, uf_duration_inf, 0, pf_duration,
    "<p>This attribute sets the minimum interval that must have passed since the most recent heartbeat from a writer, before another asynchronous (not directly related to writing) will be sent.</p>" },
    { ATTR ("minsched"), 1, "20 ms", ABSOFF (const_hb_intv_sched_min), 0, uf_duration_inf, 0, pf_duration,
    "<p>This attribute sets the minimum interval for periodic heartbeats. Other events may still cause heartbeats to go out.</p>" },
    { ATTR ("max"), 1, "8 s", ABSOFF (const_hb_intv_sched_max), 0, uf_duration_inf, 0, pf_duration,
    "<p>This attribute sets the maximum interval for periodic heartbeats.</p>" },
    END_MARKER
};

static const struct cfgelem liveliness_monitoring_attrs[] = {
  { ATTR("StackTraces"), 1, "true", ABSOFF(noprogress_log_stacktraces), 0, uf_boolean, 0, pf_boolean,
    "<p>This element controls whether or not to write stack traces to the DDSI2 trace when a thread fails to make progress (on select platforms only).</p>" },
  END_MARKER
};

static const struct cfgelem multiple_recv_threads_attrs[] = {
  { ATTR("maxretries"), 1, "4294967295", ABSOFF(recv_thread_stop_maxretries), 0, uf_uint, 0, pf_uint,
    "<p>Receive threads dedicated to a single socket can only be triggered for termination by sending a packet. Reception of any packet will do, so termination failure due to packet loss is exceedingly unlikely, but to eliminate all risks, it will retry as many times as specified by this attribute before aborting.</p>" },
  END_MARKER
};

static const struct cfgelem unsupp_cfgelems[] = {
    { MOVED("MaxMessageSize", "General/MaxMessageSize") },
    { MOVED("FragmentSize", "General/FragmentSize") },
    { LEAF("DeliveryQueueMaxSamples"), 1, "256", ABSOFF(delivery_queue_maxsamples), 0, uf_uint, 0, pf_uint,
    "<p>This element controls the Maximum size of a delivery queue, expressed in samples. Once a delivery queue is full, incoming samples destined for that queue are dropped until space becomes available again.</p>" },
    { LEAF("PrimaryReorderMaxSamples"), 1, "64", ABSOFF(primary_reorder_maxsamples), 0, uf_uint, 0, pf_uint,
    "<p>This element sets the maximum size in samples of a primary re-order administration. Each proxy writer has one primary re-order administration to buffer the packet flow in case some packets arrive out of order. Old samples are forwarded to secondary re-order administrations associated with readers in need of historical data.</p>" },
    { LEAF("SecondaryReorderMaxSamples"), 1, "16", ABSOFF(secondary_reorder_maxsamples), 0, uf_uint, 0, pf_uint,
    "<p>This element sets the maximum size in samples of a secondary re-order administration. The secondary re-order administration is per reader in need of historical data.</p>" },
    { LEAF("DefragUnreliableMaxSamples"), 1, "4", ABSOFF(defrag_unreliable_maxsamples), 0, uf_uint, 0, pf_uint,
    "<p>This element sets the maximum number of samples that can be defragmented simultaneously for a best-effort writers.</p>" },
    { LEAF("DefragReliableMaxSamples"), 1, "16", ABSOFF(defrag_reliable_maxsamples), 0, uf_uint, 0, pf_uint,
    "<p>This element sets the maximum number of samples that can be defragmented simultaneously for a reliable writer. This has to be large enough to handle retransmissions of historical data in addition to new samples.</p>" },
    { LEAF("BuiltinEndpointSet"), 1, "writers", ABSOFF(besmode), 0, uf_besmode, 0, pf_besmode,
    "<p>This element controls which participants will have which built-in endpoints for the discovery and liveliness protocols. Valid values are:</p>\n\
<ul><li><i>full</i>: all participants have all endpoints;</li>\n\
<li><i>writers</i>: all participants have the writers, but just one has the readers;</li>\n\
<li><i>minimal</i>: only one participant has built-in endpoints.</li></ul>\n\
<p>The default is <i>writers</i>, as this is thought to be compliant and reasonably efficient. <i>Minimal</i> may or may not be compliant but is most efficient, and <i>full</i> is inefficient but certain to be compliant. See also Internal/ConservativeBuiltinReaderStartup.</p>" },
{ LEAF("AggressiveKeepLastWhc|AggressiveKeepLast1Whc"), 1, "true", ABSOFF(aggressive_keep_last_whc), 0, uf_boolean, 0, pf_boolean,
"<p>This element controls whether to drop a reliable sample from a DDSI2E WHC before all readers have acknowledged it as soon as a later sample becomes available. It only affects DCPS data writers with a history QoS setting of KEEP_LAST with depth 1. The default setting, <i>false</i>, mimics the behaviour of the OpenSplice RT networking and is necessary to make the behaviour of wait_for_acknowledgements() consistent across the networking services.</p>" },
{ LEAF("ConservativeBuiltinReaderStartup"), 1, "false", ABSOFF(conservative_builtin_reader_startup), 0, uf_boolean, 0, pf_boolean,
"<p>This element forces all DDSI2E built-in discovery-related readers to request all historical data, instead of just one for each \"topic\". There is no indication that any of the current DDSI implementations requires changing of this setting, but it is conceivable that an implementation might track which participants have been informed of the existence of endpoints and which have not been, refusing communication with those that have \"can't\" know.</p>\n\
<p>Should it be necessary to hide DDSI2E's shared discovery behaviour, set this to <i>true</i> and Internal/BuiltinEndpointSet to <i>full</i>.</p>" },
{ LEAF("MeasureHbToAckLatency"), 1, "false", ABSOFF(meas_hb_to_ack_latency), 0, uf_boolean, 0, pf_boolean,
"<p>This element enables heartbeat-to-ack latency among DDSI2E services by prepending timestamps to Heartbeat and AckNack messages and calculating round trip times. This is non-standard behaviour. The measured latencies are quite noisy and are currently not used anywhere.</p>" },
{ LEAF("SuppressSPDPMulticast"), 1, "false", ABSOFF(suppress_spdp_multicast), 0, uf_boolean, 0, pf_boolean,
"<p>The element controls whether the mandatory multicasting of the participant discovery packets occurs. Completely disabling multicasting requires this element be set to <i>true</i>, and generally requires explicitly listing peers to ping for unicast discovery.</p>\n\
<p>See also General/AllowMulticast.</p>" },
{ LEAF("UnicastResponseToSPDPMessages"), 1, "true", ABSOFF(unicast_response_to_spdp_messages), 0, uf_boolean, 0, pf_boolean,
"<p>This element controls whether the response to a newly discovered participant is sent as a unicasted SPDP packet, instead of rescheduling the periodic multicasted one. There is no known benefit to setting this to <i>false</i>.</p>" },
{ LEAF("SynchronousDeliveryPriorityThreshold"), 1, "0", ABSOFF(synchronous_delivery_priority_threshold), 0, uf_int, 0, pf_int,
"<p>This element controls whether samples sent by a writer with QoS settings latency_budget <= SynchronousDeliveryLatencyBound and transport_priority greater than or equal to this element's value will be delivered synchronously from the \"recv\" thread, all others will be delivered asynchronously through delivery queues. This reduces latency at the expense of aggregate bandwidth.</p>" },
{ LEAF("SynchronousDeliveryLatencyBound"), 1, "inf", ABSOFF(synchronous_delivery_latency_bound), 0, uf_duration_inf, 0, pf_duration,
"<p>This element controls whether samples sent by a writer with QoS settings transport_priority >= SynchronousDeliveryPriorityThreshold and a latency_budget at most this element's value will be delivered synchronously from the \"recv\" thread, all others will be delivered asynchronously through delivery queues. This reduces latency at the expense of aggregate bandwidth.</p>" },
{ LEAF("MaxParticipants"), 1, "0", ABSOFF(max_participants), 0, uf_natint, 0, pf_int,
"<p>This elements configures the maximum number of DCPS domain participants this DDSI2E instance is willing to service. 0 is unlimited.</p>" },
{ LEAF("AccelerateRexmitBlockSize"), 1, "0", ABSOFF(accelerate_rexmit_block_size), 0, uf_uint, 0, pf_uint,
"<p>Proxy readers that are assumed to sill be retrieving historical data get this many samples retransmitted when they NACK something, even if some of these samples have sequence numbers outside the set covered by the NACK.</p>" },
{ LEAF("RetransmitMerging"), 1, "adaptive", ABSOFF(retransmit_merging), 0, uf_retransmit_merging, 0, pf_retransmit_merging,
"<p>This elements controls the addressing and timing of retransmits. Possible values are:</p>\n\
<ul><li><i>never</i>: retransmit only to the NACK-ing reader;</li>\n  \
<li><i>adaptive</i>: attempt to combine retransmits needed for reliability, but send historical (transient-local) data to the requesting reader only;</li>\n\
<li><i>always</i>: do not distinguish between different causes, always try to merge.</li></ul>\n\
<p>The default is <i>adaptive</i>. See also Internal/RetransmitMergingPeriod.</p>" },
{ LEAF("RetransmitMergingPeriod"), 1, "5 ms", ABSOFF(retransmit_merging_period), 0, uf_duration_us_1s, 0, pf_duration,
"<p>This setting determines the size of the time window in which a NACK of some sample is ignored because a retransmit of that sample has been multicasted too recently. This setting has no effect on unicasted retransmits.</p>\n\
<p>See also Internal/RetransmitMerging.</p>" },
{ LEAF_W_ATTRS("HeartbeatInterval", heartbeat_interval_attrs), 1, "100 ms", ABSOFF(const_hb_intv_sched), 0, uf_duration_inf, 0, pf_duration,
  "<p>This elemnents allows configuring the base interval for sending writer heartbeats and the bounds within it can vary.</p>" },
{ LEAF("MaxQueuedRexmitBytes"), 1, "50 kB", ABSOFF(max_queued_rexmit_bytes), 0, uf_memsize, 0, pf_memsize,
"<p>This setting limits the maximum number of bytes queued for retransmission. The default value of 0 is unlimited unless an AuxiliaryBandwidthLimit has been set, in which case it becomes NackDelay * AuxiliaryBandwidthLimit. It must be large enough to contain the largest sample that may need to be retransmitted.</p>" },
{ LEAF("MaxQueuedRexmitMessages"), 1, "200", ABSOFF(max_queued_rexmit_msgs), 0, uf_uint, 0, pf_uint,
"<p>This settings limits the maximum number of samples queued for retransmission.</p>" },
{ LEAF("LeaseDuration"), 1, "10 s", ABSOFF(lease_duration), 0, uf_duration_ms_1hr, 0, pf_duration,
"<p>This setting controls the default participant lease duration. <p>" },
{ LEAF("WriterLingerDuration"), 1, "1 s", ABSOFF(writer_linger_duration), 0, uf_duration_ms_1hr, 0, pf_duration,
"<p>This setting controls the maximum duration for which actual deletion of a reliable writer with unacknowledged data in its history will be postponed to provide proper reliable transmission.<p>" },
{ LEAF("MinimumSocketReceiveBufferSize"), 1, "default", ABSOFF(socket_min_rcvbuf_size), 0, uf_maybe_memsize, 0, pf_maybe_memsize,
"<p>This setting controls the minimum size of socket receive buffers. The operating system provides some size receive buffer upon creation of the socket, this option can be used to increase the size of the buffer beyond that initially provided by the operating system. If the buffer size cannot be increased to the specified size, an error is reported.</p>\n\
<p>The default setting is the word \"default\", which means DDSI2E will attempt to increase the buffer size to 1MB, but will silently accept a smaller buffer should that attempt fail.</p>" },
{ LEAF("MinimumSocketSendBufferSize"), 1, "64 KiB", ABSOFF(socket_min_sndbuf_size), 0, uf_memsize, 0, pf_memsize,
"<p>This setting controls the minimum size of socket send buffers. This setting can only increase the size of the send buffer, if the operating system by default creates a larger buffer, it is left unchanged.</p>" },
{ LEAF("NackDelay"), 1, "10 ms", ABSOFF(nack_delay), 0, uf_duration_ms_1hr, 0, pf_duration,
"<p>This setting controls the delay between receipt of a HEARTBEAT indicating missing samples and a NACK (ignored when the HEARTBEAT requires an answer). However, no NACK is sent if a NACK had been scheduled already for a response earlier than the delay requests: then that NACK will incorporate the latest information.</p>" },
{ LEAF("AutoReschedNackDelay"), 1, "1 s", ABSOFF(auto_resched_nack_delay), 0, uf_duration_inf, 0, pf_duration,
"<p>This setting controls the interval with which a reader will continue NACK'ing missing samples in the absence of a response from the writer, as a protection mechanism against writers incorrectly stopping the sending of HEARTBEAT messages.</p>" },
{ LEAF("PreEmptiveAckDelay"), 1, "10 ms", ABSOFF(preemptive_ack_delay), 0, uf_duration_ms_1hr, 0, pf_duration,
"<p>This setting controls the delay between the discovering a remote writer and sending a pre-emptive AckNack to discover the range of data available.</p>" },
{ LEAF("ScheduleTimeRounding"), 1, "0 ms", ABSOFF(schedule_time_rounding), 0, uf_duration_ms_1hr, 0, pf_duration,
"<p>This setting allows the timing of scheduled events to be rounded up so that more events can be handled in a single cycle of the event queue. The default is 0 and causes no rounding at all, i.e. are scheduled exactly, whereas a value of 10ms would mean that events are rounded up to the nearest 10 milliseconds.</p>" },
#ifdef DDSI_INCLUDE_BANDWIDTH_LIMITING
{ LEAF("AuxiliaryBandwidthLimit"), 1, "inf", ABSOFF(auxiliary_bandwidth_limit), 0, uf_bandwidth, 0, pf_bandwidth,
"<p>This element specifies the maximum transmit rate of auxiliary traffic not bound to a specific channel, such as discovery traffic, as well as auxiliary traffic related to a certain channel if that channel has elected to share this global AuxiliaryBandwidthLimit. Bandwidth limiting uses a leaky bucket scheme. The default value \"inf\" means DDSI2E imposes no limitation, the underlying operating system and hardware will likely limit the maimum transmit rate.</p>" },
#endif
{ LEAF("DDSI2DirectMaxThreads"), 1, "1", ABSOFF(ddsi2direct_max_threads), 0, uf_uint, 0, pf_uint,
"<p>This element sets the maximum number of extra threads for an experimental, undocumented and unsupported direct mode.</p>" },
{ LEAF("SquashParticipants"), 1, "false", ABSOFF(squash_participants), 0, uf_boolean, 0, pf_boolean,
"<p>This element controls whether DDSI2E advertises all the domain participants it serves in DDSI (when set to <i>false</i>), or rather only one domain participant (the one corresponding to the DDSI2E process; when set to <i>true</i>). In the latter case DDSI2E becomes the virtual owner of all readers and writers of all domain participants, dramatically reducing discovery traffic (a similar effect can be obtained by setting Internal/BuiltinEndpointSet to \"minimal\" but with less loss of information).</p>" },
{ LEAF("LegacyFragmentation"), 1, "false", ABSOFF(buggy_datafrag_flags_mode), 0, uf_boolean, 0, pf_boolean,
"<p>This option enables a backwards-compatible, non-compliant setting and interpretation of the control flags in fragmented data messages. To be enabled <i>only</i> when requiring interoperability between compliant and non-compliant versions of DDSI2E for large messages.</p>" },
{ LEAF("SPDPResponseMaxDelay"), 1, "0 ms", ABSOFF(spdp_response_delay_max), 0, uf_duration_ms_1s, 0, pf_duration,
"<p>Maximum pseudo-random delay in milliseconds between discovering a remote participant and responding to it.</p>" },
{ LEAF("LateAckMode"), 1, "false", ABSOFF(late_ack_mode), 0, uf_boolean, 0, pf_boolean,
"<p>Ack a sample only when it has been delivered, instead of when committed to delivering it.</p>" },
{ LEAF("ForwardAllMessages"), 1, "false", ABSOFF(forward_all_messages), 0, uf_boolean, 0, pf_boolean,
"<p>Forward all messages from a writer, rather than trying to forward each sample only once. The default of trying to forward each sample only once filters out duplicates for writers in multiple partitions under nearly all circumstances, but may still publish the odd duplicate. Note: the current implementation also can lose in contrived test cases, that publish more than 2**32 samples using a single data writer in conjunction with carefully controlled management of the writer history via cooperating local readers.</p>" },
{ LEAF("RetryOnRejectBestEffort"), 1, "false", ABSOFF(retry_on_reject_besteffort), 0, uf_boolean, 0, pf_boolean,
"<p>Whether or not to locally retry pushing a received best-effort sample into the reader caches when resource limits are reached.</p>" },
{ LEAF("GenerateKeyhash"), 1, "false", ABSOFF(generate_keyhash), 0, uf_boolean, 0, pf_boolean,
"<p>When true, include keyhashes in outgoing data for topics with keys.</p>" },
{ LEAF("MaxSampleSize"), 1, "2147483647 B", ABSOFF(max_sample_size), 0, uf_memsize, 0, pf_memsize,
"<p>This setting controls the maximum (CDR) serialised size of samples that DDSI2E will forward in either direction. Samples larger than this are discarded with a warning.</p>" },
{ LEAF("WriteBatch"), 1, "false", ABSOFF(whc_batch), 0, uf_boolean, 0, pf_boolean,
"<p>This element enables the batching of write operations. By default each write operation writes through the write cache and out onto the transport. Enabling write batching causes multiple small write operations to be aggregated within the write cache into a single larger write. This gives greater throughput at the expense of latency. Currently there is no mechanism for the write cache to automatically flush itself, so that if write batching is enabled, the application may havee to use the dds_write_flush function to ensure thta all samples are written.</p>" },
{ LEAF_W_ATTRS("LivelinessMonitoring", liveliness_monitoring_attrs), 1, "false", ABSOFF(liveliness_monitoring), 0, uf_boolean, 0, pf_boolean,
"<p>This element controls whether or not implementation should internally monitor its own liveliness. If liveliness monitoring is enabled, stack traces can be dumped automatically when some thread appears to have stopped making progress.</p>" },
{ LEAF("MonitorPort"), 1, "-1", ABSOFF(monitor_port), 0, uf_int, 0, pf_int,
"<p>This element allows configuring a service that dumps a text description of part the internal state to TCP clients. By default (-1), this is disabled; specifying 0 means a kernel-allocated port is used; a positive number is used as the TCP port number.</p>" },
{ LEAF("AssumeMulticastCapable"), 1, "", ABSOFF(assumeMulticastCapable), 0, uf_string, ff_free, pf_string,
"<p>This element controls which network interfaces are assumed to be capable of multicasting even when the interface flags returned by the operating system state it is not (this provides a workaround for some platforms). It is a comma-separated lists of patterns (with ? and * wildcards) against which the interface names are matched.</p>" },
{ LEAF("PrioritizeRetransmit"), 1, "true", ABSOFF(prioritize_retransmit), 0, uf_boolean, 0, pf_boolean,
"<p>This element controls whether retransmits are prioritized over new data, speeding up recovery.</p>" },
{ LEAF("UseMulticastIfMreqn"), 1, "0", ABSOFF(use_multicast_if_mreqn), 0, uf_int, 0, pf_int,
"<p>Do not use.</p>" },
{ LEAF("SendAsync"), 1, "false", ABSOFF(xpack_send_async), 0, uf_boolean, 0, pf_boolean,
"<p>This element controls whether the actual sending of packets occurs on the same thread that prepares them, or is done asynchronously by another thread.</p>" },
{ LEAF_W_ATTRS("RediscoveryBlacklistDuration", rediscovery_blacklist_duration_attrs), 1, "10s", ABSOFF(prune_deleted_ppant.delay), 0, uf_duration_inf, 0, pf_duration,
"<p>This element controls for how long a remote participant that was previously deleted will remain on a blacklist to prevent rediscovery, giving the software on a node time to perform any cleanup actions it needs to do. To some extent this delay is required internally by DDSI2E, but in the default configuration with the 'enforce' attribute set to false, DDSI2E will reallow rediscovery as soon as it has cleared its internal administration. Setting it to too small a value may result in the entry being pruned from the blacklist before DDSI2E is ready, it is therefore recommended to set it to at least several seconds.</p>" },
  { LEAF_W_ATTRS("MultipleReceiveThreads", multiple_recv_threads_attrs), 1, "true", ABSOFF(multiple_recv_threads), 0, uf_boolean, 0, pf_boolean,
         "<p>This element controls whether all traffic is handled by a single receive thread or whether multiple receive threads may be used to improve latency. Currently multiple receive threads are only used for connectionless transport (e.g., UDP) and ManySocketsMode not set to single (the default).</p>" },
{ MGROUP("ControlTopic", control_topic_cfgelems, control_topic_cfgattrs), 1, 0, 0, 0, 0, 0, 0, 0,
"<p>The ControlTopic element allows configured whether DDSI2E provides a special control interface via a predefined topic or not.<p>" },
{ GROUP("Test", unsupp_test_cfgelems),
"<p>Testing options.</p>" },
{ GROUP("Watermarks", unsupp_watermarks_cfgelems),
"<p>Watermarks for flow-control.</p>" },
END_MARKER
};

static const struct cfgelem sizing_cfgelems[] =
{
    { LEAF("ReceiveBufferSize"), 1, "1 MiB", ABSOFF(rbuf_size), 0, uf_memsize, 0, pf_memsize,
    "<p>This element sets the size of a single receive buffer. Many receive buffers may be needed. Their size must be greater than ReceiveBufferChunkSize by a modest amount.</p>" },
    { LEAF("ReceiveBufferChunkSize"), 1, "128 KiB", ABSOFF(rmsg_chunk_size), 0, uf_memsize, 0, pf_memsize,
    "<p>This element specifies the size of one allocation unit in the receive buffer. Must be greater than the maximum packet size by a modest amount (too large packets are dropped). Each allocation is shrunk immediately after processing a message, or freed straightaway.</p>" },
    END_MARKER
};

static const struct cfgelem discovery_ports_cfgelems[] = {
    { LEAF("Base"), 1, "7400", ABSOFF(port_base), 0, uf_port, 0, pf_int,
    "<p>This element specifies the base port number (refer to the DDSI 2.1 specification, section 9.6.1, constant PB).</p>" },
    { LEAF("DomainGain"), 1, "250", ABSOFF(port_dg), 0, uf_int, 0, pf_int,
    "<p>This element specifies the domain gain, relating domain ids to sets of port numbers (refer to the DDSI 2.1 specification, section 9.6.1, constant DG).</p>" },
    { LEAF("ParticipantGain"), 1, "2", ABSOFF(port_pg), 0, uf_int, 0, pf_int,
    "<p>This element specifies the participant gain, relating p0, articipant index to sets of port numbers (refer to the DDSI 2.1 specification, section 9.6.1, constant PG).</p>" },
    { LEAF("MulticastMetaOffset"), 1, "0", ABSOFF(port_d0), 0, uf_int, 0, pf_int,
    "<p>This element specifies the port number for multicast meta traffic (refer to the DDSI 2.1 specification, section 9.6.1, constant d0).</p>" },
    { LEAF("UnicastMetaOffset"), 1, "10", ABSOFF(port_d1), 0, uf_int, 0, pf_int,
    "<p>This element specifies the port number for unicast meta traffic (refer to the DDSI 2.1 specification, section 9.6.1, constant d1).</p>" },
    { LEAF("MulticastDataOffset"), 1, "1", ABSOFF(port_d2), 0, uf_int, 0, pf_int,
    "<p>This element specifies the port number for multicast meta traffic (refer to the DDSI 2.1 specification, section 9.6.1, constant d2).</p>" },
    { LEAF("UnicastDataOffset"), 1, "11", ABSOFF(port_d3), 0, uf_int, 0, pf_int,
    "<p>This element specifies the port number for unicast meta traffic (refer to the DDSI 2.1 specification, section 9.6.1, constant d3).</p>" },
    END_MARKER
};

static const struct cfgelem tcp_cfgelems[] = {
    { LEAF ("Enable"), 1, "default", ABSOFF (compat_tcp_enable), 0, uf_boolean_default, 0, pf_nop,
    "<p>This element enables the optional TCP transport - deprecated, use General/Transport instead.</p>" },
    { LEAF("NoDelay"), 1, "true", ABSOFF(tcp_nodelay), 0, uf_boolean, 0, pf_boolean,
    "<p>This element enables the TCP_NODELAY socket option, preventing multiple DDSI messages being sent in the same TCP request. Setting this option typically optimises latency over throughput.</p>" },
    { LEAF("Port"), 1, "-1", ABSOFF(tcp_port), 0, uf_dyn_port, 0, pf_int,
    "<p>This element specifies the TCP port number on which DDSI2E accepts connections. If the port is set it is used in entity locators, published with DDSI discovery. Dynamically allocated if zero. Disabled if -1 or not configured. If disabled other DDSI services will not be able to establish connections with the service, the service can only communicate by establishing connections to other services.</p>" },
    { LEAF("ReadTimeout"), 1, "2 s", ABSOFF(tcp_read_timeout), 0, uf_duration_ms_1hr, 0, pf_duration,
    "<p>This element specifies the timeout for blocking TCP read operations. If this timeout expires then the connection is closed.</p>" },
    { LEAF("WriteTimeout"), 1, "2 s", ABSOFF(tcp_write_timeout), 0, uf_duration_ms_1hr, 0, pf_duration,
    "<p>This element specifies the timeout for blocking TCP write operations. If this timeout expires then the connection is closed.</p>" },
    { LEAF ("AlwaysUsePeeraddrForUnicast"), 1, "false", ABSOFF (tcp_use_peeraddr_for_unicast), 0, uf_boolean, 0, pf_boolean,
    "<p>Setting this to true means the unicast addresses in SPDP packets will be ignored and the peer address from the TCP connection will be used instead. This may help work around incorrectly advertised addresses when using TCP.</p>" },
    END_MARKER
};

#ifdef DDSI_INCLUDE_SSL
static const struct cfgelem ssl_cfgelems[] = {
    { LEAF("Enable"), 1, "false", ABSOFF(ssl_enable), 0, uf_boolean, 0, pf_boolean,
    "<p>This enables SSL/TLS for TCP.</p>" },
    { LEAF("CertificateVerification"), 1, "true", ABSOFF(ssl_verify), 0, uf_boolean, 0, pf_boolean,
    "<p>If disabled this allows SSL connections to occur even if an X509 certificate fails verification.</p>" },
    { LEAF("VerifyClient"), 1, "true", ABSOFF(ssl_verify_client), 0, uf_boolean, 0, pf_boolean,
    "<p>This enables an SSL server checking the X509 certificate of a connecting client.</p>" },
    { LEAF("SelfSignedCertificates"), 1, "false", ABSOFF(ssl_self_signed), 0, uf_boolean, 0, pf_boolean,
    "<p>This enables the use of self signed X509 certificates.</p>" },
    { LEAF("KeystoreFile"), 1, "keystore", ABSOFF(ssl_keystore), 0, uf_string, ff_free, pf_string,
    "<p>The SSL/TLS key and certificate store file name. The keystore must be in PEM format.</p>" },
    { LEAF("KeyPassphrase"), 1, "secret", ABSOFF(ssl_key_pass), 0, uf_string, ff_free, pf_string,
    "<p>The SSL/TLS key pass phrase for encrypted keys.</p>" },
    { LEAF("Ciphers"), 1, "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH", ABSOFF(ssl_ciphers), 0, uf_string, ff_free, pf_string,
    "<p>The set of ciphers used by SSL/TLS</p>" },
    { LEAF("EntropyFile"), 1, "", ABSOFF(ssl_rand_file), 0, uf_string, ff_free, pf_string,
    "<p>The SSL/TLS random entropy file name.</p>" },
    { LEAF("MinimumTLSVersion"), 1, "1.3", ABSOFF(ssl_min_version), 0, uf_min_tls_version, 0, pf_min_tls_version,
    "<p>The minimum TLS version that may be negotiated, valid values are 1.2 and 1.3.</p>" },
    END_MARKER
};
#endif

static const struct cfgelem tp_cfgelems[] = {
    { LEAF("Enable"), 1, "false", ABSOFF(tp_enable), 0, uf_boolean, 0, pf_boolean,
    "<p>This element enables the optional thread pool.</p>" },
    { LEAF("Threads"), 1, "4", ABSOFF(tp_threads), 0, uf_natint, 0, pf_int,
    "<p>This elements configures the initial number of threads in the thread pool.</p>" },
    { LEAF("ThreadMax"), 1, "8", ABSOFF(tp_max_threads), 0, uf_natint, 0, pf_int,
    "<p>This elements configures the maximum number of threads in the thread pool.</p>" },
    END_MARKER
};

static const struct cfgelem discovery_peer_cfgattrs[] = {
    { ATTR("Address"), 1, NULL, RELOFF(config_peer_listelem, peer), 0, uf_ipv4, ff_free, pf_string,
    "<p>This element specifies an IP address to which discovery packets must be sent, in addition to the default multicast address (see also General/AllowMulticast). Both a hostnames and a numerical IP address is accepted; the hostname or IP address may be suffixed with :PORT to explicitly set the port to which it must be sent. Multiple Peers may be specified.</p>" },
    END_MARKER
};

static const struct cfgelem discovery_peers_group_cfgelems[] = {
    { MGROUP("Peer", NULL, discovery_peer_cfgattrs), 0, NULL, ABSOFF(peers_group), if_peer, 0, 0, 0,
    "<p>This element statically configures an addresses for discovery.</p>" },
    END_MARKER
};

static const struct cfgelem discovery_peers_cfgelems[] = {
    { MGROUP("Peer", NULL, discovery_peer_cfgattrs), 0, NULL, ABSOFF(peers), if_peer, 0, 0, 0,
    "<p>This element statically configures an addresses for discovery.</p>" },
    { GROUP("Group", discovery_peers_group_cfgelems),
    "<p>This element statically configures a fault tolerant group of addresses for discovery. Each member of the group is tried in sequence until one succeeds.</p>" },
    END_MARKER
};

static const struct cfgelem discovery_cfgelems[] = {
    { LEAF("AdvertiseBuiltinTopicWriters"), 1, "true", ABSOFF(advertise_builtin_topic_writers), 0, uf_boolean, 0, pf_boolean,
    "<p>This element controls whether or not DDSI2E advertises writers for the built-in topics from its discovery for backwards compatibility with older OpenSplice versions.</p>" },
    { LEAF("DSGracePeriod"), 1, "30 s", ABSOFF(ds_grace_period), 0, uf_duration_inf, 0, pf_duration,
    "<p>This setting controls for how long endpoints discovered via a Cloud discovery service will survive after the discovery service disappeared, allowing reconnect without loss of data when the discovery service restarts (or another instance takes over).</p>" },
    { GROUP("Peers", discovery_peers_cfgelems),
    "<p>This element statically configures addresses for discovery.</p>" },
    { LEAF("ParticipantIndex"), 1, "none", ABSOFF(participantIndex), 0, uf_participantIndex, 0, pf_participantIndex,
    "<p>This element specifies the DDSI participant index used by this instance of the DDSI2E service for discovery purposes. Only one such participant id is used, independent of the number of actual DomainParticipants on the node. It is either:</p>\n\
<ul><li><i>auto</i>: which will attempt to automatically determine an available participant index (see also Discovery/MaxAutoParticipantIndex), or</li>\n \
<li>a non-negative integer less than 120, or</li>\n\
<li><i>none</i>:, which causes it to use arbitrary port numbers for unicast sockets which entirely removes the constraints on the participant index but makes unicast discovery impossible.</li></ul>\n\
<p>The default is <i>auto</i>. The participant index is part of the port number calculation and if predictable port numbers are needed and fixing the participant index has no adverse effects, it is recommended that the second be option be used.</p>" },
{ LEAF("MaxAutoParticipantIndex"), 1, "9", ABSOFF(maxAutoParticipantIndex), 0, uf_natint, 0, pf_int,
"<p>This element specifies the maximum DDSI participant index selected by this instance of the DDSI2E service if the Discovery/ParticipantIndex is \"auto\".</p>" },
{ LEAF("SPDPMulticastAddress"), 1, "239.255.0.1", ABSOFF(spdpMulticastAddressString), 0, uf_ipv4, ff_free, pf_string,
"<p>This element specifies the multicast address that is used as the destination for the participant discovery packets. In IPv4 mode the default is the (standardised) 239.255.0.1, in IPv6 mode it becomes ff02::ffff:239.255.0.1, which is a non-standardised link-local multicast address.</p>" },
{ LEAF("SPDPInterval"), 1, "30 s", ABSOFF(spdp_interval), 0, uf_duration_ms_1hr, 0, pf_duration,
"<p>This element specifies the interval between spontaneous transmissions of participant discovery packets.</p>" },
{ LEAF("DefaultMulticastAddress"), 1, "auto", ABSOFF(defaultMulticastAddressString), 0, uf_networkAddress, 0, pf_networkAddress,
"<p>This element specifies the default multicast address for all traffic other than participant discovery packets. It defaults to Discovery/SPDPMulticastAddress.</p>" },
{ LEAF("EnableTopicDiscovery"), 1, "true", ABSOFF(do_topic_discovery), 0, uf_boolean, 0, pf_boolean,
"<p>Do not use.</p>" },
{ GROUP("Ports", discovery_ports_cfgelems),
"<p>The Ports element allows specifying various parameters related to the port numbers used for discovery. These all have default values specified by the DDSI 2.1 specification and rarely need to be changed.</p>" },
END_MARKER
};

static const struct cfgelem tracing_cfgelems[] = {
    { LEAF("EnableCategory"), 1, "", 0, 0, 0, uf_logcat, 0, pf_logcat,
    "<p>This element enables individual logging categories. These are enabled in addition to those enabled by Tracing/Verbosity. Recognised categories are:</p>\n\
<ul><li><i>fatal</i>: all fatal errors, errors causing immediate termination</li>\n\
<li><i>error</i>: failures probably impacting correctness but not necessarily causing immediate termination</li>\n\
<li><i>warning</i>: abnormal situations that will likely not impact correctness</li>\n\
<li><i>config</i>: full dump of the configuration</li>\n\
<li><i>info</i>: general informational notices</li>\n\
<li><i>discovery</i>: all discovery activity</li>\n\
<li><i>data</i>: include data content of samples in traces</li>\n\
<li><i>radmin</i>: receive buffer administration</li>\n\
<li><i>timing</i>: periodic reporting of CPU loads per thread</li>\n\
<li><i>traffic</i>: periodic reporting of total outgoing data</li>\n\
<li><i>whc</i>: tracing of writer history cache changes</li>\n\
<li><i>tcp</i>: tracing of TCP-specific activity</li>\n\
<li><i>topic</i>: tracing of topic definitions</li>\n\
<li>>i>plist</i>: tracing of discovery parameter list interpretation</li> </ul>\n\
<p>In addition, there is the keyword <i>trace</i> that enables all but <i>radmin</i>, <i>topic</i>, <i>plist</i> and <i>whc</i></p>.\n\
<p>The categorisation of tracing output is incomplete and hence most of the verbosity levels and categories are not of much use in the current release. This is an ongoing process and here we describe the target situation rather than the current situation. Currently, the most useful is <i>trace</i>.</p>" },
{ LEAF("Verbosity"), 1, "none", 0, 0, 0, uf_verbosity, 0, pf_nop,
"<p>This element enables standard groups of categories, based on a desired verbosity level. This is in addition to the categories enabled by the Tracing/EnableCategory setting. Recognised verbosity levels and the categories they map to are:</p>\n\
<ul><li><i>none</i>: no DDSI2E log</li>\n\
<li><i>severe</i>: error and fatal</li>\n\
<li><i>warning</i>: <i>severe</i> + warning</li>\n\
<li><i>info</i>: <i>warning</i> + info</li>\n\
<li><i>config</i>: <i>info</i> + config</li>\n\
<li><i>fine</i>: <i>config</i> + discovery</li>\n\
<li><i>finer</i>: <i>fine</i> + traffic and timing</li>\n\
<li><i>finest</i>: <i>finer</i> + trace</li></ul>\n\
<p>While <i>none</i> prevents any message from being written to a DDSI2 log file.</p>\n\
<p>The categorisation of tracing output is incomplete and hence most of the verbosity levels and categories are not of much use in the current release. This is an ongoing process and here we describe the target situation rather than the current situation. Currently, the most useful verbosity levels are <i>config</i>, <i>fine</i> and <i>finest</i>.</p>" },
{ LEAF("OutputFile"), 1, DDSC_PROJECT_NAME_NOSPACE_SMALL".log", ABSOFF(tracingOutputFileName), 0, uf_tracingOutputFileName, ff_free, pf_string,
"<p>This option specifies where the logging is printed to. Note that <i>stdout</i> and <i>stderr</i> are treated as special values, representing \"standard out\" and \"standard error\" respectively. No file is created unless logging categories are enabled using the Tracing/Verbosity or Tracing/EnabledCategory settings.</p>" },
{ LEAF_W_ATTRS("Timestamps", timestamp_cfgattrs), 1, "true", ABSOFF(tracingTimestamps), 0, uf_boolean, 0, pf_boolean,
"<p>This option has no effect.</p>" },
{ LEAF("AppendToFile"), 1, "false", ABSOFF(tracingAppendToFile), 0, uf_boolean, 0, pf_boolean,
"<p>This option specifies whether the output is to be appended to an existing log file. The default is to create a new log file each time, which is generally the best option if a detailed log is generated.</p>" },
{ LEAF("PacketCaptureFile"), 1, "", ABSOFF(pcap_file), 0, uf_string, ff_free, pf_string,
"<p>This option specifies the file to which received and sent packets will be logged in the \"pcap\" format suitable for analysis using common networking tools, such as WireShark. IP and UDP headers are ficitious, in particular the destination address of received packets. The TTL may be used to distinguish between sent and received packets: it is 255 for sent packets and 128 for received ones. Currently IPv4 only.</p>" },
END_MARKER
};

static const struct cfgelem sched_prio_cfgattrs[] = {
    { ATTR("priority_kind"), 1, "relative", ABSOFF(watchdog_sched_priority_class), 0, uf_sched_prio_class, 0, pf_sched_prio_class,
    "<p>This attribute specifies whether the specified Priority is a relative or absolute priority.</p>" },
    END_MARKER
};

static const struct cfgelem sched_cfgelems[] = {
    { LEAF("Class"), 1, "default", ABSOFF(watchdog_sched_class), 0, uf_sched_class, 0, pf_sched_class,
    "<p>This element specifies the thread scheduling class that will be used by the watchdog thread. The user may need the appropriate privileges from the underlying operating system to be able to assign some of the privileged scheduling classes.</p>" },
    { LEAF_W_ATTRS("Priority", sched_prio_cfgattrs), 1, "0", ABSOFF(watchdog_sched_priority), 0, uf_int32, 0, pf_int32,
    "<p>This element specifies the thread priority. Only priorities that are supported by the underlying operating system can be assigned to this element. The user may need special privileges from the underlying operating system to be able to assign some of the privileged priorities.</p>" },
    END_MARKER
};

static const struct cfgelem watchdog_cfgelems[] = {
    { GROUP("Scheduling", sched_cfgelems),
    "<p>This element specifies the type of OS scheduling class will be used by the thread that announces its liveliness periodically.</p>" },
    END_MARKER
};

static const struct cfgelem ddsi2_cfgelems[] = {
    { GROUP("General", general_cfgelems),
    "<p>The General element specifies overall DDSI2E service settings.</p>" },
#ifdef DDSI_INCLUDE_ENCRYPTION
    { GROUP("Security", security_cfgelems),
    "<p>The Security element specifies DDSI2E security profiles that can be used to encrypt traffic mapped to DDSI2E network partitions.</p>" },
#endif
#ifdef DDSI_INCLUDE_NETWORK_PARTITIONS
    { GROUP("Partitioning", partitioning_cfgelems),
    "<p>The Partitioning element specifies DDSI2E network partitions and how DCPS partition/topic combinations are mapped onto the network partitions.</p>" },
#endif
#ifdef DDSI_INCLUDE_NETWORK_CHANNELS
    { GROUP("Channels", channels_cfgelems),
    "<p>This element is used to group a set of channels. The channels are independent data paths through DDSI2E and by using separate threads and setting their priorities appropriately, chanenls can be used to map transport priorities to operating system scheduler priorities, ensuring system-wide end-to-end priority preservation.</p>" },
#endif
    { GROUP("Threads", threads_cfgelems),
    "<p>This element is used to set thread properties.</p>" },
    { GROUP("Sizing", sizing_cfgelems),
    "<p>The Sizing element specifies a variety of configuration settings dealing with expected system sizes, buffer sizes, &c.</p>" },
    { GROUP("Compatibility", compatibility_cfgelems),
    "<p>The Compatibility elements allows specifying various settings related to compatability with standards and with other DDSI implementations.</p>" },
    { GROUP("Discovery", discovery_cfgelems),
    "<p>The Discovery element allows specifying various parameters related to the discovery of peers.</p>" },
    { GROUP("Tracing", tracing_cfgelems),
    "<p>The Tracing element controls the amount and type of information that is written into the tracing log by the DDSI service. This is useful to track the DDSI service during application development.</p>" },
    { GROUP("Internal|Unsupported", unsupp_cfgelems),
    "<p>The Internal elements deal with a variety of settings that evolving and that are not necessarily fully supported. For the vast majority of the Internal settings, the functionality per-se is supported, but the right to change the way the options control the functionality is reserved. This includes renaming or moving options.</p>" },
    { GROUP("Watchdog", watchdog_cfgelems),
    "<p>This element specifies the type of OS scheduling class will be used by the thread that announces its liveliness periodically.</p>" },
    { GROUP("TCP", tcp_cfgelems),
    "<p>The TCP element allows specifying various parameters related to running DDSI over TCP.</p>" },
    { GROUP("ThreadPool", tp_cfgelems),
    "<p>The ThreadPool element allows specifying various parameters related to using a thread pool to send DDSI messages to multiple unicast addresses (TCP or UDP).</p>" },
#ifdef DDSI_INCLUDE_SSL
    { GROUP("SSL", ssl_cfgelems),
    "<p>The SSL element allows specifying various parameters related to using SSL/TLS for DDSI over TCP.</p>" },
#endif
    END_MARKER
};

/* Note: using 2e-1 instead of 0.2 to avoid use of the decimal
separator, which is locale dependent. */
static const struct cfgelem lease_expiry_time_cfgattrs[] = {
    { ATTR("update_factor"), 1, "2e-1", ABSOFF(servicelease_update_factor), 0, uf_float, 0, pf_float, NULL },
    END_MARKER
};

static const struct cfgelem lease_cfgelems[] = {
    { LEAF_W_ATTRS("ExpiryTime", lease_expiry_time_cfgattrs), 1, "10", ABSOFF(servicelease_expiry_time), 0, uf_float, 0, pf_float, NULL },
    END_MARKER
};


static const struct cfgelem domain_cfgelems[] = {
    { GROUP("Lease", lease_cfgelems), NULL },
    { LEAF("Id"), 1, "any", ABSOFF(domainId), 0, uf_domainId, 0, pf_domainId, NULL },
    WILDCARD,
    END_MARKER
};

static const struct cfgelem durability_cfgelems[] = {
    { LEAF("Encoding"), 1, "CDR_CLIENT", ABSOFF(durability_cdr), 0, uf_durability_cdr, 0, pf_durability_cdr, NULL },
    END_MARKER
};

static const struct cfgelem root_cfgelems[] = {
    { "Domain", domain_cfgelems, NULL, NODATA, NULL },
    { "DDSI2E|DDSI2", ddsi2_cfgelems, NULL, NODATA,
    "<p>DDSI2 settings ...</p>" },
    { "Durability", durability_cfgelems, NULL, NODATA, NULL },
    { "Lease", lease_cfgelems, NULL, NODATA, NULL },
    END_MARKER
};

static const struct cfgelem cyclonedds_root_cfgelems[] =
{
    { DDSC_PROJECT_NAME_NOSPACE, root_cfgelems, NULL, NODATA, NULL },
    END_MARKER
};

static const struct cfgelem root_cfgelem =
{ "root", cyclonedds_root_cfgelems, NULL, NODATA, NULL };


#undef ATTR
#undef GROUP
#undef LEAF_W_ATTRS
#undef LEAF
#undef WILDCARD
#undef END_MARKER
#undef NODATA
#undef RELOFF
#undef ABSOFF
#undef CO

struct config config;
struct ddsi_plugin ddsi_plugin;

static const struct unit unittab_duration[] = {
    { "ns", 1 },
    { "us", 1000 },
    { "ms", T_MILLISECOND },
    { "s", T_SECOND },
    { "min", 60 * T_SECOND },
    { "hr", 3600 * T_SECOND },
    { "day", 24 * 3600 * T_SECOND },
    { NULL, 0 }
};

/* note: order affects whether printed as KiB or kB, for consistency
with bandwidths and pedanticness, favour KiB. */
static const struct unit unittab_memsize[] = {
    { "B", 1 },
    { "KiB", 1024 },
    { "kB", 1024 },
    { "MiB", 1048576 },
    { "MB", 1048576 },
    { "GiB", 1073741824 },
    { "GB", 1073741824 },
    { NULL, 0 }
};

#ifdef DDSI_INCLUDE_BANDWIDTH_LIMITING
static const struct unit unittab_bandwidth_bps[] = {
    { "b/s", 1 },{ "bps", 1 },
    { "Kib/s", 1024 },{ "Kibps", 1024 },
    { "kb/s", 1000 },{ "kbps", 1000 },
    { "Mib/s", 1048576 },{ "Mibps", 1000 },
    { "Mb/s", 1000000 },{ "Mbps", 1000000 },
    { "Gib/s", 1073741824 },{ "Gibps", 1073741824 },
    { "Gb/s", 1000000000 },{ "Gbps", 1000000000 },
    { "B/s", 8 },{ "Bps", 8 },
    { "KiB/s", 8 * 1024 },{ "KiBps", 8 * 1024 },
    { "kB/s", 8 * 1000 },{ "kBps", 8 * 1000 },
    { "MiB/s", 8 * 1048576 },{ "MiBps", 8 * 1048576 },
    { "MB/s", 8 * 1000000 },{ "MBps", 8 * 1000000 },
    { "GiB/s", 8 * (int64_t) 1073741824 },{ "GiBps", 8 * (int64_t) 1073741824 },
    { "GB/s", 8 * (int64_t) 1000000000 },{ "GBps", 8 * (int64_t) 1000000000 },
    { NULL, 0 }
};

static const struct unit unittab_bandwidth_Bps[] = {
    { "B/s", 1 },{ "Bps", 1 },
    { "KiB/s", 1024 },{ "KiBps", 1024 },
    { "kB/s", 1000 },{ "kBps", 1000 },
    { "MiB/s", 1048576 },{ "MiBps", 1048576 },
    { "MB/s", 1000000 },{ "MBps", 1000000 },
    { "GiB/s", 1073741824 },{ "GiBps", 1073741824 },
    { "GB/s", 1000000000 },{ "GBps", 1000000000 },
    { NULL, 0 }
};
#endif

static void cfgst_push(struct cfgst *cfgst, int isattr, const struct cfgelem *elem, void *parent)
{
    assert(cfgst->path_depth < MAX_PATH_DEPTH);
    assert(isattr == 0 || isattr == 1);
    cfgst->isattr[cfgst->path_depth] = isattr;
    cfgst->path[cfgst->path_depth] = elem;
    cfgst->parent[cfgst->path_depth] = parent;
    cfgst->path_depth++;
}

static void cfgst_pop(struct cfgst *cfgst)
{
    assert(cfgst->path_depth > 0);
    cfgst->path_depth--;
}

static const struct cfgelem *cfgst_tos(const struct cfgst *cfgst)
{
    assert(cfgst->path_depth > 0);
    return cfgst->path[cfgst->path_depth - 1];
}

static void *cfgst_parent(const struct cfgst *cfgst)
{
    assert(cfgst->path_depth > 0);
    return cfgst->parent[cfgst->path_depth - 1];
}

struct cfg_note_buf {
    size_t bufpos;
    size_t bufsize;
    char *buf;
};

static size_t cfg_note_vsnprintf(struct cfg_note_buf *bb, const char *fmt, va_list ap)
{
    int x;
    x = os_vsnprintf(bb->buf + bb->bufpos, bb->bufsize - bb->bufpos, fmt, ap);
    if ( x >= 0 && (size_t) x >= bb->bufsize - bb->bufpos ) {
        size_t nbufsize = ((bb->bufsize + (size_t) x + 1) + 1023) & (size_t) (-1024);
        char *nbuf = os_realloc(bb->buf, nbufsize);
        bb->buf = nbuf;
        bb->bufsize = nbufsize;
        return nbufsize;
    }
    if ( x < 0 )
        DDS_FATAL("cfg_note_vsnprintf: os_vsnprintf failed\n");
    else
        bb->bufpos += (size_t) x;
    return 0;
}

static void cfg_note_snprintf(struct cfg_note_buf *bb, const char *fmt, ...)
{
    /* The reason the 2nd call to os_vsnprintf is here and not inside
    cfg_note_vsnprintf is because I somehow doubt that all platforms
    implement va_copy() */
    va_list ap;
    size_t r;
    va_start(ap, fmt);
    r = cfg_note_vsnprintf(bb, fmt, ap);
    va_end(ap);
    if ( r > 0 ) {
        int s;
        va_start(ap, fmt);
        s = os_vsnprintf(bb->buf + bb->bufpos, bb->bufsize - bb->bufpos, fmt, ap);
        if ( s < 0 || (size_t) s >= bb->bufsize - bb->bufpos )
            DDS_FATAL("cfg_note_snprintf: os_vsnprintf failed\n");
        va_end(ap);
        bb->bufpos += (size_t) s;
    }
}

static size_t cfg_note(struct cfgst *cfgst, uint32_t cat, size_t bsz, const char *fmt, va_list ap)
{
    /* Have to snprintf our way to a single string so we can OS_REPORT
    as well as nn_log.  Otherwise configuration errors will be lost
    completely on platforms where stderr doesn't actually work for
    outputting error messages (this includes Windows because of the
    way "ospl start" does its thing). */
    struct cfg_note_buf bb;
    int i, sidx;
    size_t r;

    if (cat & DDS_LC_ERROR) {
        cfgst->error = 1;
    }

    bb.bufpos = 0;
    bb.bufsize = (bsz == 0) ? 1024 : bsz;
    if ( (bb.buf = os_malloc(bb.bufsize)) == NULL )
        DDS_FATAL("cfg_note: out of memory\n");

    cfg_note_snprintf(&bb, "config: ");

    /* Path to element/attribute causing the error. Have to stop once an
    attribute is reached: a NULL marker may have been pushed onto the
    stack afterward in the default handling. */
    sidx = 0;
    while ( sidx < cfgst->path_depth && cfgst->path[sidx]->name == NULL )
        sidx++;
    for ( i = sidx; i < cfgst->path_depth && (i == sidx || !cfgst->isattr[i - 1]); i++ ) {
        if ( cfgst->path[i] == NULL ) {
            assert(i > sidx);
            cfg_note_snprintf(&bb, "/#text");
        } else if ( cfgst->isattr[i] ) {
            cfg_note_snprintf(&bb, "[@%s]", cfgst->path[i]->name);
        } else {
            const char *p = strchr(cfgst->path[i]->name, '|');
            int n = p ? (int) (p - cfgst->path[i]->name) : (int) strlen(cfgst->path[i]->name);
            cfg_note_snprintf(&bb, "%s%*.*s", (i == sidx) ? "" : "/", n, n, cfgst->path[i]->name);
        }
    }

    cfg_note_snprintf(&bb, ": ");
    if ( (r = cfg_note_vsnprintf(&bb, fmt, ap)) > 0 ) {
        /* Can't reset ap ... and va_copy isn't widely available - so
        instead abort and hope the caller tries again with a larger
        initial buffer */
        os_free(bb.buf);
        return r;
    }

    switch ( cat ) {
        case DDS_LC_CONFIG:
            DDS_LOG(cat, "%s\n", bb.buf);
            break;
        case DDS_LC_WARNING:
            DDS_WARNING("%s\n", bb.buf);
            break;
        case DDS_LC_ERROR:
            DDS_ERROR("%s\n", bb.buf);
            break;
        default:
            DDS_FATAL("cfg_note unhandled category %u for message %s\n", (unsigned) cat, bb.buf);
            break;
    }

    os_free(bb.buf);
    return 0;
}

#if WARN_DEPRECATED_ALIAS || WARN_DEPRECATED_UNIT
static void cfg_warning(struct cfgst *cfgst, const char *fmt, ...)
{
    va_list ap;
    size_t bsz = 0;
    do {
        va_start(ap, fmt);
        bsz = cfg_note(cfgst, DDS_LC_WARNING, bsz, fmt, ap);
        va_end(ap);
    } while ( bsz > 0 );
}
#endif

static int cfg_error(struct cfgst *cfgst, const char *fmt, ...)
{
    va_list ap;
    size_t bsz = 0;
    do {
        va_start(ap, fmt);
        bsz = cfg_note(cfgst, DDS_LC_ERROR, bsz, fmt, ap);
        va_end(ap);
    } while ( bsz > 0 );
    return 0;
}

static int cfg_log(struct cfgst *cfgst, const char *fmt, ...)
{
    va_list ap;
    size_t bsz = 0;
    do {
        va_start(ap, fmt);
        bsz = cfg_note(cfgst, DDS_LC_CONFIG, bsz, fmt, ap);
        va_end(ap);
    } while ( bsz > 0 );
    return 0;
}

static int list_index(const char *list[], const char *elem)
{
    int i;
    for ( i = 0; list[i] != NULL; i++ ) {
        if ( os_strcasecmp(list[i], elem) == 0 )
            return i;
    }
    return -1;
}

static int64_t lookup_multiplier(struct cfgst *cfgst, const struct unit *unittab, const char *value, int unit_pos, int value_is_zero, int64_t def_mult, int err_on_unrecognised)
{
    assert(0 <= unit_pos && (size_t) unit_pos <= strlen(value));
    while ( value[unit_pos] == ' ' )
        unit_pos++;
    if ( value[unit_pos] == 0 ) {
        if ( value_is_zero ) {
            /* No matter what unit, 0 remains just that.  For convenience,
            always allow 0 to be specified without a unit */
            return 1;
        } else if ( def_mult == 0 && err_on_unrecognised ) {
            cfg_error(cfgst, "%s: unit is required", value);
            return 0;
        } else {
#if WARN_DEPRECATED_UNIT
            cfg_warning(cfgst, "%s: use of default unit is deprecated", value);
#endif
            return def_mult;
        }
    } else {
        int i;
        for ( i = 0; unittab[i].name != NULL; i++ ) {
            if ( strcmp(unittab[i].name, value + unit_pos) == 0 )
                return unittab[i].multiplier;
        }
        if ( err_on_unrecognised )
            cfg_error(cfgst, "%s: unrecognised unit", value + unit_pos);
        return 0;
    }
}

static void *cfg_address(UNUSED_ARG(struct cfgst *cfgst), void *parent, struct cfgelem const * const cfgelem)
{
    assert(cfgelem->multiplicity == 1);
    return (char *) parent + cfgelem->elem_offset;
}

static void *cfg_deref_address(UNUSED_ARG(struct cfgst *cfgst), void *parent, struct cfgelem const * const cfgelem)
{
    assert(cfgelem->multiplicity != 1);
    return *((void **) ((char *) parent + cfgelem->elem_offset));
}

static void *if_common(UNUSED_ARG(struct cfgst *cfgst), void *parent, struct cfgelem const * const cfgelem, unsigned size)
{
    struct config_listelem **current = (struct config_listelem **) ((char *) parent + cfgelem->elem_offset);
    struct config_listelem *new = os_malloc(size);
    new->next = *current;
    *current = new;
    return new;
}

static int if_thread_properties(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem)
{
    struct config_thread_properties_listelem *new = if_common(cfgst, parent, cfgelem, sizeof(*new));
    if ( new == NULL )
        return -1;
    new->name = NULL;
    return 0;
}

#ifdef DDSI_INCLUDE_NETWORK_CHANNELS
static int if_channel(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem)
{
    struct config_channel_listelem *new = if_common(cfgst, parent, cfgelem, sizeof(*new));
    if ( new == NULL )
        return -1;
    new->name = NULL;
    new->channel_reader_ts = NULL;
    new->dqueue = NULL;
    new->queueId = 0;
    new->evq = NULL;
    new->transmit_conn = NULL;
    return 0;
}
#endif /* DDSI_INCLUDE_NETWORK_CHANNELS */

#ifdef DDSI_INCLUDE_ENCRYPTION
static int if_security_profile(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem)
{
    if ( if_common(cfgst, parent, cfgelem, sizeof(struct config_securityprofile_listelem)) == NULL )
        return -1;
    return 0;
}
#endif /* DDSI_INCLUDE_ENCRYPTION */

#ifdef DDSI_INCLUDE_NETWORK_PARTITIONS
static int if_network_partition(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem)
{
    struct config_networkpartition_listelem *new = if_common(cfgst, parent, cfgelem, sizeof(*new));
    if ( new == NULL )
        return -1;
    new->address_string = NULL;
#ifdef DDSI_INCLUDE_ENCRYPTION
    new->profileName = NULL;
    new->securityProfile = NULL;
#endif /* DDSI_INCLUDE_ENCRYPTION */
    return 0;
}

static int if_ignored_partition(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem)
{
    if ( if_common(cfgst, parent, cfgelem, sizeof(struct config_ignoredpartition_listelem)) == NULL )
        return -1;
    return 0;
}

static int if_partition_mapping(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem)
{
    if ( if_common(cfgst, parent, cfgelem, sizeof(struct config_partitionmapping_listelem)) == NULL )
        return -1;
    return 0;
}
#endif /* DDSI_INCLUDE_NETWORK_PARTITIONS */

static int if_peer(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem)
{
    struct config_peer_listelem *new = if_common(cfgst, parent, cfgelem, sizeof(struct config_peer_listelem));
    if ( new == NULL )
        return -1;
    new->peer = NULL;
    return 0;
}

static void ff_free(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem)
{
    void **elem = cfg_address(cfgst, parent, cfgelem);
    os_free(*elem);
}

static int uf_boolean(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, UNUSED_ARG(int first), const char *value)
{
    static const char *vs[] = { "false", "true", NULL };
    int *elem = cfg_address(cfgst, parent, cfgelem);
    int idx = list_index(vs, value);
    if ( idx < 0 )
        return cfg_error(cfgst, "'%s': undefined value", value);
    else {
        *elem = idx;
        return 1;
    }
}

static int uf_negated_boolean(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, int first, const char *value)
{
    if ( !uf_boolean(cfgst, parent, cfgelem, first, value) )
        return 0;
    else {
        int *elem = cfg_address(cfgst, parent, cfgelem);
        *elem = !*elem;
        return 1;
    }
}

static int uf_boolean_default (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, UNUSED_ARG (int first), const char *value)
{
  static const char *vs[] = { "default", "false", "true", NULL };
  static const enum boolean_default ms[] = {
    BOOLDEF_DEFAULT, BOOLDEF_FALSE, BOOLDEF_TRUE, 0,
  };
  enum boolean_default *elem = cfg_address (cfgst, parent, cfgelem);
  int idx = list_index (vs, value);
  assert (sizeof (vs) / sizeof (*vs) == sizeof (ms) / sizeof (*ms));
  if (idx < 0)
    return cfg_error (cfgst, "'%s': undefined value", value);
  *elem = ms[idx];
  return 1;
}

#if 0
static void pf_boolean_default (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, int is_default)
{
  enum besmode *p = cfg_address (cfgst, parent, cfgelem);
  const char *str = "INVALID";
  switch (*p)
  {
    case BOOLDEF_DEFAULT: str = "default"; break;
    case BOOLDEF_FALSE: str = "false"; break;
    case BOOLDEF_TRUE: str = "true"; break;
  }
  cfg_log (cfgst, "%s%s", str, is_default ? " [def]" : "");
}
#endif

static int uf_logcat(struct cfgst *cfgst, UNUSED_ARG(void *parent), UNUSED_ARG(struct cfgelem const * const cfgelem), UNUSED_ARG(int first), const char *value)
{
    static const char **vs = logcat_names;
    static const uint32_t *lc = logcat_codes;
    char *copy = os_strdup(value), *cursor = copy, *tok;
    while ( (tok = os_strsep(&cursor, ",")) != NULL ) {
        int idx = list_index(vs, tok);
        if ( idx < 0 ) {
            int ret = cfg_error(cfgst, "'%s' in '%s' undefined", tok, value);
            os_free(copy);
            return ret;
        }
        enabled_logcats |= lc[idx];
    }
    os_free(copy);
    return 1;
}

static int uf_verbosity(struct cfgst *cfgst, UNUSED_ARG(void *parent), UNUSED_ARG(struct cfgelem const * const cfgelem), UNUSED_ARG(int first), const char *value)
{
    static const char *vs[] = {
        "finest", "finer", "fine", "config", "info", "warning", "severe", "none", NULL
    };
    static const uint32_t lc[] = {
        DDS_LC_ALL, DDS_LC_TRAFFIC | DDS_LC_TIMING, DDS_LC_DISCOVERY | DDS_LC_THROTTLE, DDS_LC_CONFIG, DDS_LC_INFO, DDS_LC_WARNING, DDS_LC_ERROR | DDS_LC_FATAL, 0, 0
    };
    int idx = list_index(vs, value);
    assert(sizeof(vs) / sizeof(*vs) == sizeof(lc) / sizeof(*lc));
    if ( idx < 0 )
        return cfg_error(cfgst, "'%s': undefined value", value);
    else {
        int i;
        for ( i = (int) (sizeof(vs) / sizeof(*vs)) - 1; i >= idx; i-- )
            enabled_logcats |= lc[i];
        return 1;
    }
}

static int uf_besmode(struct cfgst *cfgst, UNUSED_ARG(void *parent), UNUSED_ARG(struct cfgelem const * const cfgelem), UNUSED_ARG(int first), const char *value)
{
    static const char *vs[] = {
        "full", "writers", "minimal", NULL
    };
    static const enum besmode ms[] = {
        BESMODE_FULL, BESMODE_WRITERS, BESMODE_MINIMAL, 0,
    };
    int idx = list_index(vs, value);
    enum besmode *elem = cfg_address(cfgst, parent, cfgelem);
    assert(sizeof(vs) / sizeof(*vs) == sizeof(ms) / sizeof(*ms));
    if ( idx < 0 )
        return cfg_error(cfgst, "'%s': undefined value", value);
    *elem = ms[idx];
    return 1;
}

static void pf_besmode(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, int is_default)
{
    enum besmode *p = cfg_address(cfgst, parent, cfgelem);
    const char *str = "INVALID";
    switch ( *p ) {
        case BESMODE_FULL: str = "full"; break;
        case BESMODE_WRITERS: str = "writers"; break;
        case BESMODE_MINIMAL: str = "minimal"; break;
    }
    cfg_log(cfgst, "%s%s", str, is_default ? " [def]" : "");
}

#ifdef DDSI_INCLUDE_SSL
static int uf_min_tls_version(struct cfgst *cfgst, UNUSED_ARG(void *parent), UNUSED_ARG(struct cfgelem const * const cfgelem), UNUSED_ARG(int first), const char *value)
{
  static const char *vs[] = {
    "1.2", "1.3", NULL
  };
  static const struct ssl_min_version ms[] = {
    {1,2}, {1,3}, {0,0}
  };
  int idx = list_index(vs, value);
  struct ssl_min_version *elem = cfg_address(cfgst, parent, cfgelem);
  assert(sizeof(vs) / sizeof(*vs) == sizeof(ms) / sizeof(*ms));
  if ( idx < 0 )
    return cfg_error(cfgst, "'%s': undefined value", value);
  *elem = ms[idx];
  return 1;
}

static void pf_min_tls_version(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, int is_default)
{
  struct ssl_min_version *p = cfg_address(cfgst, parent, cfgelem);
  cfg_log(cfgst, "%d.%d%s", p->major, p->minor, is_default ? " [def]" : "");
}
#endif

static int uf_durability_cdr(struct cfgst *cfgst, UNUSED_ARG(void *parent), UNUSED_ARG(struct cfgelem const * const cfgelem), UNUSED_ARG(int first), const char *value)
{
    static const char *vs[] = { "cdr_le", "cdr_be", "cdr_server", "cdr_client", NULL };
    static const enum durability_cdr ms[] = { DUR_CDR_LE, DUR_CDR_BE, DUR_CDR_SERVER, DUR_CDR_CLIENT, 0 };
    int idx = list_index(vs, value);
    enum durability_cdr * elem = cfg_address(cfgst, parent, cfgelem);
    if ( idx < 0 ) {
        return cfg_error(cfgst, "'%s': undefined value", value);
    }
    *elem = ms[idx];
    return 1;
}

static void pf_durability_cdr(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, int is_default)
{
    enum durability_cdr *p = cfg_address(cfgst, parent, cfgelem);
    const char *str = "INVALID";
    switch ( *p ) {
        case DUR_CDR_LE: str = "cdr_le"; break;
        case DUR_CDR_BE: str = "cdr_be"; break;
        case DUR_CDR_SERVER: str = "cdr_server"; break;
        case DUR_CDR_CLIENT: str = "cdr_client"; break;
    }
    cfg_log(cfgst, "%s%s", str, is_default ? " [def]" : "");
}


static int uf_retransmit_merging(struct cfgst *cfgst, UNUSED_ARG(void *parent), UNUSED_ARG(struct cfgelem const * const cfgelem), UNUSED_ARG(int first), const char *value)
{
    static const char *vs[] = {
        "never", "adaptive", "always", NULL
    };
    static const enum retransmit_merging ms[] = {
        REXMIT_MERGE_NEVER, REXMIT_MERGE_ADAPTIVE, REXMIT_MERGE_ALWAYS, 0,
    };
    int idx = list_index(vs, value);
    enum retransmit_merging *elem = cfg_address(cfgst, parent, cfgelem);
    assert(sizeof(vs) / sizeof(*vs) == sizeof(ms) / sizeof(*ms));
    if ( idx < 0 )
        return cfg_error(cfgst, "'%s': undefined value", value);
    *elem = ms[idx];
    return 1;
}

static void pf_retransmit_merging(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, int is_default)
{
    enum retransmit_merging *p = cfg_address(cfgst, parent, cfgelem);
    const char *str = "INVALID";
    switch ( *p ) {
        case REXMIT_MERGE_NEVER: str = "never"; break;
        case REXMIT_MERGE_ADAPTIVE: str = "adaptive"; break;
        case REXMIT_MERGE_ALWAYS: str = "always"; break;
    }
    cfg_log(cfgst, "%s%s", str, is_default ? " [def]" : "");
}

static int uf_string(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, UNUSED_ARG(int first), const char *value)
{
    char **elem = cfg_address(cfgst, parent, cfgelem);
    *elem = os_strdup(value);
    return 1;
}

OS_WARNING_MSVC_OFF(4996);
static int uf_natint64_unit(struct cfgst *cfgst, int64_t *elem, const char *value, const struct unit *unittab, int64_t def_mult, int64_t max)
{
    int pos;
    double v_dbl;
    int64_t v_int;
    int64_t mult;
    /* try convert as integer + optional unit; if that fails, try
    floating point + optional unit (round, not truncate, to integer) */
    if ( *value == 0 ) {
        *elem = 0; /* some static analyzers don't "get it" */
        return cfg_error(cfgst, "%s: empty string is not a valid value", value);
    } else if ( sscanf(value, "%lld%n", (long long int *) &v_int, &pos) == 1 && (mult = lookup_multiplier(cfgst, unittab, value, pos, v_int == 0, def_mult, 0)) != 0 ) {
        assert(mult > 0);
        if ( v_int < 0 || v_int > max / mult )
            return cfg_error(cfgst, "%s: value out of range", value);
        *elem = mult * v_int;
        return 1;
    } else if ( sscanf(value, "%lf%n", &v_dbl, &pos) == 1 && (mult = lookup_multiplier(cfgst, unittab, value, pos, v_dbl == 0, def_mult, 1)) != 0 ) {
        double dmult = (double) mult;
        assert(dmult > 0);
        if ( v_dbl < 0 || (int64_t) (v_dbl * dmult + 0.5) > max )
            return cfg_error(cfgst, "%s: value out of range", value);
        *elem = (int64_t) (v_dbl * dmult + 0.5);
        return 1;
    } else {
        *elem = 0; /* some static analyzers don't "get it" */
        return cfg_error(cfgst, "%s: invalid value", value);
    }
}
OS_WARNING_MSVC_ON(4996);

#ifdef DDSI_INCLUDE_BANDWIDTH_LIMITING
static int uf_bandwidth(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, UNUSED_ARG(int first), const char *value)
{
    int64_t bandwidth_bps = 0;
    if ( strncmp(value, "inf", 3) == 0 ) {
        /* special case: inf needs no unit */
        int *elem = cfg_address(cfgst, parent, cfgelem);
        if ( strspn(value + 3, " ") != strlen(value + 3) &&
            lookup_multiplier(cfgst, unittab_bandwidth_bps, value, 3, 1, 8, 1) == 0 )
            return 0;
        *elem = 0;
        return 1;
    } else if ( !uf_natint64_unit(cfgst, &bandwidth_bps, value, unittab_bandwidth_bps, 8, INT64_MAX) ) {
        return 0;
    } else if ( bandwidth_bps / 8 > INT_MAX ) {
        return cfg_error(cfgst, "%s: value out of range", value);
    } else {
        uint32_t *elem = cfg_address(cfgst, parent, cfgelem);
        *elem = (uint32_t) (bandwidth_bps / 8);
        return 1;
    }
}
#endif

static int uf_memsize(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, UNUSED_ARG(int first), const char *value)
{
    int64_t size = 0;
    if ( !uf_natint64_unit(cfgst, &size, value, unittab_memsize, 1, INT32_MAX) )
        return 0;
    else {
        uint32_t *elem = cfg_address(cfgst, parent, cfgelem);
        *elem = (uint32_t) size;
        return 1;
    }
}

#ifdef DDSI_INCLUDE_ENCRYPTION
static int uf_cipher(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, UNUSED_ARG(int first), const char *value)
{
    if ( q_security_plugin.cipher_type_from_string ) {
        q_cipherType *elem = cfg_address(cfgst, parent, cfgelem);
        if ( (q_security_plugin.cipher_type_from_string) (value, elem) ) {
            return 1;
        } else {
            return cfg_error(cfgst, "%s: undefined value", value);
        }
    }
    return 1;
}
#endif /* DDSI_INCLUDE_ENCRYPTION */


static int uf_tracingOutputFileName(struct cfgst *cfgst, UNUSED_ARG(void *parent), UNUSED_ARG(struct cfgelem const * const cfgelem), UNUSED_ARG(int first), const char *value)
{
    struct config *cfg = cfgst->cfg;
    {
        cfg->tracingOutputFileName = os_strdup(value);
    }
    return 1;
}

static int uf_ipv4(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, int first, const char *value)
{
    /* Not actually doing any checking yet */
    return uf_string(cfgst, parent, cfgelem, first, value);
}

static int uf_networkAddress(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, int first, const char *value)
{
    if ( os_strcasecmp(value, "auto") != 0 )
        return uf_ipv4(cfgst, parent, cfgelem, first, value);
    else {
        char **elem = cfg_address(cfgst, parent, cfgelem);
        *elem = NULL;
        return 1;
    }
}

static void ff_networkAddresses(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem)
{
    char ***elem = cfg_address(cfgst, parent, cfgelem);
    int i;
    for ( i = 0; (*elem)[i]; i++ )
        os_free((*elem)[i]);
    os_free(*elem);
}

static int uf_networkAddresses_simple(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, UNUSED_ARG(int first), const char *value)
{
    char ***elem = cfg_address(cfgst, parent, cfgelem);
    if ( (*elem = os_malloc(2 * sizeof(char *))) == NULL )
        return cfg_error(cfgst, "out of memory");
    if ( ((*elem)[0] = os_strdup(value)) == NULL ) {
        os_free(*elem);
        *elem = NULL;
        return cfg_error(cfgst, "out of memory");
    }
    (*elem)[1] = NULL;
    return 1;
}

static int uf_networkAddresses(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, int first, const char *value)
{
    /* Check for keywords first */
    {
        static const char *keywords[] = { "all", "any", "none" };
        int i;
        for ( i = 0; i < (int) (sizeof(keywords) / sizeof(*keywords)); i++ ) {
            if ( os_strcasecmp(value, keywords[i]) == 0 )
                return uf_networkAddresses_simple(cfgst, parent, cfgelem, first, keywords[i]);
        }
    }

    /* If not keyword, then comma-separated list of addresses */
    {
        char ***elem = cfg_address(cfgst, parent, cfgelem);
        char *copy;
        unsigned count;

        /* First count how many addresses we have - but do it stupidly by
        counting commas and adding one (so two commas in a row are both
        counted) */
        {
            const char *scan = value;
            count = 1;
            while ( *scan )
                count += (*scan++ == ',');
        }

        copy = os_strdup(value);

        /* Allocate an array of address strings (which may be oversized a
        bit because of the counting of the commas) */
        *elem = os_malloc((count + 1) * sizeof(char *));

        {
            char *cursor = copy, *tok;
            unsigned idx = 0;
            while ( (tok = os_strsep(&cursor, ",")) != NULL ) {
                assert(idx < count);
                (*elem)[idx] = os_strdup(tok);
                idx++;
            }
            (*elem)[idx] = NULL;
        }
        os_free(copy);
    }
    return 1;
}

static int uf_allow_multicast(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, UNUSED_ARG(int first), const char *value)
{
#ifdef DDSI_INCLUDE_SSM
    static const char *vs[] = { "false", "spdp", "asm", "ssm", "true", NULL };
    static const unsigned bs[] = { AMC_FALSE, AMC_SPDP, AMC_ASM, AMC_SSM, AMC_TRUE };
#else
    static const char *vs[] = { "false", "spdp", "asm", "true", NULL };
    static const unsigned bs[] = { AMC_FALSE, AMC_SPDP, AMC_ASM, AMC_TRUE };
#endif
    char *copy = os_strdup(value), *cursor = copy, *tok;
    unsigned *elem = cfg_address(cfgst, parent, cfgelem);
    if ( copy == NULL )
        return cfg_error(cfgst, "out of memory");
    *elem = 0;
    while ( (tok = os_strsep(&cursor, ",")) != NULL ) {
        int idx = list_index(vs, tok);
        if ( idx < 0 ) {
            int ret = cfg_error(cfgst, "'%s' in '%s' undefined", tok, value);
            os_free(copy);
            return ret;
        }
        *elem |= bs[idx];
    }
    os_free(copy);
    return 1;
}

static void pf_allow_multicast(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, int is_default)
{
    unsigned *p = cfg_address(cfgst, parent, cfgelem);
    const char *str = "INVALID";
    switch ( *p ) {
        case AMC_FALSE: str = "false"; break;
        case AMC_SPDP: str = "spdp"; break;
        case AMC_ASM: str = "asm"; break;
#ifdef DDSI_INCLUDE_SSM
        case AMC_SSM: str = "ssm"; break;
        case (AMC_SPDP | AMC_ASM): str = "spdp,asm"; break;
        case (AMC_SPDP | AMC_SSM): str = "spdp,ssm"; break;
#endif
        case AMC_TRUE: str = "true"; break;
    }
    cfg_log(cfgst, "%s%s", str, is_default ? " [def]" : "");
}

static int uf_sched_prio_class(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, UNUSED_ARG(int first), const char *value)
{
    int ret;
    q__schedPrioClass *prio;

    assert(value != NULL);

    prio = cfg_address(cfgst, parent, cfgelem);

    if ( os_strcasecmp(value, "relative") == 0 ) {
        *prio = Q__SCHED_PRIO_RELATIVE;
        ret = 1;
    } else if ( os_strcasecmp(value, "absolute") == 0 ) {
        *prio = Q__SCHED_PRIO_ABSOLUTE;
        ret = 1;
    } else {
        ret = cfg_error(cfgst, "'%s': undefined value", value);
    }

    return ret;
}

static void pf_sched_prio_class(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, int is_default)
{
    char *str;
    q__schedPrioClass *prio = cfg_address(cfgst, parent, cfgelem);

    if ( *prio == Q__SCHED_PRIO_RELATIVE ) {
        str = "relative";
    } else if ( *prio == Q__SCHED_PRIO_ABSOLUTE ) {
        str = "absolute";
    } else {
        str = "INVALID";
    }

    cfg_log(cfgst, "%s%s", str, is_default ? " [def]" : "");
}

static int uf_sched_class(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, UNUSED_ARG(int first), const char *value)
{
    static const char *vs[] = { "realtime", "timeshare", "default" };
    static const os_schedClass ms[] = { OS_SCHED_REALTIME, OS_SCHED_TIMESHARE, OS_SCHED_DEFAULT };
    int idx = list_index(vs, value);
    os_schedClass *elem = cfg_address(cfgst, parent, cfgelem);
    assert(sizeof(vs) / sizeof(*vs) == sizeof(ms) / sizeof(*ms));
    if ( idx < 0 )
        return cfg_error(cfgst, "'%s': undefined value", value);
    *elem = ms[idx];
    return 1;
}

static void pf_sched_class(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, int is_default)
{
    os_schedClass *p = cfg_address(cfgst, parent, cfgelem);
    const char *str = "INVALID";
    switch ( *p ) {
        case OS_SCHED_DEFAULT: str = "default"; break;
        case OS_SCHED_TIMESHARE: str = "timeshare"; break;
        case OS_SCHED_REALTIME: str = "realtime"; break;
    }
    cfg_log(cfgst, "%s%s", str, is_default ? " [def]" : "");
}

OS_WARNING_MSVC_OFF(4996);
static int uf_maybe_int32(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, UNUSED_ARG(int first), const char *value)
{
    struct config_maybe_int32 *elem = cfg_address(cfgst, parent, cfgelem);
    int pos;
    if ( os_strcasecmp(value, "default") == 0 ) {
        elem->isdefault = 1;
        elem->value = 0;
        return 1;
    } else if ( sscanf(value, "%d%n", &elem->value, &pos) == 1 && value[pos] == 0 ) {
        elem->isdefault = 0;
        return 1;
    } else {
        return cfg_error(cfgst, "'%s': neither 'default' nor a decimal integer\n", value);
    }
}
OS_WARNING_MSVC_ON(4996);

static int uf_maybe_memsize(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, UNUSED_ARG(int first), const char *value)
{
    struct config_maybe_uint32 *elem = cfg_address(cfgst, parent, cfgelem);
    int64_t size = 0;
    if ( os_strcasecmp(value, "default") == 0 ) {
        elem->isdefault = 1;
        elem->value = 0;
        return 1;
    } else if ( !uf_natint64_unit(cfgst, &size, value, unittab_memsize, 1, INT32_MAX) ) {
        return 0;
    } else {
        elem->isdefault = 0;
        elem->value = (uint32_t) size;
        return 1;
    }
}


static int uf_float(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, UNUSED_ARG(int first), const char *value)
{
    float *elem = cfg_address(cfgst, parent, cfgelem);
    char *endptr;
    float f = os_strtof(value, &endptr);
    if ( *value == 0 || *endptr != 0 )
        return cfg_error(cfgst, "%s: not a floating point number", value);
    *elem = f;
    return 1;
}

static int uf_int(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, UNUSED_ARG(int first), const char *value)
{
    int *elem = cfg_address(cfgst, parent, cfgelem);
    char *endptr;
    long v = strtol(value, &endptr, 10);
    if ( *value == 0 || *endptr != 0 )
        return cfg_error(cfgst, "%s: not a decimal integer", value);
    if ( v != (int) v )
        return cfg_error(cfgst, "%s: value out of range", value);
    *elem = (int) v;
    return 1;
}

static int uf_duration_gen(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, const char *value, int64_t def_mult, int64_t max_ns)
{
    return uf_natint64_unit(cfgst, cfg_address(cfgst, parent, cfgelem), value, unittab_duration, def_mult, max_ns);
}

static int uf_duration_inf(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, UNUSED_ARG(int first), const char *value)
{
    if ( os_strcasecmp(value, "inf") == 0 ) {
        int64_t *elem = cfg_address(cfgst, parent, cfgelem);
        *elem = T_NEVER;
        return 1;
    } else {
        return uf_duration_gen(cfgst, parent, cfgelem, value, 0, T_NEVER - 1);
    }
}

static int uf_duration_ms_1hr(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, UNUSED_ARG(int first), const char *value)
{
    return uf_duration_gen(cfgst, parent, cfgelem, value, T_MILLISECOND, 3600 * T_SECOND);
}

static int uf_duration_ms_1s(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, UNUSED_ARG(int first), const char *value)
{
    return uf_duration_gen(cfgst, parent, cfgelem, value, T_MILLISECOND, T_SECOND);
}

static int uf_duration_us_1s(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, UNUSED_ARG(int first), const char *value)
{
    return uf_duration_gen(cfgst, parent, cfgelem, value, 1000, T_SECOND);
}

static int uf_int32(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, UNUSED_ARG(int first), const char *value)
{
    int32_t *elem = cfg_address(cfgst, parent, cfgelem);
    char *endptr;
    long v = strtol(value, &endptr, 10);
    if ( *value == 0 || *endptr != 0 )
        return cfg_error(cfgst, "%s: not a decimal integer", value);
    if ( v != (int32_t) v )
        return cfg_error(cfgst, "%s: value out of range", value);
    *elem = (int32_t) v;
    return 1;
}

#if 0
static int uf_uint32(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, UNUSED_ARG(int first), const char *value)
{
    uint32_t *elem = cfg_address(cfgst, parent, cfgelem);
    char *endptr;
    unsigned long v = strtoul(value, &endptr, 10);
    if ( *value == 0 || *endptr != 0 )
        return cfg_error(cfgst, "%s: not a decimal integer", value);
    if ( v != (uint32_t) v )
        return cfg_error(cfgst, "%s: value out of range", value);
    *elem = (uint32_t) v;
    return 1;
}
#endif

static int uf_uint(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, UNUSED_ARG(int first), const char *value)
{
    unsigned *elem = cfg_address(cfgst, parent, cfgelem);
    char *endptr;
    unsigned long v = strtoul(value, &endptr, 10);
    if ( *value == 0 || *endptr != 0 )
        return cfg_error(cfgst, "%s: not a decimal integer", value);
    if ( v != (unsigned) v )
        return cfg_error(cfgst, "%s: value out of range", value);
    *elem = (unsigned) v;
    return 1;
}

static int uf_int_min_max(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, int first, const char *value, int min, int max)
{
    int *elem = cfg_address(cfgst, parent, cfgelem);
    if ( !uf_int(cfgst, parent, cfgelem, first, value) )
        return 0;
    else if ( *elem < min || *elem > max )
        return cfg_error(cfgst, "%s: out of range", value);
    else
        return 1;
}

OS_WARNING_MSVC_OFF(4996);
static int uf_domainId(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, UNUSED_ARG(int first), const char *value)
{
  struct config_maybe_int32 *elem = cfg_address(cfgst, parent, cfgelem);
  int pos;
  if (os_strcasecmp(value, "any") == 0) {
    elem->isdefault = 1;
    elem->value = 0;
    return 1;
  } else if (sscanf(value, "%d%n", &elem->value, &pos) == 1 && value[pos] == 0 && elem->value >= 0 && elem->value <= 230) {
    elem->isdefault = 0;
    return 1;
  } else {
    return cfg_error(cfgst, "'%s': neither 'any' nor a decimal integer in 0 .. 230\n", value);
  }
}
OS_WARNING_MSVC_ON(4996);

static int uf_participantIndex(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, int first, const char *value)
{
    int *elem = cfg_address(cfgst, parent, cfgelem);
    if ( os_strcasecmp(value, "auto") == 0 ) {
        *elem = PARTICIPANT_INDEX_AUTO;
        return 1;
    } else if ( os_strcasecmp(value, "none") == 0 ) {
        *elem = PARTICIPANT_INDEX_NONE;
        return 1;
    } else {
        return uf_int_min_max(cfgst, parent, cfgelem, first, value, 0, 120);
    }
}

static int uf_port(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, int first, const char *value)
{
    return uf_int_min_max(cfgst, parent, cfgelem, first, value, 1, 65535);
}

static int uf_dyn_port(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, int first, const char *value)
{
    return uf_int_min_max(cfgst, parent, cfgelem, first, value, -1, 65535);
}

static int uf_natint(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, int first, const char *value)
{
    return uf_int_min_max(cfgst, parent, cfgelem, first, value, 0, INT32_MAX);
}

static int uf_natint_255(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, int first, const char *value)
{
    return uf_int_min_max(cfgst, parent, cfgelem, first, value, 0, 255);
}

static int uf_transport_selector (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, UNUSED_ARG (int first), const char *value)
{
  static const char *vs[] = { "default", "udp", "udp6", "tcp", "tcp6", "raweth", NULL };
  static const enum transport_selector ms[] = {
    TRANS_DEFAULT, TRANS_UDP, TRANS_UDP6, TRANS_TCP, TRANS_TCP6, TRANS_RAWETH, 0,
  };
  enum transport_selector *elem = cfg_address (cfgst, parent, cfgelem);
  int idx = list_index (vs, value);
  assert (sizeof (vs) / sizeof (*vs) == sizeof (ms) / sizeof (*ms));
  if (idx < 0)
    return cfg_error (cfgst, "'%s': undefined value", value);
  *elem = ms[idx];
  return 1;
}

static void pf_transport_selector (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, int is_default)
{
  enum transport_selector *p = cfg_address (cfgst, parent, cfgelem);
  const char *str = "INVALID";
  switch (*p)
  {
    case TRANS_DEFAULT: str = "default"; break;
    case TRANS_UDP: str = "udp"; break;
    case TRANS_UDP6: str = "udp6"; break;
    case TRANS_TCP: str = "tcp"; break;
    case TRANS_TCP6: str = "tcp6"; break;
    case TRANS_RAWETH: str = "raweth"; break;
  }
  cfg_log (cfgst, "%s%s", str, is_default ? " [def]" : "");
}

static int uf_many_sockets_mode (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, UNUSED_ARG (int first), const char *value)
{
  static const char *vs[] = { "false", "true", "single", "none", "many", NULL };
  static const enum many_sockets_mode ms[] = {
    MSM_SINGLE_UNICAST, MSM_MANY_UNICAST, MSM_SINGLE_UNICAST, MSM_NO_UNICAST, MSM_MANY_UNICAST, 0,
  };
  enum many_sockets_mode *elem = cfg_address (cfgst, parent, cfgelem);
  int idx = list_index (vs, value);
  assert (sizeof (vs) / sizeof (*vs) == sizeof (ms) / sizeof (*ms));
  if (idx < 0)
    return cfg_error (cfgst, "'%s': undefined value", value);
  *elem = ms[idx];
  return 1;
}

static void pf_many_sockets_mode (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, int is_default)
{
  enum many_sockets_mode *p = cfg_address (cfgst, parent, cfgelem);
  const char *str = "INVALID";
  switch (*p)
  {
    case MSM_SINGLE_UNICAST: str = "single"; break;
    case MSM_MANY_UNICAST: str = "many"; break;
    case MSM_NO_UNICAST: str = "none"; break;
  }
  cfg_log (cfgst, "%s%s", str, is_default ? " [def]" : "");
}

static int uf_deaf_mute (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, int first, const char *value)
{
  return uf_boolean (cfgst, parent, cfgelem, first, value);
}

static int do_update(struct cfgst *cfgst, update_fun_t upd, void *parent, struct cfgelem const * const cfgelem, const char *value, int is_default)
{
    struct cfgst_node *n;
    struct cfgst_nodekey key;
    ut_avlIPath_t np;
    int ok;
    key.e = cfgelem;
    if ( (n = ut_avlLookupIPath(&cfgst_found_treedef, &cfgst->found, &key, &np)) == NULL ) {
        if ( (n = os_malloc(sizeof(*n))) == NULL )
            return cfg_error(cfgst, "out of memory");

        n->key = key;
        n->count = 0;
        n->failed = 0;
        n->is_default = is_default;
        ut_avlInsertIPath(&cfgst_found_treedef, &cfgst->found, n, &np);
    }
    if ( cfgelem->multiplicity == 0 || n->count < cfgelem->multiplicity )
        ok = upd(cfgst, parent, cfgelem, (n->count == n->failed), value);
    else
        ok = cfg_error(cfgst, "only %d instance(s) allowed", cfgelem->multiplicity);
    n->count++;
    if ( !ok ) {
        n->failed++;
    }
    return ok;
}

static int set_default(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem)
{
    if ( cfgelem->defvalue == NULL )
        return cfg_error(cfgst, "element missing in configuration");
    return do_update(cfgst, cfgelem->update, parent, cfgelem, cfgelem->defvalue, 1);
}

static int set_defaults(struct cfgst *cfgst, void *parent, int isattr, struct cfgelem const * const cfgelem, int clear_found)
{
    const struct cfgelem *ce;
    int ok = 1;
    for ( ce = cfgelem; ce && ce->name; ce++ ) {
        struct cfgst_node *n;
        struct cfgst_nodekey key;
        key.e = ce;
        cfgst_push(cfgst, isattr, ce, parent);
        if ( ce->multiplicity == 1 ) {
            if ( ut_avlLookup(&cfgst_found_treedef, &cfgst->found, &key) == NULL ) {
                if ( ce->update ) {
                    int ok1;
                    cfgst_push(cfgst, 0, NULL, NULL);
                    ok1 = set_default(cfgst, parent, ce);
                    cfgst_pop(cfgst);
                    ok = ok && ok1;
                }
            }
            if ( (n = ut_avlLookup(&cfgst_found_treedef, &cfgst->found, &key)) != NULL ) {
                if ( clear_found ) {
                    ut_avlDelete(&cfgst_found_treedef, &cfgst->found, n);
                    os_free(n);
                }
            }
            if ( ce->children ) {
                int ok1 = set_defaults(cfgst, parent, 0, ce->children, clear_found);
                ok = ok && ok1;
            }
            if ( ce->attributes ) {
                int ok1 = set_defaults(cfgst, parent, 1, ce->attributes, clear_found);
                ok = ok && ok1;
            }
        }
        cfgst_pop(cfgst);
    }
    return ok;
}

static void pf_nop(UNUSED_ARG(struct cfgst *cfgst), UNUSED_ARG(void *parent), UNUSED_ARG(struct cfgelem const * const cfgelem), UNUSED_ARG(int is_default))
{
}

static void pf_string(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, int is_default)
{
    char **p = cfg_address(cfgst, parent, cfgelem);
    cfg_log(cfgst, "%s%s", *p ? *p : "(null)", is_default ? " [def]" : "");
}

static void pf_int64_unit(struct cfgst *cfgst, int64_t value, int is_default, const struct unit *unittab, const char *zero_unit)
{
    if ( value == 0 ) {
        /* 0s is a bit of a special case: we don't want to print 0hr (or
        whatever unit will have the greatest multiplier), so hard-code
        as 0s */
        cfg_log(cfgst, "0 %s%s", zero_unit, is_default ? " [def]" : "");
    } else {
        int64_t m = 0;
        const char *unit = NULL;
        int i;
        for ( i = 0; unittab[i].name != NULL; i++ ) {
            if ( unittab[i].multiplier > m && (value % unittab[i].multiplier) == 0 ) {
                m = unittab[i].multiplier;
                unit = unittab[i].name;
            }
        }
        assert(m > 0);
        assert(unit != NULL);
        cfg_log(cfgst, "%lld %s%s", value / m, unit, is_default ? " [def]" : "");
    }
}

#ifdef DDSI_INCLUDE_ENCRYPTION
static void pf_key(struct cfgst *cfgst, UNUSED_ARG(void *parent), UNUSED_ARG(struct cfgelem const * const cfgelem), UNUSED_ARG(int is_default))
{
    cfg_log(cfgst, "<hidden, see configfile>");
}

static void pf_cipher(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, int is_default)
{
    q_cipherType *p = cfg_address(cfgst, parent, cfgelem);
    if ( q_security_plugin.cipher_type ) {
        cfg_log(cfgst, "%s%s", (q_security_plugin.cipher_type) (*p), is_default ? " [def]" : "");
    }
}
#endif /* DDSI_INCLUDE_ENCRYPTION */

static void pf_networkAddress(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, int is_default)
{
    char **p = cfg_address(cfgst, parent, cfgelem);
    cfg_log(cfgst, "%s%s", *p ? *p : "auto", is_default ? " [def]" : "");
}

static void pf_participantIndex(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, int is_default)
{
    int *p = cfg_address(cfgst, parent, cfgelem);
    switch ( *p ) {
        case PARTICIPANT_INDEX_NONE:
            cfg_log(cfgst, "none%s", is_default ? " [def]" : "");
            break;
        case PARTICIPANT_INDEX_AUTO:
            cfg_log(cfgst, "auto%s", is_default ? " [def]" : "");
            break;
        default:
            cfg_log(cfgst, "%d%s", *p, is_default ? " [def]" : "");
            break;
    }
}

static void pf_networkAddresses(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, int is_default)
{
    int i;
    char ***p = cfg_address(cfgst, parent, cfgelem);
    for ( i = 0; (*p)[i] != NULL; i++ )
        cfg_log(cfgst, "%s%s", (*p)[i], is_default ? " [def]" : "");
}

static void pf_int(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, int is_default)
{
    int *p = cfg_address(cfgst, parent, cfgelem);
    cfg_log(cfgst, "%d%s", *p, is_default ? " [def]" : "");
}

static void pf_uint(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, int is_default)
{
    unsigned *p = cfg_address(cfgst, parent, cfgelem);
    cfg_log(cfgst, "%u%s", *p, is_default ? " [def]" : "");
}

static void pf_duration(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, int is_default)
{
    const int64_t *elem = cfg_address(cfgst, parent, cfgelem);
    if ( *elem == T_NEVER )
        cfg_log(cfgst, "inf%s", is_default ? " [def]" : "");
    else
        pf_int64_unit(cfgst, *elem, is_default, unittab_duration, "s");
}

#ifdef DDSI_INCLUDE_BANDWIDTH_LIMITING
static void pf_bandwidth(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, int is_default)
{
    const uint32_t *elem = cfg_address(cfgst, parent, cfgelem);
    if ( *elem == 0 )
        cfg_log(cfgst, "inf%s", is_default ? " [def]" : "");
    else
        pf_int64_unit(cfgst, *elem, is_default, unittab_bandwidth_Bps, "B/s");
}
#endif

static void pf_memsize(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, int is_default)
{
    const int *elem = cfg_address(cfgst, parent, cfgelem);
    pf_int64_unit(cfgst, *elem, is_default, unittab_memsize, "B");
}

static void pf_int32(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, int is_default)
{
    int32_t *p = cfg_address(cfgst, parent, cfgelem);
    cfg_log(cfgst, "%d%s", *p, is_default ? " [def]" : "");
}

#if 0
static void pf_uint32(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, int is_default)
{
    uint32_t *p = cfg_address(cfgst, parent, cfgelem);
    cfg_log(cfgst, "%u%s", *p, is_default ? " [def]" : "");
}
#endif

static void pf_maybe_int32(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, int is_default)
{
  struct config_maybe_int32 *p = cfg_address(cfgst, parent, cfgelem);
  if ( p->isdefault )
    cfg_log(cfgst, "default%s", is_default ? " [def]" : "");
  else
    cfg_log(cfgst, "%d%s", p->value, is_default ? " [def]" : "");
}

static void pf_maybe_memsize(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, int is_default)
{
    struct config_maybe_uint32 *p = cfg_address(cfgst, parent, cfgelem);
    if ( p->isdefault )
        cfg_log(cfgst, "default%s", is_default ? " [def]" : "");
    else
        pf_int64_unit(cfgst, p->value, is_default, unittab_memsize, "B");
}

static void pf_domainId(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, int is_default)
{
  struct config_maybe_int32 *p = cfg_address(cfgst, parent, cfgelem);
  if ( p->isdefault )
    cfg_log(cfgst, "any (%d)%s", p->value, is_default ? " [def]" : "");
  else
    cfg_log(cfgst, "%d%s", p->value, is_default ? " [def]" : "");
}

static void pf_float(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, int is_default)
{
    float *p = cfg_address(cfgst, parent, cfgelem);
    cfg_log(cfgst, "%f%s", *p, is_default ? " [def]" : "");
}

static void pf_boolean(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, int is_default)
{
    int *p = cfg_address(cfgst, parent, cfgelem);
    cfg_log(cfgst, "%s%s", *p ? "true" : "false", is_default ? " [def]" : "");
}

static void pf_negated_boolean(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, int is_default)
{
    int *p = cfg_address(cfgst, parent, cfgelem);
    cfg_log(cfgst, "%s%s", *p ? "false" : "true", is_default ? " [def]" : "");
}

static int uf_standards_conformance(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, UNUSED_ARG(int first), const char *value)
{
    static const char *vs[] = {
        "pedantic", "strict", "lax", NULL
    };
    static const uint32_t lc[] = {
        NN_SC_PEDANTIC, NN_SC_STRICT, NN_SC_LAX, 0
    };
    enum nn_standards_conformance *elem = cfg_address(cfgst, parent, cfgelem);
    int idx = list_index(vs, value);
    assert(sizeof(vs) / sizeof(*vs) == sizeof(lc) / sizeof(*lc));
    if ( idx < 0 )
        return cfg_error(cfgst, "'%s': undefined value", value);
    else {
        *elem = lc[idx];
        return 1;
    }
}

static void pf_standards_conformance(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, int is_default)
{
    enum nn_standards_conformance *p = cfg_address(cfgst, parent, cfgelem);
    const char *str = "INVALID";
    switch ( *p ) {
        case NN_SC_PEDANTIC: str = "pedantic"; break;
        case NN_SC_STRICT: str = "strict"; break;
        case NN_SC_LAX: str = "lax"; break;
    }
    cfg_log(cfgst, "%s%s", str, is_default ? " [def]" : "");
}

static void pf_logcat(struct cfgst *cfgst, UNUSED_ARG(void *parent), UNUSED_ARG(struct cfgelem const * const cfgelem), UNUSED_ARG(int is_default))
{
    uint32_t remaining = config.enabled_logcats;
    char res[256] = "", *resp = res;
    const char *prefix = "";
    size_t i;
#ifndef NDEBUG
    {
        size_t max;
        for ( i = 0, max = 0; i < sizeof(logcat_codes) / sizeof(*logcat_codes); i++ )
            max += 1 + strlen(logcat_names[i]);
        max += 11; /* ,0x%x */
        max += 1; /* \0 */
        assert(max <= sizeof(res));
    }
#endif
    /* TRACE enables ALLCATS, all the others just one */
    if ( (remaining & DDS_LC_ALL) == DDS_LC_ALL ) {
        resp += snprintf(resp, 256, "%strace", prefix);
        remaining &= ~DDS_LC_ALL;
        prefix = ",";
    }
    for ( i = 0; i < sizeof(logcat_codes) / sizeof(*logcat_codes); i++ ) {
        if ( remaining & logcat_codes[i] ) {
            resp += snprintf(resp, 256, "%s%s", prefix, logcat_names[i]);
            remaining &= ~logcat_codes[i];
            prefix = ",";
        }
    }
    if ( remaining ) {
        resp += snprintf(resp, 256, "%s0x%x", prefix, (unsigned) remaining);
    }
    assert(resp <= res + sizeof(res));
    /* can't do default indicator: user may have specified Verbosity, in
    which case EnableCategory is at default, but for these two
    settings, I don't mind. */
    cfg_log(cfgst, "%s", res);
}


static void print_configitems(struct cfgst *cfgst, void *parent, int isattr, struct cfgelem const * const cfgelem, int unchecked)
{
    const struct cfgelem *ce;
    for ( ce = cfgelem; ce && ce->name; ce++ ) {
        struct cfgst_nodekey key;
        struct cfgst_node *n;
        if ( ce->name[0] == '>' || ce->name[0] == '|') /* moved or deprecated, so don't care */
            continue;
        key.e = ce;
        cfgst_push(cfgst, isattr, ce, parent);
        if ( ce->multiplicity == 1 ) {
            if ( (n = ut_avlLookup(&cfgst_found_treedef, &cfgst->found, &key)) != NULL ) {
                cfgst_push(cfgst, 0, NULL, NULL);
                ce->print(cfgst, parent, ce, n->is_default);
                cfgst_pop(cfgst);
            } else {
                if ( unchecked && ce->print ) {
                    cfgst_push(cfgst, 0, NULL, NULL);
                    ce->print(cfgst, parent, ce, 0);
                    cfgst_pop(cfgst);
                }
            }

            if ( ce->children )
                print_configitems(cfgst, parent, 0, ce->children, unchecked);
            if ( ce->attributes )
                print_configitems(cfgst, parent, 1, ce->attributes, unchecked);
        } else {
            struct config_listelem *p = cfg_deref_address(cfgst, parent, ce);
            while ( p ) {
                cfgst_push(cfgst, 0, NULL, NULL);
                if ( ce->print ) {
                    ce->print(cfgst, p, ce, 0);
                }
                cfgst_pop(cfgst);
                if ( ce->attributes )
                    print_configitems(cfgst, p, 1, ce->attributes, 1);
                if ( ce->children )
                    print_configitems(cfgst, p, 0, ce->children, 1);
                p = p->next;
            }
        }
        cfgst_pop(cfgst);
    }
}


static void free_all_elements(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem)
{
    const struct cfgelem *ce;

    for ( ce = cfgelem; ce && ce->name; ce++ ) {
        if ( ce->name[0] == '>' ) /* moved, so don't care */
            continue;

        if ( ce->free )
            ce->free(cfgst, parent, ce);

        if ( ce->multiplicity == 1 ) {
            if ( ce->children )
                free_all_elements(cfgst, parent, ce->children);
            if ( ce->attributes )
                free_all_elements(cfgst, parent, ce->attributes);
        } else {
            struct config_listelem *p = cfg_deref_address(cfgst, parent, ce);
            struct config_listelem *r;
            while ( p ) {
                if ( ce->attributes )
                    free_all_elements(cfgst, p, ce->attributes);
                if ( ce->children )
                    free_all_elements(cfgst, p, ce->children);
                r = p;
                p = p->next;
                os_free(r);
            }
        }
    }
}

static void free_configured_elements(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem)
{
    const struct cfgelem *ce;
    for ( ce = cfgelem; ce && ce->name; ce++ ) {
        struct cfgst_nodekey key;
        struct cfgst_node *n;
        if ( ce->name[0] == '>' ) /* moved, so don't care */
            continue;
        key.e = ce;
        if ( (n = ut_avlLookup(&cfgst_found_treedef, &cfgst->found, &key)) != NULL ) {
            if ( ce->free && n->count > n->failed )
                ce->free(cfgst, parent, ce);
        }

        if ( ce->multiplicity == 1 ) {
            if ( ce->children )
                free_configured_elements(cfgst, parent, ce->children);
            if ( ce->attributes )
                free_configured_elements(cfgst, parent, ce->attributes);
        } else {
            struct config_listelem *p = cfg_deref_address(cfgst, parent, ce);
            struct config_listelem *r;
            while ( p ) {
                if ( ce->attributes )
                    free_all_elements(cfgst, p, ce->attributes);
                if ( ce->children )
                    free_all_elements(cfgst, p, ce->children);
                r = p;
                p = p->next;
                os_free(r);
            }
        }
    }
}

static int matching_name_index(const char *name_w_aliases, const char *name)
{
    const char *ns = name_w_aliases, *p = strchr(ns, '|');
    int idx = 0;
    while ( p ) {
        if ( os_strncasecmp(ns, name, (size_t) (p - ns)) == 0 && name[p - ns] == 0 ) {
            /* ns upto the pipe symbol is a prefix of name, and name is terminated at that point */
            return idx;
        }
        /* If primary name followed by '||' instead of '|', aliases are non-warning */
        ns = p + 1 + (idx == 0 && p[1] == '|');
        p = strchr(ns, '|');
        idx++;
    }
    return (os_strcasecmp(ns, name) == 0) ? idx : -1;
}

static const struct cfgelem *lookup_redirect(const char *target)
{
    const struct cfgelem *cfgelem = ddsi2_cfgelems;
    char *target_copy = os_strdup(target), *p1;
    const char *p = target_copy;
    while ( p ) {
        p1 = strchr(p, '/');
        if ( p1 ) *p1++ = 0;
        for ( ; cfgelem->name; cfgelem++ ) {
            /* not supporting multiple redirects */
            assert(cfgelem->name[0] != '>');
            if ( matching_name_index(cfgelem->name, p) >= 0 )
                break;
        }
        if ( p1 ) {
            cfgelem = cfgelem->children;
        }
        p = p1;
    }
    os_free(target_copy);
    return cfgelem;
}

static int proc_elem_open(void *varg, UNUSED_ARG(uintptr_t parentinfo), UNUSED_ARG(uintptr_t *eleminfo), const char *name)
{
    struct cfgst * const cfgst = varg;
    const struct cfgelem *cfgelem = cfgst_tos(cfgst);
    const struct cfgelem *cfg_subelem;
    int moved = 0;
    if ( cfgelem == NULL ) {
        /* Ignoring, but do track the structure so we can know when to stop ignoring */
        cfgst_push(cfgst, 0, NULL, NULL);
        return 1;
    }
    for ( cfg_subelem = cfgelem->children; cfg_subelem && cfg_subelem->name && strcmp(cfg_subelem->name, "*") != 0; cfg_subelem++ ) {
        const char *csename = cfg_subelem->name;
        int idx;
        moved = (csename[0] == '>');
        if ( moved )
            csename++;
        idx = matching_name_index(csename, name);
#if WARN_DEPRECATED_ALIAS
        if ( idx > 0 ) {
            if (csename[0] == '|') {
                cfg_warning(cfgst, "'%s': deprecated setting", name);
            } else {
                int n = (int) (strchr(csename, '|') - csename);
                if ( csename[n + 1] != '|' ) {
                    cfg_warning(cfgst, "'%s': deprecated alias for '%*.*s'", name, n, n, csename);
                }
            }
        }
#endif
        if ( idx >= 0 ) {
            break;
        }
    }
    if ( cfg_subelem == NULL || cfg_subelem->name == NULL ) {
        cfgst_push(cfgst, 0, NULL, NULL);
        return cfg_error(cfgst, "%s: unknown element", name);
    } else if ( strcmp(cfg_subelem->name, "*") == 0 ) {
        /* Push a marker that we are to ignore this part of the DOM tree */
        cfgst_push(cfgst, 0, NULL, NULL);
        return 1;
    } else {
        void *parent, *dynparent;

        if ( moved ) {
#if WARN_DEPRECATED_ALIAS
            cfg_warning(cfgst, "'%s': deprecated alias for '%s'", name, cfg_subelem->defvalue);
#endif
            cfg_subelem = lookup_redirect(cfg_subelem->defvalue);
        }

        parent = cfgst_parent(cfgst);
        assert(cfgelem->init || cfgelem->multiplicity == 1); /*multi-items must have an init-func */
        if ( cfg_subelem->init ) {
            if ( cfg_subelem->init(cfgst, parent, cfg_subelem) < 0 )
                return 0;
        }

        if ( cfg_subelem->multiplicity != 1 )
            dynparent = cfg_deref_address(cfgst, parent, cfg_subelem);
        else
            dynparent = parent;

        cfgst_push(cfgst, 0, cfg_subelem, dynparent);
        return 1;
    }
}

static int proc_attr(void *varg, UNUSED_ARG(uintptr_t eleminfo), const char *name, const char *value)
{
    /* All attributes are processed immediately after opening the element */
    struct cfgst * const cfgst = varg;
    const struct cfgelem *cfgelem = cfgst_tos(cfgst);
    const struct cfgelem *cfg_attr;
    if ( cfgelem == NULL )
        return 1;
    for ( cfg_attr = cfgelem->attributes; cfg_attr && cfg_attr->name; cfg_attr++ ) {
        if ( os_strcasecmp(cfg_attr->name, name) == 0 )
            break;
    }
    if ( cfg_attr == NULL || cfg_attr->name == NULL )
        return cfg_error(cfgst, "%s: unknown attribute", name);
    else {
        void *parent = cfgst_parent(cfgst);
        char *xvalue = ut_expand_envvars(value);
        int ok;
        cfgst_push(cfgst, 1, cfg_attr, parent);
        ok = do_update(cfgst, cfg_attr->update, parent, cfg_attr, xvalue, 0);
        cfgst_pop(cfgst);
        os_free(xvalue);
        return ok;
    }
}

static int proc_elem_data(void *varg, UNUSED_ARG(uintptr_t eleminfo), const char *value)
{
    struct cfgst * const cfgst = varg;
    const struct cfgelem *cfgelem = cfgst_tos(cfgst);
    if ( cfgelem == NULL )
        return 1;
    if ( cfgelem->update == 0 )
        return cfg_error(cfgst, "%s: no data expected", value);
    else {
        void *parent = cfgst_parent(cfgst);
        char *xvalue = ut_expand_envvars(value);
        int ok;
        cfgst_push(cfgst, 0, NULL, parent);
        ok = do_update(cfgst, cfgelem->update, parent, cfgelem, xvalue, 0);
        cfgst_pop(cfgst);
        os_free(xvalue);
        return ok;
    }
}

static int proc_elem_close(void *varg, UNUSED_ARG(uintptr_t eleminfo))
{
    struct cfgst * const cfgst = varg;
    const struct cfgelem * cfgelem = cfgst_tos(cfgst);
    int ok = 1;
    if ( cfgelem && cfgelem->multiplicity != 1 ) {
        void *parent = cfgst_parent(cfgst);
        int ok1;
        ok1 = set_defaults(cfgst, parent, 1, cfgelem->attributes, 1);
        ok = ok && ok1;
        ok1 = set_defaults(cfgst, parent, 0, cfgelem->children, 1);
        ok = ok && ok1;
    }
    cfgst_pop(cfgst);
    return ok;
}

static void proc_error(void *varg, const char *msg, int line)
{
    struct cfgst * const cfgst = varg;
    cfg_error(cfgst, "parser error %s at line %d", msg, line);
}


static int cfgst_node_cmp(const void *va, const void *vb)
{
    return memcmp(va, vb, sizeof(struct cfgst_nodekey));
}

#ifdef DDSI_INCLUDE_NETWORK_CHANNELS
static int set_default_channel(struct config *cfg)
{
    if ( cfg->channels == NULL ) {
        /* create one default channel if none configured */
        struct config_channel_listelem *c;
        if ( (c = os_malloc(sizeof(*c))) == NULL )
            return ERR_OUT_OF_MEMORY;
        c->next = NULL;
        c->name = os_strdup("user");
        c->priority = 0;
        c->resolution = 1 * T_MILLISECOND;
#ifdef DDSI_INCLUDE_BANDWIDTH_LIMITING
        c->data_bandwidth_limit = 0;
        c->auxiliary_bandwidth_limit = 0;
#endif
        c->diffserv_field = 0;
        c->channel_reader_ts = NULL;
        c->queueId = 0;
        c->dqueue = NULL;
        c->evq = NULL;
        c->transmit_conn = NULL;
        cfg->channels = c;
    }
    return 0;
}

static int sort_channels_cmp(const void *va, const void *vb)
{
    const struct config_channel_listelem * const *a = va;
    const struct config_channel_listelem * const *b = vb;
    return ((*a)->priority == (*b)->priority) ? 0 : ((*a)->priority < (*b)->priority) ? -1 : 1;
}

static int sort_channels_check_nodups(struct config *cfg)
{
    /* Selecting a channel is much easier & more elegant if the channels
    are sorted on descending priority.  While we do retain the list
    structure, sorting is much easier in an array, and hence we
    convert back and forth. */
    struct config_channel_listelem **ary, *c;
    unsigned i, n;
    int result;

    n = 0;
    for ( c = cfg->channels; c; c = c->next )
        n++;
    assert(n > 0);

    ary = os_malloc(n * sizeof(*ary));

    i = 0;
    for ( c = cfg->channels; c; c = c->next )
        ary[i++] = c;
    qsort(ary, n, sizeof(*ary), sort_channels_cmp);

    result = 0;
    for ( i = 0; i < n - 1; i++ ) {
        if ( ary[i]->priority == ary[i + 1]->priority ) {
            DDS_ERROR("config: duplicate channel definition for priority %u: channels %s and %s\n",
                     ary[i]->priority, ary[i]->name, ary[i + 1]->name);
            result = ERR_ENTITY_EXISTS;
        }
    }

    if ( result == 0 ) {
        cfg->channels = ary[0];
        for ( i = 0; i < n - 1; i++ )
            ary[i]->next = ary[i + 1];
        ary[i]->next = NULL;
        cfg->max_channel = ary[i];
    }

    os_free(ary);
    return result;
}
#endif /* DDSI_INCLUDE_NETWORK_CHANNELS */

struct cfgst * config_init
(
    _In_opt_ const char *configfile
)
{
    int ok = 1;
    struct cfgst *cfgst;

    memset(&config, 0, sizeof(config));

    config.tracingOutputFile = stderr;
    config.enabled_logcats = DDS_LC_ERROR | DDS_LC_WARNING;

    /* eventually, we domainId.value will be the real domain id selected, even if it was configured
       to the default of "any" and has "isdefault" set; initializing it to the default-default
       value of 0 means "any" in the config & DDS_DOMAIN_DEFAULT in create participant automatically
       ends up on the right value */
    config.domainId.value = 0;

    cfgst = os_malloc(sizeof(*cfgst));
    memset(cfgst, 0, sizeof(*cfgst));

    ut_avlInit(&cfgst_found_treedef, &cfgst->found);
    cfgst->cfg = &config;
    cfgst->error = 0;

    /* configfile == NULL will get you the default configuration */
    if ( configfile ) {
        char *copy = os_strdup(configfile), *cursor = copy, *tok;
        while ( (tok = os_strsep(&cursor, ",")) != NULL ) {
            struct ut_xmlpCallbacks cb;
            struct ut_xmlpState *qx;
            FILE *fp;

            OS_WARNING_MSVC_OFF(4996);
            if ( (fp = fopen(tok, "r")) == NULL ) {
                if ( strncmp(tok, "file://", 7) != 0 || (fp = fopen(tok + 7, "r")) == NULL ) {
                    DDS_ERROR("can't open configuration file %s\n", tok);
                    os_free(copy);
                    os_free(cfgst);
                    return NULL;
                }
            }
            OS_WARNING_MSVC_ON(4996);

            cb.attr = proc_attr;
            cb.elem_close = proc_elem_close;
            cb.elem_data = proc_elem_data;
            cb.elem_open = proc_elem_open;
            cb.error = proc_error;

            if ( (qx = ut_xmlpNewFile(fp, cfgst, &cb)) == NULL ) {
                fclose(fp);
                os_free(copy);
                os_free(cfgst);
                return NULL;
            }
            cfgst_push(cfgst, 0, &root_cfgelem, &config);

            ok = (ut_xmlpParse(qx) >= 0) && !cfgst->error;
            /* Pop until stack empty: error handling is rather brutal */
            assert(!ok || cfgst->path_depth == 1);
            while ( cfgst->path_depth > 0 )
                cfgst_pop(cfgst);
            ut_xmlpFree(qx);
            fclose(fp);
        }
        os_free(copy);
    }

    /* Set defaults for everything not set that we have a default value
    for, signal errors for things unset but without a default. */
    {
        int ok1 = set_defaults(cfgst, cfgst->cfg, 0, root_cfgelems, 0);
        ok = ok && ok1;
    }


  /* Compatibility settings of IPv6, TCP -- a bit too complicated for
     the poor framework */
  {
    int ok1 = 1;
    switch (cfgst->cfg->transport_selector)
    {
      case TRANS_DEFAULT:
        if (cfgst->cfg->compat_tcp_enable == BOOLDEF_TRUE)
          cfgst->cfg->transport_selector = (config.compat_use_ipv6 == BOOLDEF_TRUE) ? TRANS_TCP6 : TRANS_TCP;
        else
          cfgst->cfg->transport_selector = (config.compat_use_ipv6 == BOOLDEF_TRUE) ? TRANS_UDP6 : TRANS_UDP;
        break;
      case TRANS_TCP:
        ok1 = !(cfgst->cfg->compat_tcp_enable == BOOLDEF_FALSE || cfgst->cfg->compat_use_ipv6 == BOOLDEF_TRUE);
        break;
      case TRANS_TCP6:
        ok1 = !(cfgst->cfg->compat_tcp_enable == BOOLDEF_FALSE || cfgst->cfg->compat_use_ipv6 == BOOLDEF_FALSE);
        break;
      case TRANS_UDP:
        ok1 = !(cfgst->cfg->compat_tcp_enable == BOOLDEF_TRUE || cfgst->cfg->compat_use_ipv6 == BOOLDEF_TRUE);
        break;
      case TRANS_UDP6:
        ok1 = !(cfgst->cfg->compat_tcp_enable == BOOLDEF_TRUE || cfgst->cfg->compat_use_ipv6 == BOOLDEF_FALSE);
        break;
      case TRANS_RAWETH:
        ok1 = !(cfgst->cfg->compat_tcp_enable == BOOLDEF_TRUE || cfgst->cfg->compat_use_ipv6 == BOOLDEF_TRUE);
        break;
    }
    if (!ok1)
      DDS_ERROR("config: invalid combination of Transport, IPv6, TCP\n");
    ok = ok && ok1;
    cfgst->cfg->compat_use_ipv6 = (cfgst->cfg->transport_selector == TRANS_UDP6 || cfgst->cfg->transport_selector == TRANS_TCP6) ? BOOLDEF_TRUE : BOOLDEF_FALSE;
    cfgst->cfg->compat_tcp_enable = (cfgst->cfg->transport_selector == TRANS_TCP || cfgst->cfg->transport_selector == TRANS_TCP6) ? BOOLDEF_TRUE : BOOLDEF_FALSE;
  }

#ifdef DDSI_INCLUDE_NETWORK_CHANNELS
    /* Default channel gets set outside set_defaults -- a bit too
    complicated for the poor framework */

    if ( set_default_channel(cfgst->cfg) < 0 ) {
        ok = 0;
    }
    if ( cfgst->cfg->channels && sort_channels_check_nodups(cfgst->cfg) < 0 ) {
        ok = 0;
    }
#endif

#ifdef DDSI_INCLUDE_ENCRYPTION
    /* Check security profiles */
    {
        struct config_securityprofile_listelem *s = config.securityProfiles;
        while ( s ) {
            switch ( s->cipher ) {
                case Q_CIPHER_UNDEFINED:
                case Q_CIPHER_NULL:
                    /* nop */
                    if ( s->key && strlen(s->key) > 0 ) {
                        DDS_INFO("config: DDSI2Service/Security/SecurityProfile[@cipherkey]: %s: cipher key not required\n", s->key);
                    }
                    break;

                default:
                    /* read the cipherkey if present */
                    if ( !s->key || strlen(s->key) == 0 ) {
                        DDS_ERROR("config: DDSI2Service/Security/SecurityProfile[@cipherkey]: cipher key missing\n");
                        ok = 0;
                    } else if ( q_security_plugin.valid_uri && !(q_security_plugin.valid_uri) (s->cipher, s->key) ) {
                        DDS_ERROR("config: DDSI2Service/Security/SecurityProfile[@cipherkey]: %s : incorrect key\n", s->key);
                        ok = 0;
                    }
            }
            s = s->next;
        }
    }
#endif /* DDSI_INCLUDE_ENCRYPTION */

#ifdef DDSI_INCLUDE_NETWORK_PARTITIONS
    /* Assign network partition ids */
#ifdef DDSI_INCLUDE_ENCRYPTION
    /* also create links from the network partitions to the
    securityProfiles and signal errors if profiles do not exist */
#endif /* DDSI_INCLUDE_ENCRYPTION */
    {
        struct config_networkpartition_listelem *p = config.networkPartitions;
        config.nof_networkPartitions = 0;
        while ( p ) {
#ifdef DDSI_INCLUDE_ENCRYPTION
            if ( os_strcasecmp(p->profileName, "null") == 0 )
                p->securityProfile = NULL;
            else {
                struct config_securityprofile_listelem *s = config.securityProfiles;
                while ( s && os_strcasecmp(p->profileName, s->name) != 0 )
                    s = s->next;
                if ( s )
                    p->securityProfile = s;
                else {
                    DDS_ERROR("config: DDSI2Service/Partitioning/NetworkPartitions/NetworkPartition[@securityprofile]: %s: unknown securityprofile\n", p->profileName);
                    ok = 0;
                }
            }
#endif /* DDSI_INCLUDE_ENCRYPTION */
            config.nof_networkPartitions++;
            /* also use crc32 just like native nw and ordinary ddsi2e, only
            for interoperability because it is asking for trouble &
            forces us to include a crc32 routine when we have md5
            available anyway */
            p->partitionId = config.nof_networkPartitions; /* starting at 1 */
            p->partitionHash = crc32_calc(p->name, strlen(p->name));
            p = p->next;
        }
    }

    /* Create links from the partitionmappings to the network partitions
    and signal errors if partitions do not exist */
    {
        struct config_partitionmapping_listelem * m = config.partitionMappings;
        while ( m ) {
            struct config_networkpartition_listelem * p = config.networkPartitions;
            while ( p && os_strcasecmp(m->networkPartition, p->name) != 0 ) {
                p = p->next;
            }
            if ( p ) {
                m->partition = p;
            } else {
                DDS_ERROR("config: DDSI2Service/Partitioning/PartitionMappings/PartitionMapping[@networkpartition]: %s: unknown partition\n", m->networkPartition);
                ok = 0;
            }
            m = m->next;
        }
    }
#endif /* DDSI_INCLUDE_NETWORK_PARTITIONS */

    /* Now switch to configured tracing settings */
    config.enabled_logcats = enabled_logcats;

    if ( !ok ) {
        free_configured_elements(cfgst, cfgst->cfg, root_cfgelems);
    }

    if ( ok ) {
        config.valid = 1;
        return cfgst;
    } else {
        ut_avlFree(&cfgst_found_treedef, &cfgst->found, os_free);
        os_free(cfgst);
        return NULL;
    }
}

void config_print_cfgst(_In_ struct cfgst *cfgst)
{
    if ( cfgst == NULL )
        return;
    print_configitems(cfgst, cfgst->cfg, 0, root_cfgelems, 0);
}

void config_fini(_In_ struct cfgst *cfgst)
{
    assert(cfgst);
    assert(cfgst->cfg == &config);
    assert(config.valid);

    free_all_elements(cfgst, cfgst->cfg, root_cfgelems);
    dds_set_log_file(stderr);
    dds_set_trace_file(stderr);
    if (config.tracingOutputFile && config.tracingOutputFile != stdout && config.tracingOutputFile != stderr) {
        fclose(config.tracingOutputFile);
    }
    memset(&config, 0, sizeof(config));
    config.valid = 0;

    ut_avlFree(&cfgst_found_treedef, &cfgst->found, os_free);
    os_free(cfgst);
}

#ifdef DDSI_INCLUDE_NETWORK_PARTITIONS
static char *get_partition_search_pattern(const char *partition, const char *topic)
{
    size_t sz = strlen(partition) + strlen(topic) + 2;
    char *pt = os_malloc(sz);
    snprintf(pt, sz, "%s.%s", partition, topic);
    return pt;
}

struct config_partitionmapping_listelem *find_partitionmapping(const char *partition, const char *topic)
{
    char *pt = get_partition_search_pattern(partition, topic);
    struct config_partitionmapping_listelem *pm;
    for ( pm = config.partitionMappings; pm; pm = pm->next )
        if ( WildcardOverlap(pt, pm->DCPSPartitionTopic) )
            break;
    os_free(pt);
    return pm;
}

struct config_networkpartition_listelem *find_networkpartition_by_id(uint32_t id)
{
    struct config_networkpartition_listelem *np;
    for ( np = config.networkPartitions; np; np = np->next )
        if ( np->partitionId == id )
            return np;
    return 0;
}

int is_ignored_partition(const char *partition, const char *topic)
{
    char *pt = get_partition_search_pattern(partition, topic);
    struct config_ignoredpartition_listelem *ip;
    for ( ip = config.ignoredPartitions; ip; ip = ip->next )
        if ( WildcardOverlap(pt, ip->DCPSPartitionTopic) )
            break;
    os_free(pt);
    return ip != NULL;
}
#endif /* DDSI_INCLUDE_NETWORK_PARTITIONS */

#ifdef DDSI_INCLUDE_NETWORK_CHANNELS
struct config_channel_listelem *find_channel(nn_transport_priority_qospolicy_t transport_priority)
{
    struct config_channel_listelem *c;
    /* Channel selection is to use the channel with the lowest priority
    not less than transport_priority, or else the one with the
    highest priority. */
    assert(config.channels != NULL);
    assert(config.max_channel != NULL);
    for ( c = config.channels; c; c = c->next ) {
        assert(c->next == NULL || c->next->priority > c->priority);
        if ( transport_priority.value <= c->priority )
            return c;
    }
    return config.max_channel;
}
#endif /* DDSI_INCLUDE_NETWORK_CHANNELS */
