..
   Copyright(c) 2024 ZettaScale Technology and others

   This program and the accompanying materials are made available under the
   terms of the Eclipse Public License v. 2.0 which is available at
   http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
   v. 1.0 which is available at
   http://www.eclipse.org/org/documents/edl-v10.php.

   SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

.. include:: ../external-links.part.rst
.. index:: ! QoS Provider

.. _qos_provider:

QoS Provider
############

This page provides information on using the QoS provider API of |var-project|. The QoS provider API allows users to specify the QoS settings of their DDS entities outside of application code in XML. This can be seen as a useful feature where code recompilation is restricted during the later stages of application development / during application support. The following sections explain the API and explain how to build QoS Profiles in XML.

XML file syntax
===============

The syntax for the XML configuration file is defined in |url::omg_xml_1.0|.

Entity QoS
----------

To configure the QoS for a DDS Entity using XML, the following tags have to be used:

- `<domainparticipant_qos>`
- `<publisher_qos>`
- `<subscriber_qos>`
- `<topic_qos>`
- `<datawriter_qos>`
- `<datareader_qos>`

Each XML tag with or without an associated name can be uniquely identified by its fully qualified name in C++ style.
In case of unnamed XML tag, only one tag of this kind is allowed in scope of parent `<qos_profile>`.

QoS Policies
------------

The fields in a Qos policy are described in XML using a 1-to-1 mapping with the equivalent IDL representation in the DDS specification. For example, the Reliability Qos policy is represented with the following structures:

.. code-block:: C

    struct Duration_t {
      long sec;
      unsigned long nanosec;
    };
    struct ReliabilityQosPolicy {
      ReliabilityQosPolicyKind kind;
      Duration_t max_blocking_time;
    };

The equivalent representation in XML is as follows:

.. code-block:: xml

    <reliability>
      <kind></kind>
      <max_blocking_time>
        <sec></sec>
        <nanosec></nanosec>
      </max_blocking_time>
    </reliability>

Sequences
---------

In general, the sequences contained in the QoS policies are described with the following XML format:

.. code-block:: xml

    <a_sequence_member_name>
      <element>...</element>
      <element>...</element>
      ...
    </a_sequence_member_name>

Each element of the sequence is enclosed in an <element> tag, as shown in the following example:

.. code-block:: xml

    <property>
      <value>
        <element>
          <name>my name</name>
          <value>my value</value>
        </element>
        <element>
          <name>my name2</name>
          <value>my value2</value>
        </element>
      </value>
    </property>


Enumerations
------------

Enumeration values are represented using their IDL string representation. For example:

.. code-block:: xml

    <history>
      <kind>KEEP_ALL_HISTORY_QOS</kind>
    </history>


Time values (Durations)
-----------------------

Following values can be used for fields that require seconds or nanoseconds:

- DURATION_INFINITE_SEC
- DURATION_INFINITE_NSEC

The following example shows the use of time values:

.. code-block:: xml

    <deadline>
      <period>
        <sec>DURATION_INFINITE_SEC</sec>
        <nanosec>DURATION_INFINITE_NSEC</nanosec>
      </period>
    </deadline>

QoS Profiles
------------

A QoS profile groups a set of related QoS, usually one per entity. For example:

.. code-block:: xml

    <qos_profile name="StrictReliableCommunicationProfile">
      <datareader_qos>
        <history>
          <kind>KEEP_LAST_HISTORY_QOS</kind>
          <depth>5</depth>
        </history>
      </datareader_qos>
      <datawriter_qos>
       <history>
          <kind>KEEP_LAST_HISTORY_QOS</kind>
          <depth>1</depth>
        </history>
      </datawriter_qos>
    </qos_profile>

XML Example
-----------

Consider the following XML file that describes two QoS profiles:

- FooQosProfile
    - DataReaderQos - KEEP_LAST (5)
    - DataWriterQos - KEEP_LAST (1)
    - TopicQos      - KEEP_ALL
- BarQosProfile
    - DataWriterQos - KEEP_ALL
    - TopicQos      - KEEP_LAST (5)


.. code-block:: xml

    <dds xmlns="http://www.omg.org/dds/"
         xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
         xsi:schemaLocation="file://DDS_QoSProfile.xsd">
      <qos_library name="myqoslib">
        <qos_profile name="foo_profile">
          <datareader_qos>
            <history>
              <kind>KEEP_LAST_HISTORY_QOS</kind>
              <depth>5</depth>
            </history>
          </datareader_qos>
          <datawriter_qos>
           <history>
              <kind>KEEP_LAST_HISTORY_QOS</kind>
              <depth>1</depth>
            </history>
          </datawriter_qos>
          <topic_qos name="my_topic">
            <history>
              <kind>KEEP_ALL_HISTORY_QOS</kind>
            </history>
          </topic_qos>
        </qos_profile>
        <qos_profile name="bar_profile">
          <datawriter_qos>
            <history>
              <kind>KEEP_ALL_HISTORY_QOS</kind>
            </history>
          </datawriter_qos>
            <topic_qos name="my_topic">
              <history>
                <kind>KEEP_LAST_HISTORY_QOS</kind>
                <depth>10</depth>
              </history>
            </topic_qos>
        </qos_profile>
      </qos_library>
    </dds>


Code Example
============

The following C application is an example to illustrate how the QoS settings from the above XML could be accessed.

.. tabs::

    .. group-tab:: C

      .. code-block:: C

        #include "dds/dds.h"
        #include "dds/ddsc/dds_qos_provider.h"
        #include "datatypes.h"

        int main (int argc, char **argv)
        {
          (void) argc;
          (void) argv;

          // provider will contains:
          // myqoslib::foo_profile                     READER (KEEP_LAST 5)
          // myqoslib::foo_profile                     WRITER (KEEP_LAST 1)
          // myqoslib::foo_profile::my_topic           TOPIC  (KEEP_ALL)
          // myqoslib::bar_profile                     WRITER (KEEP_ALL)
          // myqoslib::bar_profile::my_topic           TOPIC  (KEEP_LAST 10)
          dds_qos_provider_t *provider;
          dds_return_t ret = dds_create_qos_provider ("/path/to/qos_definitions.xml", &provider);
          assert (ret == DDS_RETCODE_OK);

          const dds_qos_t *tp_qos, *wr_qos;
          // qos can be accessed by <lib_name>::<profile_name>::[entity_name] if exist.
          ret = dds_qos_provider_get_qos (provider, DDS_TOPIC_QOS, "myqoslib::bar_profile::my_topic", &tp_qos);
          assert (ret == DDS_RETCODE_OK);
          // or if entity_qos is unnamed only by <lib_name>::<profile_name>.
          ret = dds_qos_provider_get_qos (provider, DDS_WRITER_QOS, "myqoslib::bar_profile", &wr_qos);
          assert (ret == DDS_RETCODE_OK);

          dds_entity_t pp = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
          dds_entity_t tp = dds_create_topic (pp, &mydatatype_desc, "topic_A", tp_qos, NULL);
          dds_entity_t wr = dds_create_writer (pp, tp, wr_qos, NULL);
          dds_delete (pp);
          dds_delete_qos_provider (provider);

          return 0;
        }

    .. group-tab:: C++

      .. code-block:: C++

        #include "dds/dds.hpp"
        #include "DataType.hpp"

        using namespace org::eclipse::cyclonedds;

        int main()
        {
          // provider will contains:
          // myqoslib::foo_profile                     READER (KEEP_LAST 5)
          // myqoslib::foo_profile                     WRITER (KEEP_LAST 1)
          // myqoslib::foo_profile::my_topic           TOPIC  (KEEP_ALL)
          // myqoslib::bar_profile                     WRITER (KEEP_ALL)
          // myqoslib::bar_profile::my_topic           TOPIC  (KEEP_LAST 10)
          dds::core::QosProvider provider("/path/to/qos_definitions.xml");
          auto topic_qos = provider.topic_qos("myqoslib::bar_profile::my_topic");
          auto writer_qos = provider.datawriter_qos("myqoslib::bar_profile");

          dds::domain::DomainParticipant participant(domain::default_id());
          dds::topic::Topic<DataType::Msg> topic(participant, "topic_A", topic_qos);
          dds::pub::Publisher publisher(participant);
          dds::pub::DataWriter<DataType::Msg> writer(publisher, topic, writer_qos);
          (void)writer;
          return 0;
        }

Also C API allows you to specify which library, profile QoS Provider should contains.
Let's extend XML file example above, and omit QoS settings details for simplicity.

.. code-block:: xml

    <dds xmlns="http://www.omg.org/dds/"
         xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
         xsi:schemaLocation="file://DDS_QoSProfile.xsd">
      <qos_library name="qos1_lib">
        <qos_profile name="foo_profile">
          <datareader_qos/>
          <datawriter_qos/>
          <topic_qos/>
        </qos_profile>
        <qos_profile name="bar_profile">
          <datawriter_qos/>
          <topic_qos/>
        </qos_profile>
      </qos_library>
      <qos_library name="qos2_lib">
        <qos_profile name="foo_profile">
          <datareader_qos/>
          <datawriter_qos/>
          <topic_qos/>
        </qos_profile>
        <qos_profile name="bar_profile">
          <datawriter_qos/>
          <topic_qos/>
        </qos_profile>
      </qos_library>
    </dds>

Let's create QoS Provider that contains versions of both profiles from different libraries.

.. tabs::

    .. group-tab:: C

        .. code-block:: C

          #include "dds/dds.h"
          #include "dds/ddsc/dds_qos_provider.h"
          #include "datatypes.h"

          int main (int argc, char **argv)
          {
            (void) argc;
            (void) argv;

            // foo_provider will contains:
            // qos1_lib::foo_profile                     READER
            // qos1_lib::foo_profile                     WRITER
            // qos1_lib::foo_profile                     TOPIC
            // qos2_lib::foo_profile                     READER
            // qos2_lib::foo_profile                     WRITER
            // qos2_lib::foo_profile                     TOPIC
            dds_qos_provider_t *foo_provider;
            char *foo_scope = "*::foo_profile";
            dds_return_t ret = dds_create_qos_provider_scope ("/path/to/qos_definitions.xml", &foo_provider, foo_scope);
            assert (ret == DDS_RETCODE_OK);

            // bar_provider will contains:
            // qos1_lib::bar_profile                     WRITER
            // qos1_lib::bar_profile                     TOPIC
            // qos2_lib::bar_profile                     WRITER
            // qos2_lib::bar_profile                     TOPIC
            dds_qos_provider_t *bar_provider;
            char *bar_scope = "*::bar_profile";
            dds_return_t ret = dds_create_qos_provider_scope ("/path/to/qos_definitions.xml", &bar_provider, bar_scope);
            assert (ret == DDS_RETCODE_OK);
            ...
            dds_delete_qos_provider (foo_provider);
            dds_delete_qos_provider (bar_provider);

            return 0;
          }

    .. group-tab:: C++

        .. code-block:: C++

          #include "dds/dds.hpp"
          #include "DataType.hpp"

          using namespace org::eclipse::cyclonedds;

          int main()
          {
            // foo_provider will contains:
            // qos1_lib::foo_profile                     READER
            // qos1_lib::foo_profile                     WRITER
            // qos1_lib::foo_profile                     TOPIC
            // qos2_lib::foo_profile                     READER
            // qos2_lib::foo_profile                     WRITER
            // qos2_lib::foo_profile                     TOPIC
            std::string foo_scope = "*::foo_profile";
            dds::core::QosProvider foo_provider("/path/to/qos_definitions.xml", foo_scope);

            // bar_provider will contains:
            // qos1_lib::bar_profile                     WRITER
            // qos1_lib::bar_profile                     TOPIC
            // qos2_lib::bar_profile                     WRITER
            // qos2_lib::bar_profile                     TOPIC
            std::string bar_scope = "*::bar_profile";
            dds::core::QosProvider bar_provider("/path/to/qos_definitions.xml", bar_scope);
            ...
            return 0;
          }

Known limitations
=================

- Inheritance of QoS policies and QoS profiles in XML using the "base_name" attribute is not supported
- The "topic_filter" attribute for writer, reader and topic QoSes to associate a set of topics to a specific QoS when that QoS is part of a DDS profile, is not supported yet.
- The "entity_factory" attribute  for participant, writer and reader QoSes, is not supported yet.
- The <(user|topic|group)_data> base64 syntax is not supported yet.
- The C++ API QosProvider may throw an UnsupportedError when trying to access a policy that is not supported yet.
