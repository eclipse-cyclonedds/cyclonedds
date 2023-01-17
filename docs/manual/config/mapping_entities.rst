.. _`Mapping of DCPS entities to DDSI entities`:

*****************************************
Mapping of DCPS Entities to DDSI Entities
*****************************************

Each DCPS domain participant in a domain has a corresponding DDSI participant.
These DDSI participants drive the discovery of participants, readers, and writers
via the discovery protocols (:ref:`discovery_behaviour`). By default, each DDSI participant 
has a unique address on the network in the form of its own UDP/IP socket with a unique 
port number.

Any DataReader or DataWriter created by a DCPS domain participant has a corresponding DDSI
reader or writer (referred to as *endpoints*). However, there is no one-to-one mapping. In DDSI:

- a *reader* is the combination of DCPS DataReader and the DCPS subscriber it belongs to,
- a *writer* is a combination of DCPS DataWriter and DCPS publisher.

There are no standardized built-in topics for describing DCPS subscribers and publishers. 
However, there are non-standard extensions that enable implementations to offer `additional` 
built-in topics to represent these entities and include them in the discovery.

In addition to the application-created readers and writers, DDSI participants have several 
DDSI built-in endpoints for discovery and liveliness checking/asserting. 

If there are no corresponding endpoints, a DDSI implementation can exclude a participant. 
For example, if a participant does not have writers, there is no need to include the DDSI 
built-in endpoints for describing writers, nor the DDSI built-in endpoint for learning of 
readers of other participants.
