.. _`Mapping of DCPS entities to DDSI entities`:

*****************************************
Mapping of DCPS Entities to DDSI Entities
*****************************************

Each DCPS domain participant in a domain has a corresponding DDSI participant.
These DDSI participants drive the discovery of participants, Readers, and Writers
via the discovery protocols (:ref:`Discovery behaviour`). By default, each DDSI participant 
has a unique address on the network in the form of its own UDP/IP socket with a unique 
port number.

Any DataReader or DataWriter created by a DCPS domain participant has a corresponding DDSI
Reader or Writer (referred to as *endpoints*). However, there is no one-to-one mapping. In DDSI:

- a *Reader* is the combination of DCPS DataReader and the DCPS Subscriber it belongs to,
- a *Writer* is a combination of DCPS DataWriter and DCPS Publisher.

There are no standardized built-in topics for describing DCPS Subscribers and Publishers. 
However, there are non-standard extensions that enable implementations to offer `additional` 
built-in topics to represent these entities and include them in the discovery.

In addition to the application-created Readers and Writers, DDSI participants have several 
DDSI built-in endpoints for discovery and liveliness checking/asserting. 

If there are no corresponding endpoints, a DDSI implementation can exclude a participant. 
For example, if a participant does not have Writers, there is no need to include the DDSI 
built-in endpoints for describing Writers, nor the DDSI built-in endpoint for learning of 
Readers of other participants.
