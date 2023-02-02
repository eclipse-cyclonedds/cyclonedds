.. index:: Python: Pubsub tool

.. _py_pubsub_tool:

Command line tool: pubsub
=========================

Introduction
------------

When using DDS, it's sometimes useful to do some manual testing, publishing and subscribing to specific topics. Pubsub is the tool for that.

For now, the **pubsub** can be used to read/write to a user-defined topic with customized QoS, and you can do the read/write by simply typing what to publish.

In the future, with the support of XTypes, the **pubsub** will also be able to read/write to arbitrary topic that exists in the system. This way, you will be able to use the **pubsub** to read/write to any topics in the system.

Usage
-----

To use the **pubsub**, you can use:

.. code-block:: console

    $ pubsub [Options]

You can view what options you have with the **pubsub** by using the command:

.. code-block:: console

    $ pubsub --help

And the help message is as follows:

.. code-block:: console

    usage: pubsub [-h] [-T TOPIC] [-f FILENAME] [-eqos {all,topic,publisher,subscriber,datawriter,datareader}] [-q QOS [QOS ...]] [-r RUNTIME] [--qoshelp]

    optional arguments:
    -h, --help              show this help message and exit
    -T TOPIC, --topic TOPIC
                            The name of the topic to publish/subscribe to
    -f FILENAME, --filename FILENAME
                            Write results to file in JSON format
    -eqos {all,topic,publisher,subscriber,datawriter,datareader}, --entityqos {all,topic,publisher,subscriber,datawriter,datareader}
                            Select the entites to set the qos.
                            Choose between all entities, topic, publisher, subscriber, datawriter and datareader. (default: all).
                            Inapplicable qos will be ignored.
    -q QOS [QOS ...], --qos QOS [QOS ...]
                            Set QoS for entities, check '--qoshelp' for available QoS and usage
    -r RUNTIME, --runtime RUNTIME
                            Limit the runtime of the tool, in seconds.
    --qoshelp               e.g.:
                                --qos Durability.TransientLocal
                                --qos History.KeepLast 10
                                --qos ReaderDataLifecycle 10, 20
                                --qos Partition [a, b, 123]
                                --qos PresentationAccessScope.Instance False, True
                                --qos DurabilityService 1000, History.KeepLast 10, 100, 10, 10
                                --qos Durability.TransientLocal History.KeepLast 10
                            
                            Available QoS and usage are:
                            --qos Reliability.BestEffort
                            --qos Reliability.Reliable [max_blocking_time<integer>]
                            --qos Durability.Volatile
                            --qos Durability.TransientLocal
                            --qos Durability.Transient
                            --qos Durability.Persistent
                            --qos History.KeepAll
                            --qos History.KeepLast [depth<integer>]
                            --qos ResourceLimits [max_samples<integer>], [max_instances<integer>], [max_samples_per_instance<integer>]
                            --qos PresentationAccessScope.Instance [coherent_access<boolean>], [ordered_access<boolean>]
                            --qos PresentationAccessScope.Topic [coherent_access<boolean>], [ordered_access<boolean>]
                            --qos PresentationAccessScope.Group [coherent_access<boolean>], [ordered_access<boolean>]
                            --qos Lifespan [lifespan<integer>]
                            --qos Deadline [deadline<integer>]
                            --qos LatencyBudget [budget<integer>]
                            --qos Ownership.Shared
                            --qos Ownership.Exclusive
                            --qos OwnershipStrength [strength<integer>]
                            --qos Liveliness.Automatic [lease_duration<integer>]
                            --qos Liveliness.ManualByParticipant [lease_duration<integer>]
                            --qos Liveliness.ManualByTopic [lease_duration<integer>]
                            --qos TimeBasedFilter [filter_time<integer>]
                            --qos Partition [partitions<Sequence[str]>]
                            --qos TransportPriority [priority<integer>]
                            --qos DestinationOrder.ByReceptionTimestamp
                            --qos DestinationOrder.BySourceTimestamp
                            --qos WriterDataLifecycle [autodispose<boolean>]
                            --qos ReaderDataLifecycle [autopurge_nowriter_samples_delay<integer>], [autopurge_disposed_samples_delay<integer>]
                            --qos DurabilityService [cleanup_delay<integer>], [History.KeepAll / History.KeepLast [depth<integer>]], [max_samples<integer>], [max_instances<integer>], [max_samples_per_instance<integer>]
                            --qos IgnoreLocal.Nothing
                            --qos IgnoreLocal.Participant
                            --qos IgnoreLocal.Process
                            --qos Userdata [data<bytes>]
                            --qos Groupdata [data<bytes>]
                            --qos Topicdata [data<bytes>]

There are several options to configure the **pubsub**, the options will be further explained in the following sections.

For publishing and subscribing data, you can use the ``--topic`` option to define the topic to publish/subscribe to.

You can also modify QoS using the ``--qos`` option, and use the ``--entityqos`` option if you want to modify the QoS for a particular entity. If you need help setting up the QoS, the ``--qoshelp`` option will show you some examples and the policies you can choose from.

For additional options, you can use the ``--filename`` option to define the name of file you want to output the result to, the ``--runtime`` option to specify how long the **pubsub** will run.

Topic
^^^^^

To startup the pubsub tool, you need to use the ``--topic`` option to specify the topic you want to read/write to.

For example, if you want the **pubsub** to read/write to the topic "HelloWord", you can use the command:

.. code-block:: console

    $ pubsub --topic HelloWorld


Read/write data
"""""""""""""""

After starting up the **pubsub** with a specif topic, you can read/write data by simply publishing your data in the terminal.

For now, the supported data types for read/write are integer, string, integer and string array, integer and string sequence.

For example, writing "420", "test", "[1, 8, 3]", "['h', 'e', 'l', 'l', 'o']", "[20]", "['test']" to the terminal respectively, you can see the subscribed message with their datatype printing in the terminal.

.. code-block:: console

    420
    Subscribed: Integer(seq=0, keyval=420)
    test
    Subscribed: String(seq=1, keyval='test')
    [1, 8, 3]
    Subscribed: IntArray(seq=2, keyval=[1, 8, 3])
    ['h','e','l','l','o']
    Subscribed: StrArray(seq=3, keyval=['h', 'e', 'l', 'l', 'o'])
    [20]
    Subscribed: IntSequence(seq=4, keyval=[20])
    ['test']
    Subscribed: StrSequence(seq=5, keyval=['test'])

In the output result:

* **Subscribed** indicates that the **pubsub** tool has subscribed to data in the topic you defined;
* The **Integer**, **String**, etc., is the datatype of the subscribed data.
* **seq** is the sequence number for the **pubsub** subscribed data;
* **keyval** is the data published.

QoS
^^^

By default, the **pubsub** uses the default QoS for DDS, but you can use the ``--qos`` option to change the QoS according to your needs.

For example, if you want to set the Reliability QoS to best effort, you can run the **pubsub** with command:

.. code-block:: console

    $ pubsub --topic hello --qos Reliability.BestEffort

You can also change multiple QoS for the **pubsub**. For example, in addition to setting the Reliability QoS, you also want to set the History to keep the last 10 samples, you can use the command:

.. code-block:: console

    $ pubsub --topic hello --qos Reliability.BestEffort History.KeepLast 10

If a policy requires multiple arguments, simply use a space or comma to separate the arguments. For example, you can set the DurabilityService QoS by the command:

.. code-block:: console

    $ pubsub --topic hello --qos DurabilityService 1000, History.KeepLast 10, 100, 10, 10

There are some freedom to type the QoS and the arguments on the command line:

* The QoS policy is case insensitive, so you can use the command like ``-qos reliAbility.REliable``;
* To separate arguments, you can use space, comma, or colon. For example ``--qos ResourceLimits: 100, -1 100``;
* For writing duration, you can use arguments like "'seconds=10;minutes=2'". For example ``--qos lifespan "seconds=10;days=1"``;
* For boolean, other than "True" and "False", you can also use "1", "on", "yes" to represent "True", and use "0", "off", "no" to represent "False". For example ``--qos WriterDataLifecycle off``.

Entity QoS
^^^^^^^^^^

When setting the QoS in the **pubsub**, it will set the QoS for all the entities by default. However, you can also use the ``--entityqos`` option to set the QoS for a certain entity. You can choose to set the desired QoS on topic, subscriber, publisher, data reader or data writer.

For example, to set the Durability QoS to TransientLocal on the data writer, use the command:

.. code-block:: console

    $ pubsub --topic hello --qos Durability.TransientLocal --entityqos datawriter

.. note::
    The ``--entityqos`` option needs to be used together with the ``--qos`` option.

Inapplicable QoS for entity
"""""""""""""""""""""""""""

Some QoS policy has limitation as to which entity it can apply to, such as the Topicdata QoS can only be applied to the topic.

If you selected a QoS policy that is not applicable to the entity you selected, the QoS will be ignored and use the default QoS policy value for the entity.

For example, if you set Topicdata QoS on data reader, using the command:

.. code-block:: console

    $ pubsub --topic hello --qos Topicdata test --entityqos datareader

It will show a warning stating that the policy is not applicable and will be ignored, like this:

.. code-block:: console

    InapplicableQosWarning: The Policy.Topicdata(data=b'test') is not applicable for datareader, will be ignored.


Incompatible QoS
""""""""""""""""

To publish and subscribe to data, some QoS need to be compatible between the publisher and subscriber ends, this is the RxO (Requested/Offered) property.

When setting the entity QoS in the **pubsub**, if the QoS you set has RxO property, and if the QoS policy is incompatible between what is requested by the subscriber and what is offered by the publisher, the **pubsub** will print out a warning message, stating that it detects request/offer incompatibility, may not be able to publish and subscribe successfully.

For example, if you set the Durability QoS to TransientLocal for data reader, using the command:

.. code-block:: console

    $ pubsub --topic hello --qos Durability.TransientLocal --entityqos datareader

The **pubsub** will print out a warning message for incompatible QoS, like this:

.. code-block:: console

    IncompatibleQosWarning: The Qos requested for subscription is incompatible with the Qos offered by publication.PubSub may not be available.


And if you try to publish data in the terminal, the **pubsub** won't be able to subscribe to that published data.

Write to file
^^^^^^^^^^^^^

When data is subscribed in the **pubsub**, the subscribed data will be printed out in the terminal. There is an option to also write the subscribed data to a file, using the ``--filename`` option to provide the name of the file you want to write to. The outputted file will be written in JSON format.

For example, you can choose to write the result to a file named "pubsub_data.json" using the command:

.. code-block::

    $ pubsub --topic hello --filename pubsub_data.json

When publishing data in the terminal, you can view the subscribed results in the terminal and the results will be written to the "pubsub_data.json" file when the **pubsub** stops running. The "pubsub_data.json" file will look like this:

.. code-block:: JSON

    {
        "sequence 0": {
            "type": "integer",
            "keyval": 420
        },
        "sequence 1": {
            "type": "integer",
            "keyval": 33
        },
        "sequence 2": {
            "type": "string",
            "keyval": "hello"
        },
        "sequence 3": {
            "type": "int_array",
            "keyval": [
                2,
                3,
                3
            ]
        }
    }

In the output file:

* **"sequence <n>"** is the sequence number for the subscribed data;
* **"type"** is the datatype for the topic;
* **"keyval"** is the data you published.

Runtime
^^^^^^^

By default, the **pubsub** will run indefinitely until there is an interrupt, but you can also use the ``--runtime`` option to stop the **pubsub** after running for a certain time.

For example, if you want to run the **pubsub** for 5 seconds, you can use the command:

.. code-block:: console

    $ pubsub --topic hello --runtime 5

And the **pubsub** will be run for 5 seconds, publish and subscribe to data in the terminal within the 5 seconds, then automatically exit the tool after 5 seconds.