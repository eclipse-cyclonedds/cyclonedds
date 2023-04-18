#include "dds/dds.h"
#include "dds/ddsc/dds_loan_api.h"
#include "ShmThroughput.h"
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
static void start_writing(dds_entity_t writer, void *sample,
    int burstInterval, uint32_t burstSize, int timeOut);
static int parse_args(int argc, char **argv, int *burstInterval,
    uint32_t *burstSize, int *timeOut, char **partitionName);
static dds_entity_t prepare_dds(dds_entity_t *writer, const char *partitionName);
static void finalize_dds(dds_entity_t participant, dds_entity_t writer, void *sample);

static void sigint (int sig)
{
  (void)sig;
  done = true;
}

static uint32_t payloadSize = 8192;

int main (int argc, char **argv)
{
  int burstInterval = 0;
  uint32_t burstSize = 1;
  int timeOut = 0;
  char * partitionName = "Throughput example";
  dds_entity_t participant;
  dds_entity_t writer;
  dds_return_t rc;

  if (parse_args(argc, argv, &burstInterval, &burstSize, &timeOut, &partitionName) == EXIT_FAILURE) {
    return EXIT_FAILURE;
  }

  void* sample = NULL;
  switch (payloadSize)
  {
  case 16:
    sample = ThroughputModule_DataType_16__alloc();
    break;
  case 32:
    sample = ThroughputModule_DataType_32__alloc();
    break;
  case 64:
    sample = ThroughputModule_DataType_64__alloc();
    break;
  case 128:
    sample = ThroughputModule_DataType_128__alloc();
    break;
  case 256:
    sample = ThroughputModule_DataType_256__alloc();
    break;
  case 512:
    sample = ThroughputModule_DataType_512__alloc();
    break;
  case 1024:
    sample = ThroughputModule_DataType_1024__alloc();
    break;
  case 2048:
    sample = ThroughputModule_DataType_2048__alloc();
    break;
  case 4096:
    sample = ThroughputModule_DataType_4096__alloc();
    break;
  case 8192:
    sample = ThroughputModule_DataType_8192__alloc();
    break;
  case 16384:
    sample = ThroughputModule_DataType_16384__alloc();
    break;
  case 32768:
    sample = ThroughputModule_DataType_32768__alloc();
    break;
  case 65536:
    sample = ThroughputModule_DataType_65536__alloc();
    break;
  case 131072:
    sample = ThroughputModule_DataType_131072__alloc();
    break;
  case 262144:
    sample = ThroughputModule_DataType_262144__alloc();
    break;
  case 524288:
    sample = ThroughputModule_DataType_524288__alloc();
    break;
  case 1048576:
    sample = ThroughputModule_DataType_1048576__alloc();
    break;
  default:
    assert(0);
  }

  /* Fill the sample payload with data */
  ThroughputModule_DataType_Base* ptr = (ThroughputModule_DataType_Base*)sample;
  ptr->payloadsize = payloadSize - (uint32_t) sizeof(ThroughputModule_DataType_Base);
  ptr->count = 0;
  memset((char*)sample + offsetof(ThroughputModule_DataType_16, payload), 'a', ptr->payloadsize);

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

  /* Register handler for Ctrl-C */
  signal (SIGINT, sigint);

  /* Register the sample instance and write samples repeatedly or until time out */
  start_writing(writer, sample, burstInterval, burstSize, timeOut);

  /* Cleanup */
  finalize_dds(participant, writer, sample);
  return EXIT_SUCCESS;
}

static int parse_args(
    int argc,
    char **argv,
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
    payloadSize = (uint32_t) atoi (argv[1]); /* The size of the payload in bytes */
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

  printf ("payloadSize: %"PRIu32" bytes burstInterval: %u ms burstSize: %"PRId32" timeOut: %u seconds partitionName: %s\n",
    payloadSize, *burstInterval, *burstSize, *timeOut, *partitionName);
  fflush (stdout);

  return result;
}

static dds_entity_t prepare_dds(dds_entity_t *writer, const char *partitionName)
{
  dds_entity_t participant;
  dds_entity_t topic = DDS_RETCODE_BAD_PARAMETER;
  dds_entity_t publisher;
  const char *pubParts[1];
  dds_qos_t *pubQos;
  dds_qos_t *dwQos;

  /* A domain participant is created for the default domain. */
  participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  if (participant < 0)
    DDS_FATAL("dds_create_participant: %s\n", dds_strretcode(-participant));

  /* A topic is created for our sample type on the domain participant. */
  switch (payloadSize)
  {
  case 16:
    topic = dds_create_topic(participant, &ThroughputModule_DataType_16_desc, "Throughput", NULL, NULL);
    break;
  case 32:
    topic = dds_create_topic(participant, &ThroughputModule_DataType_32_desc, "Throughput", NULL, NULL);
    break;
  case 64:
    topic = dds_create_topic(participant, &ThroughputModule_DataType_64_desc, "Throughput", NULL, NULL);
    break;
  case 128:
    topic = dds_create_topic(participant, &ThroughputModule_DataType_128_desc, "Throughput", NULL, NULL);
    break;
  case 256:
    topic = dds_create_topic(participant, &ThroughputModule_DataType_256_desc, "Throughput", NULL, NULL);
    break;
  case 512:
    topic = dds_create_topic(participant, &ThroughputModule_DataType_512_desc, "Throughput", NULL, NULL);
    break;
  case 1024:
    topic = dds_create_topic(participant, &ThroughputModule_DataType_1024_desc, "Throughput", NULL, NULL);
    break;
  case 2048:
    topic = dds_create_topic(participant, &ThroughputModule_DataType_2048_desc, "Throughput", NULL, NULL);
    break;
  case 4096:
    topic = dds_create_topic(participant, &ThroughputModule_DataType_4096_desc, "Throughput", NULL, NULL);
    break;
  case 8192:
    topic = dds_create_topic(participant, &ThroughputModule_DataType_8192_desc, "Throughput", NULL, NULL);
    break;
  case 16384:
    topic = dds_create_topic(participant, &ThroughputModule_DataType_16384_desc, "Throughput", NULL, NULL);
    break;
  case 32768:
    topic = dds_create_topic(participant, &ThroughputModule_DataType_32768_desc, "Throughput", NULL, NULL);
    break;
  case 65536:
    topic = dds_create_topic(participant, &ThroughputModule_DataType_65536_desc, "Throughput", NULL, NULL);
    break;
  case 131072:
    topic = dds_create_topic(participant, &ThroughputModule_DataType_131072_desc, "Throughput", NULL, NULL);
    break;
  case 262144:
    topic = dds_create_topic(participant, &ThroughputModule_DataType_262144_desc, "Throughput", NULL, NULL);
    break;
  case 524288:
    topic = dds_create_topic(participant, &ThroughputModule_DataType_524288_desc, "Throughput", NULL, NULL);
    break;
  case 1048576:
    topic = dds_create_topic(participant, &ThroughputModule_DataType_1048576_desc, "Throughput", NULL, NULL);
    break;
  default:
    assert(0);
  }
  if (topic < 0)
    DDS_FATAL("dds_create_topic: %s\n", dds_strretcode(-topic));

  /* A publisher is created on the domain participant. */
  pubQos = dds_create_qos ();
  pubParts[0] = partitionName;
  dds_qset_partition (pubQos, 1, pubParts);
  publisher = dds_create_publisher (participant, pubQos, NULL);
  if (publisher < 0)
    DDS_FATAL("dds_create_publisher: %s\n", dds_strretcode(-publisher));
  dds_delete_qos (pubQos);

  /* A DataWriter is created on the publisher. */
  dwQos = dds_create_qos ();
  dds_qset_reliability (dwQos, DDS_RELIABILITY_RELIABLE, DDS_SECS (10));
  dds_qset_history (dwQos, DDS_HISTORY_KEEP_LAST, 16);
  dds_qset_deadline(dwQos, DDS_INFINITY);
  dds_qset_durability(dwQos, DDS_DURABILITY_VOLATILE);
  dds_qset_liveliness(dwQos, DDS_LIVELINESS_AUTOMATIC, DDS_SECS(1));
  dds_qset_resource_limits (dwQos, MAX_SAMPLES, DDS_LENGTH_UNLIMITED, DDS_LENGTH_UNLIMITED);
  dds_qset_writer_batching(dwQos, true);
  *writer = dds_create_writer (publisher, topic, dwQos, NULL);
  if (*writer < 0)
    DDS_FATAL("dds_create_writer: %s\n", dds_strretcode(-*writer));
  dds_delete_qos (dwQos);

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
    void *sample,
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
        void *loaned_sample;

        if ((status = dds_loan_sample(writer, &loaned_sample)) < 0)
          DDS_FATAL("dds_loan_sample: %s\n", dds_strretcode(-status));
        memcpy(loaned_sample, sample, payloadSize);
        status = dds_write (writer, loaned_sample);
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
          ((ThroughputModule_DataType_Base*)sample)->count++;
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

    printf ("=== [Publisher]  %s, %llu samples written.\n", done ? "Terminated" : "Timed out", (unsigned long long) ((ThroughputModule_DataType_Base*)sample)->count);
    fflush (stdout);
  }
}

static void finalize_dds(dds_entity_t participant, dds_entity_t writer, void *sample)
{
  dds_return_t status = dds_dispose(writer, sample);
  if (status != DDS_RETCODE_TIMEOUT && status < 0)
    DDS_FATAL("dds_dispose: %s\n", dds_strretcode(-status));

  switch (payloadSize)
  {
  case 16:
    ThroughputModule_DataType_16_free(sample, DDS_FREE_ALL);
    break;
  case 32:
    ThroughputModule_DataType_32_free(sample, DDS_FREE_ALL);
    break;
  case 64:
    ThroughputModule_DataType_64_free(sample, DDS_FREE_ALL);
    break;
  case 128:
    ThroughputModule_DataType_128_free(sample, DDS_FREE_ALL);
    break;
  case 256:
    ThroughputModule_DataType_256_free(sample, DDS_FREE_ALL);
    break;
  case 512:
    ThroughputModule_DataType_512_free(sample, DDS_FREE_ALL);
    break;
  case 1024:
    ThroughputModule_DataType_1024_free(sample, DDS_FREE_ALL);
    break;
  case 2048:
    ThroughputModule_DataType_2048_free(sample, DDS_FREE_ALL);
    break;
  case 4096:
    ThroughputModule_DataType_4096_free(sample, DDS_FREE_ALL);
    break;
  case 8192:
    ThroughputModule_DataType_8192_free(sample, DDS_FREE_ALL);
    break;
  case 16384:
    ThroughputModule_DataType_16384_free(sample, DDS_FREE_ALL);
    break;
  case 32768:
    ThroughputModule_DataType_32768_free(sample, DDS_FREE_ALL);
    break;
  case 65536:
    ThroughputModule_DataType_65536_free(sample, DDS_FREE_ALL);
    break;
  case 131072:
    ThroughputModule_DataType_131072_free(sample, DDS_FREE_ALL);
    break;
  case 262144:
    ThroughputModule_DataType_262144_free(sample, DDS_FREE_ALL);
    break;
  case 524288:
    ThroughputModule_DataType_524288_free(sample, DDS_FREE_ALL);
    break;
  case 1048576:
    ThroughputModule_DataType_1048576_free(sample, DDS_FREE_ALL);
    break;
  }

  //dds_free (sample.payload._buffer);
  status = dds_delete (participant);
  if (status < 0)
    DDS_FATAL("dds_delete: %s\n", dds_strretcode(-status));
}
