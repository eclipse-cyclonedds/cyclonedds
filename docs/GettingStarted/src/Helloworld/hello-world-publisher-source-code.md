## _Hello World!_ Publisher Source Code

The **Publisher.c** contains the source that will write a _Hello World!_ Message.

From the DDS perspective the publisher application code is almost symmetric to the subscriber one, except that you need to create a Data Writer instead of a Data Reader. To assure data is written only when Cyclone DDS discovers at least a matching reader, a synchronization statement is added to main thread. Synchronizing the main thread till a reader is discovered assures we can start the publisher or subscriber program in any order.

```
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

    printf("=== [Publisher]	Waiting for a reader to be discovered ...\n");

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

    printf ("=== [Publisher]	Writing : ");
    printf ("Message (%d, %s)\n", msg.userID, msg.message);

    ret = dds_write (writer, &msg);
    DDS_ERR_CHECK (ret, DDS_CHECK_REPORT | DDS_CHECK_EXIT);

    /* Deleting the participant will delete all its children recursively as well. */
    ret = dds_delete (participant);
    DDS_ERR_CHECK (ret, DDS_CHECK_REPORT | DDS_CHECK_EXIT);

    return EXIT_SUCCESS;
}

```


We will be using the DDS API and the `HelloWorldData_Msg` type to send data, we therefore need to include the appropriate header files as we did in the subscriber code.

```
#include "ddsc/dds.h"
#include "HelloWorldData.h"
```


Like with the reader in **subscriber.c**, we need a participant and a topic to be able to create a writer. We use also need to use the same topic name as the one specified in the **subscriber.c**. 

```
dds_entity_t participant; 
dds_entity_t topic; 
dds_entity_t writer;

participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL); 
topic = dds_create_topic (participant, &HelloWorldData_Msg_desc,
"HelloWorldData_Msg", NULL, NULL); 
writer = dds_create_writer (participant, topic, NULL, NULL);
```

When Cyclone DDS discovers readers and writers sharing the same data type and topic name, it connects them without the application involvement. In order to write data only when a Data Readers pops up, a sort of Rendez-vous pattern is required. Such pattern can be implemented using:

1. Wait for the publication/subscription matched events, where the Publisher waits and blocks the writing-thread until the appropriate publication matched event is raised, or
2. Regularly, polls the publication matching status. This is the preferred option we will implement in this example. The following line of code instructs Cyclone DDS to listen on the DDS_PUBLICATION_MATCHED_STATUS:

```
dds_set_status_mask(writer, DDS_PUBLICATION_MATCHED_STATUS);
```


At regular interval we get the status change and for a matching publication. In between, the writing thread sleeps for a time period equal DDS_MSECS

```
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
```


After this loop, we are sure that a matching reader has been discovered. Now, we commence the writing of the data instance. First, the data must be created and initialized

```
HelloWorldData_Msg msg;

msg.userID = 1;
msg.message = "Hello World";
```


Then we can send the data instance of the keyed message.
```
ret = dds_write (writer, &msg);
```

When terminating the program, we free the DDS allocated resources by deleting the root entity of the all the others: the domain participant.
```
ret = dds_delete (participant);
```

All the underlying entities such as topic, writer … etc are deleted.
