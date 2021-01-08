## Data-Centric Architecture

In a Service-centric architecture, to interact, applications need to know the interfaces to each other to share data, share events, share commands or replies. These interfaces are modeled as sets of operations and functions that are managed in centralized repositories. This kind of architecture creates unnecessarily dependencies that ends-up creating a tightly coupled system. The centralized interface repositories are usually seen as single point of failure.

In a Data-centric architecture, your design focuses on the data each application produces and decides to share rather than on the Interfaces' operations and the internal processing that produced them.

A Data centric architecture creates therefore a decoupled system that focus on the data and applications states' that need to be shared rather than the applications' details. In a data centric system, data and their associated quality of services are the only contract that bounds the applications together. With DDS, the system decoupling is bi-dimensional, in Space and in Time.

Space-decoupling drives from the fact that applications do not need to, neither know the identity of the Data produced (or consumers) nor their logical or their physical location in the network. Under the hood, DDS runs a Zero-configuration, interoperable discovery protocol that searches matching Data Readers and Data Writes that are interested by the same data topic.

Time-decoupling derives from the fact that, fundamentally, the nature of the communication is asynchronous. Data Producers and Data Consumers, known respectively, as Data Writers, and Data Readers are not forced to be active and connected at the same time to share data. In this scenario, the DDS middleware can handle and manage data on behalf of the late joining Data Readers applications and delivered it when they join the system.

Time and space decoupling gives applications the freedom to be plugged or unplugged in the system at any time, from anywhere, in any order. Thiskeeps the complexity and administration of a data-centric architecture relatively low when adding more and more data Readers and data Writers applications.
