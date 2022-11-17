# DDS Security effort

ADLink has decided to donate their Vortex OpenSplice DDS Security
implementation to the Cyclone DDS project. However, that will not be a simple
code drop.

This document catches all the work that is foreseen to port Vortex OpenSplice
DDS Security to Cyclone DDS.

This document can be removed when DDS Security has been implemented.

**Table of contents**
- [Definition of done](#done)
- [Footprint](#footprint)
- [Multi process testing (done)](#testing)
- [Runtime library loading (done)](#loading)
- [Hopscotch utility (done)](#hopscotch)
- [FSM utility (in progress)](#fsm)
- [Port DDS Security plugin API (done)](#port-api)
- [De-Serializing messages in DDSI (done)](#deserializing)
- [De-Serializing security message parameters in DDSI (done)](#deserializing_plist)
- [Port DDS Security builtin plugins (in progress)](#port-plugins)
- [Port DDSI DDS Security (in progress)](#port-ddsi)
- [Move configuration (in progress)](#Move-configuration)
- [Failure handling](#failures)
- [Multiple configurations](#multiple-configurations)
- [Example](#example)
- [QosProvider](#qosprovider)
- [Data Tags (optional)](#datatags)


## Definition of done<a name="done" />

When this document tells that a certain aspect is 'done', it means that it
has been accepted into the security branch of the cyclonedds repository
(https://github.com/eclipse-cyclonedds/cyclonedds/tree/security).

However, it is possible that various parts need some rework before the security
branch can be merged into the cyclonedds master branch.


## Footprint<a name="footprint" />

A non-functional requirement is that cyclonedds should be buildable without
the DDS Security support in it. That will reduce the footprint (and possibly
improve performance) for applications that don't need security.

For that, the ENABLE_SECURITY build option is introduced that translates into
the DDSI_INCLUDE_SECURITY compile switch. However, the usage of that switch
should not explode. That'll reduce the maintainability.

For instance, the usage of the switch can be minimized by using functions that
will reduce to an inline function that just returns a hardcode value when
security is not included (otherwise they'll do some certain task).
The compiler can use these inline functions to do clever stuff regarding
footprint and performance.

There can be other solutions to decrease security footprint without impeding
on the maintainability of the code by inserting the switch too much.


## Multi process testing (done)<a name="testing" />

To properly test DDS Security, multi process testing will be necessary.
This is not yet available in Cyclone DDS.
See the [Multi Process Testing](multi_process_testing.md) document for
more information.


## Runtime library loading (done)<a name="loading" />

The ddsi component needs to be able to load DDS Security plugins at runtime.
These plugins are provided as libraries.<br>
Loading libraries at runtime is currently not possible in Cyclone DDS.


## Hopscotch utility (done)<a name="hopscotch" />

This hash table is used by the Security plugins.<br>
Both versions on OpenSplice and Cyclone are equivalent.<br>
No additional effort is expected.


## FSM utility (in progress)<a name="fsm" />

The Finite State Machine utility has been introduced in OpenSplice to support
the handshake of DDS Security.<br>
This has to be ported to Cyclone.

However, it already has some technical dept, which should be removed before
adding it to Cyclone. This means that a refactor should happen as part of the
porting.

The related DBTs should also be ported to Cyclone unit tests.

It was decided to just port the FSM at the moment. The refactor will take place
when trying to get the security branch into master.


## Port DDS Security plugin API (done)<a name="port-api" />

The DDS Security plugin API are just a few header files. The ddsi component
uses that to link against. The implementation of the API is done in the
individual plugins. The plugins are [loaded at runtime](#loading) (when
configured).

This means that ddsi can be DDS Security prepared (after building against this
API) without there being actual DDS Security plugins.

It seems to be just a code drop of a number of header files.<br>
Maybe add some CMake module for both ddsi and the plugins to easily link
against?


## De-Serializing messages in DDSI (done)<a name="deserializing" />

DDSI needs to be able to (de)serialize a few Security messages. In OpenSplice,
some functionality of the database is used. This is unavailable in Cyclone.

What is available is a serializer that uses marshaling operations (see, for
for instance, m_ops in the dds_topic_descriptor struct).

The (de)serializing of the Security messages should be supported by supplying
the m_ops sequences, message structs (if not yet available) and some
convenience functions using both.


## De-Serializing security message parameters in DDSI (done)<a name="deserializing_plist" />

DDSI needs to be able to (de)serialize a few message parameters that have
been introduced by the DDS Security spec.


## Port DDS Security builtin plugins (in progress)<a name="port-plugins" />

No major changes between the DDS Security plugins in OpenSplice and Cyclone
are expected.

The DDS Security plugins require OpenSSL. Cyclone DDS already uses OpenSSL.
However, it expects (or at least it's preferred to have) version 1.1 or newer,
while the OpenSplice Security plugins are build against 1.0.2. There are some
API changes between the two versions. This will take some porting effort.

The build system should be ported from makefiles to cmake files.

There are security_plugin DBTs in OpenSplice. These tests are base on cunit,
which is also used in Cyclone. However, it is used slightly different. A small
porting effort is expected (i.e. let it work with cmake and runner generation).

This means some additional effort, compared to just a code drop. But it is not
expected to be major.

- Authentication plugin (done).
- Access Control plugin (in progress).
- Cryptography plugin (done).

There are a few sub-features that can be implemented separately.
- Check/handle expiry dates (in progress).
- Trusted directory support.
- etc?


## Port DDSI DDS Security (in progress)<a name="port-ddsi" />

There is already quite a bit of difference between the DDSI codebases in
OpenSplice and Cyclone. So, the copy/merge of the DDSI Security code from
OpenSplice to Cyclone will not be trivial.

Most parts of the merging will not be trivial, but should be quite
straightforward nonetheless. Things that are noticed to be somewhat different
between the DDSI code bases that could impact the merging work:
- Entity matching is slightly different.
- The q_entity.c has a lot of differences that can obfuscate the differences
  related to DDS Security.
- Unacked messages logic has changed a bit. Does that impact gaps?
- (De)serializing, of course
  (see also [De-Serializing in DDSI](#deserializing)).
- Writer history cache is different, which can impact the builtin volatile
  Security endpoints.
- Unknown unknowns.

The buildsystem has to be upgraded.<br>
- A few files are added which are easy to add to cmake.<br>
- There's a new dependency on the [DDS Security API](#port-api), which is done.

Then, of course, there are the tests<br>
First of all, [Multi Process Testing](#testing) should be available, which now
it is.<br>
When that's the case, then the OpenSplice tests business
logic have to be ported from scripts and applications to that new framework.
That porting shouldn't be that hard. However, it will probably take a while.

The DDSI Port doesn't have to be a big bang. It can be split up into various
different pull requests. Examples are
- Extend configuration XML parsing with the security configuration (done).
- Extend ddsi_qos with security related policies. Fill them with values from the
  configuration when applicable (done).
- Add DDS Security endpoints that are non-volatile (done).
- Add DDS Security endpoint that is volatile. This change has more impact than
  all the non-volatile endpoints combined (done).
- Handshake (in progress).
- Payload (en)(de)coding (DDSI support: done. Wrapper: todo).
- Submsg (en)(de)coding (DDSI support: done. Wrapper: todo).
- RTPSmsg (en)(de)coding (DDSI support: done. Wrapper: todo).
- Etc



## Move configuration (in progress)<a name="Move-configuration" />

After the port, the DDS Security configuration is still (partly) done through
the overall configuration XML file (rest is coming from the permissions and
governance files).<br>
However, according to the specification, the configuration should happen by
means of the Participant QoS.

The ddsc dds_qos_t is mapped on the ddsi xqos. The ddsi xqos already has the
necessary policy (after the [port](#port-ddsi)), namely the property_policy.
This means that the ddsc qos itself is automatically prepared.<br>
However, getting and setting policies are done through getter and setter
functions in ddsc.<br>
This means we have to add these functions for the security configuration values.

The ddsc policy getter and setter functions use (arrays of) primitive types as
arguments. The configuration of Security is given by means of the property
policy, which isn't a primitive. To keep in line with the QoS API, we could add
something like:
```cpp
typedef struct dds_properties_t; /* opaque type in API, but mapped to
                                    ddsi_property_qospolicy_t internally */
dds_properties_t *dds_properties_create();
void dds_properties_delete(dds_properties_t *);
void dds_properties_merge(dds_properties_t *, dds_properties_t *);
void dds_properties_add_property(dds_properties_t *, char *name, char *value);
void dds_properties_add_binaryproperty(dds_properties_t *, char *name,
                                       uchar *value, int valuelength);
void dds_qset_properties(dds_qos_t*, dds_properties_t *);
void dds_qget_properties(dds_qos_t*, dds_properties_t **);
```
But this is very preliminary and is still up for debate.

After moving the Security configuration to the participant QoS, it's possible
to have different configurations within a single application if you have
multiple participants. However, ddsi only supports one Security configuration
for now. That doesn't change by changing where that configuration comes
from.<br>
To solve this, it is expected that creation of a participant with a different
configuration will force a failure for now.
Until [Multiple Configurations](#multiple-configurations) is implemented.

After the ddsc API has been extended, we can decide on what to do with the
configuration through XML.
- Keep it. It seems usable: no need to change applications when changing (or
  adding) Security settings. However, conflicts between XML and QoS
  configuration could cause problems. Simplest seems to be to only allow QoS
  security configuration when it's not configured in XML already.
- Remove it. No conflict resolving needed.

All the Security tests depend on providing (different) configurations through
XML. Depending on if we keep or remove the XML configuration option, a lot of
tests have to be updated, or a few added (that test security configuration
through QoS).

For the loading of the plugin libraries, properties with specific names have to
be added to the property policy to know the location and names of the plugins.
As inspiration, fastrtps can be used:
https://github.com/ros2/rmw_fastrtps/blob/master/rmw_fastrtps_shared_cpp/src/rmw_node.cpp#L296


## Failure handling<a name="failures" />

Currently, when an local action is tried that isn't allowed by DDS Security
(like creating a participant when it's not permitted), DDSI is shut down.<br>
Mainly because in OpenSplice it's quite hard to get a failure state from DDSI
to the application.

In Cyclone, however, ddsc::dds_create_participant() results in a direct call to
ddsi::ddsi_new_participant(). This means that if creation of an entity (or
participant in this example) fails due to security issues in ddsi, we can fail
the actual ddsc API call with a proper error result (there's already the
DDS_RETCODE_NOT_ALLOWED_BY_SECURITY in the ddsc API (not used)).

Maybe we have to do some additional cleanup when a failure is encountered.

Some tests probably have to be adjusted for the new behaviour.


## Multiple configurations<a name="multiple-configurations" />

Currently (because it's done through the overall XML configuration), only one
DDS Security configuration could be supported. Because of this fact, at various
locations, shortcuts could be made in both DDSI and plugins.<br>
However, because the configuration is coming from participants now (see
[Move Configuration](#Move-configuration), we should be able to support
multiple different DDS Security configurations.

Until now, the creation of a second participant with a different configuration
would force a failure (again, see [Move Configuration](#Move-configuration)).

It is expected that the plugin loading still happens through the configuration
XML (see [Move Configuration](#Move-configuration)). This means that DDSI doesn't have to support
multiple sets of plugins. Just the one set, provided at initialization. This
means that DDSI shouldn't have to be changed to support this.

So, it's the plugins need to be able to support multiple configurations.

The Cryptography plugin doesn't seem to care about global DDS Security
configurations. It has basically configurations per participant/topic/
endpoints, which already works. So, this plugin doesn't have to be upgraded.

The Authentication plugin does have global DDS Security configurations. Main
function related to that is validate_local_identity(). This function already
creates a new local identity every time it is called. So, this plugin doesn't
have to be upgraded either.

That leaves the Access Control plugin.<br>
The main function related to configuration is validate_local_permissions().
This function creates access rights depending on Permissions and Governance
files. Currently, there's only one local 'rights' structure that is linked
directly to the plugin (see also the ACCESS_CONTROL_USE_ONE_PERMISSION compile
switch).<br>
This has to change.<br>
The local rights structure needs to be coupled to a participant. This also
means that we have to search for it instead of having direct access when
entering the plugin.<br>
The remote rights can be used as example. That is basically a list of rights/
permissions with the remote identity handle as key.

Tests have to be added to make sure that a setup with different Security
configurations works.


## Example<a name="example" />

A Security example has to be added.


## QosProvider<a name="qosprovider" />

The Participant QoS now contains Security related information. This means that
the QosProvider has to be upgraded to support that.


## Data Tags (optional)<a name="datatags" />

The specification is somewhat fuzzy about the data tags.

The following is a summary (still elaborate) of how it seems to work:

The permissions document can contain the <data_tags> tag on publish and
subscribe level. It's related to the data samples, but don't have to be related
to the keys of those samples.<br>
The QoS of a writer/reader can also have data tags by means of the
DataTagQosPolicy. A writer/reader can only be created when the data_tags in the
QoS matches those in the permissions document. This check should happen on both
the local and remote level.

This creation check is the only thing that DDS Security actually does with the
data tags. They are only authenticated by DDS Security, but not interpreted
further.<br>
This is only a minor security addition, because the publisher can still publish
data that doesn't match the data tags because DDS doesn't interpret the data
nor compares it with the data tags.

What it can be used for is a kind of custom access control scheme on the
application level.<br>
An application that consumes data can see if a publisher is allowed to publish
that sample by comparing the data within the sample with the data tag(s)
associated with that publisher. As said, this comparison is not done on the DDS
level, but has to be done within the application itself.

That leaves the question, how does the application get the tags associated with
the related writer?<br>
In other words; the application gets a sample. It has to know from which writer
it originated and it has to have access to the data tag(s) of that writer.
The dds_sample_info_t contains the dds_instance_handle_t publication_handle,
which is unique to the writer (locally).<br>
That dds_instance_handle_t can be used to get the right
DDS_Security_PublicationBuiltinTopicDataSecure sample for the related secure
builtin reader [**note1**].<br>
DDS_Security_PublicationBuiltinTopicDataSecure contains QoS information
regarding that writer, including the DataTagQosPolicy. The remote
DDS_Security_PublicationBuiltinTopicDataSecure contents have been authenticated
by DDS Security and the data tags can be trusted.<br>
The application can check the sample data against the data_tags within that
QoS.

Things to do:
- Add DataTagQosPolicy to the ddsc API and the related QoSses.
- Add DDS_Security_PublicationBuiltinTopicDataSecure data type to the ddsc API
  (better yet, all secure builtin types).
- Add the related builtin reader to the ddsc99 API (better yet, all secure
  builtin readers).
- Add test regarding the secure builtin readers and data.
- Add data tag comparisons between QoS and permission documents during local
  and remote entity creation.
- Add data tag remote/local (mis)match tests.

Especially because of the lack of access to builtin secure readers, supporting
data tags doesn't seem feasible in the near future. Also, it's optional in the
specification.

**note1**
That DDS_Security_PublicationBuiltinTopicDataSecure reader is not yet available
within the ddsc API, nor is the related data type. Don't know how much work it
would be to add them to that API.
