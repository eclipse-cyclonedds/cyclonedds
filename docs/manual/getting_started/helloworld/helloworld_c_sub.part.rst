
The ``Subscriber.c`` file mainly contains the statements to wait for a HelloWorld 
message and reads it when it receives it.

.. important::

    The |var-project-short| ``read`` semantics keep the data sample in the data 
    reader cache. To prevent resource exhaustion, It is important to use ``take``, 
    where appropriate.

The subscriber application implements the steps defined in :ref:`key_steps`.

The following is a copy of the **subscriber.c** file that is available from the 
|url::helloworld_c_github| repository.

.. literalinclude:: ../../../../examples/helloworld/subscriber.c
    :language: c
    :linenos:

To create a subscriber:

#.  To recieve data using the DDS API and the ``HelloWorldData_Msg`` type, include the 
    appropriate header files:

    - The ``dds.h`` file to give access to the DDS APIs
    - The ``HelloWorldData.h`` is specific to the data type defined in the IDL

    .. code-block:: C
       :linenos:
       :lineno-start: 1
 
        #include "ddsc/dds.h"
        #include "HelloWorldData.h"

#.  At least three DDS entities are needed to build a minimalistic application:

    - Domain participant
    - Topic
    - Reader

    |var-project-short| implicitly creates a DDS Subscriber. If required, this 
    behavior can be overridden.

    .. code-block:: C
        :linenos:
        :lineno-start: 12

        dds_entity_t participant;
        dds_entity_t topic;
        dds_entity_t reader;

#.  To handle the data, create and declare some buffers:

    .. code-block:: C
        :linenos:
        :lineno-start: 15

        HelloWorldData_Msg *msg;
        void *samples[MAX_SAMPLES];
        dds_sample_info_t info[MAX_SAMPLES];

    The ``read()`` operation can return more than one data sample (where several 
    publishing applications are started simultaneously to write different message 
    instances), an array of samples is therefore needed.

    In |var-project-short|, data and metadata are propagated together. To handle 
    the metadata, the ``dds_sample_info`` array must be declared.

#.  The DDS participant is always attached to a specific DDS domain. In the HelloWorld 
    example, it is part of the ``Default\_Domain``, which is specified in the XML configuration 
    file. To override the default behavior, create or edit a configuration file (for example, 
    ``cyclonedds.xml``). For further information, refer to the :ref:`config-docs` and the 
    :ref:`configuration_reference`.

    .. code-block:: C
        :linenos:
        :lineno-start: 24

        participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);

#.  Create the topic with a given name. Topics with the same data type description 
    and with different names are considered other topics. This means that readers 
    or writers created for a given topic do not interfere with readers or writers 
    created with another topic even if they have the same data type. Topics with the 
    same name but incompatible datatype are considered an error and should be avoided.

    .. code-block:: C
        :linenos:
        :lineno-start: 29

        topic = dds_create_topic (
            participant, &HelloWorldData_Msg_desc, "HelloWorldData_Msg", NULL, NULL);
        if (topic < 0)
            DDS_FATAL("dds_create_topic: %s\n", dds_strretcode(-topic));

#.  Create a data reader and attach to it:

    .. code-block:: C
        :linenos:
        :lineno-start: 35

        qos = dds_create_qos ();
        dds_qset_reliability (qos, DDS_RELIABILITY_RELIABLE, DDS_SECS (10));
        reader = dds_create_reader (participant, topic, qos, NULL);
        if (reader < 0)
            DDS_FATAL("dds_create_reader: %s\n", dds_strretcode(-reader));
        dds_delete_qos(qos);

    The read operation expects an array of pointers to a valid memory location. This 
    means the samples array needs initialization by pointing the void pointer within 
    the buffer array to a valid sample memory location. In the example, there is an 
    array of one element; (``#define MAX_SAMPLES 1``.)

#.  Allocate memory for one ``HelloWorldData_Msg``:

    .. code-block:: C
        :linenos:
        :lineno-start: 47

        samples[0] = HelloWorldData_Msg__alloc ();

#.  Attempt to read data by going into a polling loop that regularly scrutinizes 
    and examines the arrival of a message:

    .. code-block:: C
        :linenos:
        :lineno-start: 54

        rc = dds_read (reader, samples, infos, MAX_SAMPLES, MAX_SAMPLES);
        if (rc < 0)
            DDS_FATAL("dds_read: %s\n", dds_strretcode(-rc));

    The ``dds_read`` operation returns the number of samples equal to the
    parameter ``MAX_SAMPLE``. If data has arrived in the reader's cache, 
    that number will exceed 0.

#.  The Sample\_info (``info``) structure shows whether the data is either: 
    
    - Valid data means that it contains the payload provided by the publishing application. 
    - Invalid data means we are reading the DDS state of the data Instance. 
    
    The state of a data instance can be, *DISPOSED* by the writer, or it is
    *NOT\_ALIVE* anymore, which could happen if the publisher application terminates 
    while the subscriber is still active. In this case, the sample is not considered 
    valid, and its sample ``info[].Valid_data`` the field is ``False``:

    .. code-block:: C
        :linenos:
        :lineno-start: 59

        if ((ret > 0) && (info[0].valid_data))

#.  If data is read, cast the void pointer to the actual message data type 
    and display the contents:

    .. code-block:: C
        :linenos:
        :lineno-start: 62

        msg = (HelloWorldData_Msg*) samples[0]; 
        printf ("=== [Subscriber] Received : ");
        printf ("Message (%d, %s)\n", msg->userID, msg->message);
        break;

#.  When data is received and the polling loop is stopped, release the
    allocated memory and delete the domain participant:

    .. code-block:: C
        :linenos:
        :lineno-start: 76
    
        HelloWorldData_Msg_free (samples[0], DDS_FREE_ALL); 

#.  All the entities that are created under the participant, such as the
    data reader and topic, are recursively deleted.

    .. code-block:: C
        :linenos:
        :lineno-start: 79
    
        rc = dds_delete (participant);
        if (rc != DDS_RETCODE_OK)
            DDS_FATAL("dds_delete: %s\n", dds_strretcode(-rc));


