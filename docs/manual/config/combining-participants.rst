.. _`Combining multiple participants`:

*******************************
Combining Multiple Participants
*******************************

If a single process creates multiple participants, these are mirrored in DDSI
participants, so that a single process can appear like an extensive system with many
participants. The :ref:`Internal/SquashParticipants <//CycloneDDS/Domain/Internal/SquashParticipants>`
option can be used to simulate the existence of only one participant, which owns all
endpoints on that node. This reduces the background messages because far fewer liveliness
assertions need to be sent, but there are some downsides.

Firstly, the liveliness monitoring features related to domain participants will
be affected if multiple DCPS domain participants are combined into a single DDSI domain
participant. For the "automatic" liveliness setting, this is not an issue.

Secondly, this option makes it impossible for tooling to show the system
topology.

Thirdly, the QoS of this sole participant is simply that of the first participant
created in the process. In particular, no matter what other participants specify as
their "user data", it will not be visible on remote nodes.

An alternative that sits between squashing participants and normal operation,
is setting :ref:`Internal/BuiltinEndpointSet <//CycloneDDS/Domain/Internal/BuiltinEndpointSet>` to ``minimal``.
In the default setting, each DDSI participant handled has its Writers for
built-in topics and publishes discovery data on its entities, but when set to "minimal",
only the first participant has these Writers and publishes data on all entities. This is not fully
compatible with other implementations as it means endpoint discovery data can be
received for a participant that has not yet been discovered.
