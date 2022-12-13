.. _`Reliable communication`:

**********************
Reliable Communication
**********************

*Best-effort* communication is a wrapper around UDP/IP. That is, the packet(s) containing
a sample are sent to the addresses where the Readers reside. No state is maintained
on the Writer. If a packet is lost, the Reader ignores the samples contained in the 
lost packet and continue with the next one.

When using *reliable* communication, the Writer maintains a copy of the sample in the Writer 
History Cache (or *WHC*) of the DDSI Writer. If a Reader detects that there are lost packets, 
the Reader can request a re-transmission of the sample. To ensure that all Readers learn of 
new samples in the WHC, the DDSI Writer periodically sends *Heartbeats* to its Readers. 
If all matched Readers have acknowledged all samples in the WHC, the DDSI Writer can 
suppress these periodic Heartbeats.

If a Reader receives a Heartbeat and detects it did not receive all samples, it requests 
a re-transmission by sending an *AckNack* message to the Writer. 

.. note::

    The Heartbeat timing is adjustable. If the latency is longer than the Heartbeat interval, 
    this can result in multiple re-transmit requests for a single sample.

In addition to requesting the re-transmission of some samples, a Reader also uses the 
AckNack messages to inform the Writer of samples received and not received. Whenever the 
Writer indicates that it requires a response to a Heartbeat, the Readers sends an AckNack 
message (even when no samples are missing).

Combining these behaviours allows the Writer to remove old samples from its WHC when it fills 
up the cache, enabling Readers to receive all data reliably. 

The default |var-project-short| behaviour is to never to consider Readers unresponsive. The 
DDSI specification does not define how to handle a situation where Readers do not respond 
to a Heartbeat, or fail to receive samples from a Writer after a re-transmission request.
A solution to this situation is to periodically check the participant containing the Reader.

.. note::

    The DDSI specification allows for the suppressing of heartbeats, merging AckNacks, and 
    re-transmissions, and so on. These techniques are required to allow for a performant 
    DDSI implementation while avoiding the need for sending redundant messages.
