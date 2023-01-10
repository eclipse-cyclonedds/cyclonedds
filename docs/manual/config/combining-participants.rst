.. include:: ../external-links.part.rst

.. index:: ! Participant, Multiple participants

.. _combining_multiple_participants:

*******************************
Combining multiple participants
*******************************

If a single process creates multiple participants, these are mirrored in DDSI
participants, where a single process can resemble an extensive system with many
participants. To simulate the existence of only one participant, which owns all
endpoints on that node, set: 
:ref:`Internal/SquashParticipants <//CycloneDDS/Domain/Internal/SquashParticipants>`
to ``true``. This reduces the background messages because far fewer liveliness 
assertions need to be sent. However, there are some downsides:

- This option makes it impossible for the tooling to show the system topology.

- If multiple DCPS domain participants are combined into a single DDSI domain
  participant, the liveliness monitoring features related to domain participants are
  affected . For the "automatic" liveliness setting, this is not an issue (see |url::c-api-liveliness|).

- The QoS of the sole participant is the first participant created in the process. 
  That is, no matter what other participants specify as their "user data", it is not 
  visible on remote nodes.

Another option is to set 
:ref:`Internal/BuiltinEndpointSet <//CycloneDDS/Domain/Internal/BuiltinEndpointSet>` 
to ``minimal``. Only the first participant has these Writers and publishes data on all entities. 

 .. note::
     This is not fully compatible with other implementations as it means endpoint discovery data can be
     received for a participant that has not yet been discovered.
