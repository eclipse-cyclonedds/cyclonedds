## Data-Centric Architecture

In a service-centric architecture, to interact, applications need to know each other's interfaces to share data, share events, share commands, or replies. These interfaces are modeled as sets of operations and functions that are managed in centralized repositories. This kind of architecture creates unnecessary dependencies that end-up creating a tightly coupled system. The centralized interface repositories are usually seen as a single point of failure.

In a data-centric architecture, your design focuses on the data each application produces and decides to share rather than on the Interfaces' operations and the internal processing that produced them.

A data-centric architecture creates a decoupled system that focuses on the data and applications states' that need to be shared rather than the applications' details. In a data-centric system, data and their associated quality of services are the only contract that bounds the applications together. With DDS, the system decoupling is bi-dimensional, in Space and in Time.

Space-decoupling derives from the fact that applications do not need to, either know the identity of the data produced (or consumers) nor their logical or their physical location in the network. Under the hood, DDS runs a zero-configuration, interoperable discovery protocol that searches matching data readers and data writes that are interested by the same data topic.

Time-decoupling derives from the fact that, fundamentally, the nature of the communication is asynchronous. Data producers and data consumers, known respectively, as Ddta Wwiters, and data readers are not forced to be active and connected at the same time to share data. In this scenario, the DDS middleware can handle and manage data on behalf of the late joining data readers applications and delivered to it when they join the system.

Time and space decoupling gives applications the freedom to be plugged or unplugged in the system at any time, from anywhere, in any order. Thiskeeps the complexity and administration of a data-centric architecture relatively low when adding more and more data readers and data writers applications.
