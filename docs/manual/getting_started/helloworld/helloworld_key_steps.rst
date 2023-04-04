.. include:: ../../external-links.part.rst

.. index:: HelloWorld; Key steps

.. _key_steps:

HelloWorld keys steps
=====================

The **Hello World!** example has a minimal 'data layer' with a data
model made of one data type ``Msg`` that represents keyed messages.

To exchange data with |var-project|, applications' business logic needs
to:

#. Declare its participation and involvement in a *DDS domain*. A DDS
   domain is an administrative boundary that defines, scopes, and
   gathers all the DDS applications, data, and infrastructure that must 
   interconnect by sharing the same data space. Each DDS
   domain has a unique identifier. Applications declare their
   participation within a DDS domain by creating a **Domain Participant
   entity**.
#. Create a **Data topic** with the data type described in a data
   model. The data types define the structure of the Topic. The Topic is
   therefore, an association between the topic's name and a datatype.
   QoSs can be optionally added to this association. The concept Topic
   therefore discriminates and categorizes the data in logical classes
   and streams.
#. Create the **Data Readers** and **Writers** entities 
   specific to the topic. Applications may want to change the default
   QoSs. In the ``Hello world!`` Example, the ``ReliabilityQoS`` is changed
   from its default value (``Best-effort``) to ``Reliable``.
#. When the previous DDS computational entities are in place, the
   application logic can start writing or reading the data.

At the application level, readers and writers do not need to be aware of
each other. The reading application, now called Subscriber, polls the
data reader periodically until a publishing application, now called
The publisher writes the required data into the shared topic, namely
``HelloWorldData_Msg``.

The data type is described using the |url::idl_4.2| language located in
the ``HelloWorldData.idl`` file and is the data model of the example.

.. tabs::

    .. group-tab:: Core DDS (C)

      This data model is preprocessed and compiled by the IDL Compiler
      to generate a C representation of the data as described in Chapter 2.
      These generated source and header files are used by the
      ``HelloworldSubscriber.c`` and ``HelloworldPublishe.c`` programs to
      share the **Hello World!** Message instance and sample.

    .. group-tab:: C++
      This data model is preprocessed and compiled by |var-project-short| C++
      IDL-Compiler to generate a C++ representation of the data as described
      in Chapter 6. These generated source and header files are used by the
      ``HelloworldSubscriber.cpp`` and ``HelloworldPublisher.cpp``
      application programs to share the *Hello World!* Message instance and
      sample.