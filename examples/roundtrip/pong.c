#include "dds/dds.h"
#include "dds/ddsrt/misc.h"
#include "RoundTrip.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>

static dds_entity_t waitSet;
#define MAX_SAMPLES 10

/* Forward declarations */
static dds_entity_t prepare_dds(dds_entity_t *writer, dds_entity_t *reader, dds_entity_t *readCond, dds_listener_t *listener);
static void finalize_dds(dds_entity_t participant, RoundTripModule_DataType data[MAX_SAMPLES]);

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

static RoundTripModule_DataType data[MAX_SAMPLES];
static void * samples[MAX_SAMPLES];
static dds_sample_info_t info[MAX_SAMPLES];

static dds_entity_t participant;
static dds_entity_t reader;
static dds_entity_t writer;
static dds_entity_t readCond;

static void data_available(dds_entity_t rd, void *arg)
{
  int status, samplecount;
  (void)arg;
  samplecount = dds_take (rd, samples, info, MAX_SAMPLES, MAX_SAMPLES);
  if (samplecount < 0)
    DDS_FATAL("dds_take: %s\n", dds_strretcode(-samplecount));
  for (int j = 0; !dds_triggered (waitSet) && j < samplecount; j++)
  {
    /* If writer has been disposed terminate pong */

    if (info[j].instance_state == DDS_IST_NOT_ALIVE_DISPOSED)
    {
      printf ("Received termination request. Terminating.\n");
      dds_waitset_set_trigger (waitSet, true);
      break;
    }
    else if (info[j].valid_data)
    {
      /* If sample is valid, send it back to ping */
      RoundTripModule_DataType * valid_sample = &data[j];
      status = dds_write_ts (writer, valid_sample, info[j].source_timestamp);
      if (status < 0)
        DDS_FATAL("dds_write_ts: %d\n", -status);
    }
  }
}

int main (int argc, char *argv[])
{
  dds_duration_t waitTimeout = DDS_INFINITY;
  unsigned int i;
  int status;
  dds_attach_t wsresults[1];
  size_t wsresultsize = 1U;

  dds_listener_t *listener = NULL;
  bool use_listener = false;
  int argidx = 1;

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

  /* Initialize sample data */
  memset (data, 0, sizeof (data));
  for (i = 0; i < MAX_SAMPLES; i++)
  {
    samples[i] = &data[i];
  }

  participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  if (participant < 0)
    DDS_FATAL("dds_create_participant: %s\n", dds_strretcode(-participant));

  if (use_listener)
  {
    listener = dds_create_listener(NULL);
    dds_lset_data_available(listener, data_available);
  }

  (void)prepare_dds(&writer, &reader, &readCond, listener);

  while (!dds_triggered (waitSet))
  {
    /* Wait for a sample from ping */

    status = dds_waitset_wait (waitSet, wsresults, wsresultsize, waitTimeout);
    if (status < 0)
      DDS_FATAL("dds_waitset_wait: %s\n", dds_strretcode(-status));

    /* Take samples */
    if (listener == NULL) {
      data_available (reader, 0);
    }
  }

#ifdef _WIN32
  SetConsoleCtrlHandler (0, FALSE);
#elif !DDSRT_WITH_FREERTOS && !__ZEPHYR__
  sigaction (SIGINT, &oldAction, 0);
#endif

  /* Clean up */
  finalize_dds(participant, data);

  return EXIT_SUCCESS;
}

static void finalize_dds(dds_entity_t pp, RoundTripModule_DataType xs[MAX_SAMPLES])
{
  dds_return_t status;
  status = dds_delete (pp);
  if (status < 0)
    DDS_FATAL("dds_delete: %s\n", dds_strretcode(-status));
  for (unsigned int i = 0; i < MAX_SAMPLES; i++)
  {
    RoundTripModule_DataType_free (&xs[i], DDS_FREE_CONTENTS);
  }
}

static dds_entity_t prepare_dds(dds_entity_t *wr, dds_entity_t *rd, dds_entity_t *rdcond, dds_listener_t *rdlist)
{
  const char *pubPartitions[] = { "pong" };
  const char *subPartitions[] = { "ping" };
  dds_qos_t *qos;
  dds_entity_t subscriber;
  dds_entity_t publisher;
  dds_entity_t topic;
  dds_return_t status;

  /* A DDS Topic is created for our sample type on the domain participant. */

  qos = dds_create_qos ();
  dds_qset_reliability (qos, DDS_RELIABILITY_RELIABLE, DDS_SECS(10));
  topic = dds_create_topic (participant, &RoundTripModule_DataType_desc, "RoundTrip", qos, NULL);
  if (topic < 0)
    DDS_FATAL("dds_create_topic: %s\n", dds_strretcode(-topic));
  dds_delete_qos (qos);

  /* A DDS Publisher is created on the domain participant. */

  qos = dds_create_qos ();
  dds_qset_partition (qos, 1, pubPartitions);

  publisher = dds_create_publisher (participant, qos, NULL);
  if (publisher < 0)
    DDS_FATAL("dds_create_publisher: %s\n", dds_strretcode(-publisher));
  dds_delete_qos (qos);

  /* A DDS DataWriter is created on the Publisher & Topic with a modififed Qos. */

  qos = dds_create_qos ();
  dds_qset_writer_data_lifecycle (qos, false);
  *wr = dds_create_writer (publisher, topic, qos, NULL);
  if (*wr < 0)
    DDS_FATAL("dds_create_writer: %s\n", dds_strretcode(-*wr));
  dds_delete_qos (qos);

  /* A DDS Subscriber is created on the domain participant. */

  qos = dds_create_qos ();
  dds_qset_partition (qos, 1, subPartitions);

  subscriber = dds_create_subscriber (participant, qos, NULL);
  if (subscriber < 0)
    DDS_FATAL("dds_create_subscriber: %s\n", dds_strretcode(-subscriber));
  dds_delete_qos (qos);

  /* A DDS DataReader is created on the Subscriber & Topic with a modified QoS. */

  *rd = dds_create_reader (subscriber, topic, NULL, rdlist);
  if (*rd < 0)
    DDS_FATAL("dds_create_reader: %s\n", dds_strretcode(-*rd));

  waitSet = dds_create_waitset (participant);
  if (rdlist == NULL) {
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

  printf ("Waiting for samples from ping to send back...\n");
  fflush (stdout);

  return participant;
}
