#include "dds/dds.h"
#include "Throughput.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

/*
 * The Throughput example measures data throughput in bytes per second. The publisher
 * allows you to specify a payload size in bytes as well as allowing you to specify
 * whether to send data in bursts. The publisher will continue to send data forever
 * unless a time out is specified. The subscriber will receive data and output the
 * total amount received and the data rate in bytes per second. It will also indicate
 * if any samples were received out of order. A maximum number of cycles can be
 * specified and once this has been reached the subscriber will terminate and output
 * totals and averages.
 */

#define MAX_SAMPLES 100

static bool done = false;

/* Forward declarations */
static dds_return_t wait_for_reader(dds_entity_t writer, dds_entity_t participant);
static void start_writing(dds_entity_t writer, ThroughputModule_DataType *sample,
    int burstInterval, uint32_t burstSize, int timeOut);
static int parse_args(int argc, char **argv, uint32_t *payloadSize, int *burstInterval,
    uint32_t *burstSize, int *timeOut, char **partitionName);
static dds_entity_t prepare_dds(dds_entity_t *writer, const char *partitionName);
static void finalize_dds(dds_entity_t participant, dds_entity_t writer, ThroughputModule_DataType sample);

#if !DDSRT_WITH_FREERTOS && !__ZEPHYR__
static void sigint (int sig)
{
  (void)sig;
  done = true;
}
#endif

int main (int argc, char **argv)
{
  uint32_t payloadSize = 8192;
  int burstInterval = 0;
  uint32_t burstSize = 1;
  int timeOut = 0;
  char * partitionName = "Throughput example";
  dds_entity_t participant;
  dds_entity_t writer;
  dds_return_t rc;
  ThroughputModule_DataType sample;

  if (parse_args(argc, argv, &payloadSize, &burstInterval, &burstSize, &timeOut, &partitionName) == EXIT_FAILURE) {
    return EXIT_FAILURE;
  }

  participant = prepare_dds(&writer, partitionName);

  /* Wait until have a reader */
  if (wait_for_reader(writer, participant) == 0) {
    printf ("=== [Publisher]  Did not discover a reader.\n");
    fflush (stdout);
    rc = dds_delete (participant);
    if (rc < 0)
      DDS_FATAL("dds_delete: %s\n", dds_strretcode(-rc));
    return EXIT_FAILURE;
  }

  /* Fill the sample payload with data */
  sample.count = 0;
  sample.payload._buffer = dds_alloc (payloadSize);
  sample.payload._length = payloadSize;
  sample.payload._release = true;
  for (uint32_t i = 0; i < payloadSize; i++) {
    sample.payload._buffer[i] = 'a';
  }

  /* Register handler for Ctrl-C */
#if !DDSRT_WITH_FREERTOS && !__ZEPHYR__
  signal (SIGINT, sigint);
#endif

  /* Register the sample instance and write samples repeatedly or until time out */
  start_writing(writer, &sample, burstInterval, burstSize, timeOut);

  /* Cleanup */
  finalize_dds(participant, writer, sample);
  return EXIT_SUCCESS;
}

static int parse_args(
    int argc,
    char **argv,
    uint32_t *payloadSize,
    int *burstInterval,
    uint32_t *burstSize,
    int *timeOut,
    char **partitionName)
{
  int result = EXIT_SUCCESS;
  /*
   * Get the program parameters
   * Parameters: publisher [payloadSize] [burstInterval] [burstSize] [timeOut] [partitionName]
   */
  if (argc == 2 && (strcmp (argv[1], "-h") == 0 || strcmp (argv[1], "--help") == 0))
  {
    printf ("Usage (parameters must be supplied in order):\n");
    printf ("./publisher [payloadSize (bytes)] [burstInterval (ms)] [burstSize (samples)] [timeOut (seconds)] [partitionName]\n");
    printf ("Defaults:\n");
    printf ("./publisher 8192 0 1 0 \"Throughput example\"\n");
    return EXIT_FAILURE;
  }
  if (argc > 1)
  {
    *payloadSize = (uint32_t) atoi (argv[1]); /* The size of the payload in bytes */
  }
  if (argc > 2)
  {
    *burstInterval = atoi (argv[2]); /* The time interval between each burst in ms */
  }
  if (argc > 3)
  {
    *burstSize = (uint32_t) atoi (argv[3]); /* The number of samples to send each burst */
  }
  if (argc > 4)
  {
    *timeOut = atoi (argv[4]); /* The number of seconds the publisher should run for (0 = infinite) */
  }
  if (argc > 5)
  {
    *partitionName = argv[5]; /* The name of the partition */
  }

  printf ("payloadSize: %"PRIu32" bytes burstInterval: %d ms burstSize: %"PRIu32" timeOut: %d seconds partitionName: %s\n",
    *payloadSize, *burstInterval, *burstSize, *timeOut, *partitionName);
  fflush (stdout);

  return result;
}

static dds_entity_t prepare_dds(dds_entity_t *writer, const char *partitionName)
{
  dds_entity_t participant;
  dds_entity_t topic;
  dds_entity_t publisher;
  const char *pubParts[1];
  dds_qos_t *pubQos;
  dds_qos_t *wrQos;
  dds_qos_t *tQos;

  /* A domain participant is created for the default domain. */
  participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  if (participant < 0)
    DDS_FATAL("dds_create_participant: %s\n", dds_strretcode(-participant));

  /* A topic is created for our sample type on the domain participant. */
  tQos = dds_create_qos ();
  dds_qset_reliability (tQos, DDS_RELIABILITY_RELIABLE, DDS_SECS (10));
  dds_qset_history (tQos, DDS_HISTORY_KEEP_ALL, 0);
  dds_qset_resource_limits (tQos, MAX_SAMPLES, DDS_LENGTH_UNLIMITED, DDS_LENGTH_UNLIMITED);
  topic = dds_create_topic (participant, &ThroughputModule_DataType_desc, "Throughput", tQos, NULL);
  if (topic < 0)
    DDS_FATAL("dds_create_topic: %s\n", dds_strretcode(-topic));
  dds_delete_qos (tQos);

  /* A publisher is created on the domain participant. */
  pubQos = dds_create_qos ();
  pubParts[0] = partitionName;
  dds_qset_partition (pubQos, 1, pubParts);
  publisher = dds_create_publisher (participant, pubQos, NULL);
  if (publisher < 0)
    DDS_FATAL("dds_create_publisher: %s\n", dds_strretcode(-publisher));
  dds_delete_qos (pubQos);

  /* A DataWriter is created on the publisher. */
  wrQos = dds_create_qos ();
  dds_qset_writer_batching (wrQos, true);
  *writer = dds_create_writer (publisher, topic, wrQos, NULL);
  if (*writer < 0)
    DDS_FATAL("dds_create_writer: %s\n", dds_strretcode(-*writer));
  dds_delete_qos (wrQos);

  return participant;
}

static dds_return_t wait_for_reader(dds_entity_t writer, dds_entity_t participant)
{
  printf ("\n=== [Publisher]  Waiting for a reader ...\n");
  fflush (stdout);

  dds_return_t rc;
  dds_entity_t waitset;

  rc = dds_set_status_mask(writer, DDS_PUBLICATION_MATCHED_STATUS);
  if (rc < 0)
    DDS_FATAL("dds_set_status_mask: %s\n", dds_strretcode(-rc));

  waitset = dds_create_waitset(participant);
  if (waitset < 0)
    DDS_FATAL("dds_create_waitset: %s\n", dds_strretcode(-waitset));

  rc = dds_waitset_attach(waitset, writer, (dds_attach_t)NULL);
  if (rc < 0)
    DDS_FATAL("dds_waitset_attach: %s\n", dds_strretcode(-rc));

  rc = dds_waitset_wait(waitset, NULL, 0, DDS_SECS(30));
  if (rc < 0)
    DDS_FATAL("dds_waitset_wait: %s\n", dds_strretcode(-rc));

  return rc;
}

static void start_writing(
    dds_entity_t writer,
    ThroughputModule_DataType *sample,
    int burstInterval,
    uint32_t burstSize,
    int timeOut)
{
  bool timedOut = false;
  dds_time_t pubStart = dds_time ();
  dds_time_t now;
  dds_time_t deltaTv;
  dds_return_t status;

  if (!done)
  {
    dds_time_t burstStart = pubStart;
    unsigned int burstCount = 0;

    printf ("=== [Publisher]  Writing samples...\n");
    fflush (stdout);

    while (!done && !timedOut)
    {
      /* Write data until burst size has been reached */

      if (burstCount < burstSize)
      {
        status = dds_write (writer, sample);
        if (status == DDS_RETCODE_TIMEOUT)
        {
          timedOut = true;
        }
        else if (status < 0)
        {
          DDS_FATAL("dds_write: %s\n", dds_strretcode(-status));
        }
        else
        {
          sample->count++;
          burstCount++;
        }
      }
      else if (burstInterval)
      {
        /* Sleep until burst interval has passed */

        dds_time_t time = dds_time ();
        deltaTv = time - burstStart;
        if (deltaTv < DDS_MSECS (burstInterval))
        {
          dds_write_flush (writer);
          dds_sleepfor (DDS_MSECS (burstInterval) - deltaTv);
        }
        burstStart = dds_time ();
        burstCount = 0;
      }
      else
      {
        burstCount = 0;
      }

      if (timeOut)
      {
        now = dds_time ();
        deltaTv = now - pubStart;
        if ((deltaTv) > DDS_SECS (timeOut))
        {
          timedOut = true;
        }
      }
    }
    dds_write_flush (writer);

    printf ("=== [Publisher]  %s, %llu samples written.\n", done ? "Terminated" : "Timed out", (unsigned long long) sample->count);
    fflush (stdout);
  }
}

static void finalize_dds(dds_entity_t participant, dds_entity_t writer, ThroughputModule_DataType sample)
{
  dds_return_t status = dds_dispose (writer, &sample);
  if (status != DDS_RETCODE_TIMEOUT && status < 0)
    DDS_FATAL("dds_dispose: %s\n", dds_strretcode(-status));

  dds_free (sample.payload._buffer);
  status = dds_delete (participant);
  if (status < 0)
    DDS_FATAL("dds_delete: %s\n", dds_strretcode(-status));
}
