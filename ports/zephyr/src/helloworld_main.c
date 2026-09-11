#include <zephyr/kernel.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dds/dds.h>
#include "HelloWorldData.h"
#include "helloworld_config.h"

/* cdds_xml_config[] is generated from config.xml by GENERATE_CDDS_CONF and holds
 * the NUL-terminated string "CYCLONEDDS_URI=<CycloneDDS>...</CycloneDDS>".
 */
#define CDDS_URI_PREFIX "CYCLONEDDS_URI="
#define CDDS_GENERAL    "<General>"

/* Boards on which the DDS interface has to be selected explicitly. */
#if defined(CONFIG_BOARD_QEMU_X86)
#define CDDS_NETIF "eth0"
#elif defined(CONFIG_BOARD_S32Z270DC2_RTU0_R52)
#define CDDS_NETIF "ethernet@74b00000"
#endif

static void set_cyclonedds_uri(void)
{
  const char *xml = (const char *)cdds_xml_config + strlen(CDDS_URI_PREFIX);

#ifdef CDDS_NETIF
  /* Inject <Interfaces> into <General> so that the network interface is chosen
     explicitly, like the raw-config based version of this example did. */
  static const char iface[] =
    "<Interfaces><NetworkInterface name=\"" CDDS_NETIF "\"/></Interfaces>";
  static char buf[sizeof(cdds_xml_config) + sizeof(iface)];
  const char *general = strstr(xml, CDDS_GENERAL);

  if (general != NULL) {
    size_t head = (size_t)(general - xml) + strlen(CDDS_GENERAL);

    memcpy(buf, xml, head);
    memcpy(&buf[head], iface, sizeof(iface) - 1);
    strcpy(&buf[head + sizeof(iface) - 1], general + strlen(CDDS_GENERAL));
    xml = buf;
  }
#endif

  /* Publish the configuration through the environment; the POSIX layer owns
     the environ block, so no application-side environ definition is needed. */
  if (setenv("CYCLONEDDS_URI", xml, 1) != 0)
    printf("Failed to set CYCLONEDDS_URI\n");
}

void helloworld_publisher()
{
  dds_entity_t participant;
  dds_entity_t topic;
  dds_entity_t writer;
  dds_return_t rc;
  HelloWorldData_Msg msg;
  uint32_t status = 0;

  /* Create a Participant. */
  participant = dds_create_participant (1, NULL, NULL);
  if (participant < 0)
    DDS_FATAL("dds_create_participant: %s\n", dds_strretcode(-participant));

  /* Create a Topic. */
  topic = dds_create_topic (
    participant, &HelloWorldData_Msg_desc, "HelloWorldData_Msg", NULL, NULL);
  if (topic < 0)
    DDS_FATAL("dds_create_topic: %s\n", dds_strretcode(-topic));

  /* Create a Writer. */
  writer = dds_create_writer (participant, topic, NULL, NULL);
  if (writer < 0)
    DDS_FATAL("dds_create_writer: %s\n", dds_strretcode(-writer));

  printf("=== [Publisher]  Waiting for a reader to be discovered ...\n");
  fflush (stdout);

  rc = dds_set_status_mask(writer, DDS_PUBLICATION_MATCHED_STATUS);
  if (rc != DDS_RETCODE_OK)
    DDS_FATAL("dds_set_status_mask: %s\n", dds_strretcode(-rc));

  while(!(status & DDS_PUBLICATION_MATCHED_STATUS))
  {
    rc = dds_get_status_changes (writer, &status);
    if (rc != DDS_RETCODE_OK)
      DDS_FATAL("dds_get_status_changes: %s\n", dds_strretcode(-rc));

    /* Polling sleep. */
    dds_sleepfor (DDS_MSECS (20));
  }

  /* Create a message to write. */
  msg.userID = 1;
  msg.message = "Hello World";

  printf ("=== [Publisher]  Writing : ");
  printf ("Message (%"PRId32", %s)\n", msg.userID, msg.message);
  fflush (stdout);

  rc = dds_write (writer, &msg);
  if (rc != DDS_RETCODE_OK)
    DDS_FATAL("dds_write: %s\n", dds_strretcode(-rc));

  /* Deleting the participant will delete all its children recursively as well. */
  rc = dds_delete (participant);
  if (rc != DDS_RETCODE_OK)
    DDS_FATAL("dds_delete: %s\n", dds_strretcode(-rc));
}

#define MAX_SAMPLES 1

void helloworld_subscriber()
{
  dds_entity_t participant;
  dds_entity_t topic;
  dds_entity_t reader;
  HelloWorldData_Msg *msg;
  void *samples[MAX_SAMPLES];
  dds_sample_info_t infos[MAX_SAMPLES];
  dds_return_t rc;
  dds_qos_t *qos;

  /* Create a Participant. */
  participant = dds_create_participant (1, NULL, NULL);
  if (participant < 0)
    DDS_FATAL("dds_create_participant: %s\n", dds_strretcode(-participant));

  /* Create a Topic. */
  topic = dds_create_topic (
    participant, &HelloWorldData_Msg_desc, "HelloWorldData_Msg", NULL, NULL);
  if (topic < 0)
    DDS_FATAL("dds_create_topic: %s\n", dds_strretcode(-topic));

  /* Create a reliable Reader. */
  qos = dds_create_qos ();
  dds_qset_reliability (qos, DDS_RELIABILITY_RELIABLE, DDS_SECS (10));

  reader = dds_create_reader (participant, topic, qos, NULL);
  if (reader < 0)
    DDS_FATAL("dds_create_reader: %s\n", dds_strretcode(-reader));
  dds_delete_qos(qos);

  printf ("\n=== [Subscriber] Waiting for a sample ...\n");
  fflush (stdout);

  /* Initialize sample buffer, by pointing the void pointer within
   * the buffer array to a valid sample memory location. */
  samples[0] = HelloWorldData_Msg__alloc ();

  /* Poll until data has been read. */
  while (true)
  {
    /* Do the actual read.
     * The return value contains the number of read samples. */
    rc = dds_read (reader, samples, infos, MAX_SAMPLES, MAX_SAMPLES);
    if (rc < 0)
      DDS_FATAL("dds_read: %s\n", dds_strretcode(-rc));

    /* Check if we read some data and it is valid. */
    if ((rc > 0) && (infos[0].valid_data))
    {
      /* Print Message. */
      msg = (HelloWorldData_Msg*) samples[0];
      printf ("=== [Subscriber] Received : ");
      printf ("Message (%"PRId32", %s)\n", msg->userID, msg->message);
      fflush (stdout);
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
  rc = dds_delete (participant);
  if (rc != DDS_RETCODE_OK)
    DDS_FATAL("dds_delete (participant): %s\n", dds_strretcode(-rc));
}

int main(void)
{
    printf("CycloneDDS Hello World! %s\n", CONFIG_BOARD);
    set_cyclonedds_uri();
#if BUILD_HELLOWORLD_PUB
    helloworld_publisher();
#else
    helloworld_subscriber();
#endif
    printf("Done\n");
    return 0;
}
