HelloWorld publisher C source code
==================================

The ``Publisher.c`` contains the source that writes a **Hello World!**
Message.

The DDS publisher application code is almost symmetric to the subscriber, except 
that you must create a data writer instead of a data reader. To ensure data is 
written only when at least one matching reader is discovered, a synchronization
statement is added to the main thread. Synchronizing the main thread until a reader 
is discovered ensures we can start the publisher or subscriber program in any order.


.. code-block:: C
    :linenos:

    #include "ddsc/dds.h"
    #include "HelloWorldData.h"
    #include <stdio.h>
    #include <stdlib.h>

    int main (int argc, char ** argv)
    {
        dds_entity_t participant; 
        dds_entity_t topic; 
        dds_entity_t writer; 
        dds_return_t ret;
        HelloWorldData_Msg msg; 
        (void)argc;
        (void)argv;

        /* Create a Participant. */
        participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL); 
        DDS_ERR_CHECK (participant, DDS_CHECK_REPORT | DDS_CHECK_EXIT);

        /* Create a Topic. */
        topic = dds_create_topic (participant, &HelloWorldData_Msg_desc, "HelloWorldData_Msg", NULL, NULL); 
        DDS_ERR_CHECK (topic, DDS_CHECK_REPORT | DDS_CHECK_EXIT);

        /* Create a Writer. */
        writer = dds_create_writer (participant, topic, NULL, NULL);

        printf("=== [Publisher] Waiting for a reader to be discovered ...\n");

        ret = dds_set_status_mask(writer, DDS_PUBLICATION_MATCHED_STATUS); 
        DDS_ERR_CHECK (ret, DDS_CHECK_REPORT | DDS_CHECK_EXIT);

        while(true)
        {
            uint32_t status;
            ret = dds_get_status_changes (writer, &status); 
            DDS_ERR_CHECK (ret, DDS_CHECK_REPORT | DDS_CHECK_EXIT);

            if (status == DDS_PUBLICATION_MATCHED_STATUS) {
                break;
            }
            /* Polling sleep. */
            dds_sleepfor (DDS_MSECS (20));
        }

        /* Create a message to write. */
        msg.userID = 1;
        msg.message = "Hello World";

        printf ("=== [Publisher]    Writing : ");
        printf ("Message (%d, %s)\n", msg.userID, msg.message);

        ret = dds_write (writer, &msg);
        DDS_ERR_CHECK (ret, DDS_CHECK_REPORT | DDS_CHECK_EXIT);

        /* Deleting the participant will delete all its children recursively as well. */
        ret = dds_delete (participant);
        DDS_ERR_CHECK (ret, DDS_CHECK_REPORT | DDS_CHECK_EXIT);

        return EXIT_SUCCESS;
    }

To create a publisher:

#.  Send data using the DDS API and the ``HelloWorldData_Msg`` type, include the 
    appropriate header files:

    .. code-block:: C

        #include "ddsc/dds.h"
        #include "HelloWorldData.h"

#.  Create a writer. You must have a participant and a topic (must have the 
    same topic name as specified in ``subscriber.c``):

    .. code-block:: C

        dds_entity_t participant; 
        dds_entity_t topic; 
        dds_entity_t writer;

        participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL); 
        topic = dds_create_topic (participant, &HelloWorldData_Msg_desc,
        "HelloWorldData_Msg", NULL, NULL); 
        writer = dds_create_writer (participant, topic, NULL, NULL);

#.  When readers and writers are sharing the same data type and topic name, it connects 
    them without the application's involvement. To write data only when a DataReader 
    appears, a rendezvous pattern is required. A rendezvous pattern can be implemented by
    either:

    - Regularly polling the publication matching status (the preferred option in this 
      example).

    - Waiting for the publication/subscription matched events, where the publisher waits 
      and blocks the writing thread until the appropriate publication-matched event is 
      raised.

    The following line of code instructs |var-project-short| to listen on the 
    DDS\_PUBLICATION\_MATCHED\_STATUS:

    .. code-block:: C

        dds_set_status_mask(writer, DDS_PUBLICATION_MATCHED_STATUS);

#.  At regular intervals, the status change and a matching publication is received. In between, 
    the writing thread sleeps for a time period equal ``DDS\_MSECS`` (in milliseconds).

    .. code-block:: C

        while(true)
        {
            uint32_t status;
            ret = dds_get_status_changes (writer, &status);
            DDS_ERR_CHECK(ret, DDS_CHECK_REPORT | DDS_CHECK_EXIT);

            if (status == DDS_PUBLICATION_MATCHED_STATUS) {
                break;
            }
            /* Polling sleep. */ 
            dds_sleepfor (DDS_MSECS (20));
        }

    After this loop, a matching reader has been discovered.

#.  To write the data instance, create and initialize the data:

    .. code-block:: C

        HelloWorldData_Msg msg;

        msg.userID = 1;
        msg.message = "Hello World";

#.  Send the data instance of the keyed message:

    .. code-block:: C

        ret = dds_write (writer, &msg);

#.  When terminating the program, free the DDS allocated resources by deleting the 
    root entity of all the others (the domain participant):

    .. code-block:: C

        ret = dds_delete (participant);

    All the underlying entities, such as topic, writer, and so on, are deleted.
