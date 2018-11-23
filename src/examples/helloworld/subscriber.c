#include "ddsc/dds.h"
#include "HelloWorldData.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* An array of one message (aka sample in dds terms) will be used. */
#define MAX_SAMPLES 1

int main (int argc, char ** argv)
{
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

    /* Initialize sample buffer, by pointing the void pointer within
     * the buffer array to a valid sample memory location. */
    samples[0] = HelloWorldData_Msg__alloc ();

    /* Poll until data has been read. */
    while (true)
    {
        /* Do the actual read.
         * The return value contains the number of read samples. */
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
