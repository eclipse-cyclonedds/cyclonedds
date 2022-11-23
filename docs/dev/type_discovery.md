# Type Discovery

Type discovery in Cyclone is based on the DDS-XTypes type discovery, as described in section 7.6.3 of the DDS-XTypes specification.

Note: as Cyclone currently (end 2020) does not yet support the XTypes type system, the implementation of type discovery is based on the existing type system in Cyclone and is not interoperable with other vendors.

## Type Identifiers

As part of the type discovery implementation a _type identifier_ for a DDSI sertype is introduced. A type identifier field is added to the (proxy) endpoints and topics in DDSI, which stores the identifier of the type used by the endpoint. The type identifier is retrieved by using the `typeid_hash` function that is part of the `ddsi_sertype_ops` interface. Currently this hash function is only implemented for `ddsi_sertype_default`, as the other implementations in Cyclone (*sertype_builtin_topic*, *sertype_plist* and *sertype_pserop*) are not used for application types and these types are not exchanged using the type discovery protocol.

The `ddsi_sertype_default` implementation of the sertype interface uses a MD5 hash of the (little endian) serialized sertype as the type identifier. This simplified approach (compared to TypeIdentifier and TypeObject definition in the XTypes specification) does not allow to include full type information in the hash (as is the case for the XTypes type system for certain types), and it also does not allow assignability checking for two type identifiers (see below).

## Type resolving

Discovery information (SEDP messages) for topics, publications, and subscriptions contains the type identifier and not the full type information. This allows remote nodes to identify the type used by a proxy reader/writer/topic, without the overhead of sending a full type descriptor over the wire. With the type identifier a node can do a lookup of the type in its local type lookup meta-data administration, which is implemented as a domain scoped hash table (`ddsi_domaingv.tl_admin`).

A type lookup request can be triggered by the application, using the API function `dds_domain_resolve_type` or by the endpoint matching code (explained below). The API function for resolving a type takes an optional time-out, that allows the function call to wait for the requested type to become available. This is implemented by a condition variable `ddsi_domaingv.tl_resolved_cond`, which is triggered by the type lookup reply handler when type information is received.

The type discovery implementation adds a set of built-in endpoints to a participant that can be used to lookup type information: the type lookup request reader/writer and the type lookup response reader/writer (see section 7.6.3.3 of the DDS XTypes specification). A type lookup request message contains a list of type identifiers to be resolved. A node that receives the request (and has a type lookup response writer) writes a reply with the serialized type information from its own type administration. Serializing and deserializing a sertype is also part of the sertype ops interface, using the serialize and deserialize functions. For `ddsi_sertype_default` (the only sertype implementation that currently supports this) the generic plist serializer is used using a predefined sequence of ops for serializing the sertype.

Note: In the current type discovery implementation a type lookup request is sent to all nodes and any node that reads this request writes a type lookup reply message (in which the set of reply types can be empty if none of the request type ids are known in that node). This may be optimized in a future release, sending the request only to the node that has sent that specific type identifier in one of its SEDP messages.

## QoS Matching

The type discovery implementation introduces a number of additional checks in QoS matching in DDSI (function `ddsi_qos_match_mask_p`). After the check for matching _topic name_ and _type name_ for reader and writer, the matching function checks if the type information is resolved for the type identifiers of both endpoints. In case any of the types (typically from the proxy endpoint) is not resolved, matching is delayed and a type lookup request for that type identifier is sent.

An incoming type lookup reply triggers the (domain scoped) `tl_resolved_cond` condition variable (so that the type lookup API function can check if the requested type is resolved) and triggers the endpoint matching for all proxy endpoints that are using one of the resolved types. This list of proxy endpoints is retrieved from the type lookup meta-data administration. For each of these proxy endpoints the function `ddsi_update_proxy_endpoint_matching` is called, which tries to connect the proxy endpoint to local endpoints.
