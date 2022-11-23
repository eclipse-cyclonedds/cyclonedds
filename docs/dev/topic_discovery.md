# Topic Discovery

Topic discovery enables the exchange of topic information between endpoints using the built-in topic announcer and detector endpoints, as described in the DDS RTPS Specification section 8.5. Topic discovery is based on the SEDP protocol.

This document describes the design of the topic discovery implementation in Cyclone DDS. First the types (entities) that are used in topic discovery in the DDSC and DDSI layers are described. Next section explains the design of topic discovery using the DDS SEDP protocol and after that the API functions and built-in writers are described.

## DDSC types

In the DDSC layer, the following types are related to topic discovery:

### dds_topic

A *dds_topic* has a counted reference to a *dds_ktopic*, which stores the type name and QoS for a topic. Topics that share the same type name and QoS have a reference to the same dds_ktopic. A topic also has a reference to the DDSI *sertype* it is associated with.

### dds_ktopic

A *dds_ktopic* maps the topic name to its corresponding type name and QoS, and contains a mapping to the associated DDSI topic entities. The mapping to DDSI topics uses the type identifier (of the sertype associated with the dds_topic) as the key and stores a pointer to the DDSI topic entity and the (DDSI) GUID of this entity.

dds_ktopics are scoped to a participant: a dds_participant has an AVL tree to keep track of the ktopics

## DDSI types

This section describes the DDSI types that are used to represent local and discovered topics.

### ddsi_topic_definition

The topic discovery implementation introduces the *ddsi_topic_definition* type. A topic definition has a key, which is a hash value calculated from its type identifier and QoS (the latter also contains the topic name and type name for the topic). It also contains a reference to a DDSI sertype, a QoS struct and a type identifier. Topic definitions are stored in a domain scoped hash table (in the domain global variables).

### topic

A DDSI *topic* has a reference to a topic definition and a (back) reference to the participant. A DDSI topic is an entity: it has an entity_common element and is stored in the (domain scoped) entity index. As the DDS specification does not provide an entity kind for DDSI topic entities, vendor specific entity kinds `DDSI_ENTITYID_KIND_CYCLONE_TOPIC_BUILTIN` and `DDSI_ENTITYID_KIND_CYCLONE_TOPIC_USER` are used (different kinds for built-in and user topic are required here because `DDSI_ENTITYID_SOURCE_BUILTIN` and `DDSI_ENTITYID_SOURCE_VENDOR` cannot be combined).

Multiple dds_topics can share a single DDSI topic entity. In that case the topics need to share their dds_ktopic, so they have the same type name and the same QoS. In case the type identifier of their types is also equal, the topics will get a reference to the same DDSI topic. In case the type identifiers are different, a DDSI topic will be created for each type identifier.

### proxy_topic

Topics that are discovered via received SEDP messages are stored as DDSI proxy topics. A proxy topic is not a DDSI entity: proxy topics are stored in a list in the proxy participant they belong to. A proxy topic object has an entity ID field that contains the entity ID part of its GUID (the prefix for the GUID is equal to the prefix of the participants GUID, so the complete GUID can be reconstructed using the 2 parts). A proxy topic also has a reference to a topic definition, which can be shared with other (proxy) topics.

Proxy topic GUIDs are added in the proxy GUID list in the type lookup meta-data records, to keep track of the proxy topics that are referring to the type lookup meta-data (see type discovery documentation).

## SEDP for topic discovery

Topic discovery in Cyclone is implemented by SEDP writers and readers that exchange the topic information. The key for the SEDP samples is the topic definition hash that is described above. This key is sent in a vendor specific plist parameter `DDSI_PID_CYCLONE_TOPIC_GUID` (vendor specific parameter 0x1b). The endpoint GUID parameter (as used for endpoint discovery) is not used here, because the value is a (16 bytes) hash value and not a GUID.

Introducing a new DDSI topic triggers writing a SEDP (topic) sample. As DDSI topics can be shared over multiple dds_topics, this does not necessarily mean that for every dds_topic that is created in a node an SEDP sample will be written. E.g. when introducing a new dds_topic by calling the dds_find_topic function, this topic will be re-using an existing DDSI topic (as the topic is already known in the node).
The built-in endpoints for exchanging topic discovery information can be enabled/disabled via the configuration option `//CycloneDDS/Domain/Discovery/EnableTopicDiscoveryEndpoints`. The default value of this setting is _disabled_, so there will be no overhead of topic discovery data exchange unless this option is explicitly enabled.

### The built-in DCPSTopic topic

Topic discovery uses a local built-in (orphan) writer that writes a DCPSTopic sample when a new topic is discovered. A _new topic_ in this context means that the topic definition for the topic (that was created locally or discovered by SEDP) was not yet in the topic definition administration, i.e. has a key that was not found in the global topic definition hash table. Subsequent topics (local or discovered) that are using the same topic definition are not reported by the built-in topics writer (possibly a new built-in writer that reports all topic instances may be added in a future release of Cyclone).

Similar to the built-in publication and subscription writers, the topics writer is optimized so that it only creates and returns a sample when a reader requests data. The writer has a custom writer history cache implementation that enumerates the current set of topic definitions (for all participants) at the moment data is requested (i.e. a reader reads the data). This reduces the overhead of topic discovery significantly when there are no readers for this topic.

### Find topic API

The API for finding a topic is split into two functions: `dds_find_topic_locally` and `dds_find_topic_globally`. The former can be used to search for a topic that was locally created and the latter will search for topics that are discovered using the built-in SEDP endpoint, as well as for locally created topics. Both functions take an optional time-out parameter (can be 0 to disable) to allow waiting for the topic to become available in case no topic is found at the moment of calling the find function.

The time-out mechanism is implemented using a condition variable that is triggered when new topic definitions are registered. When triggered, the find topic implementation will search again in the updated set of topic definitions.

When a topic definition is found, the find topic implementation will look for the sertype. In case the topic that is found is a local topic, the sertype from that topic is used to create the topic that will be returned from the find function call. In case a remote (discovered) topic is found, the sertype may not be known. In that case, `dds_domain_resolve_type` will be used to retrieve the type using the type-identifier as key.

The dds_find_topic functions require a dds_entity as first argument. This can be either a dds_domain or a dds_participant. In case a domain is provided, the topic that is found is created for the first (in the entity index) participant for this domain. If a participant is provided, the topic will be created for that participant.
