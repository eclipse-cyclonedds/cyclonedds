.. _`Compatibility issues with RTI`:

========================
RTI Compatibility Issues
========================

In *lax* mode, most topic types should not have significant issues when working across a 
network. Previously within a single host, there was an issue with how RTI DDS uses or attempts 
to use its shared memory transport to communicate with peers, even when they advertise only 
UDP/IP addresses. The result is an inability to establish bidirectional communication between 
the two reliably.

Disposing of data may also cause problems, as RTI DDS leaves out the serialised key value
and expects the Reader to rely on an embedded hash of the key value. In the
strict modes, |var-project-short| requires a valid key value to be supplied; in the relaxed
mode, it is willing to accept key hash, provided it is of a form that contains the key
values in an unmangled form.

If an RTI DDS DataWriter disposes of an instance with a key of which the serialised
representation may be larger than 16 bytes, this problem is likely to occur. In
practice, the most likely cause is using a key as a string, either unbounded, or with a
maximum length larger than 11 bytes. See the DDSI specification for details.

In *strict* mode, there is interoperation with RTI DDS, but at the cost of incredibly
high CPU and network load, caused by a Heartbeats and AckNacks going back-and-forth
between a reliable RTI DDS DataWriter and a reliable |var-project-short| data Reader. The
problem is that once |var-project-short| informs the RTI Writer that it has received all data
(using a valid AckNack message), the RTI Writer immediately publishes a message listing
the range of available sequence numbers and requesting an acknowledgment, which becomes
an endless loop.

Furthermore, there is a difference in interpretation of the meaning of the
"autodispose_unregistered_instances" QoS on the Writer. |var-project-short|
aligns with OpenSplice.