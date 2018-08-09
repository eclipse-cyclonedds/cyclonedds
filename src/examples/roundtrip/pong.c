#include "ddsc/dds.h"
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
  dds_waitset_set_trigger (waitSet, true);
  return true; //Don't let other handlers handle this key
}
#else
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

static void data_available(dds_entity_t reader, void *arg)
{
  int status, samplecount;
  (void)arg;
  samplecount = dds_take (reader, samples, info, MAX_SAMPLES, MAX_SAMPLES);
  DDS_ERR_CHECK (samplecount, DDS_CHECK_REPORT | DDS_CHECK_EXIT);
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
      DDS_ERR_CHECK (status, DDS_CHECK_REPORT | DDS_CHECK_EXIT);
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
  SetConsoleCtrlHandler ((PHANDLER_ROUTINE)CtrlHandler, TRUE);
#else
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
  DDS_ERR_CHECK (participant, DDS_CHECK_REPORT | DDS_CHECK_EXIT);

  if (use_listener)
  {
    listener = dds_listener_create(NULL);
    dds_lset_data_available(listener, data_available);
  }

  (void)prepare_dds(&writer, &reader, &readCond, listener);

  while (!dds_triggered (waitSet))
  {
    /* Wait for a sample from ping */

    status = dds_waitset_wait (waitSet, wsresults, wsresultsize, waitTimeout);
    DDS_ERR_CHECK (status, DDS_CHECK_REPORT | DDS_CHECK_EXIT);

    /* Take samples */
    if (listener == NULL) {
      data_available (reader, 0);
    }
  }

#ifdef _WIN32
  SetConsoleCtrlHandler (0, FALSE);
#else
  sigaction (SIGINT, &oldAction, 0);
#endif

  /* Clean up */
  finalize_dds(participant, data);

  return EXIT_SUCCESS;
}

static void finalize_dds(dds_entity_t participant, RoundTripModule_DataType data[MAX_SAMPLES])
{
  dds_return_t status;
  status = dds_delete (participant);
  DDS_ERR_CHECK (status, DDS_CHECK_REPORT | DDS_CHECK_EXIT);
  for (unsigned int i = 0; i < MAX_SAMPLES; i++)
  {
    RoundTripModule_DataType_free (&data[i], DDS_FREE_CONTENTS);
  }
}

static dds_entity_t prepare_dds(dds_entity_t *writer, dds_entity_t *reader, dds_entity_t *readCond, dds_listener_t *listener)
{
  const char *pubPartitions[] = { "pong" };
  const char *subPartitions[] = { "ping" };
  dds_qos_t *qos;
  dds_entity_t subscriber;
  dds_entity_t publisher;
  dds_entity_t topic;
  dds_return_t status;

  /* A DDS Topic is created for our sample type on the domain participant. */

  topic = dds_create_topic (participant, &RoundTripModule_DataType_desc, "RoundTrip", NULL, NULL);
  DDS_ERR_CHECK (topic, DDS_CHECK_REPORT | DDS_CHECK_EXIT);

  /* A DDS Publisher is created on the domain participant. */

  qos = dds_qos_create ();
  dds_qset_partition (qos, 1, pubPartitions);

  publisher = dds_create_publisher (participant, qos, NULL);
  DDS_ERR_CHECK (publisher, DDS_CHECK_REPORT | DDS_CHECK_EXIT);
  dds_qos_delete (qos);

  /* A DDS DataWriter is created on the Publisher & Topic with a modififed Qos. */

  qos = dds_qos_create ();
  dds_qset_reliability (qos, DDS_RELIABILITY_RELIABLE, DDS_SECS(10));
  dds_qset_writer_data_lifecycle (qos, false);
  *writer = dds_create_writer (publisher, topic, qos, NULL);
  DDS_ERR_CHECK (*writer, DDS_CHECK_REPORT | DDS_CHECK_EXIT);
  dds_qos_delete (qos);

  /* A DDS Subscriber is created on the domain participant. */

  qos = dds_qos_create ();
  dds_qset_partition (qos, 1, subPartitions);

  subscriber = dds_create_subscriber (participant, qos, NULL);
  DDS_ERR_CHECK (subscriber, DDS_CHECK_REPORT | DDS_CHECK_EXIT);
  dds_qos_delete (qos);

  /* A DDS DataReader is created on the Subscriber & Topic with a modified QoS. */

  qos = dds_qos_create ();
  dds_qset_reliability (qos, DDS_RELIABILITY_RELIABLE, DDS_SECS(10));
  *reader = dds_create_reader (subscriber, topic, qos, listener);
  DDS_ERR_CHECK (*reader, DDS_CHECK_REPORT | DDS_CHECK_EXIT);
  dds_qos_delete (qos);

  waitSet = dds_create_waitset (participant);
  if (listener == NULL) {
    *readCond = dds_create_readcondition (*reader, DDS_ANY_STATE);
    status = dds_waitset_attach (waitSet, *readCond, *reader);
    DDS_ERR_CHECK (status, DDS_CHECK_REPORT | DDS_CHECK_EXIT);
  } else {
    *readCond = 0;
  }
  status = dds_waitset_attach (waitSet, waitSet, waitSet);
  DDS_ERR_CHECK (status, DDS_CHECK_REPORT | DDS_CHECK_EXIT);

  printf ("Waiting for samples from ping to send back...\n");
  fflush (stdout);

  return participant;
}
