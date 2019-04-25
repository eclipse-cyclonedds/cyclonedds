#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "mpt/mpt.h"

#include "dds/dds.h"
#include "helloworlddata.h"

#include "dds/ddsrt/time.h"
#include "dds/ddsrt/strtol.h"
#include "dds/ddsrt/process.h"
#include "dds/ddsrt/environ.h"
#include "dds/ddsrt/cdtors.h"
#include "dds/ddsrt/sync.h"


/* An array of one message (aka sample in dds terms) will be used. */
#define MAX_SAMPLES 1

static int g_publication_matched_count = 0;
static ddsrt_mutex_t g_mutex;
static ddsrt_cond_t  g_cond;

static void
publication_matched_cb(
        dds_entity_t writer,
        const dds_publication_matched_status_t status,
        void* arg)
{
  (void)arg;
  (void)writer;
  ddsrt_mutex_lock(&g_mutex);
  g_publication_matched_count = (int)status.current_count;
  ddsrt_cond_broadcast(&g_cond);
  ddsrt_mutex_unlock(&g_mutex);
}

static void
data_available_cb(
        dds_entity_t reader,
        void* arg)
{
  (void)arg;
  (void)reader;
  ddsrt_mutex_lock(&g_mutex);
  ddsrt_cond_broadcast(&g_cond);
  ddsrt_mutex_unlock(&g_mutex);
}

void
hello_init(void)
{
  ddsrt_init();
  ddsrt_mutex_init(&g_mutex);
  ddsrt_cond_init(&g_cond);
}

void
hello_fini(void)
{
  ddsrt_cond_destroy(&g_cond);
  ddsrt_mutex_destroy(&g_mutex);
  ddsrt_fini();
}


/*
 * The HelloWorld publisher.
 * It waits for a publication matched, and then writes a sample.
 * It quits when the publication matched has been reset again.
 */
MPT_ProcessEntry(hello_publisher,
                 MPT_Args(dds_domainid_t domainid,
                          const char *topic_name,
                          int sub_cnt,
                          const char *text))
{
  HelloWorldData_Msg msg;
  dds_listener_t *listener;
  dds_entity_t participant;
  dds_entity_t topic;
  dds_entity_t writer;
  dds_return_t rc;
  dds_qos_t *qos;
  int id = (int)ddsrt_getpid();

  assert(topic_name);
  assert(text);

  printf("=== [Publisher(%d)] Start(%d) ...\n", id, domainid);

  /*
   * A reliable volatile sample, written after publication matched, can still
   * be lost when the subscriber wasn't able to match its subscription yet.
   * Use transient_local reliable to make sure the sample is received.
   */
  qos = dds_create_qos();
  dds_qset_durability(qos, DDS_DURABILITY_TRANSIENT_LOCAL);
  dds_qset_reliability(qos, DDS_RELIABILITY_RELIABLE, DDS_SECS(10));

  /* Use listener to get number of publications matched. */
  listener = dds_create_listener(NULL);
  MPT_ASSERT_FATAL_NOT_NULL(listener, "Could not create listener");
  dds_lset_publication_matched(listener, publication_matched_cb);

  /* Create a Writer. */
  participant = dds_create_participant (domainid, NULL, NULL);
  MPT_ASSERT_FATAL_GT(participant, 0, "Could not create participant: %s\n", dds_strretcode(-participant));

  topic = dds_create_topic (
            participant, &HelloWorldData_Msg_desc, topic_name, qos, NULL);
  MPT_ASSERT_FATAL_GT(topic, 0, "Could not create topic: %s\n", dds_strretcode(-topic));

  writer = dds_create_writer (participant, topic, qos, listener);
  MPT_ASSERT_FATAL_GT(writer, 0, "Could not create writer: %s\n", dds_strretcode(-writer));

  /* Wait for expected nr of subscriber(s). */
  ddsrt_mutex_lock(&g_mutex);
  while (g_publication_matched_count != sub_cnt) {
    ddsrt_cond_waitfor(&g_cond, &g_mutex, DDS_INFINITY);
  }
  ddsrt_mutex_unlock(&g_mutex);

  /* Write sample. */
  msg.userID = (int32_t)id;
  msg.message = (char*)text;
  printf("=== [Publisher(%d)] Send: { %d, %s }\n", id, msg.userID, msg.message);
  rc = dds_write (writer, &msg);
  MPT_ASSERT_EQ(rc, DDS_RETCODE_OK, "Could not write sample\n");

  /* Wait for subscriber(s) to have finished. */
  ddsrt_mutex_lock(&g_mutex);
  while (g_publication_matched_count != 0) {
    ddsrt_cond_waitfor(&g_cond, &g_mutex, DDS_INFINITY);
  }
  ddsrt_mutex_unlock(&g_mutex);

  rc = dds_delete (participant);
  MPT_ASSERT_EQ(rc, DDS_RETCODE_OK, "Teardown failed\n");

  dds_delete_listener(listener);
  dds_delete_qos(qos);

  printf("=== [Publisher(%d)] Done\n", id);
}


/*
 * The HelloWorld subscriber.
 * It waits for sample(s) and checks the content.
 */
MPT_ProcessEntry(hello_subscriber,
                 MPT_Args(dds_domainid_t domainid,
                          const char *topic_name,
                          int sample_cnt,
                          const char *text))
{
  HelloWorldData_Msg *msg;
  void *samples[MAX_SAMPLES];
  dds_sample_info_t infos[MAX_SAMPLES];
  dds_listener_t *listener;
  dds_entity_t participant;
  dds_entity_t topic;
  dds_entity_t reader;
  dds_return_t rc;
  dds_qos_t *qos;
  int recv_cnt;
  int id = (int)ddsrt_getpid();

  assert(topic_name);
  assert(text);

  printf("--- [Subscriber(%d)] Start(%d) ...\n", id, domainid);

  /*
   * A reliable volatile sample, written after publication matched, can still
   * be lost when the subscriber wasn't able to match its subscription yet.
   * Use transient_local reliable to make sure the sample is received.
   */
  qos = dds_create_qos();
  dds_qset_durability(qos, DDS_DURABILITY_TRANSIENT_LOCAL);
  dds_qset_reliability(qos, DDS_RELIABILITY_RELIABLE, DDS_SECS(10));

  /* Use listener to get data available trigger. */
  listener = dds_create_listener(NULL);
  MPT_ASSERT_FATAL_NOT_NULL(listener, "Could not create listener");
  dds_lset_data_available(listener, data_available_cb);

  /* Create a Reader. */
  participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  MPT_ASSERT_FATAL_GT(participant, 0, "Could not create participant: %s\n", dds_strretcode(-participant));
  topic = dds_create_topic (
            participant, &HelloWorldData_Msg_desc, topic_name, qos, NULL);
  MPT_ASSERT_FATAL_GT(topic, 0, "Could not create topic: %s\n", dds_strretcode(-topic));
  reader = dds_create_reader (participant, topic, qos, listener);
  MPT_ASSERT_FATAL_GT(reader, 0, "Could not create reader: %s\n", dds_strretcode(-reader));

  printf("--- [Subscriber(%d)] Waiting for %d sample(s) ...\n", id, sample_cnt);

  /* Initialize sample buffer, by pointing the void pointer within
   * the buffer array to a valid sample memory location. */
  samples[0] = HelloWorldData_Msg__alloc ();

  /* Wait until expected nr of samples have been taken. */
  ddsrt_mutex_lock(&g_mutex);
  recv_cnt = 0;
  while (recv_cnt < sample_cnt) {
    /* Use a take with mask to work around the #146 issue. */
    rc = dds_take_mask(reader, samples, infos, MAX_SAMPLES, MAX_SAMPLES, DDS_NEW_VIEW_STATE);
    MPT_ASSERT_GEQ(rc, 0, "Could not read: %s\n", dds_strretcode(-rc));

    /* Check if we read some data and it is valid. */
    if ((rc > 0) && (infos[0].valid_data)) {
      /* Print Message. */
      msg = (HelloWorldData_Msg*)samples[0];
      printf("--- [Subscriber(%d)] Received: { %d, %s }\n", id,
                                    msg->userID, msg->message);
      MPT_ASSERT_STR_EQ(msg->message, text,
                        "Messages do not match: \"%s\" vs \"%s\"\n",
                        msg->message, text);
      recv_cnt++;
    } else {
      ddsrt_cond_waitfor(&g_cond, &g_mutex, DDS_INFINITY);
    }
  }
  ddsrt_mutex_unlock(&g_mutex);

  /* Free the data location. */
  HelloWorldData_Msg_free (samples[0], DDS_FREE_ALL);

  rc = dds_delete (participant);
  MPT_ASSERT_EQ(rc, DDS_RETCODE_OK, "Teardown failed\n");

  dds_delete_listener(listener);
  dds_delete_qos(qos);

  printf("--- [Subscriber(%d)] Done\n", id);
}
