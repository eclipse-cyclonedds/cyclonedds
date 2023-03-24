#include "dds/dds.h"
#include "dds/ddsrt/misc.h"
#include "RoundTrip.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <inttypes.h>

#define TIME_STATS_SIZE_INCREMENT 50000
#define MAX_SAMPLES 100
#define US_IN_ONE_SEC 1000000LL

/* Forward declaration */

static dds_entity_t prepare_dds(dds_entity_t *writer, dds_entity_t *reader, dds_entity_t *readCond, dds_listener_t *listener);
static void finalize_dds(dds_entity_t participant);

typedef struct ExampleTimeStats
{
  dds_time_t * values;
  unsigned long valuesSize;
  unsigned long valuesMax;
  double average;
  dds_time_t min;
  dds_time_t max;
  unsigned long count;
} ExampleTimeStats;

static void exampleInitTimeStats (ExampleTimeStats *stats)
{
  stats->values = (dds_time_t*) malloc (TIME_STATS_SIZE_INCREMENT * sizeof (dds_time_t));
  stats->valuesSize = 0;
  stats->valuesMax = TIME_STATS_SIZE_INCREMENT;
  stats->average = 0;
  stats->min = 0;
  stats->max = 0;
  stats->count = 0;
}

static void exampleResetTimeStats (ExampleTimeStats *stats)
{
  memset (stats->values, 0, stats->valuesMax * sizeof (dds_time_t));
  stats->valuesSize = 0;
  stats->average = 0;
  stats->min = 0;
  stats->max = 0;
  stats->count = 0;
}

static void exampleDeleteTimeStats (ExampleTimeStats *stats)
{
  free (stats->values);
}

static ExampleTimeStats *exampleAddTimingToTimeStats
  (ExampleTimeStats *stats, dds_time_t timing)
{
  if (stats->valuesSize > stats->valuesMax)
  {
    dds_time_t * temp = (dds_time_t*) realloc (stats->values, (stats->valuesMax + TIME_STATS_SIZE_INCREMENT) * sizeof (dds_time_t));
    stats->values = temp;
    stats->valuesMax += TIME_STATS_SIZE_INCREMENT;
  }
  if (stats->values != NULL && stats->valuesSize < stats->valuesMax)
  {
    stats->values[stats->valuesSize++] = timing;
  }
  stats->average = ((double)stats->count * stats->average + (double)timing) / (double)(stats->count + 1);
  stats->min = (stats->count == 0 || timing < stats->min) ? timing : stats->min;
  stats->max = (stats->count == 0 || timing > stats->max) ? timing : stats->max;
  stats->count++;

  return stats;
}

static int exampleCompareul (const void* a, const void* b)
{
  dds_time_t ul_a = *((dds_time_t*)a);
  dds_time_t ul_b = *((dds_time_t*)b);

  if (ul_a < ul_b) return -1;
  if (ul_a > ul_b) return 1;
  return 0;
}

static double exampleGetMedianFromTimeStats (ExampleTimeStats *stats)
{
  double median = 0.0;

  qsort (stats->values, stats->valuesSize, sizeof (dds_time_t), exampleCompareul);

  if (stats->valuesSize % 2 == 0)
  {
    median = (double)(stats->values[stats->valuesSize / 2 - 1] + stats->values[stats->valuesSize / 2]) / 2;
  }
  else
  {
    median = (double)stats->values[stats->valuesSize / 2];
  }

  return median;
}

static dds_time_t exampleGet99PercentileFromTimeStats (ExampleTimeStats *stats)
{
  qsort (stats->values, stats->valuesSize, sizeof (dds_time_t), exampleCompareul);
  return stats->values[stats->valuesSize - stats->valuesSize / 100];
}

static dds_entity_t waitSet;

#ifdef _WIN32
#include <Windows.h>
static bool CtrlHandler (DWORD fdwCtrlType)
{
  (void)fdwCtrlType;
  dds_waitset_set_trigger (waitSet, true);
  return true; //Don't let other handlers handle this key
}
#elif !DDSRT_WITH_FREERTOS && !__ZEPHYR__
static void CtrlHandler (int sig)
{
  (void)sig;
  dds_waitset_set_trigger (waitSet, true);
}
#endif

static dds_entity_t writer;
static dds_entity_t reader;
static dds_entity_t participant;
static dds_entity_t readCond;

static ExampleTimeStats roundTrip;
static ExampleTimeStats writeAccess;
static ExampleTimeStats readAccess;
static ExampleTimeStats roundTripOverall;
static ExampleTimeStats writeAccessOverall;
static ExampleTimeStats readAccessOverall;

static RoundTripModule_DataType pub_data;
static RoundTripModule_DataType sub_data[MAX_SAMPLES];
static void *samples[MAX_SAMPLES];
static dds_sample_info_t info[MAX_SAMPLES];

static dds_time_t startTime;
static dds_time_t preWriteTime;
static dds_time_t postWriteTime;
static dds_time_t preTakeTime;
static dds_time_t postTakeTime;
static dds_time_t elapsed = 0;

static bool warmUp = true;

static void data_available(dds_entity_t rd, void *arg)
{
  dds_time_t difference = 0;
  int status;
  (void)arg;
  /* Take sample and check that it is valid */
  preTakeTime = dds_time ();
  status = dds_take (rd, samples, info, MAX_SAMPLES, MAX_SAMPLES);
  if (status < 0)
    DDS_FATAL("dds_take: %s\n", dds_strretcode(-status));
  postTakeTime = dds_time ();

  /* Update stats */
  difference = (postWriteTime - preWriteTime)/DDS_NSECS_IN_USEC;
  writeAccess = *exampleAddTimingToTimeStats (&writeAccess, difference);
  writeAccessOverall = *exampleAddTimingToTimeStats (&writeAccessOverall, difference);

  difference = (postTakeTime - preTakeTime)/DDS_NSECS_IN_USEC;
  readAccess = *exampleAddTimingToTimeStats (&readAccess, difference);
  readAccessOverall = *exampleAddTimingToTimeStats (&readAccessOverall, difference);

  difference = (postTakeTime - info[0].source_timestamp)/DDS_NSECS_IN_USEC;
  roundTrip = *exampleAddTimingToTimeStats (&roundTrip, difference);
  roundTripOverall = *exampleAddTimingToTimeStats (&roundTripOverall, difference);

  if (!warmUp) {
    /* Print stats each second */
    difference = (postTakeTime - startTime)/DDS_NSECS_IN_USEC;
    if (difference > US_IN_ONE_SEC)
    {
      printf("%9" PRIi64 " %9lu %8.0f %8" PRIi64 " %8" PRIi64 " %8" PRIi64 " %10lu %8.0f %8" PRIi64 " %10lu %8.0f %8" PRIi64 "\n",
             elapsed + 1,
             roundTrip.count,
             exampleGetMedianFromTimeStats (&roundTrip) / 2,
             roundTrip.min / 2,
             exampleGet99PercentileFromTimeStats (&roundTrip) / 2,
             roundTrip.max / 2,
             writeAccess.count,
             exampleGetMedianFromTimeStats (&writeAccess),
             writeAccess.min,
             readAccess.count,
             exampleGetMedianFromTimeStats (&readAccess),
             readAccess.min);
      fflush (stdout);

      exampleResetTimeStats (&roundTrip);
      exampleResetTimeStats (&writeAccess);
      exampleResetTimeStats (&readAccess);
      startTime = dds_time ();
      elapsed++;
    }
  }

  preWriteTime = dds_time();
  status = dds_write_ts (writer, &pub_data, preWriteTime);
  if (status < 0)
    DDS_FATAL("dds_write_ts: %s\n", dds_strretcode(-status));
  postWriteTime = dds_time();
}

static void usage(void)
{
  printf ("Usage (parameters must be supplied in order):\n"
          "./ping [-l] [payloadSize (bytes, 0 - 100M)] [numSamples (0 = infinite)] [timeOut (seconds, 0 = infinite)]\n"
          "./ping quit - ping sends a quit signal to pong.\n"
          "Defaults:\n"
          "./ping 0 0 0\n");
  exit(EXIT_FAILURE);
}

int main (int argc, char *argv[])
{
  uint32_t payloadSize = 0;
  uint64_t numSamples = 0;
  bool invalidargs = false;
  dds_time_t timeOut = 0;
  dds_time_t time;
  dds_time_t difference = 0;

  dds_attach_t wsresults[1];
  size_t wsresultsize = 1U;
  dds_time_t waitTimeout = DDS_SECS (1);
  unsigned long i;
  int status;

  dds_listener_t *listener = NULL;
  bool use_listener = false;
  int argidx = 1;

  /* poor man's getopt works even on Windows */
  if (argc > argidx && strcmp(argv[argidx], "-l") == 0)
  {
    argidx++;
    use_listener = true;
  }

  /* Register handler for Ctrl-C */
#ifdef _WIN32
  DDSRT_WARNING_GNUC_OFF(cast-function-type)
  SetConsoleCtrlHandler ((PHANDLER_ROUTINE)CtrlHandler, TRUE);
  DDSRT_WARNING_GNUC_ON(cast-function-type)
#elif !DDSRT_WITH_FREERTOS && !__ZEPHYR__
  struct sigaction sat, oldAction;
  sat.sa_handler = CtrlHandler;
  sigemptyset (&sat.sa_mask);
  sat.sa_flags = 0;
  sigaction (SIGINT, &sat, &oldAction);
#endif

  exampleInitTimeStats (&roundTrip);
  exampleInitTimeStats (&writeAccess);
  exampleInitTimeStats (&readAccess);
  exampleInitTimeStats (&roundTripOverall);
  exampleInitTimeStats (&writeAccessOverall);
  exampleInitTimeStats (&readAccessOverall);

  memset (&sub_data, 0, sizeof (sub_data));
  memset (&pub_data, 0, sizeof (pub_data));

  for (i = 0; i < MAX_SAMPLES; i++)
  {
    samples[i] = &sub_data[i];
  }

  participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  if (participant < 0)
    DDS_FATAL("dds_create_participant: %s\n", dds_strretcode(-participant));

  if (use_listener)
  {
    listener = dds_create_listener(NULL);
    dds_lset_data_available(listener, data_available);
  }
  prepare_dds(&writer, &reader, &readCond, listener);

  if (argc - argidx == 1 && strcmp (argv[argidx], "quit") == 0)
  {
    printf ("Sending termination request.\n");
    fflush (stdout);
    /* pong uses a waitset which is triggered by instance disposal, and
      quits when it fires. */
    dds_sleepfor (DDS_SECS (1));
    pub_data.payload._length = 0;
    pub_data.payload._buffer = NULL;
    pub_data.payload._release = true;
    pub_data.payload._maximum = 0;
    status = dds_writedispose (writer, &pub_data);
    if (status < 0)
      DDS_FATAL("dds_writedispose: %s\n", dds_strretcode(-status));
    dds_sleepfor (DDS_SECS (1));
    goto done;
  }

  if (argc - argidx == 0)
  {
    invalidargs = true;
  }
  if (argc - argidx >= 1)
  {
    payloadSize = (uint32_t) atol (argv[argidx]);

    if (payloadSize > 100 * 1048576)
    {
      invalidargs = true;
    }
  }
  if (argc - argidx >= 2)
  {
    numSamples = (uint64_t) atol (argv[argidx+1]);
  }
  if (argc - argidx >= 3)
  {
    timeOut = atol (argv[argidx+2]);
  }
  if (invalidargs || (argc - argidx == 1 && (strcmp (argv[argidx], "-h") == 0 || strcmp (argv[argidx], "--help") == 0)))
    usage();
  printf ("# payloadSize: %" PRIu32 " | numSamples: %" PRIu64 " | timeOut: %" PRIi64 "\n\n", payloadSize, numSamples, timeOut);
  fflush (stdout);

  pub_data.payload._length = payloadSize;
  pub_data.payload._buffer = payloadSize ? dds_alloc (payloadSize) : NULL;
  pub_data.payload._release = true;
  pub_data.payload._maximum = 0;
  for (i = 0; i < payloadSize; i++)
  {
    pub_data.payload._buffer[i] = 'a';
  }

  startTime = dds_time ();
  printf ("# Waiting for startup jitter to stabilise\n");
  fflush (stdout);
  /* Write a sample that pong can send back */
  while (!dds_triggered (waitSet) && difference < DDS_SECS(5))
  {
    status = dds_waitset_wait (waitSet, wsresults, wsresultsize, waitTimeout);
    if (status < 0)
      DDS_FATAL("dds_waitset_wait: %s\n", dds_strretcode(-status));

    if (status > 0 && listener == NULL) /* data */
    {
      status = dds_take (reader, samples, info, MAX_SAMPLES, MAX_SAMPLES);
      if (status < 0)
        DDS_FATAL("dds_take: %s\n", dds_strretcode(-status));
    }

    time = dds_time ();
    difference = time - startTime;
  }
  if (!dds_triggered (waitSet))
  {
    warmUp = false;
    printf("# Warm up complete.\n\n");
    printf("# Latency measurements (in us)\n");
    printf("#             Latency [us]                                   Write-access time [us]       Read-access time [us]\n");
    printf("# Seconds     Count   median      min      99%%      max      Count   median      min      Count   median      min\n");
    fflush (stdout);
  }

  exampleResetTimeStats (&roundTrip);
  exampleResetTimeStats (&writeAccess);
  exampleResetTimeStats (&readAccess);
  startTime = dds_time ();
  /* Write a sample that pong can send back */
  preWriteTime = dds_time ();
  status = dds_write_ts (writer, &pub_data, preWriteTime);
  if (status < 0)
    DDS_FATAL("dds_write_ts: %s\n", dds_strretcode(-status));
  postWriteTime = dds_time ();
  for (i = 0; !dds_triggered (waitSet) && (!numSamples || i < numSamples) && !(timeOut && elapsed >= timeOut); i++)
  {
    status = dds_waitset_wait (waitSet, wsresults, wsresultsize, waitTimeout);
    if (status < 0)
      DDS_FATAL("dds_waitset_wait: %s\n", dds_strretcode(-status));
    if (status != 0 && listener == NULL) {
      data_available(reader, NULL);
    }
  }

  if (!warmUp)
  {
    printf
    (
      "\n%9s %9lu %8.0f %8" PRIi64 " %8" PRIi64 " %8" PRIi64 " %10lu %8.0f %8" PRIi64 " %10lu %8.0f %8" PRIi64 "\n",
      "# Overall",
      roundTripOverall.count,
      exampleGetMedianFromTimeStats (&roundTripOverall) / 2,
      roundTripOverall.min / 2,
      exampleGet99PercentileFromTimeStats (&roundTripOverall) / 2,
      roundTripOverall.max / 2,
      writeAccessOverall.count,
      exampleGetMedianFromTimeStats (&writeAccessOverall),
      writeAccessOverall.min,
      readAccessOverall.count,
      exampleGetMedianFromTimeStats (&readAccessOverall),
      readAccessOverall.min
    );
    fflush (stdout);
  }

done:

#ifdef _WIN32
  SetConsoleCtrlHandler (0, FALSE);
#elif !DDSRT_WITH_FREERTOS && !__ZEPHYR__
  sigaction (SIGINT, &oldAction, 0);
#endif

  finalize_dds(participant);

  /* Clean up */
  exampleDeleteTimeStats (&roundTrip);
  exampleDeleteTimeStats (&writeAccess);
  exampleDeleteTimeStats (&readAccess);
  exampleDeleteTimeStats (&roundTripOverall);
  exampleDeleteTimeStats (&writeAccessOverall);
  exampleDeleteTimeStats (&readAccessOverall);

  for (i = 0; i < MAX_SAMPLES; i++)
  {
    RoundTripModule_DataType_free (&sub_data[i], DDS_FREE_CONTENTS);
  }
  RoundTripModule_DataType_free (&pub_data, DDS_FREE_CONTENTS);

  return EXIT_SUCCESS;
}

static dds_entity_t prepare_dds(dds_entity_t *wr, dds_entity_t *rd, dds_entity_t *rdcond, dds_listener_t *listener)
{
  dds_return_t status;
  dds_entity_t topic;
  dds_entity_t publisher;
  dds_entity_t subscriber;

  const char *pubPartitions[] = { "ping" };
  const char *subPartitions[] = { "pong" };
  dds_qos_t *pubQos;
  dds_qos_t *subQos;
  dds_qos_t *tQos;
  dds_qos_t *wQos;

  /* A DDS_Topic is created for our sample type on the domain participant. */
  tQos = dds_create_qos ();
  dds_qset_reliability (tQos, DDS_RELIABILITY_RELIABLE, DDS_SECS (10));
  topic = dds_create_topic (participant, &RoundTripModule_DataType_desc, "RoundTrip", tQos, NULL);
  if (topic < 0)
    DDS_FATAL("dds_create_topic: %s\n", dds_strretcode(-topic));
  dds_delete_qos (tQos);

  /* A DDS_Publisher is created on the domain participant. */
  pubQos = dds_create_qos ();
  dds_qset_partition (pubQos, 1, pubPartitions);

  publisher = dds_create_publisher (participant, pubQos, NULL);
  if (publisher < 0)
    DDS_FATAL("dds_create_publisher: %s\n", dds_strretcode(-publisher));
  dds_delete_qos (pubQos);

  /* A DDS_DataWriter is created on the Publisher & Topic with a modified Qos. */
  wQos = dds_create_qos ();
  dds_qset_writer_data_lifecycle (wQos, false);
  *wr = dds_create_writer (publisher, topic, wQos, NULL);
  if (*wr < 0)
    DDS_FATAL("dds_create_writer: %s\n", dds_strretcode(-*wr));
  dds_delete_qos (wQos);

  /* A DDS_Subscriber is created on the domain participant. */
  subQos = dds_create_qos ();

  dds_qset_partition (subQos, 1, subPartitions);

  subscriber = dds_create_subscriber (participant, subQos, NULL);
  if (subscriber < 0)
    DDS_FATAL("dds_create_subscriber: %s\n", dds_strretcode(-subscriber));
  dds_delete_qos (subQos);
  /* A DDS_DataReader is created on the Subscriber & Topic with a modified QoS. */
  *rd = dds_create_reader (subscriber, topic, NULL, listener);
  if (*rd < 0)
    DDS_FATAL("dds_create_reader: %s\n", dds_strretcode(-*rd));

  waitSet = dds_create_waitset (participant);
  if (listener == NULL) {
    *rdcond = dds_create_readcondition (*rd, DDS_ANY_STATE);
    status = dds_waitset_attach (waitSet, *rdcond, *rd);
    if (status < 0)
      DDS_FATAL("dds_waitset_attach: %s\n", dds_strretcode(-status));
  } else {
    *rdcond = 0;
  }
  status = dds_waitset_attach (waitSet, waitSet, waitSet);
  if (status < 0)
    DDS_FATAL("dds_waitset_attach: %s\n", dds_strretcode(-status));

  return participant;
}

static void finalize_dds(dds_entity_t ppant)
{
  dds_return_t status;
  status = dds_delete (ppant);
  if (status < 0)
    DDS_FATAL("dds_delete: %s\n", dds_strretcode(-status));
}
