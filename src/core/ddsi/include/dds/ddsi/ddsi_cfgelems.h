/*
 * Copyright(c) 2020 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#ifndef DDSI_CFGELEMS_H
#define DDSI_CFGELEMS_H

static struct cfgelem general_cfgelems[] = {
  STRING("NetworkInterfaceAddress", NULL, 1, "auto",
    MEMBER(networkAddressString),
    FUNCTIONS(0, uf_networkAddress, ff_free, pf_networkAddress),
    DESCRIPTION(
      "<p>This element specifies the preferred network interface for use by "
      "Cyclone DDS. The preferred network interface determines the IP address "
      "that Cyclone DDS advertises in the discovery protocol (but see also "
      "General/ExternalNetworkAddress), and is also the only interface over "
      "which multicasts are transmitted. The interface can be identified by "
      "its IP address, network interface name or network portion of the "
      "address. If the value \"auto\" is entered here, Cyclone DDS will "
      "select what it considers the most suitable interface.</p>"
    )),
  STRING("MulticastRecvNetworkInterfaceAddresses", NULL, 1, "preferred",
    MEMBER(networkRecvAddressStrings),
    FUNCTIONS(0, uf_networkAddresses, ff_networkAddresses, pf_networkAddresses),
    DESCRIPTION(
      "<p>This element specifies on which network interfaces Cyclone DDS "
      "listens to multicasts. The following options are available:</p>\n"
      "<ul>\n"
      "<li><i>all</i>: "
      "listen for multicasts on all multicast-capable interfaces; or"
      "</li>\n"
      "<li><i>any</i>: "
      "listen for multicasts on the operating system default interface; or"
      "</li>\n"
      "<li><i>preferred</i>: "
      "listen for multicasts on the preferred interface "
      "(General/NetworkInterfaceAddress); or"
      "</li>\n"
      "<li><i>none</i>: "
      "does not listen for multicasts on any interface; or"
      "</li>\n"
      "<li>a comma-separated list of network addresses: configures Cyclone DDS to "
      "listen for multicasts on all of the listed addresses."
      "</li>\n"
      "</ul>\n"
      "<p>If Cyclone DDS is in IPv6 mode and the address of the preferred network "
      "interface is a link-local address, \"all\" is treated as a synonym for "
      "\"preferred\" and a comma-separated list is treated as \"preferred\" "
      "if it contains the preferred interface and as \"none\" if not.</p>"
    )),
  STRING("ExternalNetworkAddress", NULL, 1, "auto",
    MEMBER(externalAddressString),
    FUNCTIONS(0, uf_networkAddress, ff_free, pf_networkAddress),
    DESCRIPTION(
      "<p>This element allows explicitly overruling the network address "
      "Cyclone DDS advertises in the discovery protocol, which by default is "
      "the address of the preferred network interface "
      "(General/NetworkInterfaceAddress), to allow Cyclone DDS to communicate "
      "across a Network Address Translation (NAT) device.</p>")),
  STRING("ExternalNetworkMask", NULL, 1, "0.0.0.0",
    MEMBER(externalMaskString),
    FUNCTIONS(0, uf_string, ff_free, pf_string),
    DESCRIPTION(
      "<p>This element specifies the network mask of the external network "
      "address. This element is relevant only when an external network "
      "address (General/ExternalNetworkAddress) is explicitly configured. In "
      "this case locators received via the discovery protocol that are "
      "within the same external subnet (as defined by this mask) will be "
      "translated to an internal address by replacing the network portion of "
      "the external address with the corresponding portion of the preferred "
      "network interface address. This option is IPv4-only.</p>")),
  LIST("AllowMulticast", NULL, 1, "default",
    MEMBER(allowMulticast),
    FUNCTIONS(0, uf_allow_multicast, 0, pf_allow_multicast),
    DESCRIPTION(
      "<p>This element controls whether Cyclone DDS uses multicasts for data "
      "traffic.</p>\n"
      "<p>It is a comma-separated list of some of the following keywords: "
      "\"spdp\", \"asm\", \"ssm\", or either of \"false\" or \"true\", or "
      "\"default\".</p>\n"
      "<ul>\n"
      "<li><i>spdp</i>: "
      "enables the use of ASM (any-source multicast) for participant "
      "discovery, joining the multicast group on the discovery socket, "
      "transmitting SPDP messages to this group, but never advertising nor "
      "using any multicast address in any discovery message, thus forcing "
      "unicast communications for all endpoint discovery and user data."
      "</li>\n"
      "<li><i>asm</i>: "
      "enables the use of ASM for all traffic, including receiving SPDP but "
      "not transmitting SPDP messages via multicast"
      "</li>\n"
      "<li><i>ssm</i>: "
      "enables the use of SSM (source-specific multicast) for all non-SPDP "
      "traffic (if supported)"
      "</li>\n"
      "</ul>\n"
      "<p>When set to \"false\" all multicasting is disabled. The default, "
      "\"true\" enables full use of multicasts. Listening for multicasts can "
      "be controlled by General/MulticastRecvNetworkInterfaceAddresses.</p>\n"
      "<p>\"default\" maps on spdp if the network is a WiFi network, on true "
      "if it is a wired network</p>"),
    VALUES("false","spdp","asm","ssm","true")),
  BOOL("PreferMulticast", NULL, 1, "false",
    MEMBER(prefer_multicast),
    FUNCTIONS(0, uf_boolean, 0, pf_boolean),
    DESCRIPTION(
      "<p>When false (default) Cyclone DDS uses unicast for data whenever "
      "there a single unicast suffices. Setting this to true makes it prefer "
      "multicasting data, falling back to unicast only when no multicast "
      "address is available.</p>")),
  INT("MulticastTimeToLive", NULL, 1, "32",
    MEMBER(multicast_ttl),
    FUNCTIONS(0, uf_natint_255, 0, pf_int),
    DESCRIPTION(
      "<p>This element specifies the time-to-live setting for outgoing "
      "multicast packets.</p>"),
    RANGE("0;255")),
  BOOL("DontRoute", NULL, 1, "false",
    MEMBER(dontRoute),
    FUNCTIONS(0, uf_boolean, 0, pf_boolean),
    DESCRIPTION(
      "<p>This element allows setting the SO_DONTROUTE option for outgoing "
      "packets, to bypass the local routing tables. This is generally useful "
      "only when the routing tables cannot be trusted, which is highly "
      "unusual.</p>")),
  ENUM("UseIPv6", NULL, 1, "default",
    MEMBER(compat_use_ipv6),
    FUNCTIONS(0, uf_boolean_default, 0, pf_nop),
    DESCRIPTION("<p>Deprecated (use Transport instead)</p>"),
    VALUES("false","true","default")),
  ENUM("Transport", NULL, 1, "default",
    MEMBER(transport_selector),
    FUNCTIONS(0, uf_transport_selector, 0, pf_transport_selector),
    DESCRIPTION(
      "<p>This element allows selecting the transport to be used (udp, udp6, "
      "tcp, tcp6, raweth)</p>"),
    VALUES("default","udp","udp6","tcp","tcp6","raweth")),
  BOOL("EnableMulticastLoopback", NULL, 1, "true",
    MEMBER(enableMulticastLoopback),
    FUNCTIONS(0, uf_boolean, 0, pf_boolean),
    DESCRIPTION(
      "<p>This element specifies whether Cyclone DDS allows IP multicast "
      "packets to be visible to all DDSI participants in the same node, "
      "including itself. It must be \"true\" for intra-node multicast "
      "communications, but if a node runs only a single Cyclone DDS service "
      "and does not host any other DDSI-capable programs, it should be set "
      "to \"false\" for improved performance.</p>")),
  STRING("MaxMessageSize", NULL, 1, "14720 B",
    MEMBER(max_msg_size),
    FUNCTIONS(0, uf_memsize, 0, pf_memsize),
    DESCRIPTION(
      "<p>This element specifies the maximum size of the UDP payload that "
      "Cyclone DDS will generate. Cyclone DDS will try to maintain this limit within "
      "the bounds of the DDSI specification, which means that in some cases "
      "(especially for very low values of MaxMessageSize) larger payloads "
      "may sporadically be observed (currently up to 1192 B).</p>\n"
      "<p>On some networks it may be necessary to set this item to keep the "
      "packetsize below the MTU to prevent IP fragmentation.</p>"),
    UNIT("memsize")),
  STRING("MaxRexmitMessageSize", NULL, 1, "1456 B",
    MEMBER(max_rexmit_msg_size),
    FUNCTIONS(0, uf_memsize, 0, pf_memsize),
    DESCRIPTION(
      "<p>This element specifies the maximum size of the UDP payload that "
      "Cyclone DDS will generate for a retransmit. Cyclone DDS will try to "
      "maintain this limit within the bounds of the DDSI specification, which "
      "means that in some cases (especially for very low values) larger payloads "
      "may sporadically be observed (currently up to 1192 B).</p>\n"
      "<p>On some networks it may be necessary to set this item to keep the "
      "packetsize below the MTU to prevent IP fragmentation.</p>"),
    UNIT("memsize")),
  STRING("FragmentSize", NULL, 1, "1344 B",
    MEMBER(fragment_size),
    FUNCTIONS(0, uf_memsize16, 0, pf_memsize16),
    DESCRIPTION(
      "<p>This element specifies the size of DDSI sample fragments generated "
      "by Cyclone DDS. Samples larger than FragmentSize are fragmented into "
      "fragments of FragmentSize bytes each, except the last one, which may "
      "be smaller. The DDSI spec mandates a minimum fragment size of 1025 "
      "bytes, but Cyclone DDS will do whatever size is requested, accepting "
      "fragments of which the size is at least the minimum of 1025 and "
      "FragmentSize.</p>"),
    UNIT("memsize")),
  END_MARKER
};

#ifdef DDSI_INCLUDE_SECURITY
static struct cfgelem authentication_library_attributes[] = {
  STRING("path", NULL, 1, "dds_security_auth",
    MEMBEROF(config_omg_security_listelem, cfg.authentication_plugin.library_path),
    FUNCTIONS(0, uf_string, ff_free, pf_string),
    DESCRIPTION(
      "<p>This element points to the path of Authentication plugin library.</p>\n"
      "<p>It can be either absolute path excluding file extension "
      "( /usr/lib/dds_security_auth ) or single file without extension "
      "( dds_security_auth ).</p>\n"
      "<p>If single file is supplied, the library located by way of the "
      "current working directory, or LD_LIBRARY_PATH for Unix systems, and "
      "PATH for Windows system.</p>"
    )),
  STRING("initFunction", NULL, 1, "init_authentication",
    MEMBEROF(config_omg_security_listelem, cfg.authentication_plugin.library_init),
    FUNCTIONS(0, uf_string, ff_free, pf_string),
    DESCRIPTION(
      "<p>This element names the initialization function of Authentication "
      "plugin. This function is called after loading the plugin library for "
      "instantiation purposes. Init function must return an object that "
      "implements DDS Security Authentication interface.</p>"
    )),
  STRING("finalizeFunction", NULL, 1, "finalize_authentication",
    MEMBEROF(config_omg_security_listelem, cfg.authentication_plugin.library_finalize),
    FUNCTIONS(0, uf_string, ff_free, pf_string),
    DESCRIPTION(
      "<p>This element names the finalization function of Authentication "
      "plugin. This function is called to let the plugin release its "
      "resources.</p>"
    )),
  END_MARKER
};

static struct cfgelem access_control_library_attributes[] = {
  STRING("path", NULL, 1, "dds_security_ac",
    MEMBEROF(config_omg_security_listelem, cfg.access_control_plugin.library_path),
    FUNCTIONS(0, uf_string, ff_free, pf_string),
    DESCRIPTION(
      "<p>This element points to the path of Access Control plugin library.</p>\n"
      "<p>It can be either absolute path excluding file extension "
      "( /usr/lib/dds_security_ac ) or single file without extension "
      "( dds_security_ac ).</p>\n"
      "<p>If single file is supplied, the library located by way of the "
      "current working directory, or LD_LIBRARY_PATH for Unix systems, and "
      "PATH for Windows systems.</p>"
    )),
  STRING("initFunction", NULL, 1, "init_access_control",
    MEMBEROF(config_omg_security_listelem, cfg.access_control_plugin.library_init),
    FUNCTIONS(0, uf_string, ff_free, pf_string),
    DESCRIPTION(
      "<p>This element names the initialization function of Access Control "
      "plugin. This function is called after loading the plugin library for "
      "instantiation purposes. Init function must return an object that "
      "implements DDS Security Access Control interface.</p>"
    )),
  STRING("finalizeFunction", NULL, 1, "finalize_access_control",
    MEMBEROF(config_omg_security_listelem, cfg.access_control_plugin.library_finalize),
    FUNCTIONS(0, uf_string, ff_free, pf_string),
    DESCRIPTION(
      "<p>This element names the finalization function of Access Control "
      "plugin. This function is called to let the plugin release its "
      "resources.</p>"
    )),
  END_MARKER
};

static struct cfgelem cryptography_library_attributes[] = {
  STRING("path", NULL, 1, "dds_security_crypto",
    MEMBEROF(config_omg_security_listelem, cfg.cryptography_plugin.library_path),
    FUNCTIONS(0, uf_string, ff_free, pf_string),
    DESCRIPTION(
      "<p>This element points to the path of Cryptographic plugin library.</p>\n"
      "<p>It can be either absolute path excluding file extension "
      "( /usr/lib/dds_security_crypto ) or single file without extension "
      "( dds_security_crypto ).</p>\n"
      "<p>If single file is supplied, the library located by way of the "
      "current working directory, or LD_LIBRARY_PATH for Unix systems, and "
      "PATH for Windows systems.</p>"
    )),
  STRING("initFunction", NULL, 1, "init_crypto",
    MEMBEROF(config_omg_security_listelem, cfg.cryptography_plugin.library_init),
    FUNCTIONS(0, uf_string, ff_free, pf_string),
    DESCRIPTION(
      "<p>This element names the initialization function of Cryptographic "
      "plugin. This function is called after loading the plugin library for "
      "instantiation purposes. Init function must return an object that "
      "implements DDS Security Cryptographic interface.</p>"
    )),
  STRING("finalizeFunction", NULL, 1, "finalize_crypto",
    MEMBEROF(config_omg_security_listelem, cfg.cryptography_plugin.library_finalize),
    FUNCTIONS(0, uf_string, ff_free, pf_string),
    DESCRIPTION(
      "<p>This element names the finalization function of Cryptographic "
      "plugin. This function is called to let the plugin release its "
      "resources.</p>"
    )),
  END_MARKER
};

static struct cfgelem authentication_config_elements[] = {
  STRING("Library", authentication_library_attributes, 1, "",
    MEMBEROF(config_omg_security_listelem, cfg.authentication_plugin),
    FUNCTIONS(0, 0, 0, pf_string),
    DESCRIPTION(
      "<p>This element specifies the library to be loaded as the DDS "
      "Security Access Control plugin.</p>"
    )),
  STRING("IdentityCertificate", NULL, 1, NULL,
    MEMBEROF(config_omg_security_listelem, cfg.authentication_properties.identity_certificate),
    FUNCTIONS(0, uf_string, ff_free, pf_string),
    DESCRIPTION(
      "<p>Identity certificate that will be used for identifying all "
      "participants in the OSPL instance.<br>The content is URI to a X509 "
      "certificate signed by the IdentityCA in PEM format containing the "
      "signed public key.</p>\n"
      "<p>Supported URI schemes: file, data</p>\n"
      "<p>Examples:</p>\n"
      "<p><IdentityCertificate>file:participant1_identity_cert.pem</IdentityCertificate></p>\n"
      "<p><IdentityCertificate>data:,-----BEGIN CERTIFICATE-----<br>\n"
      "MIIDjjCCAnYCCQDCEu9...6rmT87dhTo=<br>\n"
      "-----END CERTIFICATE-----</IdentityCertificate></p>"
    )),
  STRING("IdentityCA", NULL, 1, NULL,
    MEMBEROF(config_omg_security_listelem, cfg.authentication_properties.identity_ca),
    FUNCTIONS(0, uf_string, ff_free, pf_string),
    DESCRIPTION(
      "<p>URI to the X509 certificate [39] of the Identity CA that is the "
      "signer of Identity Certificate.</p>\n"
      "<p>Supported URI schemes: file, data</p>\n"
      "<p>The file and data schemas shall refer to a X.509 v3 certificate "
      "(see X.509 v3 ITU-T Recommendation X.509 (2005) [39]) in PEM format.</p>\n"
      "<p>Examples:</p>\n"
      "<p><IdentityCA>file:identity_ca.pem</IdentityCA></p>\n"
      "<p><IdentityCA>data:,-----BEGIN CERTIFICATE-----<br>\n"
      "MIIC3DCCAcQCCQCWE5x+Z...PhovK0mp2ohhRLYI0ZiyYQ==<br>\n"
      "-----END CERTIFICATE-----</IdentityCA></p>"
    )),
  STRING("PrivateKey", NULL, 1, NULL,
    MEMBEROF(config_omg_security_listelem, cfg.authentication_properties.private_key),
    FUNCTIONS(0, uf_string, ff_free, pf_string),
    DESCRIPTION(
      "<p>URI to access the private Private Key for all of the participants "
      "in the OSPL federation.</p>\n"
      "<p>Supported URI schemes: file, data</p>\n"
      "<p>Examples:</p>\n"
      "<p><PrivateKey>file:identity_ca_private_key.pem</PrivateKey></p>\n"
      "<p><PrivateKey>data:,-----BEGIN RSA PRIVATE KEY-----<br>\n"
      "MIIEpAIBAAKCAQEA3HIh...AOBaaqSV37XBUJg==<br>\n"
      "-----END RSA PRIVATE KEY-----</PrivateKey></p>"
    )),
  STRING("Password", NULL, 1, "",
    MEMBEROF(config_omg_security_listelem, cfg.authentication_properties.password),
    FUNCTIONS(0, uf_string, ff_free, pf_string),
    DESCRIPTION(
      "<p>A password used to decrypt the private_key.</p>\n"
      "<p>The value of the password property shall be interpreted as the "
      "Base64 encoding of the AES-128 key that shall be used to decrypt the "
      "private_key using AES128-CBC.</p>\n"
      "<p>If the password property is not present, then the value supplied in "
      "the private_key property must contain the unencrypted private key.</p>"
    )),
  STRING("TrustedCADirectory", NULL, 1, "",
    MEMBEROF(config_omg_security_listelem, cfg.authentication_properties.trusted_ca_dir),
    FUNCTIONS(0, uf_string, ff_free, pf_string),
    DESCRIPTION(
      "<p>Trusted CA Directory which contains trusted CA certificates as "
      "separated files.</p>"
    )),
  BOOL("IncludeOptionalFields", NULL, 1, "false",
    MEMBEROF(config_omg_security_listelem, cfg.authentication_properties.include_optional_fields),
    FUNCTIONS(0, uf_boolean, 0, pf_boolean),
    DESCRIPTION(
      "<p>The authentication handshake tokens may contain optional fields to "
      "be included for finding interoperability problems. If this parameter "
      "is set to true the optional fields are included in the handshake token "
      "exchange.</p>"
    )),
  END_MARKER
};

static struct cfgelem access_control_config_elements[] = {
  STRING("Library", access_control_library_attributes, 1, "",
    MEMBEROF(config_omg_security_listelem, cfg.access_control_plugin),
    FUNCTIONS(0, 0, 0, pf_string),
    DESCRIPTION(
      "<p>This element specifies the library to be loaded as the "
      "DDS Security Access Control plugin.</p>"
    )),
  STRING("PermissionsCA", NULL, 1, "",
    MEMBEROF(config_omg_security_listelem, cfg.access_control_properties.permissions_ca),
    FUNCTIONS(0, uf_string, ff_free, pf_string),
    DESCRIPTION(
      "<p>URI to a X509 certificate for the PermissionsCA in PEM format.</p>\n"
      "<p>Supported URI schemes: file, data</p>\n"
      "<p>The file and data schemas shall refer to a X.509 v3 certificate "
      "(see X.509 v3 ITU-T Recommendation X.509 (2005) [39]) in PEM format.</p><br>\n"
      "<p>Examples:</p><br>\n"
      "<p><PermissionsCA>file:permissions_ca.pem</PermissionsCA></p>\n"
      "<p><PermissionsCA>file:/home/myuser/permissions_ca.pem</PermissionsCA></p><br>\n"
      "<p><PermissionsCA>data:<strong>,</strong>-----BEGIN CERTIFICATE-----</p>\n"
      "<p>MIIC3DCCAcQCCQCWE5x+Z ... PhovK0mp2ohhRLYI0ZiyYQ==</p>\n"
      "<p>-----END CERTIFICATE-----</PermissionsCA></p>"
    )),
  STRING("Governance", NULL, 1, "",
    MEMBEROF(config_omg_security_listelem, cfg.access_control_properties.governance),
    FUNCTIONS(0, uf_string, ff_free, pf_string),
    DESCRIPTION(
      "<p>URI to the shared Governance Document signed by the Permissions CA in S/MIME format</p>\n"
      "<p>URI schemes: file, data</p><br>\n"
      "<p>Examples file URIs:</p>\n"
      "<p><Governance>file:governance.smime</Governance></p>\n"
      "<p><Governance>file:/home/myuser/governance.smime</Governance></p><br>\n"
      "<p><Governance><![CDATA[data:,MIME-Version: 1.0</p>\n"
      "<p>Content-Type: multipart/signed; protocol=\"application/x-pkcs7-signature\"; micalg=\"sha-256\"; boundary=\"----F9A8A198D6F08E1285A292ADF14DD04F\"</p>\n"
      "<p>This is an S/MIME signed message </p>\n"
      "<p>------F9A8A198D6F08E1285A292ADF14DD04F</p>\n"
      "<p><?xml version=\"1.0\" encoding=\"UTF-8\"?></p>\n"
      "<p><dds xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"</p>\n"
      "<p>xsi:noNamespaceSchemaLocation=\"omg_shared_ca_governance.xsd\"></p>\n"
      "<p><domain_access_rules></p>\n"
      "<p> . . . </p>\n"
      "<p></domain_access_rules></p>\n"
      "<p></dds></p>\n"
      "<p>...</p>\n"
      "<p>------F9A8A198D6F08E1285A292ADF14DD04F</p>\n"
      "<p>Content-Type: application/x-pkcs7-signature; name=\"smime.p7s\"</p>\n"
      "<p>Content-Transfer-Encoding: base64</p>\n"
      "<p>Content-Disposition: attachment; filename=\"smime.p7s\"</p>\n"
      "<p>MIIDuAYJKoZIhv ...al5s=</p>\n"
      "<p>------F9A8A198D6F08E1285A292ADF14DD04F-]]</Governance></p>"
    )),
  STRING("Permissions", NULL, 1, "",
    MEMBEROF(config_omg_security_listelem, cfg.access_control_properties.permissions),
    FUNCTIONS(0, uf_string, ff_free, pf_string),
    DESCRIPTION(
      "<p>URI to the DomainParticipant permissions document signed by the "
      "Permissions CA in S/MIME format</p>\n"
      "<p>The permissions document specifies the permissions to be applied to a domain.</p><br>\n"
      "<p>Example file URIs:</p>\n"
      "<p><Permissions>file:permissions_document.p7s</Permissions></p>\n"
      "<p><Permissions>file:/path_to/permissions_document.p7s</Permissions></p>\n"
      "<p>Example data URI:</p>\n"
      "<p><Permissions><![CDATA[data:,.........]]</Permissions></p>"
    )),
  END_MARKER
};

static struct cfgelem cryptography_config_elements[] = {
  STRING("Library", cryptography_library_attributes, 1, "",
    MEMBEROF(config_omg_security_listelem, cfg.cryptography_plugin),
    FUNCTIONS(0, 0, 0, pf_string),
    DESCRIPTION(
      "<p>This element specifies the library to be loaded as the DDS Security Cryptographic plugin.</p>"
    )),
  END_MARKER
};

static struct cfgelem security_omg_config_elements[] = {
  GROUP("Authentication", authentication_config_elements, NULL, 1,
    NOMEMBER,
    NOFUNCTIONS,
    DESCRIPTION(
      "<p>This element configures the Authentication plugin of the DDS Security specification.</p>"
    )),
  GROUP("AccessControl", access_control_config_elements, NULL, 1,
    NOMEMBER,
    NOFUNCTIONS,
    DESCRIPTION(
      "<p>This element configures the Access Control plugin of the DDS Security specification.</p>"
    )),
  GROUP("Cryptographic", cryptography_config_elements, NULL, 1,
    NOMEMBER,
    NOFUNCTIONS,
    DESCRIPTION(
      "<p>This element configures the Cryptographic plugin of the DDS Security specification.</p>"
    )),
  END_MARKER
};
#endif /* DDSI_INCLUDE_SECURITY */

#ifdef DDSI_INCLUDE_NETWORK_PARTITIONS
static struct cfgelem networkpartition_cfgattrs[] = {
  STRING("Name", NULL, 1, NULL,
    MEMBEROF(config_networkpartition_listelem, name),
    FUNCTIONS(0, uf_string, ff_free, pf_string),
    DESCRIPTION(
      "<p>This attribute specifies the name of this Cyclone DDS network "
      "partition. Two network partitions cannot have the same name.</p>")),
  STRING("Address", NULL, 1, NULL,
    MEMBEROF(config_networkpartition_listelem, address_string),
    FUNCTIONS(0, uf_string, ff_free, pf_string),
    DESCRIPTION(
      "<p>This attribute specifies the multicast addresses associated with "
      "the network partition as a comma-separated list. Readers matching "
      "this network partition (cf. Partitioning/PartitionMappings) will "
      "listen for multicasts on all of these addresses and advertise them "
      "in the discovery protocol. The writers will select the most suitable "
      "address from the addresses advertised by the readers.</p>")),
  BOOL("Connected", NULL, 1, "true",
    MEMBEROF(config_networkpartition_listelem, connected),
    FUNCTIONS(0, uf_boolean, 0, pf_boolean),
    DESCRIPTION("<p>This attribute is a placeholder.</p>")),
  END_MARKER
};

static struct cfgelem networkpartitions_cfgelems[] = {
  STRING("NetworkPartition", networkpartition_cfgattrs, INT_MAX, 0,
    MEMBER(networkPartitions),
    FUNCTIONS(if_network_partition, 0, 0, 0),
    DESCRIPTION(
      "<p>This element defines a Cyclone DDS network partition.</p>"
    )),
  END_MARKER
};

static struct cfgelem ignoredpartitions_cfgattrs[] = {
  STRING("DCPSPartitionTopic", NULL, 1, NULL,
    MEMBEROF(config_ignoredpartition_listelem, DCPSPartitionTopic),
    FUNCTIONS(0, uf_string, ff_free, pf_string),
    DESCRIPTION(
      "<p>This attribute specifies a partition and a topic expression, "
      "separated by a single '.', that are used to determine if a given "
      "partition and topic will be ignored or not. The expressions may use "
      "the usual wildcards '*' and '?'. Cyclone DDS will consider an wildcard "
      "DCPS partition to match an expression iff there exists a string that "
      "satisfies both expressions.</p>"
    )),
  END_MARKER
};

static struct cfgelem ignoredpartitions_cfgelems[] = {
  STRING("IgnoredPartition", ignoredpartitions_cfgattrs, INT_MAX, 0,
    MEMBER(ignoredPartitions),
    FUNCTIONS(if_ignored_partition, 0, 0, 0),
    DESCRIPTION(
      "<p>This element can be used to prevent certain combinations of DCPS "
      "partition and topic from being transmitted over the network. Cyclone DDS "
      "will complete ignore readers and writers for which all DCPS "
      "partitions as well as their topic is ignored, not even creating DDSI "
      "readers and writers to mirror the DCPS ones.</p>"
    )),
  END_MARKER
};

static struct cfgelem partitionmappings_cfgattrs[] = {
  STRING("NetworkPartition", NULL, 1, NULL,
    MEMBEROF(config_partitionmapping_listelem, networkPartition),
    FUNCTIONS(0, uf_string, ff_free, pf_string),
    DESCRIPTION(
      "<p>This attribute specifies which Cyclone DDS network partition is to be "
      "used for DCPS partition/topic combinations matching the "
      "DCPSPartitionTopic attribute within this PartitionMapping element.</p>"
    )),
  STRING("DCPSPartitionTopic", NULL, 1, NULL,
    MEMBEROF(config_partitionmapping_listelem, DCPSPartitionTopic),
    FUNCTIONS(0, uf_string, ff_free, pf_string),
    DESCRIPTION(
      "<p>This attribute specifies a partition and a topic expression, "
      "separated by a single '.', that are used to determine if a given "
      "partition and topic maps to the Cyclone DDS network partition named by the "
      "NetworkPartition attribute in this PartitionMapping element. The "
      "expressions may use the usual wildcards '*' and '?'. Cyclone DDS will "
      "consider a wildcard DCPS partition to match an expression if there "
      "exists a string that satisfies both expressions.</p>"
    )),
  END_MARKER
};

static struct cfgelem partitionmappings_cfgelems[] = {
  STRING("PartitionMapping", partitionmappings_cfgattrs, INT_MAX, 0,
    MEMBER(partitionMappings),
    FUNCTIONS(if_partition_mapping, 0, 0, 0),
    DESCRIPTION(
      "<p>This element defines a mapping from a DCPS partition/topic "
      "combination to a Cyclone DDS network partition. This allows partitioning "
      "data flows by using special multicast addresses for part of the data "
      "and possibly also encrypting the data flow.</p>"
    )),
  END_MARKER
};

static struct cfgelem partitioning_cfgelems[] = {
  GROUP("NetworkPartitions", networkpartitions_cfgelems, NULL, 1,
    NOMEMBER,
    NOFUNCTIONS,
    DESCRIPTION(
      "<p>The NetworkPartitions element specifies the Cyclone DDS network "
      "partitions.</p>"
    )),
  GROUP("IgnoredPartitions", ignoredpartitions_cfgelems, NULL, 1,
    NOMEMBER,
    NOFUNCTIONS,
    DESCRIPTION(
      "<p>The IgnoredPartitions element specifies DCPS partition/topic "
      "combinations that are not distributed over the network.</p>"
    )),
  GROUP("PartitionMappings", partitionmappings_cfgelems, NULL, 1,
    NOMEMBER,
    NOFUNCTIONS,
    DESCRIPTION(
      "<p>The PartitionMappings element specifies the mapping from DCPS "
      "partition/topic combinations to Cyclone DDS network partitions.</p>"
    )),
  END_MARKER
};
#endif /* DDSI_INCLUDE_NETWORK_PARTITIONS */

#ifdef DDSI_INCLUDE_NETWORK_CHANNELS
static struct cfgelem channel_cfgelems[] = {
#ifdef DDSI_INCLUDE_BANDWIDTH_LIMITING
  STRING("DataBandwidthLimit", NULL, 1, "inf",
    MEMBEROF(config_channel_listelem, data_bandwidth_limit),
    FUNCTIONS(0, uf_bandwidth, 0, pf_bandwidth),
    DESCRIPTION(
      "<p>This element specifies the maximum transmit rate of new samples "
      "and directly related data, for this channel. Bandwidth limiting uses "
      "a leaky bucket scheme. The default value \"inf\" means Cyclone DDS imposes "
      "no limitation, the underlying operating system and hardware will "
      "likely limit the maimum transmit rate.</p>")
    UNIT("bandwidth")),
  STRING("AuxiliaryBandwidthLimit", NULL, 1, "inf",
    MEMBEROF(config_channel_listelem, auxiliary_bandwidth_limit),
    FUNCTIONS(0, uf_bandwidth, 0, pf_bandwidth),
    DESCRIPTION(
      "<p>This element specifies the maximum transmit rate of auxiliary "
      "traffic on this channel (e.g. retransmits, heartbeats, etc). "
      "Bandwidth limiting uses a leaky bucket scheme. The default value "
      "\"inf\" means Cyclone DDS imposes no limitation, the underlying operating "
      "system and hardware will likely limit the maximum transmit rate.</p>")
    UNIT("bandwidth")),
#endif
  INT("DiffServField", NULL, 1, "0",
    MEMBEROF(config_channel_listelem, diffserv_field),
    FUNCTIONS(0, uf_natint, 0, pf_int),
    DESCRIPTION(
      "<p>This element describes the DiffServ setting the channel will apply "
      "to the networking messages. This parameter determines the value of "
      "the diffserv field of the IP version 4 packets sent on this channel "
      "which allows QoS setting to be applied to the network traffic send on "
      "this channel.<br/>\n"
      "Windows platform support for setting the diffserv field is dependent "
      "on the OS version.<br/>\n"
      "For Windows version 7 or higher a new API (qWAVE) has been introduced. "
      "For these platforms the specified diffserv value is mapped to one of "
      "the support traffic types.\n"
      "The mapping is as follows: 1-8 background traffic; 9-40 excellent "
      "traffic; 41-55 audio/video traffic; 56 voice traffic; 57-63 control "
      "traffic.\n"
      "When an application is run without Administrative priveleges then "
      "only the diffserv value of 0, 8, 40 or 56 is allowed.</p>"
    )),
  END_MARKER
};

static struct cfgelem channel_cfgattrs[] = {
  STRING("Name", NULL, 1, NULL,
    MEMBEROF(config_channel_listelem, name),
    FUNCTIONS(0, uf_string, ff_free, pf_string),
    DESCRIPTION(
      "<p>This attribute specifies name of this channel. The name should "
      "uniquely identify the channel.</p>"
    )),
  INT("TransportPriority", NULL, 1, "0",
    MEMBEROF(config_channel_listelem, priority),
    FUNCTIONS(0, uf_natint, 0, pf_int),
    DESCRIPTION(
      "<p>This attribute sets the transport priority threshold for the "
      "channel. Each DCPS data writer has a \"transport_priority\" QoS and "
      "this QoS is used to select a channel for use by this writer. The "
      "selected channel is the one with the largest threshold not greater "
      "than the writer's transport priority, and if no such channel exists, "
      "the channel with the lowest threshold.</p>"
    )),
  END_MARKER
};

static struct cfgelem channels_cfgelems[] = {
  GROUP("Channel", channel_cfgelems, channel_cfgattrs, INT_MAX,
    MEMBER(channels),
    FUNCTIONS(if_channel, 0, 0, 0),
    DESCRIPTION("<p>This element defines a channel.</p>")),
  END_MARKER
};
#endif /* DDSI_INCLUDE_NETWORK_CHANNELS */

static struct cfgelem thread_properties_sched_cfgelems[] = {
  ENUM("Class", NULL, 1, "default",
    MEMBEROF(config_thread_properties_listelem, sched_class),
    FUNCTIONS(0, uf_sched_class, 0, pf_sched_class),
    DESCRIPTION(
      "<p>This element specifies the thread scheduling class "
      "(<i>realtime</i>, <i>timeshare</i> or <i>default</i>). The user may "
      "need special privileges from the underlying operating system to be "
      "able to assign some of the privileged scheduling classes.</p>"),
    VALUES("realtime","timeshare","default")),
  STRING("Priority", NULL, 1, "default",
    MEMBEROF(config_thread_properties_listelem, sched_priority),
    FUNCTIONS(0, uf_maybe_int32, 0, pf_maybe_int32),
    DESCRIPTION(
      "<p>This element specifies the thread priority (decimal integer or "
      "<i>default</i>). Only priorities that are supported by the underlying "
      "operating system can be assigned to this element. The user may need "
      "special privileges from the underlying operating system to be able to "
      "assign some of the privileged priorities.</p>"
    )),
  END_MARKER
};

static struct cfgelem thread_properties_cfgattrs[] = {
  STRING("Name", NULL, 1, NULL,
    MEMBEROF(config_thread_properties_listelem, name),
    FUNCTIONS(0, uf_string, ff_free, pf_string),
    DESCRIPTION(
      "<p>The Name of the thread for which properties are being set. The "
      "following threads exist:</p>\n"
      "<ul>\n"
      "<li><i>gc</i>: "
      "garbage collector thread involved in deleting entities;</li>\n"
      "<li><i>recv</i>: "
      "receive thread, taking data from the network and running the protocol "
      "state machine;</li>\n"
      "<li><i>dq.builtins</i>: "
      "delivery thread for DDSI-builtin data, primarily for discovery;</li>\n"
      "<li><i>lease</i>: "
      "DDSI liveliness monitoring;</li>\n"
      "<li><i>tev</i>: "
      "general timed-event handling, retransmits and discovery;</li>\n"
      "<li><i>fsm</i>: "
      "finite state machine thread for handling security handshake;</li>\n"
      "<li><i>xmit.CHAN</i>: "
      "transmit thread for channel CHAN;</li>\n"
      "<li><i>dq.CHAN</i>: "
      "delivery thread for channel CHAN;</li>\n"
      "<li><i>tev.CHAN</i>: "
      "timed-event thread for channel CHAN.</li></ul>"
    )),
  END_MARKER
};

static struct cfgelem thread_properties_cfgelems[] = {
  GROUP("Scheduling", thread_properties_sched_cfgelems, NULL, 1,
    NOMEMBER,
    NOFUNCTIONS,
    DESCRIPTION(
      "<p>This element configures the scheduling properties of the thread.</p>"
    )),
  STRING("StackSize", NULL, 1, "default",
    MEMBEROF(config_thread_properties_listelem, stack_size),
    FUNCTIONS(0, uf_maybe_memsize, 0, pf_maybe_memsize),
    DESCRIPTION(
      "<p>This element configures the stack size for this thread. The "
      "default value <i>default</i> leaves the stack size at the operating "
      "system default.</p>"),
    UNIT("memsize")),
  END_MARKER
};

static struct cfgelem threads_cfgelems[] = {
  GROUP("Thread", thread_properties_cfgelems, thread_properties_cfgattrs, INT_MAX,
    MEMBER(thread_properties),
    FUNCTIONS(if_thread_properties, 0, 0, 0),
    DESCRIPTION("<p>This element is used to set thread properties.</p>")),
  END_MARKER
};

static struct cfgelem compatibility_cfgelems[] = {
  ENUM("StandardsConformance", NULL, 1, "lax",
    MEMBER(standards_conformance),
    FUNCTIONS(0, uf_standards_conformance, 0, pf_standards_conformance),
    DESCRIPTION(
      "<p>This element sets the level of standards conformance of this "
      "instance of the Cyclone DDS Service. Stricter conformance typically means "
      "less interoperability with other implementations. Currently three "
      "modes are defined:</p>\n"
      "<ul><li><i>pedantic</i>: very strictly conform to the specification, "
      "ultimately for compliancy testing, but currently of little value "
      "because it adheres even to what will most likely turn out to be "
      "editing errors in the DDSI standard. Arguably, as long as no errata "
      "have been published it is the current text that is in effect, and "
      "that is what pedantic currently does.</li>\n"
      "<li><i>strict</i>: a slightly less strict view of the standard than "
      "does pedantic: it follows the established behaviour where the "
      "standard is obviously in error.</li>\n"
      "<li><i>lax</i>: attempt to provide the smoothest possible "
      "interoperability, anticipating future revisions of elements in the "
      "standard in areas that other implementations do not adhere to, even "
      "though there is no good reason not to.</li></ul>"),
    VALUES("lax","strict","pedantic")),
  BOOL("ExplicitlyPublishQosSetToDefault", NULL, 1, "false",
    MEMBER(explicitly_publish_qos_set_to_default),
    FUNCTIONS(0, uf_boolean, 0, pf_boolean),
    DESCRIPTION(
      "<p>This element specifies whether QoS settings set to default values "
      "are explicitly published in the discovery protocol. Implementations "
      "are to use the default value for QoS settings not published, which "
      "allows a significant reduction of the amount of data that needs to be "
      "exchanged for the discovery protocol, but this requires all "
      "implementations to adhere to the default values specified by the "
      "specifications.</p>\n"
      "<p>When interoperability is required with an implementation that does "
      "not follow the specifications in this regard, setting this option to "
      "true will help.</p>"
    )),
  ENUM("ManySocketsMode", NULL, 1, "single",
    MEMBER(many_sockets_mode),
    FUNCTIONS(0, uf_many_sockets_mode, 0, pf_many_sockets_mode),
    DESCRIPTION(
      "<p>This option specifies whether a network socket will be created for "
      "each domain participant on a host. The specification seems to assume "
      "that each participant has a unique address, and setting this option "
      "will ensure this to be the case. This is not the default.</p>\n"
      "<p>Disabling it slightly improves performance and reduces network "
      "traffic somewhat. It also causes the set of port numbers needed by "
      "Cyclone DDS to become predictable, which may be useful for firewall and "
      "NAT configuration.</p>"),
    VALUES("false","true","single","none","many")),
  BOOL("AssumeRtiHasPmdEndpoints", NULL, 1, "false",
    MEMBER(assume_rti_has_pmd_endpoints),
    FUNCTIONS(0, uf_boolean, 0, pf_boolean),
    DESCRIPTION(
      "<p>This option assumes ParticipantMessageData endpoints required by "
      "the liveliness protocol are present in RTI participants even when not "
      "properly advertised by the participant discovery protocol.</p>"
    )),
  END_MARKER
};

static struct cfgelem internal_test_cfgelems[] = {
  INT("XmitLossiness", NULL, 1, "0",
    MEMBER(xmit_lossiness),
    FUNCTIONS(0, uf_int, 0, pf_int),
    DESCRIPTION(
      "<p>This element controls the fraction of outgoing packets to drop, "
      "specified as samples per thousand.</p>"
    )),
  END_MARKER
};

static struct cfgelem internal_watermarks_cfgelems[] = {
  STRING("WhcLow", NULL, 1, "1 kB",
    MEMBER(whc_lowwater_mark),
    FUNCTIONS(0, uf_memsize, 0, pf_memsize),
    DESCRIPTION(
      "<p>This element sets the low-water mark for the Cyclone DDS WHCs, "
      "expressed in bytes. A suspended writer resumes transmitting when its "
      "Cyclone DDS WHC shrinks to this size.</p>"),
    UNIT("memsize")),
  STRING("WhcHigh", NULL, 1, "500 kB",
    MEMBER(whc_highwater_mark),
    FUNCTIONS(0, uf_memsize, 0, pf_memsize),
    DESCRIPTION(
      "<p>This element sets the maximum allowed high-water mark for the "
      "Cyclone DDS WHCs, expressed in bytes. A writer is suspended when the WHC "
      "reaches this size.</p>"),
    UNIT("memsize")),
  STRING("WhcHighInit", NULL, 1, "30 kB",
    MEMBER(whc_init_highwater_mark),
    FUNCTIONS(0, uf_maybe_memsize, 0, pf_maybe_memsize),
    DESCRIPTION(
      "<p>This element sets the initial level of the high-water mark for the "
      "Cyclone DDS WHCs, expressed in bytes.</p>"),
    UNIT("memsize")),
  BOOL("WhcAdaptive|WhcAdaptative", NULL, 1, "true",
    MEMBER(whc_adaptive),
    FUNCTIONS(0, uf_boolean, 0, pf_boolean),
    DESCRIPTION(
      "<p>This element controls whether Cyclone DDS will adapt the high-water "
      "mark to current traffic conditions, based on retransmit requests and "
      "transmit pressure.</p>"
    )),
  END_MARKER
};

static struct cfgelem internal_burstsize_cfgelems[] = {
  STRING("MaxRexmit", NULL, 1, "1 MiB",
    MEMBER(max_rexmit_burst_size),
    FUNCTIONS(0, uf_memsize, 0, pf_memsize),
    DESCRIPTION(
      "<p>This element specifies the amount of data to be retransmitted in "
      "response to one NACK.</p>"),
    UNIT("memsize")),
  STRING("MaxInitTransmit", NULL, 1, "4294967295",
    MEMBER(init_transmit_extra_pct),
    FUNCTIONS(0, uf_uint, 0, pf_uint),
    DESCRIPTION(
      "<p>This element specifies how much more than the (presumed or discovered) "
      "receive buffer size may be sent when transmitting a sample for the first "
      "time, expressed as a percentage; the remainder will then be handled via "
      "retransmits. Usually the receivers can keep up with transmitter, at least "
      "on average, and so generally it is better to hope for the best and recover. "
      "Besides, the retransmits will be unicast, and so any multicast advantage "
      "will be lost as well.</p>"),
    UNIT("memsize")),
  END_MARKER
};

static struct cfgelem control_topic_cfgattrs[] = {
  BOOL(DEPRECATED("Enable"), NULL, 1, "false",
    MEMBER(enable_control_topic),
    FUNCTIONS(0, uf_boolean, 0, pf_boolean),
    DESCRIPTION(
      "<p>This element controls whether Cyclone DDS should create a topic to "
      "control Cyclone DDS's behaviour dynamically.<p>")),
  STRING(DEPRECATED("InitialReset"), NULL, 1, "inf",
    MEMBER(initial_deaf_mute_reset),
    FUNCTIONS(0, uf_duration_inf, 0, pf_duration),
    DESCRIPTION(
      "<p>This element controls after how much time an initial deaf/mute "
      "state will automatically reset.<p>")),
  END_MARKER
};

static struct cfgelem control_topic_cfgelems[] = {
  BOOL(DEPRECATED("Deaf"), NULL, 1, "false",
    MEMBER(initial_deaf),
    FUNCTIONS(0, uf_deaf_mute, 0, pf_boolean),
    DESCRIPTION(
      "<p>This element controls whether Cyclone DDS defaults to deaf mode or to "
      "normal mode. This controls both the initial behaviour and what "
      "behaviour it auto-reverts to.</p>")),
  BOOL(DEPRECATED("Mute"), NULL, 1, "false",
    MEMBER(initial_mute),
    FUNCTIONS(0, uf_deaf_mute, 0, pf_boolean),
    DESCRIPTION(
      "<p>This element controls whether Cyclone DDS defaults to mute mode or to "
      "normal mode. This controls both the initial behaviour and what "
      "behaviour it auto-reverts to.</p>")),
  END_MARKER
};

static struct cfgelem rediscovery_blacklist_duration_attrs[] = {
  BOOL("enforce", NULL, 1, "false",
    MEMBER(prune_deleted_ppant.enforce_delay),
    FUNCTIONS(0, uf_boolean, 0, pf_boolean),
    DESCRIPTION(
      "<p>This attribute controls whether the configured time during which "
      "recently deleted participants will not be rediscovered (i.e., \"black "
      "listed\") is enforced and following complete removal of the "
      "participant in Cyclone DDS, or whether it can be rediscovered earlier "
      "provided all traces of that participant have been removed already.</p>"
    )),
  END_MARKER
};

static struct cfgelem heartbeat_interval_attrs[] = {
  STRING("min", NULL, 1, "5 ms",
    MEMBER(const_hb_intv_min),
    FUNCTIONS(0, uf_duration_inf, 0, pf_duration),
    DESCRIPTION(
      "<p>This attribute sets the minimum interval that must have passed "
      "since the most recent heartbeat from a writer, before another "
      "asynchronous (not directly related to writing) will be sent.</p>"),
    UNIT("duration_inf")),
  STRING("minsched", NULL, 1, "20 ms",
    MEMBER(const_hb_intv_sched_min),
    FUNCTIONS(0, uf_duration_inf, 0, pf_duration),
    DESCRIPTION(
      "<p>This attribute sets the minimum interval for periodic heartbeats. "
      "Other events may still cause heartbeats to go out.</p>"),
    UNIT("duration_inf")),
  STRING("max", NULL, 1, "8 s",
    MEMBER(const_hb_intv_sched_max),
    FUNCTIONS(0, uf_duration_inf, 0, pf_duration),
    DESCRIPTION(
      "<p>This attribute sets the maximum interval for periodic heartbeats.</p>"),
    UNIT("duration_inf")),
  END_MARKER
};

static struct cfgelem liveliness_monitoring_attrs[] = {
  BOOL("StackTraces", NULL, 1, "true",
    MEMBER(noprogress_log_stacktraces),
    FUNCTIONS(0, uf_boolean, 0, pf_boolean),
    DESCRIPTION(
      "<p>This element controls whether or not to write stack traces to the "
      "DDSI2 trace when a thread fails to make progress (on select platforms "
      "only).</p>")),
  STRING("Interval", NULL, 1, "1s",
    MEMBER(liveliness_monitoring_interval),
    FUNCTIONS(0, uf_duration_100ms_1hr, 0, pf_duration),
    DESCRIPTION(
      "<p>This element controls the interval at which to check whether "
      "threads have been making progress.</p>"),
    UNIT("duration"),
    RANGE("100ms;1hr")),
  END_MARKER
};

static struct cfgelem multiple_recv_threads_attrs[] = {
  INT("maxretries", NULL, 1, "4294967295",
    MEMBER(recv_thread_stop_maxretries),
    FUNCTIONS(0, uf_uint, 0, pf_uint),
    DESCRIPTION(
      "<p>Receive threads dedicated to a single socket can only be triggered "
      "for termination by sending a packet. Reception of any packet will do, "
      "so termination failure due to packet loss is exceedingly unlikely, "
      "but to eliminate all risks, it will retry as many times as specified "
      "by this attribute before aborting.</p>"
    )),
  END_MARKER
};

static struct cfgelem internal_cfgelems[] = {
  MOVED("MaxMessageSize", "CycloneDDS/General/MaxMessageSize"),
  MOVED("FragmentSize", "CycloneDDS/General/FragmentSize"),
  INT("DeliveryQueueMaxSamples", NULL, 1, "256",
    MEMBER(delivery_queue_maxsamples),
    FUNCTIONS(0, uf_uint, 0, pf_uint),
    DESCRIPTION(
      "<p>This element controls the Maximum size of a delivery queue, "
      "expressed in samples. Once a delivery queue is full, incoming samples "
      "destined for that queue are dropped until space becomes available "
      "again.</p>")),
  INT("PrimaryReorderMaxSamples", NULL, 1, "128",
    MEMBER(primary_reorder_maxsamples),
    FUNCTIONS(0, uf_uint, 0, pf_uint),
    DESCRIPTION(
      "<p>This element sets the maximum size in samples of a primary "
      "re-order administration. Each proxy writer has one primary re-order "
      "administration to buffer the packet flow in case some packets arrive "
      "out of order. Old samples are forwarded to secondary re-order "
      "administrations associated with readers in need of historical "
      "data.</p>")),
  INT("SecondaryReorderMaxSamples", NULL, 1, "128",
    MEMBER(secondary_reorder_maxsamples),
    FUNCTIONS(0, uf_uint, 0, pf_uint),
    DESCRIPTION(
      "<p>This element sets the maximum size in samples of a secondary "
      "re-order administration. The secondary re-order administration is per "
      "reader in need of historical data.</p>")),
  INT("DefragUnreliableMaxSamples", NULL, 1, "4",
    MEMBER(defrag_unreliable_maxsamples),
    FUNCTIONS(0, uf_uint, 0, pf_uint),
    DESCRIPTION(
      "<p>This element sets the maximum number of samples that can be "
      "defragmented simultaneously for a best-effort writers.</p>")),
  INT("DefragReliableMaxSamples", NULL, 1, "16",
    MEMBER(defrag_reliable_maxsamples),
    FUNCTIONS(0, uf_uint, 0, pf_uint),
    DESCRIPTION(
      "<p>This element sets the maximum number of samples that can be "
      "defragmented simultaneously for a reliable writer. This has to be "
      "large enough to handle retransmissions of historical data in addition "
      "to new samples.</p>")),
  ENUM("BuiltinEndpointSet", NULL, 1, "writers",
    MEMBER(besmode),
    FUNCTIONS(0, uf_besmode, 0, pf_besmode),
    DESCRIPTION(
      "<p>This element controls which participants will have which built-in "
      "endpoints for the discovery and liveliness protocols. Valid values "
      "are:</p>\n"
      "<ul><li><i>full</i>: all participants have all endpoints;</li>\n"
      "<li><i>writers</i>: "
      "all participants have the writers, but just one has the readers;</li>\n"
      "<li><i>minimal</i>: "
      "only one participant has built-in endpoints.</li></ul>\n"
      "<p>The default is <i>writers</i>, as this is thought to be compliant "
      "and reasonably efficient. <i>Minimal</i> may or may not be compliant "
      "but is most efficient, and <i>full</i> is inefficient but certain to "
      "be compliant. See also Internal/ConservativeBuiltinReaderStartup.</p>"),
    VALUES("full","writers","minimal")),
  BOOL("MeasureHbToAckLatency", NULL, 1, "false",
    MEMBER(meas_hb_to_ack_latency),
    FUNCTIONS(0, uf_boolean, 0, pf_boolean),
    DESCRIPTION(
      "<p>This element enables heartbeat-to-ack latency among Cyclone DDS "
      "services by prepending timestamps to Heartbeat and AckNack messages "
      "and calculating round trip times. This is non-standard behaviour. The "
      "measured latencies are quite noisy and are currently not used "
      "anywhere.</p>")),
  BOOL("UnicastResponseToSPDPMessages", NULL, 1, "true",
    MEMBER(unicast_response_to_spdp_messages),
    FUNCTIONS(0, uf_boolean, 0, pf_boolean),
    DESCRIPTION(
      "<p>This element controls whether the response to a newly discovered "
      "participant is sent as a unicasted SPDP packet, instead of "
      "rescheduling the periodic multicasted one. There is no known benefit "
      "to setting this to <i>false</i>.</p>")),
  INT("SynchronousDeliveryPriorityThreshold", NULL, 1, "0",
    MEMBER(synchronous_delivery_priority_threshold),
    FUNCTIONS(0, uf_int, 0, pf_int),
    DESCRIPTION(
      "<p>This element controls whether samples sent by a writer with QoS "
      "settings latency_budget <= SynchronousDeliveryLatencyBound and "
      "transport_priority greater than or equal to this element's value will "
      "be delivered synchronously from the \"recv\" thread, all others will "
      "be delivered asynchronously through delivery queues. This reduces "
      "latency at the expense of aggregate bandwidth.</p>")),
  STRING("SynchronousDeliveryLatencyBound", NULL, 1, "inf",
    MEMBER(synchronous_delivery_latency_bound),
    FUNCTIONS(0, uf_duration_inf, 0, pf_duration),
    DESCRIPTION(
      "<p>This element controls whether samples sent by a writer with QoS "
      "settings transport_priority >= SynchronousDeliveryPriorityThreshold "
      "and a latency_budget at most this element's value will be delivered "
      "synchronously from the \"recv\" thread, all others will be delivered "
      "asynchronously through delivery queues. This reduces latency at the "
      "expense of aggregate bandwidth.</p>"),
    UNIT("duration_inf")),
  INT("MaxParticipants", NULL, 1, "0",
    MEMBER(max_participants),
    FUNCTIONS(0, uf_natint, 0, pf_int),
    DESCRIPTION(
      "<p>This elements configures the maximum number of DCPS domain "
      "participants this Cyclone DDS instance is willing to service. 0 is "
      "unlimited.</p>")),
  INT("AccelerateRexmitBlockSize", NULL, 1, "0",
    MEMBER(accelerate_rexmit_block_size),
    FUNCTIONS(0, uf_uint, 0, pf_uint),
    DESCRIPTION(
      "<p>Proxy readers that are assumed to sill be retrieving historical "
      "data get this many samples retransmitted when they NACK something, "
      "even if some of these samples have sequence numbers outside the set "
      "covered by the NACK.</p>")),
  ENUM("RetransmitMerging", NULL, 1, "never",
    MEMBER(retransmit_merging),
    FUNCTIONS(0, uf_retransmit_merging, 0, pf_retransmit_merging),
    DESCRIPTION(
      "<p>This elements controls the addressing and timing of retransmits. "
      "Possible values are:</p>\n"
      "<ul><li><i>never</i>: retransmit only to the NACK-ing reader;</li>\n"
      "<li><i>adaptive</i>: attempt to combine retransmits needed for "
      "reliability, but send historical (transient-local) data to the "
      "requesting reader only;</li>\n"
      "<li><i>always</i>: do not distinguish between different causes, "
      "always try to merge.</li></ul>\n"
      "<p>The default is <i>never</i>. See also "
      "Internal/RetransmitMergingPeriod.</p>"),
    VALUES("never","adaptive","always")),
  STRING("RetransmitMergingPeriod", NULL, 1, "5 ms",
    MEMBER(retransmit_merging_period),
    FUNCTIONS(0, uf_duration_us_1s, 0, pf_duration),
    DESCRIPTION(
      "<p>This setting determines the size of the time window in which a "
      "NACK of some sample is ignored because a retransmit of that sample "
      "has been multicasted too recently. This setting has no effect on "
      "unicasted retransmits.</p>\n"
      "<p>See also Internal/RetransmitMerging.</p>"),
    UNIT("duration"),
    RANGE("0;1s")),
  STRING("HeartbeatInterval", heartbeat_interval_attrs, 1, "100 ms",
    MEMBER(const_hb_intv_sched),
    FUNCTIONS(0, uf_duration_inf, 0, pf_duration),
    DESCRIPTION(
      "<p>This element allows configuring the base interval for sending "
      "writer heartbeats and the bounds within which it can vary.</p>"),
    UNIT("duration_inf")),
  STRING("MaxQueuedRexmitBytes", NULL, 1, "512 kB",
    MEMBER(max_queued_rexmit_bytes),
    FUNCTIONS(0, uf_memsize, 0, pf_memsize),
    DESCRIPTION(
      "<p>This setting limits the maximum number of bytes queued for "
      "retransmission. The default value of 0 is unlimited unless an "
      "AuxiliaryBandwidthLimit has been set, in which case it becomes "
      "NackDelay * AuxiliaryBandwidthLimit. It must be large enough to "
      "contain the largest sample that may need to be retransmitted.</p>"),
    UNIT("memsize")),
  INT("MaxQueuedRexmitMessages", NULL, 1, "200",
    MEMBER(max_queued_rexmit_msgs),
    FUNCTIONS(0, uf_uint, 0, pf_uint),
    DESCRIPTION(
      "<p>This settings limits the maximum number of samples queued for "
      "retransmission.</p>"
    )),
  STRING("LeaseDuration", NULL, 1, "10 s",
    MEMBER(lease_duration),
    FUNCTIONS(0, uf_duration_ms_1hr, 0, pf_duration),
    DESCRIPTION(
      "<p>This setting controls the default participant lease duration.<p>"),
    UNIT("duration")),
  STRING("WriterLingerDuration", NULL, 1, "1 s",
    MEMBER(writer_linger_duration),
    FUNCTIONS(0, uf_duration_ms_1hr, 0, pf_duration),
    DESCRIPTION(
      "<p>This setting controls the maximum duration for which actual "
      "deletion of a reliable writer with unacknowledged data in its history "
      "will be postponed to provide proper reliable transmission.<p>"),
    UNIT("duration")),
  STRING("MinimumSocketReceiveBufferSize", NULL, 1, "default",
    MEMBER(socket_min_rcvbuf_size),
    FUNCTIONS(0, uf_maybe_memsize, 0, pf_maybe_memsize),
    DESCRIPTION(
      "<p>This setting controls the minimum size of socket receive buffers. "
      "The operating system provides some size receive buffer upon creation "
      "of the socket, this option can be used to increase the size of the "
      "buffer beyond that initially provided by the operating system. If the "
      "buffer size cannot be increased to the specified size, an error is "
      "reported.</p>\n"
      "<p>The default setting is the word \"default\", which means Cyclone DDS "
      "will attempt to increase the buffer size to 1MB, but will silently "
      "accept a smaller buffer should that attempt fail.</p>"),
    UNIT("memsize")),
  STRING("MinimumSocketSendBufferSize", NULL, 1, "64 KiB",
    MEMBER(socket_min_sndbuf_size),
    FUNCTIONS(0, uf_memsize, 0, pf_memsize),
    DESCRIPTION(
      "<p>This setting controls the minimum size of socket send buffers. "
      "This setting can only increase the size of the send buffer, if the "
      "operating system by default creates a larger buffer, it is left "
      "unchanged.</p>"),
    UNIT("memsize")),
  STRING("NackDelay", NULL, 1, "100 ms",
    MEMBER(nack_delay),
    FUNCTIONS(0, uf_duration_ms_1hr, 0, pf_duration),
    DESCRIPTION(
      "<p>This setting controls the delay between receipt of a HEARTBEAT "
      "indicating missing samples and a NACK (ignored when the HEARTBEAT "
      "requires an answer). However, no NACK is sent if a NACK had been "
      "scheduled already for a response earlier than the delay requests: "
      "then that NACK will incorporate the latest information.</p>"),
    UNIT("duration")),
  STRING("AckDelay", NULL, 1, "10 ms",
    MEMBER(ack_delay),
    FUNCTIONS(0, uf_duration_ms_1hr, 0, pf_duration),
    DESCRIPTION(
      "<p>This setting controls the delay between sending identical "
      "acknowledgements.</p>"),
    UNIT("duration")),
  STRING("AutoReschedNackDelay", NULL, 1, "1 s",
    MEMBER(auto_resched_nack_delay),
    FUNCTIONS(0, uf_duration_inf, 0, pf_duration),
    DESCRIPTION(
      "<p>This setting controls the interval with which a reader will "
      "continue NACK'ing missing samples in the absence of a response from "
      "the writer, as a protection mechanism against writers incorrectly "
      "stopping the sending of HEARTBEAT messages.</p>"),
    UNIT("duration_inf")),
  STRING("PreEmptiveAckDelay", NULL, 1, "10 ms",
    MEMBER(preemptive_ack_delay),
    FUNCTIONS(0, uf_duration_ms_1hr, 0, pf_duration),
    DESCRIPTION(
      "<p>This setting controls the delay between the discovering a remote "
      "writer and sending a pre-emptive AckNack to discover the range of "
      "data available.</p>"),
    UNIT("duration")),
  STRING("ScheduleTimeRounding", NULL, 1, "0 ms",
    MEMBER(schedule_time_rounding),
    FUNCTIONS(0, uf_duration_ms_1hr, 0, pf_duration),
    DESCRIPTION(
      "<p>This setting allows the timing of scheduled events to be rounded "
      "up so that more events can be handled in a single cycle of the event "
      "queue. The default is 0 and causes no rounding at all, i.e. are "
      "scheduled exactly, whereas a value of 10ms would mean that events are "
      "rounded up to the nearest 10 milliseconds.</p>"),
    UNIT("duration")),
#ifdef DDSI_INCLUDE_BANDWIDTH_LIMITING
  STRING("AuxiliaryBandwidthLimit", NULL, 1, "inf",
    MEMBER(auxiliary_bandwidth_limit),
    FUNCTIONS(0, uf_bandwidth, 0, pf_bandwidth),
    DESCRIPTION(
      "<p>This element specifies the maximum transmit rate of auxiliary "
      "traffic not bound to a specific channel, such as discovery traffic, "
      "as well as auxiliary traffic related to a certain channel if that "
      "channel has elected to share this global AuxiliaryBandwidthLimit. "
      "Bandwidth limiting uses a leaky bucket scheme. The default value "
      "\"inf\" means Cyclone DDS imposes no limitation, the underlying operating "
      "system and hardware will likely limit the maimum transmit rate.</p>"
    )),
#endif
  INT("DDSI2DirectMaxThreads", NULL, 1, "1",
    MEMBER(ddsi2direct_max_threads),
    FUNCTIONS(0, uf_uint, 0, pf_uint),
    DESCRIPTION(
      "<p>This element sets the maximum number of extra threads for an "
      "experimental, undocumented and unsupported direct mode.</p>")),
  BOOL("SquashParticipants", NULL, 1, "false",
    MEMBER(squash_participants),
    FUNCTIONS(0, uf_boolean, 0, pf_boolean),
    DESCRIPTION(
      "<p>This element controls whether Cyclone DDS advertises all the domain "
      "participants it serves in DDSI (when set to <i>false</i>), or rather "
      "only one domain participant (the one corresponding to the Cyclone DDS "
      "process; when set to <i>true</i>). In the latter case Cyclone DDS becomes "
      "the virtual owner of all readers and writers of all domain "
      "participants, dramatically reducing discovery traffic (a similar "
      "effect can be obtained by setting Internal/BuiltinEndpointSet to "
      "\"minimal\" but with less loss of information).</p>"
    )),
  STRING("SPDPResponseMaxDelay", NULL, 1, "0 ms",
    MEMBER(spdp_response_delay_max),
    FUNCTIONS(0, uf_duration_ms_1s, 0, pf_duration),
    DESCRIPTION(
      "<p>Maximum pseudo-random delay in milliseconds between discovering a"
      "remote participant and responding to it.</p>"),
    UNIT("duration")),
  BOOL("LateAckMode", NULL, 1, "false",
    MEMBER(late_ack_mode),
    FUNCTIONS(0, uf_boolean, 0, pf_boolean),
    DESCRIPTION(
      "<p>Ack a sample only when it has been delivered, instead of when "
      "committed to delivering it.</p>")),
  BOOL("RetryOnRejectBestEffort", NULL, 1, "false",
    MEMBER(retry_on_reject_besteffort),
    FUNCTIONS(0, uf_boolean, 0, pf_boolean),
    DESCRIPTION(
      "<p>Whether or not to locally retry pushing a received best-effort "
      "sample into the reader caches when resource limits are reached.</p>")),
  BOOL("GenerateKeyhash", NULL, 1, "false",
    MEMBER(generate_keyhash),
    FUNCTIONS(0, uf_boolean, 0, pf_boolean),
    DESCRIPTION(
      "<p>When true, include keyhashes in outgoing data for topics with "
      "keys.</p>"
    )),
  STRING("MaxSampleSize", NULL, 1, "2147483647 B",
    MEMBER(max_sample_size),
    FUNCTIONS(0, uf_memsize, 0, pf_memsize),
    DESCRIPTION(
      "<p>This setting controls the maximum (CDR) serialised size of samples "
      "that Cyclone DDS will forward in either direction. Samples larger than "
      "this are discarded with a warning.</p>"),
    UNIT("memsize")),
  BOOL("WriteBatch", NULL, 1, "false",
    MEMBER(whc_batch),
    FUNCTIONS(0, uf_boolean, 0, pf_boolean),
    DESCRIPTION(
      "<p>This element enables the batching of write operations. By default "
      "each write operation writes through the write cache and out onto the "
      "transport. Enabling write batching causes multiple small write "
      "operations to be aggregated within the write cache into a single "
      "larger write. This gives greater throughput at the expense of "
      "latency. Currently there is no mechanism for the write cache to "
      "automatically flush itself, so that if write batching is enabled, "
      "the application may have to use the dds_write_flush function to "
      "ensure that all samples are written.</p>"
    )),
  BOOL("LivelinessMonitoring", liveliness_monitoring_attrs, 1, "false",
    MEMBER(liveliness_monitoring),
    FUNCTIONS(0, uf_boolean, 0, pf_boolean),
    DESCRIPTION(
      "<p>This element controls whether or not implementation should "
      "internally monitor its own liveliness. If liveliness monitoring is "
      "enabled, stack traces can be dumped automatically when some thread "
      "appears to have stopped making progress.</p>"
    )),
  INT("MonitorPort", NULL, 1, "-1",
    MEMBER(monitor_port),
    FUNCTIONS(0, uf_int, 0, pf_int),
    DESCRIPTION(
      "<p>This element allows configuring a service that dumps a text "
      "description of part the internal state to TCP clients. By default "
      "(-1), this is disabled; specifying 0 means a kernel-allocated port is "
      "used; a positive number is used as the TCP port number.</p>"
    )),
  STRING("AssumeMulticastCapable", NULL, 1, "",
    MEMBER(assumeMulticastCapable),
    FUNCTIONS(0, uf_string, ff_free, pf_string),
    DESCRIPTION(
      "<p>This element controls which network interfaces are assumed to be "
      "capable of multicasting even when the interface flags returned by the "
      "operating system state it is not (this provides a workaround for some "
      "platforms). It is a comma-separated lists of patterns (with ? and * "
      "wildcards) against which the interface names are matched.</p>"
    )),
  BOOL("PrioritizeRetransmit", NULL, 1, "true",
    MEMBER(prioritize_retransmit),
    FUNCTIONS(0, uf_boolean, 0, pf_boolean),
    DESCRIPTION(
      "<p>This element controls whether retransmits are prioritized over new "
      "data, speeding up recovery.</p>"
    )),
  INT("UseMulticastIfMreqn", NULL, 1, "0",
    MEMBER(use_multicast_if_mreqn),
    FUNCTIONS(0, uf_int, 0, pf_int),
    DESCRIPTION("<p>Do not use.</p>")),
  BOOL("SendAsync", NULL, 1, "false",
    MEMBER(xpack_send_async),
    FUNCTIONS(0, uf_boolean, 0, pf_boolean),
    DESCRIPTION(
      "<p>This element controls whether the actual sending of packets occurs "
      "on the same thread that prepares them, or is done asynchronously by "
      "another thread.</p>"
    )),
  STRING("RediscoveryBlacklistDuration", rediscovery_blacklist_duration_attrs, 1, "0s",
    MEMBER(prune_deleted_ppant.delay),
    FUNCTIONS(0, uf_duration_inf, 0, pf_duration),
    DESCRIPTION(
      "<p>This element controls for how long a remote participant that was "
      "previously deleted will remain on a blacklist to prevent rediscovery, "
      "giving the software on a node time to perform any cleanup actions it "
      "needs to do. To some extent this delay is required internally by "
      "Cyclone DDS, but in the default configuration with the 'enforce' attribute "
      "set to false, Cyclone DDS will reallow rediscovery as soon as it has "
      "cleared its internal administration. Setting it to too small a value "
      "may result in the entry being pruned from the blacklist before Cyclone DDS "
      "is ready, it is therefore recommended to set it to at least several "
      "seconds.</p>"),
    UNIT("duration_inf")),
  ENUM("MultipleReceiveThreads", multiple_recv_threads_attrs, 1, "default",
    MEMBER(multiple_recv_threads),
    FUNCTIONS(0, uf_boolean_default, 0, pf_boolean_default),
    DESCRIPTION(
    "<p>This element controls whether all traffic is handled by a single "
    "receive thread (false) or whether multiple receive threads may be used "
    "to improve latency (true). By default it is disabled on Windows because "
    "it appears that one cannot count on being able to send packets to "
    "oneself, which is necessary to stop the thread during shutdown. "
    "Currently multiple receive threads are only used for connectionless "
    "transport (e.g., UDP) and ManySocketsMode not set to single (the "
    "default).</p>"),
    VALUES("false","true","default")),
  GROUP("ControlTopic", control_topic_cfgelems, control_topic_cfgattrs, 1,
    NOMEMBER,
    NOFUNCTIONS,
    DESCRIPTION(
      "<p>The ControlTopic element allows configured whether Cyclone DDS provides "
      "a special control interface via a predefined topic or not.<p>"
    )),
  GROUP("Test", internal_test_cfgelems, NULL, 1,
    NOMEMBER,
    NOFUNCTIONS,
    DESCRIPTION("<p>Testing options.</p>")),
  GROUP("Watermarks", internal_watermarks_cfgelems, NULL, 1,
    NOMEMBER,
    NOFUNCTIONS,
    DESCRIPTION("<p>Watermarks for flow-control.</p>")),
  GROUP("BurstSize", internal_burstsize_cfgelems, NULL, 1,
    NOMEMBER,
    NOFUNCTIONS,
    DESCRIPTION("<p>Setting for controlling the size of transmit bursts.</p>")),
  LIST("EnableExpensiveChecks", NULL, 1, "",
    MEMBER(enabled_xchecks),
    FUNCTIONS(0, uf_xcheck, 0, pf_xcheck),
    DESCRIPTION(
      "<p>This element enables expensive checks in builds with assertions "
      "enabled and is ignored otherwise. Recognised categories are:</p>\n"
      "<ul>\n"
      "<li><i>whc</i>: writer history cache checking</li>\n"
      "<li><i>rhc</i>: reader history cache checking</li>\n"
      "<p>In addition, there is the keyword <i>all</i> that enables all "
      "checks.</p>"),
    VALUES("whc","rhc","all")),
  END_MARKER
};

static struct cfgelem sizing_cfgelems[] = {
  STRING("ReceiveBufferSize", NULL, 1, "1 MiB",
    MEMBER(rbuf_size),
    FUNCTIONS(0, uf_memsize, 0, pf_memsize),
    DESCRIPTION(
      "<p>This element sets the size of a single receive buffer. Many receive "
      "buffers may be needed. The minimum workable size a little bit larger "
      "than Sizing/ReceiveBufferChunkSize, and the value used is taken as the "
      "configured value and the actual minimum workable size.</p>"),
    UNIT("memsize")),
  STRING("ReceiveBufferChunkSize", NULL, 1, "128 KiB",
    MEMBER(rmsg_chunk_size),
    FUNCTIONS(0, uf_memsize, 0, pf_memsize),
    DESCRIPTION(
      "<p>This element specifies the size of one allocation unit in the "
      "receive buffer. Must be greater than the maximum packet size by a "
      "modest amount (too large packets are dropped). Each allocation is "
      "shrunk immediately after processing a message, or freed "
      "straightaway.</p>"),
    UNIT("memsize")),
  END_MARKER
};

static struct cfgelem discovery_ports_cfgelems[] = {
  INT("Base", NULL, 1, "7400",
    MEMBER(ports.base),
    FUNCTIONS(0, uf_uint, 0, pf_uint),
    DESCRIPTION(
      "<p>This element specifies the base port number (refer to the DDSI 2.1 "
      "specification, section 9.6.1, constant PB).</p>"
    )),
  INT("DomainGain", NULL, 1, "250",
    MEMBER(ports.dg),
    FUNCTIONS(0, uf_uint, 0, pf_uint),
    DESCRIPTION(
      "<p>This element specifies the domain gain, relating domain ids to sets "
      "of port numbers (refer to the DDSI 2.1 specification, section 9.6.1, "
      "constant DG).</p>"
    )),
  INT("ParticipantGain", NULL, 1, "2",
    MEMBER(ports.pg),
    FUNCTIONS(0, uf_uint, 0, pf_uint),
    DESCRIPTION(
      "<p>This element specifies the participant gain, relating p0, "
      "participant index to sets of port numbers (refer to the DDSI 2.1 "
      "specification, section 9.6.1, constant PG).</p>"
    )),
  INT("MulticastMetaOffset", NULL, 1, "0",
    MEMBER(ports.d0),
    FUNCTIONS(0, uf_uint, 0, pf_uint),
    DESCRIPTION(
      "<p>This element specifies the port number for multicast meta traffic "
      "(refer to the DDSI 2.1 specification, section 9.6.1, constant d0).</p>"
    )),
  INT("UnicastMetaOffset", NULL, 1, "10",
    MEMBER(ports.d1),
    FUNCTIONS(0, uf_uint, 0, pf_uint),
    DESCRIPTION(
      "<p>This element specifies the port number for unicast meta traffic "
      "(refer to the DDSI 2.1 specification, section 9.6.1, constant d1).</p>"
    )),
  INT("MulticastDataOffset", NULL, 1, "1",
    MEMBER(ports.d2),
    FUNCTIONS(0, uf_uint, 0, pf_uint),
    DESCRIPTION(
      "<p>This element specifies the port number for multicast data traffic "
      "(refer to the DDSI 2.1 specification, section 9.6.1, constant d2).</p>"
    )),
  INT("UnicastDataOffset", NULL, 1, "11",
    MEMBER(ports.d3),
    FUNCTIONS(0, uf_uint, 0, pf_uint),
    DESCRIPTION(
      "<p>This element specifies the port number for unicast data traffic "
      "(refer to the DDSI 2.1 specification, section 9.6.1, constant d3).</p>"
    )),
  END_MARKER
};

static struct cfgelem tcp_cfgelems[] = {
  ENUM("Enable", NULL, 1, "default",
    MEMBER(compat_tcp_enable),
    FUNCTIONS(0, uf_boolean_default, 0, pf_nop),
    DESCRIPTION(
      "<p>This element enables the optional TCP transport - deprecated, "
      "use General/Transport instead.</p>"),
    VALUES("false","true","default")),
  BOOL("NoDelay", NULL, 1, "true",
    MEMBER(tcp_nodelay),
    FUNCTIONS(0, uf_boolean, 0, pf_boolean),
    DESCRIPTION(
      "<p>This element enables the TCP_NODELAY socket option, preventing "
      "multiple DDSI messages being sent in the same TCP request. Setting "
      "this option typically optimises latency over throughput.</p>"
    )),
  INT("Port", NULL, 1, "-1",
    MEMBER(tcp_port),
    FUNCTIONS(0, uf_dyn_port, 0, pf_int),
    DESCRIPTION(
      "<p>This element specifies the TCP port number on which Cyclone DDS accepts "
      "connections. If the port is set it is used in entity locators, "
      "published with DDSI discovery. Dynamically allocated if zero. Disabled "
      "if -1 or not configured. If disabled other DDSI services will not be "
      "able to establish connections with the service, the service can only "
      "communicate by establishing connections to other services.</p>"),
    RANGE("-1;65535")),
  STRING("ReadTimeout", NULL, 1, "2 s",
    MEMBER(tcp_read_timeout),
    FUNCTIONS(0, uf_duration_ms_1hr, 0, pf_duration),
    DESCRIPTION(
      "<p>This element specifies the timeout for blocking TCP read "
      "operations. If this timeout expires then the connection is closed.</p>"),
    UNIT("duration")),
  STRING("WriteTimeout", NULL, 1, "2 s",
    MEMBER(tcp_write_timeout),
    FUNCTIONS(0, uf_duration_ms_1hr, 0, pf_duration),
    DESCRIPTION(
      "<p>This element specifies the timeout for blocking TCP write "
      "operations. If this timeout expires then the connection is closed.</p>"),
    UNIT("duration")),
  BOOL("AlwaysUsePeeraddrForUnicast", NULL, 1, "false",
    MEMBER(tcp_use_peeraddr_for_unicast),
    FUNCTIONS(0, uf_boolean, 0, pf_boolean),
    DESCRIPTION(
      "<p>Setting this to true means the unicast addresses in SPDP packets "
      "will be ignored and the peer address from the TCP connection will be "
      "used instead. This may help work around incorrectly advertised "
      "addresses when using TCP.</p>"
    )),
  END_MARKER
};

#ifdef DDSI_INCLUDE_SSL
static struct cfgelem ssl_cfgelems[] = {
  BOOL("Enable", NULL, 1, "false",
    MEMBER(ssl_enable),
    FUNCTIONS(0, uf_boolean, 0, pf_boolean),
    DESCRIPTION("<p>This enables SSL/TLS for TCP.</p>")),
  BOOL("CertificateVerification", NULL, 1, "true",
    MEMBER(ssl_verify),
    FUNCTIONS(0, uf_boolean, 0, pf_boolean),
    DESCRIPTION(
      "<p>If disabled this allows SSL connections to occur even if an X509 "
      "certificate fails verification.</p>"
    )),
  BOOL("VerifyClient", NULL, 1, "true",
    MEMBER(ssl_verify_client),
    FUNCTIONS(0, uf_boolean, 0, pf_boolean),
    DESCRIPTION(
      "<p>This enables an SSL server checking the X509 certificate of a "
      "connecting client.</p>"
    )),
  BOOL("SelfSignedCertificates", NULL, 1, "false",
    MEMBER(ssl_self_signed),
    FUNCTIONS(0, uf_boolean, 0, pf_boolean),
    DESCRIPTION(
      "<p>This enables the use of self signed X509 certificates.</p>"
    )),
  STRING("KeystoreFile", NULL, 1, "keystore",
    MEMBER(ssl_keystore),
    FUNCTIONS(0, uf_string, ff_free, pf_string),
    DESCRIPTION(
      "<p>The SSL/TLS key and certificate store file name. The keystore must "
      "be in PEM format.</p>"
    )),
  STRING("KeyPassphrase", NULL, 1, "secret",
    MEMBER(ssl_key_pass),
    FUNCTIONS(0, uf_string, ff_free, pf_string),
    DESCRIPTION("<p>The SSL/TLS key pass phrase for encrypted keys.</p>")),
  STRING("Ciphers", NULL, 1, "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH",
    MEMBER(ssl_ciphers),
    FUNCTIONS(0, uf_string, ff_free, pf_string),
    DESCRIPTION("<p>The set of ciphers used by SSL/TLS</p>")),
  STRING("EntropyFile", NULL, 1, "",
    MEMBER(ssl_rand_file),
    FUNCTIONS(0, uf_string, ff_free, pf_string),
    DESCRIPTION("<p>The SSL/TLS random entropy file name.</p>")),
  STRING("MinimumTLSVersion", NULL, 1, "1.3",
    MEMBER(ssl_min_version),
    FUNCTIONS(0, uf_min_tls_version, 0, pf_min_tls_version),
    DESCRIPTION(
      "<p>The minimum TLS version that may be negotiated, valid values are "
      "1.2 and 1.3.</p>"
    )),
  END_MARKER
};
#endif

static struct cfgelem tp_cfgelems[] = {
  BOOL("Enable", NULL, 1, "false",
    MEMBER(tp_enable),
    FUNCTIONS(0, uf_boolean, 0, pf_boolean),
    DESCRIPTION("<p>Enable optional use of a thread pool.</p>")),
  INT("Threads", NULL, 1, "4",
    MEMBER(tp_threads),
    FUNCTIONS(0, uf_natint, 0, pf_int),
    DESCRIPTION("<p>Initial number of threads in the thread pool.</p>")),
  INT("ThreadMax", NULL, 1, "8",
    MEMBER(tp_max_threads),
    FUNCTIONS(0, uf_natint, 0, pf_int),
    DESCRIPTION("<p>Maximum number of threads in the thread pool.</p>")),
  END_MARKER
};

static struct cfgelem discovery_peer_cfgattrs[] = {
  STRING("Address", NULL, 1, NULL,
    MEMBEROF(config_peer_listelem, peer),
    FUNCTIONS(0, uf_ipv4, ff_free, pf_string),
    DESCRIPTION(
      "<p>This element specifies an IP address to which discovery packets "
      "must be sent, in addition to the default multicast address (see also "
      "General/AllowMulticast). Both a hostnames and a numerical IP address "
      "is accepted; the hostname or IP address may be suffixed with :PORT to "
      "explicitly set the port to which it must be sent. Multiple Peers may "
      "be specified.</p>"
    )),
  END_MARKER
};

static struct cfgelem discovery_peers_group_cfgelems[] = {
  GROUP("Peer", NULL, discovery_peer_cfgattrs, INT_MAX,
    MEMBER(peers_group),
    FUNCTIONS(if_peer, 0, 0, 0),
    DESCRIPTION(
      "<p>This element statically configures an addresses for discovery.</p>"
    )),
  END_MARKER
};

static struct cfgelem discovery_peers_cfgelems[] = {
  GROUP("Peer", NULL, discovery_peer_cfgattrs, INT_MAX,
    MEMBER(peers),
    FUNCTIONS(if_peer, 0, 0, 0),
    DESCRIPTION(
      "<p>This element statically configures an addresses for discovery.</p>"
    )),
  GROUP("Group", discovery_peers_group_cfgelems, NULL, 1,
    NOMEMBER,
    NOFUNCTIONS,
    DESCRIPTION(
      "<p>This element statically configures a fault tolerant group of "
      "addresses for discovery. Each member of the group is tried in "
      "sequence until one succeeds.</p>"
    ),
    MAXIMUM(0)), /* Group element can occur more than once, but 1 is required
                    because of the way its processed (for now) */
  END_MARKER
};

static struct cfgelem discovery_cfgelems[] = {
  STRING("Tag", NULL, 0, "",
    MEMBER(domainTag),
    FUNCTIONS(0, uf_string, ff_free, pf_string),
    DESCRIPTION(
      "<p>String extension for domain id that remote participants must match "
      "to be discovered.</p>"
    )),
  STRING("ExternalDomainId", NULL, 1, "default",
    MEMBER(extDomainId),
    FUNCTIONS(0, uf_maybe_int32, 0, pf_maybe_int32),
    DESCRIPTION(
      "<p>An override for the domain id, to be used in discovery and for "
      "determining the port number mapping. This allows creating multiple "
      "domains in a single process while making them appear as a single "
      "domain on the network. The value \"default\" disables the override.</p>"
    )),
  STRING("DSGracePeriod", NULL, 1, "30 s",
    MEMBER(ds_grace_period),
    FUNCTIONS(0, uf_duration_inf, 0, pf_duration),
    DESCRIPTION(
      "<p>This setting controls for how long endpoints discovered via a "
      "Cloud discovery service will survive after the discovery service "
      "disappeared, allowing reconnect without loss of data when the "
      "discovery service restarts (or another instance takes over).</p>"),
    UNIT("duration_inf")),
  GROUP("Peers", discovery_peers_cfgelems, NULL, 1,
    NOMEMBER,
    NOFUNCTIONS,
    DESCRIPTION(
      "<p>This element statically configures addresses for discovery.</p>"
    )),
  STRING("ParticipantIndex", NULL, 1, "none",
    MEMBER(participantIndex),
    FUNCTIONS(0, uf_participantIndex, 0, pf_participantIndex),
    DESCRIPTION(
      "<p>This element specifies the DDSI participant index used by this "
      "instance of the Cyclone DDS service for discovery purposes. Only one such "
      "participant id is used, independent of the number of actual "
      "DomainParticipants on the node. It is either:</p>\n"
      "<ul><li><i>auto</i>: which will attempt to automatically determine an "
      "available participant index "
      "(see also Discovery/MaxAutoParticipantIndex), or</li>\n"
      "<li>a non-negative integer less than 120, or</li>\n"
      "<li><i>none</i>:, which causes it to use arbitrary port numbers for "
      "unicast sockets which entirely removes the constraints on the "
      "participant index but makes unicast discovery impossible.</li></ul>\n"
      "<p>The default is <i>auto</i>. The participant index is part of the "
      "port number calculation and if predictable port numbers are needed "
      "and fixing the participant index has no adverse effects, it is "
      "recommended that the second be option be used.</p>"
    )),
  INT("MaxAutoParticipantIndex", NULL, 1, "9",
    MEMBER(maxAutoParticipantIndex),
    FUNCTIONS(0, uf_natint, 0, pf_int),
    DESCRIPTION(
      "<p>This element specifies the maximum DDSI participant index selected "
      "by this instance of the Cyclone DDS service if the "
      "Discovery/ParticipantIndex is \"auto\".</p>"
    )),
  STRING("SPDPMulticastAddress", NULL, 1, "239.255.0.1",
    MEMBER(spdpMulticastAddressString),
    FUNCTIONS(0, uf_ipv4, ff_free, pf_string),
    DESCRIPTION(
      "<p>This element specifies the multicast address that is used as the "
      "destination for the participant discovery packets. In IPv4 mode the "
      "default is the (standardised) 239.255.0.1, in IPv6 mode it becomes "
      "ff02::ffff:239.255.0.1, which is a non-standardised link-local "
      "multicast address.</p>"
    )),
  STRING("SPDPInterval", NULL, 1, "30 s",
    MEMBER(spdp_interval),
    FUNCTIONS(0, uf_duration_ms_1hr, 0, pf_duration),
    DESCRIPTION(
      "<p>This element specifies the interval between spontaneous "
      "transmissions of participant discovery packets.</p>"),
    UNIT("duration")),
  STRING("DefaultMulticastAddress", NULL, 1, "auto",
    MEMBER(defaultMulticastAddressString),
    FUNCTIONS(0, uf_networkAddress, 0, pf_networkAddress),
    DESCRIPTION(
      "<p>This element specifies the default multicast address for all "
      "traffic other than participant discovery packets. It defaults to "
      "Discovery/SPDPMulticastAddress.</p>"
    )),
  GROUP("Ports", discovery_ports_cfgelems, NULL, 1,
    NOMEMBER,
    NOFUNCTIONS,
    DESCRIPTION(
      "<p>The Ports element allows specifying various parameters related to "
      "the port numbers used for discovery. These all have default values "
      "specified by the DDSI 2.1 specification and rarely need to be "
      "changed.</p>"
    )),
  END_MARKER
};

static struct cfgelem tracing_cfgelems[] = {
  LIST("Category|EnableCategory", NULL, 1, "",
    NOMEMBER,
    FUNCTIONS(0, uf_tracemask, 0, pf_tracemask),
    DESCRIPTION(
      "<p>This element enables individual logging categories. These are "
      "enabled in addition to those enabled by Tracing/Verbosity. Recognised "
      "categories are:</p>\n"
      "<ul>\n"
      "<li><i>fatal</i>: all fatal errors, errors causing immediate termination</li>\n"
      "<li><i>error</i>: failures probably impacting correctness but not necessarily causing immediate termination</li>\n"
      "<li><i>warning</i>: abnormal situations that will likely not impact correctness</li>\n"
      "<li><i>config</i>: full dump of the configuration</li>\n"
      "<li><i>info</i>: general informational notices</li>\n"
      "<li><i>discovery</i>: all discovery activity</li>\n"
      "<li><i>data</i>: include data content of samples in traces</li>\n"
      "<li><i>radmin</i>: receive buffer administration</li>\n"
      "<li><i>timing</i>: periodic reporting of CPU loads per thread</li>\n"
      "<li><i>traffic</i>: periodic reporting of total outgoing data</li>\n"
      "<li><i>whc</i>: tracing of writer history cache changes</li>\n"
      "<li><i>tcp</i>: tracing of TCP-specific activity</li>\n"
      "<li><i>topic</i>: tracing of topic definitions</li>\n"
      "<li><i>plist</i>: tracing of discovery parameter list interpretation</li>"
      "</ul>\n"
      "<p>In addition, there is the keyword <i>trace</i> that enables all "
      "but <i>radmin</i>, <i>topic</i>, <i>plist</i> and <i>whc</i></p>.\n"
      "<p>The categorisation of tracing output is incomplete and hence most "
      "of the verbosity levels and categories are not of much use in the "
      "current release. This is an ongoing process and here we describe the "
      "target situation rather than the current situation. Currently, the "
      "most useful is <i>trace</i>.</p>"),
    VALUES(
      "fatal","error","warning","info","config","discovery","data","radmin",
      "timing","traffic","topic","tcp","plist","whc","throttle","rhc",
      "content","trace"
    )),
  ENUM("Verbosity", NULL, 1, "none",
    NOMEMBER,
    FUNCTIONS(0, uf_verbosity, 0, pf_nop),
    DESCRIPTION(
      "<p>This element enables standard groups of categories, based on a "
      "desired verbosity level. This is in addition to the categories "
      "enabled by the Tracing/Category setting. Recognised verbosity levels "
      "and the categories they map to are:</p>\n"
      "<ul><li><i>none</i>: no Cyclone DDS log</li>\n"
      "<li><i>severe</i>: error and fatal</li>\n"
      "<li><i>warning</i>: <i>severe</i> + warning</li>\n"
      "<li><i>info</i>: <i>warning</i> + info</li>\n"
      "<li><i>config</i>: <i>info</i> + config</li>\n"
      "<li><i>fine</i>: <i>config</i> + discovery</li>\n"
      "<li><i>finer</i>: <i>fine</i> + traffic and timing</li>\n"
      "<li><i>finest</i>: <i>finer</i> + trace</li></ul>\n"
      "<p>While <i>none</i> prevents any message from being written to a "
      "DDSI2 log file.</p>\n"
      "<p>The categorisation of tracing output is incomplete and hence most "
      "of the verbosity levels and categories are not of much use in the "
      "current release. This is an ongoing process and here we describe the "
      "target situation rather than the current situation. Currently, the "
      "most useful verbosity levels are <i>config</i>, <i>fine</i> and "
      "<i>finest</i>.</p>"),
    VALUES(
      "finest","finer","fine","config","info","warning","severe","none"
    )),
  STRING("OutputFile", NULL, 1, "cyclonedds.log",
    MEMBER(tracefile),
    FUNCTIONS(0, uf_tracingOutputFileName, ff_free, pf_string),
    DESCRIPTION(
      "<p>This option specifies where the logging is printed to. Note that "
      "<i>stdout</i> and <i>stderr</i> are treated as special values, "
      "representing \"standard out\" and \"standard error\" respectively. No "
      "file is created unless logging categories are enabled using the "
      "Tracing/Verbosity or Tracing/EnabledCategory settings.</p>"
    )),
  BOOL("AppendToFile", NULL, 1, "false",
    MEMBER(tracingAppendToFile),
    FUNCTIONS(0, uf_boolean, 0, pf_boolean),
    DESCRIPTION(
      "<p>This option specifies whether the output is to be appended to an "
      "existing log file. The default is to create a new log file each time, "
      "which is generally the best option if a detailed log is generated.</p>"
    )),
  STRING("PacketCaptureFile", NULL, 1, "",
    MEMBER(pcap_file),
    FUNCTIONS(0, uf_string, ff_free, pf_string),
    DESCRIPTION(
      "<p>This option specifies the file to which received and sent packets "
      "will be logged in the \"pcap\" format suitable for analysis using "
      "common networking tools, such as WireShark. IP and UDP headers are "
      "fictitious, in particular the destination address of received packets. "
      "The TTL may be used to distinguish between sent and received packets: "
      "it is 255 for sent packets and 128 for received ones. Currently IPv4 "
      "only.</p>"
    )),
  END_MARKER
};

static struct cfgelem domain_cfgattrs[] = {
  STRING("Id", NULL, 0, "any",
    MEMBER(domainId),
    FUNCTIONS(0, uf_domainId, 0, pf_domainId),
    DESCRIPTION(
      "<p>Domain id this configuration applies to, or \"any\" if it applies "
      "to all domain ids.</p>"
    )),
  END_MARKER
};

static struct cfgelem domain_cfgelems[] = {
  MOVED("Id", "CycloneDDS/Domain[@Id]"),
  GROUP("General", general_cfgelems, NULL, 1,
    NOMEMBER,
    NOFUNCTIONS,
    DESCRIPTION(
      "<p>The General element specifies overall Cyclone DDS service settings.</p>"
    )),
#ifdef DDSI_INCLUDE_SECURITY
  GROUP("Security|DDSSecurity", security_omg_config_elements, NULL, INT_MAX,
    MEMBER(omg_security_configuration),
    FUNCTIONS(if_omg_security, 0, 0, 0),
    DESCRIPTION(
      "<p>This element is used to configure Cyclone DDS with the DDS Security "
      "specification plugins and settings.</p>"
    ),
    MAXIMUM(1)), /* Security must occur at most once, but INT_MAX is required
                    because of the way its processed (for now) */
#endif
#ifdef DDSI_INCLUDE_NETWORK_PARTITIONS
  GROUP("Partitioning", partitioning_cfgelems, NULL, 1,
    NOMEMBER,
    NOFUNCTIONS,
    DESCRIPTION(
      "<p>The Partitioning element specifies Cyclone DDS network partitions and "
      "how DCPS partition/topic combinations are mapped onto the network "
      "partitions.</p>"
    )),
#endif
#ifdef DDSI_INCLUDE_NETWORK_CHANNELS
  GROUP("Channels", channels_cfgelems, NULL, 1,
    NOMEMBER,
    NOFUNCTIONS,
    DESCRIPTION(
      "<p>This element is used to group a set of channels. The channels are "
      "independent data paths through Cyclone DDS and by using separate threads "
      "and setting their priorities appropriately, chanenls can be used to "
      "map transport priorities to operating system scheduler priorities, "
      "ensuring system-wide end-to-end priority preservation.</p>"
    )),
#endif
  GROUP("Threads", threads_cfgelems, NULL, 1,
    NOMEMBER,
    NOFUNCTIONS,
    DESCRIPTION(
      "<p>This element is used to set thread properties.</p>"
    )),
  GROUP("Sizing", sizing_cfgelems, NULL, 1,
    NOMEMBER,
    NOFUNCTIONS,
    DESCRIPTION(
      "<p>The Sizing element specifies a variety of configuration settings "
      "dealing with expected system sizes, buffer sizes, &c.</p>"
    )),
  GROUP("Compatibility", compatibility_cfgelems, NULL, 1,
    NOMEMBER,
    NOFUNCTIONS,
    DESCRIPTION(
      "<p>The Compatibility elements allows specifying various settings "
      "related to compatability with standards and with other DDSI "
      "implementations.</p>"
    )),
  GROUP("Discovery", discovery_cfgelems, NULL, 1,
    NOMEMBER,
    NOFUNCTIONS,
    DESCRIPTION(
      "<p>The Discovery element allows specifying various parameters related "
      "to the discovery of peers.</p>"
    )),
  GROUP("Tracing", tracing_cfgelems, NULL, 1,
    NOMEMBER,
    NOFUNCTIONS,
    DESCRIPTION(
      "<p>The Tracing element controls the amount and type of information "
      "that is written into the tracing log by the DDSI service. This is "
      "useful to track the DDSI service during application development.</p>"
    )),
  GROUP("Internal|Unsupported", internal_cfgelems, NULL, 1,
    NOMEMBER,
    NOFUNCTIONS,
    DESCRIPTION(
      "<p>The Internal elements deal with a variety of settings that "
      "evolving and that are not necessarily fully supported. For the vast "
      "majority of the Internal settings, the functionality per-se is "
      "supported, but the right to change the way the options control the "
      "functionality is reserved. This includes renaming or moving "
      "options.</p>"
    )),
  GROUP("TCP", tcp_cfgelems, NULL, 1,
    NOMEMBER,
    NOFUNCTIONS,
    DESCRIPTION(
      "<p>The TCP element allows specifying various parameters related to "
      "running DDSI over TCP.</p>"
    )),
  GROUP("ThreadPool", tp_cfgelems, NULL, 1,
    NOMEMBER,
    NOFUNCTIONS,
    DESCRIPTION(
      "<p>The ThreadPool element allows specifying various parameters "
      "related to using a thread pool to send DDSI messages to multiple "
      "unicast addresses (TCP or UDP).</p>"
    )),
#ifdef DDSI_INCLUDE_SSL
  GROUP("SSL", ssl_cfgelems, NULL, 1,
    NOMEMBER,
    NOFUNCTIONS,
    DESCRIPTION(
      "<p>The SSL element allows specifying various parameters related to "
      "using SSL/TLS for DDSI over TCP.</p>"
    )),
#endif
  END_MARKER
};

static struct cfgelem root_cfgelems[] = {
  GROUP("Domain", domain_cfgelems, domain_cfgattrs, 1,
    NOMEMBER,
    NOFUNCTIONS,
    DESCRIPTION(
      "<p>The General element specifying Domain related settings.</p>"
    )),
  MOVED("General", "CycloneDDS/Domain/General"),
#if DDSI_INCLUDE_NETWORK_PARTITIONS
  MOVED("Partitioning", "CycloneDDS/Domain/Partitioning"),
#endif
#if DDSI_INCLUDE_NETWORK_CHANNELS
  MOVED("Channels", "CycloneDDS/Domain/Channels"),
#endif
  MOVED("Threads", "CycloneDDS/Domain/Threads"),
  MOVED("Sizing", "CycloneDDS/Domain/Sizing"),
  MOVED("Compatibility", "CycloneDDS/Domain/Compatibility"),
  MOVED("Discovery", "CycloneDDS/Domain/Discovery"),
  MOVED("Tracing", "CycloneDDS/Domain/Tracing"),
  MOVED("Internal|Unsupported", "CycloneDDS/Domain/Internal"),
  MOVED("TCP", "CycloneDDS/Domain/TCP"),
  MOVED("ThreadPool", "CycloneDDS/Domain/ThreadPool"),
#if DDSI_INCLUDE_SECURITY
  MOVED("DDSSecurity", "CycloneDDS/Domain/Security"),
#endif
#if DDSI_INCLUDE_SSL
  MOVED("SSL", "CycloneDDS/Domain/SSL"),
#endif
  MOVED("DDSI2E|DDSI2", "CycloneDDS/Domain"),
  END_MARKER
};

static struct cfgelem root_cfgattrs[] = {
  NOP("xmlns"),
  NOP("xmlns:xsi"),
  NOP("xsi:schemaLocation"),
  NOP("xsi:noNamespaceSchemaLocation"),
  END_MARKER
};

static struct cfgelem cyclonedds_root_cfgelems[] = {
  GROUP("CycloneDDS", root_cfgelems, root_cfgattrs, 1,
    NOMEMBER,
    NOFUNCTIONS,
    DESCRIPTION("CycloneDDS configuration")),
  END_MARKER
};

#endif /* DDSI_CFGELEMS_H */
