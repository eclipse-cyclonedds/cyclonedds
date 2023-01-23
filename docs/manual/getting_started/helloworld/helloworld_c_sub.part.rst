HelloWorld subscriber C source code
==================================

The ``Subscriber.c`` file mainly contains the statements to wait for a HelloWorld 
message and reads it when it receives it.

.. important::

    The |var-project-short| ``read`` semantics keep the data sample in the data 
    reader cache. To prevent resource exhaustion, It is important to use ``take``, 
    where appropriate.

The subscriber application implements the steps defined in :ref:`key_steps`.

.. code-block:: C
    :linenos:

    #include "ddsc/dds.h"
    #include "HelloWorldData.h"
    #include <stdio.h>
    #include <string.h>
    #include <stdlib.h>

    /* An array of one message (aka sample in dds terms) will be used. */
    #define MAX_SAMPLES 1
    int main (int argc, char ** argv) {
      dds_entity_t participant;
      dds_entity_t topic;
      dds_entity_t reader;
      HelloWorldData_Msg *msg;
      void *samples[MAX_SAMPLES];
      dds_sample_info_t infos[MAX_SAMPLES];
      dds_return_t ret;
      dds_qos_t *qos;
      (void)argc;
      (void)argv;

      /* Create a Participant. */
      participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
      DDS_ERR_CHECK (participant, DDS_CHECK_REPORT | DDS_CHECK_EXIT);

      /* Create a Topic. */
      topic = dds_create_topic (participant, &HelloWorldData_Msg_desc,
      "HelloWorldData_Msg", NULL, NULL);
      DDS_ERR_CHECK (topic, DDS_CHECK_REPORT | DDS_CHECK_EXIT);

      /* Create a reliable Reader. */
      qos = dds_create_qos ();
      dds_qset_reliability (qos, DDS_RELIABILITY_RELIABLE, DDS_SECS (10));
      reader = dds_create_reader (participant, topic, qos, NULL);
      DDS_ERR_CHECK (reader, DDS_CHECK_REPORT | DDS_CHECK_EXIT);
      dds_delete_qos(qos);

      printf ("\n=== [Subscriber] Waiting for a sample ...\n");

      /* Initialize the sample buffer, by pointing the void pointer within
      * the buffer array to a valid sample memory location. */
      samples[0] = HelloWorldData_Msg alloc ();

      /* Poll until data has been read. */
      while (true)
      {
        /* Do the actual read.
        * The return value contains the number of samples read. */
        ret = dds_read (reader, samples, infos, MAX_SAMPLES, MAX_SAMPLES);
        DDS_ERR_CHECK (ret, DDS_CHECK_REPORT | DDS_CHECK_EXIT);

        /* Check if we read some data and it is valid. */
        if ((ret > 0) && (infos[0].valid_data))
        {
            /* Print Message. */
            msg = (HelloWorldData_Msg*) samples[0];
            printf ("=== [Subscriber] Received : ");
            printf ("Message (%d, %s)\n", msg->userID, msg->message);
            break;
        }
        else
        {
            /* Polling sleep. */
            dds_sleepfor (DDS_MSECS (20));
        }
      }
      /* Free the data location. */
      HelloWorldData_Msg_free (samples[0], DDS_FREE_ALL);
      /* Deleting the participant will delete all its children recursively as well. */
      ret = dds_delete (participant);
      DDS_ERR_CHECK (ret, DDS_CHECK_REPORT | DDS_CHECK_EXIT);

      return EXIT_SUCCESS;
    }

To create a subscriber:

#.  To recieve data using the DDS API and the ``HelloWorldData_Msg`` type, include the 
    appropriate header files:

    - The ``dds.h`` file to give access to the DDS APIs
    - The ``HelloWorldData.h`` is specific to the data type defined in the IDL

    .. code-block:: C

        #include "ddsc/dds.h"
        #include "HelloWorldData.h"

#.  At least three DDS entities are needed to build a minimalistic application:

    - Domain participant
    - Topic
    - Reader

    |var-project-short| implicitly creates a DDS Subscriber. If required, this 
    behavior can be overridden.

    .. code-block:: C

        dds_entity_t participant;
        dds_entity_t topic;
        dds_entity_t reader;

#.  To handle the data, create and declare some buffers:

    .. code-block:: C

        HelloWorldData_Msg *msg;
        void *samples[MAX_SAMPLES];
        dds_sample_info_t info[MAX_SAMPLES];

    The ``read()`` operation can return more than one data sample (where several 
    publishing applications are started simultaneously to write different message 
    instances), an array of samples is therefore needed.

    In |var-project-short|, data and metadata are propagated together. To handle 
    the metadata, the ``dds_sample_info`` array must be declared.

#.  The DDS participant is always attached to a specific DDS domain. In the HelloWorld 
    example, it is part of the ``Default\_Domain``, which is specified in the XML deployment 
    file. To override the default behavior, create or edit a deployment file (for example, 
    ``cyclonedds.xml``).

    .. code-block:: C

        participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);

#.  Create the topic with a given name. Topics with the same data type description 
    and with different names are considered other topics. This means that readers 
    or writers created for a given topic do not interfere with readers or writers 
    created with another topic even if they have the same data type. Topics with the 
    same name but incompatible datatype are considered an error and should be avoided.

    .. code-block:: C

        topic = dds_create_topic (participant, &HelloWorldData_Msg_desc, "HelloWorldData_Msg", NULL, NULL);

#.  Create a data reader and attach to it:

    .. code-block:: C

        dds_qos_t *qos = dds_create_qos ();
        dds_qset_reliability (qos, DDS_RELIABILITY_RELIABLE, DDS_SECS (10));
        reader = dds_create_reader (participant, topic, qos, NULL);
        dds_delete_qos(qos);

    The read operation expects an array of pointers to a valid memory location. This 
    means the samples array needs initialization by pointing the void pointer within 
    the buffer array to a valid sample memory location. In the example, there is an 
    array of one element; (``#define MAX_SAMPLES 1``.)

#.  Allocate memory for one ``HelloWorldData_Msg``:

    .. code-block:: C

        samples[0] = HelloWorldData_Msg_alloc ();

#.  Attempt to read data by going into a polling loop that regularly scrutinizes 
    and examines the arrival of a message:

    .. code-block:: C

        ret = dds_read (reader, samples, info, MAX_SAMPLES, MAX_SAMPLES);

    The ``dds_read`` operation returns the number of samples equal to the
    parameter ``MAX_SAMPLE``. If data has arrived in the reader's cache, 
    that number will exceed 0.

    The Sample\_info (``info``) structure shows whether the data is either: 
    
    - Valid data means that it contains the payload provided by the publishing application. 
    - Invalid data means we are reading the DDS state of the data Instance. 
    
    The state of a data instance can be, *DISPOSED* by the writer, or it is
    *NOT\_ALIVE* anymore, which could happen if the publisher application terminates 
    while the subscriber is still active. In this case, the sample is not considered 
    Valid, and its sample ``info[].Valid_data`` the field is ``False``:

    .. code-block:: C

        if ((ret > 0) && (info[0].valid_data))

#.  If data is read, cast the void pointer to the actual message data type 
    and display the contents:

    .. code-block:: C

        msg = (HelloWorldData_Msg*) samples[0]; 
        printf ("=== [Subscriber] Received : ");
        printf ("Message (%d, %s)\n", msg->userID, msg->message);
        break;

#.  When data is received and the polling loop is stopped, release the
    allocated memory and delete the domain participant:

    .. code-block:: C

        HelloWorldData_Msg_free (samples[0], DDS_FREE_ALL); 
        dds_delete (participant);

    All the entities that are created under the participant, such as the
    data reader and topic, are recursively deleted.
