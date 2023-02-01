.. index:: Data-centric architecture

.. _data_centric_architecture:

Data-centric architecture
-------------------------

In a service-centric architecture, applications need to
know each other's interfaces to share data, share events, and share
commands or replies to interact. These interfaces are modeled as sets of 
operations and functions that are managed in centralized repositories. 
This type of architecture creates unnecessary dependencies that form a
tightly coupled system. The centralized interface repositories are
usually seen as a single point of failure.

In a data-centric architecture, your design focuses on the data each
application produces and decides to share rather than on the Interfaces'
operations and the internal processing that produced them.

A data-centric architecture creates a decoupled system that focuses on
the data and applications states' that need to be shared rather than the
applications' details. In a data-centric system, data and their
associated quality of services are the only contracts that bound the
applications together. With |var-project|, the system decoupling is
bi-dimensional, in both space and time.

Space-decoupling derives from the fact that applications do not need to 
know the identity of the data produced or consumed, nor their logical 
or a physical location in the network. Under the hood, |var-project-short|
runs a zero-configuration, interoperable discovery protocol that
searches matching data readers and data writers that are interested in
the same data topic.

Time-decoupling derives from the fact that, fundamentally, the nature of
communication is asynchronous. Data producers and consumers,
known as :ref:`datawriters_bm` and :ref:`datareaders_bm`, are not forced to
be active and connected simultaneously to share data. In this
scenario, the |var-project-short| middleware can handle and manage data on behalf of
late joining ``DataReader`` applications and deliver it to them when they
join the system.

Time and space decoupling gives applications the freedom to be plugged
or unplugged from the system at any time, from anywhere, in any order.
This keeps the complexity and administration of a data-centric
architecture relatively low when adding more and more DataReader and
DataWriter applications.