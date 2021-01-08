## Keys steps to build the Hello World! application

The _Hello World!_ example has a very simple 'data layer' with a data model made of one data type `Msg` who represents keyed messages (c,f next subsection).

To exchange data, applications' business logic needs with Cyclone DDS to:

1. Declare its subscription and involvement into a _ **DDS domain** _. A DDS domain is an administrative boundary that defines, scopes and gather all the DDS applications, data and infrastructure that needs to interconnect and share the same data space. Each DDS domain has its unique identifier. Applications declare their participation within a DDS domain by creating a **Domain Participant entity**.
2. Create a **Data topic** that has the data type described in the data model. The data types define the structure of the Topic. The Topic is therefore an association between the topic name and datatype. QoSs can be optionally added to this association. A Topic therefore categories the data in logical classes and streams.
3. Create at least a **Publisher** , a **Subscriber** and **Data Readers** and **Writers** objects that are specific to the topic created earlier. Applications way want to change the default QoSs at this stage. In the Hello world! example, the `ReliabilityQoS` is changed from its default value (Best-effort) to Reliable.
4. Once the previous DDS computational object s are in place, the application logic can start writing or reading the data.

At the application level, readers and writers do not need to be aware of each other. The reading application, hereby designated as application Subscriber polls the data reader periodically, until a writing application, hereby called application Publisher, provides the required data into the share topic, namely `HelloWorldData_Msg`.

The data type is described using the OMG [IDL Language](http://www.omg.org/gettingstarted/omg_idl.htm) located in **HelloWorldData.idl** file. This IDL file will be considered the Data Model of our example.

This data model will be preprocessed and compiled by Cyclone-DDS-CXX IDL-Compiler to generate a CXX representation of the data as described in Chapter 6. These generated source and header files will be used by the **HelloworldSubscriber.cpp** and **HelloworldPublishe.cpp** applicationprograms to share the _Hello World!_ Message instance and sample.