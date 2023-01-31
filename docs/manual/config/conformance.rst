.. index:: Conformance modes, RTI

.. _conformance_modes:

=================
Conformance modes
=================

|var-project| operates in one of three modes: 

- *lax* (default). The |var-project| accepts some invalid messages and transmits them, which is important 
  for interoperability.

- *pedantic* (Depreciated)

- *strict* (Depreciated)

To configure the mode, set :ref:`Compatibility/StandardsConformance <//CycloneDDS/Domain/Compatibility/StandardsConformance>`

.. note::

  If there are two |var-project| processes with different compliancy modes, the one in the 
  stricter mode will complain about messages sent by the one in the less strict mode.

.. index:: RTI

.. _rti_compatibility:

RTI compatibility issues
------------------------

In *lax* mode, most topic types should not have significant issues when working across a 
network.

Disposing of data may cause problems, as RTI DDS leaves out the serialised key value
and expects the Reader to rely on an embedded hash of the key value. In the
strict modes, |var-project-short| requires a valid key value to be supplied; in the relaxed
mode, it is willing to accept key hash, provided it is of a form that contains the key
values in an unmangled form.

If an RTI DDS DataWriter disposes of an instance with a key of which the serialised
representation may be larger than 16 bytes, this problem is likely to occur. In
practice, the most likely cause is using a key as a string, either unbounded, or with a
maximum length larger than 11 bytes. See the DDSI specification for details.

In *strict* mode, there is interoperation with RTI DDS, but at the cost of very
high CPU and network load. This is caused by Heartbeats and AckNacks going back-and-forth
between a reliable RTI DDS DataWriter and a reliable |var-project-short| data Reader. When 
|var-project-short| informs the RTI Writer that it has received all data
(using a valid AckNack message), the RTI Writer immediately publishes a message listing
the range of available sequence numbers and requesting an acknowledgment, which becomes
an endless loop.

In addition, there is a difference in interpretation of the meaning of the
"autodispose_unregistered_instances" QoS on the Writer. |var-project-short|
aligns with OpenSplice.
