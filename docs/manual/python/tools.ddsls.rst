.. index:: Python; DDSLS tool

.. _py_ddsls_tool:

Command line tool: ddsls
========================

Introduction
------------

When using DDS, it's sometimes useful to know what DDS entities are running in the system. The ddsls is a tool that subscribes to the Built-in topics, and shows the information of the domain participants, data readers and data writers in the system.


Usage
-----

To run the **ddsls**, you can use:

.. code-block:: console

    $ ddsls [Options]

You can view what options you have with the **ddsls** by using the command:

.. code-block:: console

    $ ddsls --help

And the help message is as follows:

.. code-block:: console

    usage: ddsls [-h] [-i ID] [-f FILENAME] [-j] [-w] [-v] [-r RUNTIME] (-a | -t {dcpsparticipant,dcpssubscription,dcpspublication})

    optional arguments:
    -h, --help            show this help message and exit
    -i ID, --id ID        Define the domain participant id
    -f FILENAME, --filename FILENAME
                          Write results to file in JSON format
    -j, --json            Print output in JSON format
    -w, --watch           Watch for data reader & writer & qoses changes
    -v, --verbose         View the sample when Qos changes
    -r RUNTIME, --runtime RUNTIME
                          Limit the runtime of the tool, in seconds.
    -a, --all             for all topics
    -t {dcpsparticipant,dcpssubscription,dcpspublication}, --topic {dcpsparticipant,dcpssubscription,dcpspublication}
                          for one specific topic

There are several options you can choose to use the **ddsls** according to your needs, which will be further explained in the following sections.

For checking the DDS entity information, you can use the ``--topic`` or ``--all`` option to specify which entity's information you want to see, the ``--id`` option to define which domain you like to check using the id of the domain participant, the ``--watch`` option to monitor the changes in the system as entities being created and disposed.

For viewing options, you can use the ``--json`` option to view the output data in JSON format, the ``--verbose`` option to view the full sample information when the entity's QoS changes.

For additional options, you can use the ``--filename`` option to define the name of file you want to output the result to, the ``--runtime`` option to specify how long the **ddsls** will run.


Topic
^^^^^

To use the **ddsls**, you need to specify the topic you're subscribing to using the ``--topic`` or ``--all`` option.

For ``--topic`` option, you can choose from ``dcpsparticipant``, ``dcpssubscription`` and ``dcpspublication``.

The ``dcpsparticipant``, ``dcpssubscription`` and ``dcpspublication`` subscribes to the **BuiltinTopicDcpsParticipant**, **BuiltinTopicSubscription** and **BuiltinTopicDcpsPublication** topic respectively, and will show the information of all the participants, subscribers and publishers in the domain respectively.

Here's an example of running **ddsls** with the ``dcpsparticipant`` topic, run the command:

.. code-block:: console

    $ ddsls --all

If there are DDS participant, data reader and data writer running in the default domain, it will show up in the **ddsls** like this:

.. code-block:: console

    -- New PARTICIPANT --
    key: 9ce21001-ad00-8ac9-4428-52f1000001c1
    qos: Qos()

    key: 350c1001-c5e3-ca6b-712a-ee09000001c1
    qos: Qos()


    -- New PUBLICATION --
    key: 9ce21001-ad00-8ac9-4428-52f100000102
    participant_key: 9ce21001-ad00-8ac9-4428-52f1000001c1
    participant_instance_handle: 11822753457071331301
    topic_name: Vehicle
    type_name: vehicles::Vehicle
    qos: Qos(Policy.Deadline(deadline=10000), Policy.DestinationOrder.ByReceptionTimestamp, Policy.Durability.Transient, Policy.DurabilityService(cleanup_delay=0, history=Policy.History.KeepLast(depth=1), max_samples=-1, max_instances=-1, max_samples_per_instance=-1), Policy.History.KeepLast(depth=10), Policy.IgnoreLocal.Nothing, Policy.LatencyBudget(budget=0), Policy.Lifespan(lifespan=9223372036854775807), Policy.Liveliness.Automatic(lease_duration=9223372036854775807), Policy.Ownership.Shared, Policy.OwnershipStrength(strength=0), Policy.PresentationAccessScope.Instance(coherent_access=False, ordered_access=False), Policy.Reliability.BestEffort, Policy.ResourceLimits(max_samples=-1, max_instances=-1, max_samples_per_instance=-1), Policy.TransportPriority(priority=0), Policy.WriterDataLifecycle(autodispose=True))


    -- New SUBSCRIPTION --
    key: 350c1001-c5e3-ca6b-712a-ee0900000107
    participant_key: 350c1001-c5e3-ca6b-712a-ee09000001c1
    participant_instance_handle: 5513147631977453825
    topic_name: Vehicle
    type_name: vehicles::Vehicle
    qos: Qos(Policy.Deadline(deadline=10000), Policy.DestinationOrder.ByReceptionTimestamp, Policy.Durability.Transient, Policy.History.KeepLast(depth=10), Policy.IgnoreLocal.Nothing, Policy.LatencyBudget(budget=0), Policy.Liveliness.Automatic(lease_duration=9223372036854775807), Policy.Ownership.Shared, Policy.PresentationAccessScope.Instance(coherent_access=False, ordered_access=False), Policy.ReaderDataLifecycle(autopurge_nowriter_samples_delay=9223372036854775807, autopurge_disposed_samples_delay=9223372036854775807), Policy.Reliability.BestEffort, Policy.ResourceLimits(max_samples=-1, max_instances=-1, max_samples_per_instance=-1), Policy.TimeBasedFilter(filter_time=0), Policy.TransportPriority(priority=0))


Comprehend output
"""""""""""""""""

* The result above shows that there is two participant, one data reader and one data writer running in the default domain;

* **New** in "New PARTICIPANT", "New SUBSCRIPTION", "New PUBLICATION" indicates that the entities are alive. If the entities are no longer alive, the message will be **Disposed** instead, such as "Disposed PARTICIPANT".

* And the fields for the entities are:

  * **PARTICIPANT**:

    * **key**: The GUID (Globally Unique Identifier) of the domain participant.

  * **SUBSCRIPTION** and **PUBLICATION**:

    * **key**: The GUID of the data reader or data writer;
    * **participant_key**: The GUID of the domain participant that created the data reader or data writer;
    * **participant_instance_handle**: The instance handle of the domain participant;
    * **topic_name**: The name of the topic that the data reader / data writer is subscribing / writing to;
    * **type_name**: The type name used in the topic of the data reader / data writer;
    * **qos**: The QoS (Quality of Service) of the data reader / data writer.

Domain participant id
^^^^^^^^^^^^^^^^^^^^^

By default, the **ddsls** subscribes to the default domain (domain 0) and displays information of entities in that domain. However, if you want to view the entity information in another domain, you can use the option ``-- id`` to change the domain to which the **ddsls** subscribes.

The ``--id`` option will set the id of the **ddsls** domain participant, allowing the **ddsls** to view entities in the domain you chooses.

For example, if you run a small script using domain 1 as the domain participant:

.. code-block:: python
    :linenos:

    from cyclonedds.domain import DomainParticipant

    dp = DomainParticipant(1)

If you run ``ddsls --topic dcpsparticipant``, the participant you've just created will not be there, since it's only viewing entities in the default domain.

To view this participant information, you need to use:

.. code-block:: console

    $ ddsls --topic dcpsparticipant --id 1

And the result of the participant in domain 1 will be:

.. code-block:: console

    -- New PARTICIPANT --
    key: 02371001-8251-a889-325a-cad5000001c1
    qos: Qos()


Watch mode
^^^^^^^^^^
By default, the **ddsls** will run for 1 second and then automatically exit. However, if you want to monitor the entities in the system, you can use the ``--watch`` option to enable the watch mode.

In watch mode, the **ddsls** will not automatically exit (if the ``--runtime`` option is not selected).  The watch mode monitors entities and displays entity information as they are created and disposed, or as their QoS changes.


For example, if you have the **ddsls** monitoring the ``dcpsparticipant`` topic, using the command:

.. code-block:: console

    $ ddsls --topic dcpsparticipant --watch

Then start and exit a script that creates a domain participant entity in the default domain, you can get a result like this:

.. code-block:: console

    -- New PARTICIPANT --
    key: 713b1001-bb82-49db-9f2a-46f4000001c1
    qos: Qos()


    -- Disposed PARTICIPANT --
    key: 713b1001-68a5-e15c-2709-d195000001c1
    qos: Qos()

* **New** indicates that the participant is alive in the domain;
* **Disposed** indicates that the participant has already been disposed.

Verbose mode
^^^^^^^^^^^^

Verbose mode is an optional mode for the **ddsls**. By default, the **ddsls** will only display information of the specific QoS policies that has been changed. But you can use the ``--verbose`` option to enable to verbose mode, to not only view the specific information, but also the complete information of the entity that has QoS changed.

By default, when the QoS changes in an entity, the **ddsls** will display which policy has been changed, on which topic and which entity, and the old and new value for the policy.

For example, when QoS changes in a data writer, you can get a result like this:

.. code-block:: console

    Qos changed on topic 'MessageTopic' publication:
     key = 1d681001-c040-7b68-2e4d-5fb900000102
     Policy.OwnershipStrength(strength=10) -> Policy.OwnershipStrength(strength=20)
     Policy.Userdata(data=b'Old') -> Policy.Userdata(data=b'New')

In this example:

* The QoS changed happened in the **publication**, which means it's a data writer or a publisher, writing to the topic named "MessageTopic";
* The **key** is the GUID of the entity that has QoS changed;
* The changed QoS **policy** are OwnershipStrength" and "Userdata", "OwnershipStrength" changed from 10 to 20 and "Userdata" changed from "Old" to "New".

When verbose mode is activated, using the command:

.. code-block:: console

    $ ddsls --all --watch --verbose

The **ddsls** will not only display the specific QoS change information, but also display the entity information on which the QoS changes occurs.

.. code-block:: console

    Qos changed on topic 'MessageTopic' publication:
     key = e4921001-4edb-926f-14be-adb500000102
     Policy.OwnershipStrength(strength=10) -> Policy.OwnershipStrength(strength=20)
     Policy.Userdata(data=b'Old') -> Policy.Userdata(data=b'New')

    -- New PUBLICATION --
    key: e4921001-4edb-926f-14be-adb500000102
    participant_key: e4921001-4edb-926f-14be-adb5000001c1
    participant_instance_handle: 2236347693610277994
    topic_name: MessageTopic
    type_name: testtopics::message::Message
    qos: Qos(Policy.Deadline(deadline=9223372036854775807), Policy.DestinationOrder.ByReceptionTimestamp, Policy.Durability.Volatile, Policy.DurabilityService(cleanup_delay=0, history=Policy.History.KeepLast(depth=1), max_samples=-1, max_instances=-1, max_samples_per_instance=-1), Policy.History.KeepLast(depth=1), Policy.IgnoreLocal.Nothing, Policy.LatencyBudget(budget=0), Policy.Lifespan(lifespan=9223372036854775807), Policy.Liveliness.Automatic(lease_duration=9223372036854775807), Policy.Ownership.Shared, Policy.OwnershipStrength(strength=20), Policy.PresentationAccessScope.Instance(coherent_access=False, ordered_access=False), Policy.Reliability.Reliable(max_blocking_time=100000000), Policy.ResourceLimits(max_samples=-1, max_instances=-1, max_samples_per_instance=-1), Policy.TransportPriority(priority=0), Policy.Userdata(data=b'New'), Policy.WriterDataLifecycle(autodispose=True))

In this verbose mode example:

* The **key** in "New PUBLICATION" is the same as the one in the specific QoS change information, indicating that this is the entity that had QoS changed;
* The value of the policies "OwnershipStrength" and "Userdata" has been changed to the new values.

JSON mode
^^^^^^^^^

For better viewing the entity information, you can use the ``--json`` option to view the results in JSON format.

For example, start up the **ddsls** in JSON mode using the command:

.. code-block:: console

    $ ddsls --topic participant --json

And you can get a result in JSON format like this:

.. code-block:: JSON

    [
    {
        "type": "PARTICIPANT",
        "event": "new",
        "value": [
            {
                "key": "5dc81001-75dc-1fe3-5468-48b3000001c1"
            },
            {
                "key": "764e1001-d9da-53dd-ca0b-ab06000001c1"
            }
        ]
    }]

In JSON mode, the output result is divided into 3 parts:

* **type** is the type of entity, "PARTICIPANT" or "SUBSCRIPTION" or "PUBLICATION";
* **event** indicates whether the entity is alive or disposed, using "new" or "disposed";
* **value** is the properties of the entity, such as the GUID of the entity.

Write to file
^^^^^^^^^^^^^
Other than printing the results in the terminal, you can also choose to write the results to a file using the ``--filename`` option, providing the name of the file you want to write to. The results will be written to the file in JSON format.

For example, you can choose to write the result to a file named "test.json" using the command:

.. code-block:: console

    $ ddsls --topic participant --watch --filename ddsls_data.json

After stopping the **ddsls**, the results will be written to the test.json file in your current directory. And the ddsls_data.json file will look like this:

.. code-block:: JSON

    {
        "PARTICIPANT": {
            "New": {
                "da531001-77b3-aef6-0cb8-647f000001c1": {
                    "key": "da531001-77b3-aef6-0cb8-647f000001c1"
                }
            },
            "Disposed": {
                "8e281001-e010-0c8d-305c-20a3000001c1": {
                    "key": "8e281001-e010-0c8d-305c-20a3000001c1"
                }
            }
        }
    }

In the output file:

* **"PARTICIPANT"** is the type of the entity;
* **"New"** or **"Disposed"** indicates whether the entity is alive or disposed at the time **ddsls** stopped running;
* The **GUID** of the entity will group the entity information, such as the **"key"**, **"qos"**, in a JSON dictionary.

Runtime
^^^^^^^

Besides using the **ddsls** by default, which will only run for 1 seconds, and using the **ddsls** in watch mode, which will run indefinitely until there is an interrupt, you can use the ``--runtime`` option to customize the running time according to your needs.

For example, if you want to run the **ddsls** for 10 seconds, you can use the command:

.. code-block:: console

    $ ddsls --all --runtime 10

And the tool will automatically shut down after running for 10 seconds. This ``--runtime`` option can also apply to watch mode.