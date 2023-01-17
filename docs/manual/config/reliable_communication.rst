.. index:: Reliable Communication, Writer History Cache, Heartbeat, Best-effort communication

.. _reliable_coms:

**********************
Reliable communication
**********************

*Best-effort* communication is a wrapper around UDP/IP. That is, the packet(s) containing
a sample are sent to the addresses where the readers reside. No state is maintained
on the writer. If a packet is lost, the reader ignores the samples contained in the 
lost packet and continue with the next one.

When using *reliable* communication, the writer maintains a copy of the sample in the 
Writer History Cache (WHC) of the DDSI writer. If a reader detects that there are lost packets, 
the reader can request a re-transmission of the sample. To ensure that all readers learn of 
new samples in the WHC, the DDSI writer periodically sends *heartbeats* to its readers. 
If all matched readers have acknowledged all samples in the WHC, the DDSI writer can 
suppress these periodic heartbeats.

If a reader receives a heartbeat and detects it did not receive all samples, it requests 
a re-transmission by sending an *AckNack* message to the writer. 

.. note::

    The heartbeat timing is adjustable. If the latency is longer than the heartbeat interval, 
    this can result in multiple re-transmit requests for a single sample.

In addition to requesting the re-transmission of some samples, a reader also uses the 
AckNack messages to inform the writer of samples received and not received. Whenever the 
writer indicates that it requires a response to a heartbeat, the readers sends an AckNack 
message (even when no samples are missing).

Combining these behaviours allows the writer to remove old samples from its WHC when it fills 
up the cache, enabling readers to receive all data reliably. 

The default |var-project-short| behaviour is to never to consider readers unresponsive. The 
DDSI specification does not define how to handle a situation where readers do not respond 
to a heartbeat, or fail to receive samples from a writer after a re-transmission request.
A solution to this situation is to periodically check the participant containing the reader.

.. note::

    The DDSI specification allows for the suppressing of heartbeats, merging AckNacks, and 
    re-transmissions, and so on. These techniques are required to allow for a performant 
    DDSI implementation while avoiding the need for sending redundant messages.
