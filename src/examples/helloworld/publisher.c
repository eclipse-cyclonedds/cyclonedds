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
    topic = dds_create_topic (participant, &HelloWorldData_Msg_desc,
                              "HelloWorldData_Msg", NULL, NULL);
    DDS_ERR_CHECK (topic, DDS_CHECK_REPORT | DDS_CHECK_EXIT);

    /* Create a Writer. */
    writer = dds_create_writer (participant, topic, NULL, NULL);

    printf("=== [Publisher]  Waiting for a reader to be discovered ...\n");

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

    printf ("=== [Publisher]  Writing : ");
    printf ("Message (%d, %s)\n", msg.userID, msg.message);

    ret = dds_write (writer, &msg);
    DDS_ERR_CHECK (ret, DDS_CHECK_REPORT | DDS_CHECK_EXIT);

    /* Deleting the participant will delete all its children recursively as well. */
    ret = dds_delete (participant);
    DDS_ERR_CHECK (ret, DDS_CHECK_REPORT | DDS_CHECK_EXIT);

    return EXIT_SUCCESS;
}
