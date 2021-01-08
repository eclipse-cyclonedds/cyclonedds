## Keys steps to build the Hello World! application

The _Hello World!_ example has a very simple 'data layer' with a data model made of one data type Msg that represents keyed messages (c,f next subsection).

To exchange data, applications' business logic needs with Cyclone DDS to:

1. Declare its participation and involvement into a _DDS domain_. A DDS domain is an administrative boundary that defines, scopes and gather all the DDS applications, data and infrastructure that needs to interconnect together by share the same data space. Each DDS domain has its unique identifier. Applications declare their participation within a DDS domain by creating a **Domain Participant entity**.
2. Create a **Data topic** that has the data type described in a data model. The data types define the structure of the Topic. The Topic is therefore an association between the topic's name and a datatype. QoSs can be optionally added to this association. The concept Topic therefore discriminates and categories the data in logical classes and streams.
3. Create the **Data Readers** and **Writers** entities that are specific to the topic. Applications may to change the default QoSs. In the Hello world! example, the `ReliabilityQoS` is changed from its default value (`Best-effort`) to `Reliable`.
4. Once the previous DDS computational entities are in place, the application logic can start writing or reading the data.

At the application level, readers and writers do not need to be aware of each other. The reading application, hereby calls Subscriber polls the data reader periodically, until a publishing application, hereby called Publisher the required data into the share topic, namely HelloWorldData\_Msg.

The data type is described using the OMG [IDL Language](http://www.omg.org/gettingstarted/omg_idl.htm)located in **HelloWorldData.idl** file. Such IDL file will be seen as the Data Model of our example.

This data model will be preprocessed and compiled by Cyclone-DDS IDL-Compiler to generate a C representation of the data as described in Chapter 2. These generated source and header files will be used by the **HelloworldSubscriber.c** and **HelloworldPublishe.c** programs to share the _Hello_ _World!_ Message instance and sample.