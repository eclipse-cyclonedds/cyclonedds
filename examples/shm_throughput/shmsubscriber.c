#include "dds/dds.h"
#include "ShmThroughput.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <assert.h>
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

#define BYTES_PER_SEC_TO_MEGABITS_PER_SEC 125000
#define MAX_SAMPLES 1000

typedef struct HandleEntry
{
  dds_instance_handle_t handle;
  unsigned long long count;
  struct HandleEntry * next;
} HandleEntry;

typedef struct HandleMap
{
  HandleEntry *entries;
} HandleMap;

static long pollingDelay = -1; /* i.e. use a listener */

static HandleMap * imap;
static unsigned long long outOfOrder = 0;
static unsigned long long total_bytes = 0;
static unsigned long long total_samples = 0;

static dds_time_t startTime = 0;

static int payloadSize = 8192;

static void * data [MAX_SAMPLES];
static void * samples[MAX_SAMPLES];

static dds_entity_t waitSet;

static volatile sig_atomic_t done = false;

/* Forward declarations */
static HandleMap * HandleMap__alloc (void);
static void HandleMap__free (HandleMap *map);
static HandleEntry * store_handle (HandleMap *map, dds_instance_handle_t key);
static HandleEntry * retrieve_handle (HandleMap *map, dds_instance_handle_t key);

static void data_available_handler (dds_entity_t reader, void *arg);
static int parse_args(int argc, char **argv, unsigned long long *maxCycles, char **partitionName);
static void process_samples(dds_entity_t reader, unsigned long long maxCycles);
static dds_entity_t prepare_dds(dds_entity_t *reader, const char *partitionName);
static void finalize_dds(dds_entity_t participant);

static void sigint (int sig)
{
  (void) sig;
  done = true;
}

int main (int argc, char **argv)
{
  unsigned long long maxCycles = 0;
  char *partitionName = "Throughput example";

  dds_entity_t participant;
  dds_entity_t reader;

  if (parse_args(argc, argv, &maxCycles, &partitionName) == EXIT_FAILURE)
  {
    return EXIT_FAILURE;
  }

  printf ("Cycles: %llu | PollingDelay: %ld | Partition: %s\n", maxCycles, pollingDelay, partitionName);
  fflush (stdout);

  participant = prepare_dds(&reader, partitionName);

  printf ("=== [Subscriber] Waiting for samples...\n");
  fflush (stdout);

  /* Process samples until Ctrl-C is pressed or until maxCycles */
  /* has been reached (0 = infinite) */
  signal (SIGINT, sigint);
  process_samples(reader, maxCycles);

  (void) dds_set_status_mask (reader, 0);
  HandleMap__free (imap);
  finalize_dds (participant);
  return EXIT_SUCCESS;
}

/*
 * This struct contains all of the entities used in the publisher and subscriber.
 */
static HandleMap * HandleMap__alloc (void)
{
  HandleMap * map = malloc (sizeof (*map));
  assert(map);
  memset (map, 0, sizeof (*map));
  return map;
}

static void HandleMap__free (HandleMap *map)
{
  HandleEntry * entry;

  while (map->entries)
  {
    entry = map->entries;
    map->entries = entry->next;
    free (entry);
  }
  free (map);
}

static HandleEntry * store_handle (HandleMap *map, dds_instance_handle_t key)
{
  HandleEntry * entry = malloc (sizeof (*entry));
  assert(entry);
  memset (entry, 0, sizeof (*entry));

  entry->handle = key;
  entry->next = map->entries;
  map->entries = entry;

  return entry;
}

static HandleEntry * retrieve_handle (HandleMap *map, dds_instance_handle_t key)
{
  HandleEntry * entry = map->entries;

  while (entry)
  {
    if (entry->handle == key)
    {
      break;
    }
    entry = entry->next;
  }
  return entry;
}

static int do_take (dds_entity_t reader)
{
  int samples_received;
  dds_sample_info_t *info = NULL;
  dds_instance_handle_t ph = 0;
  HandleEntry * current = NULL;

  if (startTime == 0)
  {
    startTime = dds_time ();
  }

  /* Take samples and iterate through them */

  info = dds_alloc (sizeof(dds_sample_info_t) * MAX_SAMPLES);

  samples_received = dds_take (reader, samples, info, MAX_SAMPLES, MAX_SAMPLES);
  if (samples_received < 0)
  {
    dds_free( info );
    DDS_FATAL("dds_take: %s\n", dds_strretcode(-samples_received));
  }

  for (int i = 0; !done && i < samples_received; i++)
  {
    if (info[i].valid_data)
    {
      ph = info[i].publication_handle;
      current = retrieve_handle (imap, ph);
      ThroughputModule_DataType_Base * this_sample = (ThroughputModule_DataType_Base*)data[i];

      if (current == NULL)
      {
        current = store_handle (imap, ph);
        current->count = this_sample->count;
      }
      else if (this_sample->count <= current->count)
      {
        outOfOrder++;
      }

      current->count = this_sample->count;

      /* Add the sample payload size to the total received */

      total_bytes += this_sample->payloadsize + sizeof(ThroughputModule_DataType_Base);
      total_samples++;
    }
  }
  dds_free (info);
  return samples_received;
}

static void data_available_handler (dds_entity_t reader, void *arg)
{
  (void)arg;
  (void) do_take (reader);
}

static int parse_args(int argc, char **argv, unsigned long long *maxCycles, char **partitionName)
{
  /*
   * Get the program parameters
   * Parameters: subscriber [maxCycles] [pollingDelay] [partitionName]
   */
  if (argc == 2 && (strcmp (argv[1], "-h") == 0 || strcmp (argv[1], "--help") == 0))
  {
    printf ("Usage (parameters must be supplied in order):\n");
    printf ("./subscriber [maxCycles (0 = infinite)] [pollingDelay (ms, 0 = waitset, -1 = listener)] [partitionName]\n");
    printf ("Defaults:\n");
    printf ("./subscriber 0 0 \"Throughput example\"\n");
    return EXIT_FAILURE;
  }

  if (argc > 1)
  {
    *maxCycles = (unsigned long long) atoi (argv[1]); /* The number of times to output statistics before terminating */
  }
  if (argc > 2)
  {
    pollingDelay = atoi (argv[2]); /* The number of ms to wait between reads (0 = waitset, -1 = listener) */
  }
  if (argc > 3)
  {
    *partitionName = argv[3]; /* The name of the partition */
  }
  if (argc > 4)
  {
    payloadSize = atoi(argv[4]);; /* The size of the payload in the sample*/
  }
  return EXIT_SUCCESS;
}

static void process_samples(dds_entity_t reader, unsigned long long maxCycles)
{
  dds_return_t status;
  unsigned long long prev_bytes = 0;
  unsigned long long prev_samples = 0;
  dds_attach_t wsresults[2];
  dds_time_t deltaTv;
  bool first_batch = true;
  unsigned long cycles = 0;
  double deltaTime = 0;
  dds_time_t prev_time = 0;
  dds_time_t time_now = 0;
  double maxsamples = 0;
  double maxspeed = 0;

  while (!done && (maxCycles == 0 || cycles < maxCycles))
  {
    if (pollingDelay > 0)
      dds_sleepfor (DDS_MSECS (pollingDelay));
    else
    {
      status = dds_waitset_wait (waitSet, wsresults, sizeof(wsresults)/sizeof(wsresults[0]), DDS_MSECS(100));
      if (status < 0)
        DDS_FATAL("dds_waitset_wait: %s\n", dds_strretcode(-status));
    }

    if (pollingDelay >= 0)
    {
      while (do_take (reader))
        ;
    }

    time_now = dds_time();
    if (!first_batch)
    {
      deltaTv = time_now - prev_time;
      deltaTime = (double) deltaTv / DDS_NSECS_IN_SEC;

      if (deltaTime >= 1.0 && total_samples != prev_samples)
      {
        double cursamples = (deltaTime != 0.0) ? ((double)(total_samples - prev_samples) / deltaTime) : 0;
        double curspeed = (deltaTime != 0.0) ? (((double)(total_bytes - prev_bytes) / BYTES_PER_SEC_TO_MEGABITS_PER_SEC) / deltaTime) : 0;
        printf ("=== [Subscriber] %5.3f Payload size: %d | Total received: %llu samples, %llu bytes | Out of order: %llu samples "
                "Transfer rate: %.2lf samples/s, %.2lf Mbit/s\n",
                deltaTime, payloadSize, total_samples, total_bytes, outOfOrder,
                cursamples, curspeed);
        fflush (stdout);
        if (curspeed > maxspeed)
          maxspeed = curspeed;
        if (cursamples > maxsamples)
          maxsamples = cursamples;
        cycles++;
        prev_time = time_now;
        prev_bytes = total_bytes;
        prev_samples = total_samples;
      }
    }
    else
    {
      prev_time = time_now;
      first_batch = false;
    }
  }

  /* Output totals and averages */
  deltaTv = time_now - startTime;
  deltaTime = (double) (deltaTv / DDS_NSECS_IN_SEC);
  printf ("\nTotal received: %llu samples, %llu bytes\n", total_samples, total_bytes);
  printf ("Out of order: %llu samples\n", outOfOrder);
  printf ("Average transfer rate: %.2lf samples/s, ", (double)total_samples / deltaTime);
  printf ("Maximum transfer rate: %.2lf samples/s, ", maxsamples);
  printf ("Average throughput : %.2lf Mbit/s\n", ((double)total_bytes / BYTES_PER_SEC_TO_MEGABITS_PER_SEC) / deltaTime);
  printf ("Maximum throughput : %.2lf Mbit/s\n", maxspeed);
  fflush (stdout);
}

static dds_entity_t prepare_dds(dds_entity_t *reader, const char *partitionName)
{
  dds_return_t status;
  dds_entity_t topic = DDS_RETCODE_BAD_PARAMETER;
  dds_entity_t subscriber;
  dds_listener_t *rd_listener;
  dds_entity_t participant;

  int32_t maxSamples = 4000;
  const char *subParts[1];
  dds_qos_t *subQos = dds_create_qos ();
  dds_qos_t *drQos = dds_create_qos ();

  /* A Participant is created for the default domain. */

  participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  if (participant < 0)
    DDS_FATAL("dds_create_participant: %s\n", dds_strretcode(-participant));

  /* A Topic is created for our sample type on the domain participant. */

  switch (payloadSize)
  {
  case 16:
    topic = dds_create_topic(participant, &ThroughputModule_DataType_16_desc, "Throughput", NULL, NULL);
    for (int i = 0; i < MAX_SAMPLES; i++)
      data[i] = ThroughputModule_DataType_16__alloc();
    break;
  case 32:
    topic = dds_create_topic(participant, &ThroughputModule_DataType_32_desc, "Throughput", NULL, NULL);
    for (int i = 0; i < MAX_SAMPLES; i++)
      data[i] = ThroughputModule_DataType_32__alloc();
    break;
  case 64:
    topic = dds_create_topic(participant, &ThroughputModule_DataType_64_desc, "Throughput", NULL, NULL);
    for (int i = 0; i < MAX_SAMPLES; i++)
      data[i] = ThroughputModule_DataType_64__alloc();
    break;
  case 128:
    topic = dds_create_topic(participant, &ThroughputModule_DataType_128_desc, "Throughput", NULL, NULL);
    for (int i = 0; i < MAX_SAMPLES; i++)
      data[i] = ThroughputModule_DataType_128__alloc();
    break;
  case 256:
    topic = dds_create_topic(participant, &ThroughputModule_DataType_256_desc, "Throughput", NULL, NULL);
    for (int i = 0; i < MAX_SAMPLES; i++)
      data[i] = ThroughputModule_DataType_256__alloc();
    break;
  case 512:
    topic = dds_create_topic(participant, &ThroughputModule_DataType_512_desc, "Throughput", NULL, NULL);
    for (int i = 0; i < MAX_SAMPLES; i++)
      data[i] = ThroughputModule_DataType_512__alloc();
    break;
  case 1024:
    topic = dds_create_topic(participant, &ThroughputModule_DataType_1024_desc, "Throughput", NULL, NULL);
    for (int i = 0; i < MAX_SAMPLES; i++)
      data[i] = ThroughputModule_DataType_1024__alloc();
    break;
  case 2048:
    topic = dds_create_topic(participant, &ThroughputModule_DataType_2048_desc, "Throughput", NULL, NULL);
    for (int i = 0; i < MAX_SAMPLES; i++)
      data[i] = ThroughputModule_DataType_2048__alloc();
    break;
  case 4096:
    topic = dds_create_topic(participant, &ThroughputModule_DataType_4096_desc, "Throughput", NULL, NULL);
    for (int i = 0; i < MAX_SAMPLES; i++)
      data[i] = ThroughputModule_DataType_4096__alloc();
    break;
  case 8192:
    topic = dds_create_topic(participant, &ThroughputModule_DataType_8192_desc, "Throughput", NULL, NULL);
    for (int i = 0; i < MAX_SAMPLES; i++)
      data[i] = ThroughputModule_DataType_8192__alloc();
    break;
  case 16384:
    topic = dds_create_topic(participant, &ThroughputModule_DataType_16384_desc, "Throughput", NULL, NULL);
    for (int i = 0; i < MAX_SAMPLES; i++)
      data[i] = ThroughputModule_DataType_16384__alloc();
    break;
  case 32768:
    topic = dds_create_topic(participant, &ThroughputModule_DataType_32768_desc, "Throughput", NULL, NULL);
    for (int i = 0; i < MAX_SAMPLES; i++)
      data[i] = ThroughputModule_DataType_32768__alloc();
    break;
  case 65536:
    topic = dds_create_topic(participant, &ThroughputModule_DataType_65536_desc, "Throughput", NULL, NULL);
    for (int i = 0; i < MAX_SAMPLES; i++)
      data[i] = ThroughputModule_DataType_65536__alloc();
    break;
  case 131072:
    topic = dds_create_topic(participant, &ThroughputModule_DataType_131072_desc, "Throughput", NULL, NULL);
    for (int i = 0; i < MAX_SAMPLES; i++)
      data[i] = ThroughputModule_DataType_131072__alloc();
    break;
  case 262144:
    topic = dds_create_topic(participant, &ThroughputModule_DataType_262144_desc, "Throughput", NULL, NULL);
    for (int i = 0; i < MAX_SAMPLES; i++)
      data[i] = ThroughputModule_DataType_262144__alloc();
    break;
  case 524288:
    topic = dds_create_topic(participant, &ThroughputModule_DataType_524288_desc, "Throughput", NULL, NULL);
    for (int i = 0; i < MAX_SAMPLES; i++)
      data[i] = ThroughputModule_DataType_524288__alloc();
    break;
  case 1048576:
    topic = dds_create_topic(participant, &ThroughputModule_DataType_1048576_desc, "Throughput", NULL, NULL);
    for (int i = 0; i < MAX_SAMPLES; i++)
      data[i] = ThroughputModule_DataType_1048576__alloc();
    break;
  default:
    assert(0);
  }

  if (topic < 0)
    DDS_FATAL("dds_create_topic: %s\n", dds_strretcode(-topic));

  /* A Subscriber is created on the domain participant. */

  subParts[0] = partitionName;
  dds_qset_partition (subQos, 1, subParts);
  subscriber = dds_create_subscriber (participant, subQos, NULL);
  if (subscriber < 0)
    DDS_FATAL("dds_create_subscriber: %s\n", dds_strretcode(-subscriber));
  dds_delete_qos (subQos);

  /* A Reader is created on the Subscriber & Topic with a modified Qos. */

  dds_qset_reliability(drQos, DDS_RELIABILITY_RELIABLE, DDS_SECS(10));
  dds_qset_history(drQos, DDS_HISTORY_KEEP_LAST, 16);
  dds_qset_deadline(drQos, DDS_INFINITY);
  dds_qset_durability(drQos, DDS_DURABILITY_VOLATILE);
  dds_qset_liveliness(drQos, DDS_LIVELINESS_AUTOMATIC, DDS_SECS(1));
  dds_qset_resource_limits (drQos, maxSamples, DDS_LENGTH_UNLIMITED, DDS_LENGTH_UNLIMITED);

  rd_listener = dds_create_listener(NULL);
  dds_lset_data_available(rd_listener, data_available_handler);

  /* A Read Condition is created which is triggered when data is available to read */
  waitSet = dds_create_waitset (participant);
  if (waitSet < 0)
    DDS_FATAL("dds_create_waitset: %s\n", dds_strretcode(-waitSet));

  imap = HandleMap__alloc ();

  for (unsigned int i = 0; i < MAX_SAMPLES; i++)
    samples[i] = data[i];

  *reader = dds_create_reader (subscriber, topic, drQos, pollingDelay < 0 ? rd_listener : NULL);
  if (*reader < 0)
    DDS_FATAL("dds_create_reader: %s\n", dds_strretcode(-*reader));

  if (pollingDelay == 0)
  {
    status = dds_waitset_attach (waitSet, *reader, *reader);
    if (status < 0)
      DDS_FATAL("dds_waitset_attach: %s\n", dds_strretcode(-status));
  }

  dds_delete_qos (drQos);
  dds_delete_listener(rd_listener);

  return participant;
}

static void finalize_dds(dds_entity_t participant)
{
  dds_return_t status;

  for (unsigned int i = 0; i < MAX_SAMPLES; i++)
  {
    switch (payloadSize)
    {
    case 16:
      ThroughputModule_DataType_16_free(data[i], DDS_FREE_ALL);
      break;
    case 32:
      ThroughputModule_DataType_32_free(data[i], DDS_FREE_ALL);
      break;
    case 64:
      ThroughputModule_DataType_64_free(data[i], DDS_FREE_ALL);
      break;
    case 128:
      ThroughputModule_DataType_128_free(data[i], DDS_FREE_ALL);
      break;
    case 256:
      ThroughputModule_DataType_256_free(data[i], DDS_FREE_ALL);
      break;
    case 512:
      ThroughputModule_DataType_512_free(data[i], DDS_FREE_ALL);
      break;
    case 1024:
      ThroughputModule_DataType_1024_free(data[i], DDS_FREE_ALL);
      break;
    case 2048:
      ThroughputModule_DataType_2048_free(data[i], DDS_FREE_ALL);
      break;
    case 4096:
      ThroughputModule_DataType_4096_free(data[i], DDS_FREE_ALL);
      break;
    case 8192:
      ThroughputModule_DataType_8192_free(data[i], DDS_FREE_ALL);
      break;
    case 16384:
      ThroughputModule_DataType_16384_free(data[i], DDS_FREE_ALL);
      break;
    case 32768:
      ThroughputModule_DataType_32768_free(data[i], DDS_FREE_ALL);
      break;
    case 65536:
      ThroughputModule_DataType_65536_free(data[i], DDS_FREE_ALL);
      break;
    case 131072:
      ThroughputModule_DataType_131072_free(data[i], DDS_FREE_ALL);
      break;
    case 262144:
      ThroughputModule_DataType_262144_free(data[i], DDS_FREE_ALL);
      break;
    case 524288:
      ThroughputModule_DataType_524288_free(data[i], DDS_FREE_ALL);
      break;
    case 1048576:
      ThroughputModule_DataType_1048576_free(data[i], DDS_FREE_ALL);
      break;
    }
  }

  status = dds_waitset_detach (waitSet, waitSet);
  if (status < 0)
    DDS_FATAL("dds_waitset_detach: %s\n", dds_strretcode(-status));
  status = dds_delete (waitSet);
  if (status < 0)
    DDS_FATAL("dds_delete: %s\n", dds_strretcode(-status));
  status = dds_delete (participant);
  if (status < 0)
    DDS_FATAL("dds_delete: %s\n", dds_strretcode(-status));
}
