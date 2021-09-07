#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "dds/dds.h"

/* Compile time constants representing the (DCPSPublication and DCPSSubscription) built-in
   topics that used for monitoring whether we should have discovered some topics on the
   DCPSTopic built-in topic. */
static const dds_entity_t ep_topics[] = {
  DDS_BUILTIN_TOPIC_DCPSPUBLICATION,
  DDS_BUILTIN_TOPIC_DCPSSUBSCRIPTION
};

struct keystr {
  char v[36]; /* (8 hex digits) : (8 h d) : (8 h d) : (8 h d) \0 */
};

static char *keystr (struct keystr *gs, const dds_builtintopic_topic_key_t *g)
{
  (void) snprintf (gs->v, sizeof (gs->v),
                   "%02x%02x%02x%02x:%02x%02x%02x%02x:%02x%02x%02x%02x:%02x%02x%02x%02x",
                   g->d[0], g->d[1], g->d[2], g->d[3], g->d[4], g->d[5], g->d[6], g->d[7],
                   g->d[8], g->d[9], g->d[10], g->d[11], g->d[12], g->d[13], g->d[14], g->d[15]);
  return gs->v;
}

static const char *instance_state_str (dds_instance_state_t s)
{
  switch (s)
  {
    case DDS_ALIVE_INSTANCE_STATE: return "alive";
    case DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE: return "nowriters";
    case DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE: return "disposed";
  }
  assert (0);
  return "";
}

static bool process_topic (dds_entity_t readcond)
{
#define MAXCOUNT 10
  /* dds_take allocates memory for the data if samples[0] is a null pointer, and reuses
     it otherwise, so it must be initialized properly.  The "10" is arbitrary. */
  void *samples[MAXCOUNT] = { NULL };
  dds_sample_info_t infos[MAXCOUNT];
  samples[0] = NULL;
  /* Using the condition that was attached to the waitset means one never accidentally
     filters out some samples that the waitset triggers on.  Because waitsets are
     level-triggered, that would result in a spinning thread. */
  int32_t n = dds_take (readcond, samples, infos, MAXCOUNT, MAXCOUNT);
  bool topics_seen = false;
  for (int32_t i = 0; i < n; i++)
  {
    dds_builtintopic_topic_t const * const sample = samples[i];
    struct keystr gs;
    printf ("%s: %s", instance_state_str (infos[i].instance_state), keystr (&gs, &sample->key));
    if (infos[i].valid_data)
    {
      printf (" %s %s", sample->topic_name, sample->type_name);
      if (strncmp (sample->topic_name, "DCPS", 4) != 0)
      {
        /* Topic names starting with DCPS are guaranteed to be built-in topics, so we
           have discovered an application topic if the name doesn't start with DCPS */
        topics_seen = true;
      }
    }
    printf ("\n");
  }
  /* Release memory allocated by dds_take */
  (void) dds_return_loan (readcond, samples, n);
#undef MAXCOUNT
  return topics_seen;
}

static bool process_pubsub (dds_entity_t ep_readconds[])
{
  bool endpoints_exist = false;
  for (size_t k = 0; k < sizeof (ep_topics) / sizeof (ep_topics[0]) && !endpoints_exist; k++)
  {
    /* Reuse samples/infos arrays when checking for readers/writers, using a single sample
       is just as arbitrary as using MAXCOUNT samples in process_topic */
    void *sampleptr = NULL;
    dds_sample_info_t info;
    int32_t n = dds_take (ep_readconds[k], &sampleptr, &info, 1, 1);
    if (n > 0)
    {
      dds_builtintopic_endpoint_t const * const sample = sampleptr;
      if (info.valid_data && strncmp (sample->topic_name, "DCPS", 4) != 0)
        endpoints_exist = true;
    }
    (void) dds_return_loan (ep_readconds[k], &sampleptr, n);
  }
  return endpoints_exist;
}

int main (int argc, char **argv)
{
  (void)argc;
  (void)argv;

  const dds_entity_t participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  if (participant < 0)
  {
    fprintf (stderr, "dds_create_participant: %s\n", dds_strretcode (participant));
    return 1;
  }

  const dds_entity_t waitset = dds_create_waitset (participant);

  /* Create a reader for the DCPSTopic built-in topic.  The built-in topics are identified
     using compile-time constants rather than ordinary, dynamically allocated handles.  In
     Cyclone's C API, any QoS settings left unspecified in the reader and writer QoS are
     inherited from the topic QoS, and so specifying no QoS object results in a
     transient-local, reliable reader. */
  const dds_entity_t reader = dds_create_reader (participant, DDS_BUILTIN_TOPIC_DCPSTOPIC, NULL, NULL);
  if (reader < 0)
  {
    if (reader == DDS_RETCODE_UNSUPPORTED)
      fprintf (stderr, "Topic discovery is not included in the build, rebuild with ENABLE_TOPIC_DISCOVERY=ON\n");
    else
      fprintf (stderr, "dds_create_reader(DCPSTopic): %s\n", dds_strretcode (reader));
    dds_delete (participant);
    return 1;
  }

  /* Create a read condition and attach it to the waitset.  Using a read condition for
     ANY_STATE is almost, but not quite, equivalent to setting the status mask to
     DATA_AVAILABLE and attaching the reader directly: the read condition remains in a
     triggered state until the reader history cache no longer contains any matching
     samples, but the DATA_AVAILABLE state is reset on a call to read/take and only raised
     again on the receipt of the next sample.  Reading only a limited number of samples
     every time DATA_AVAILABLE triggers therefore risks never reading some samples. */
  const dds_entity_t readcond = dds_create_readcondition (reader, DDS_ANY_STATE);
  (void) dds_waitset_attach (waitset, readcond, 0);

  /* Also create readers for the DCPSPublication and DCPSSubscription topics so we can
     warn if topic discovery is most likely disabled in the configuration. */
  dds_entity_t ep_readers[sizeof (ep_topics) / sizeof (ep_topics[0])];
  dds_entity_t ep_readconds[sizeof (ep_topics) / sizeof (ep_topics[0])];
  for (size_t k = 0; k < sizeof (ep_topics) / sizeof (ep_topics[0]); k++)
  {
    ep_readers[k] = dds_create_reader (participant, ep_topics[k], NULL, NULL);
    ep_readconds[k] = dds_create_readcondition (ep_readers[k], DDS_ANY_STATE);
    (void) dds_waitset_attach (waitset, ep_readconds[k], 0);
  }

  /* Keep track of whether (non-built-in) topics were discovered and of whether
     (non-built-in) endpoints were for generating a warning that the configuration likely
     has topic discovery disabled if only endpoints got discovered. */
  bool topics_seen = false;
  bool endpoints_exist = false;

  /* Monitor topic creation/deletion for 10s.  There is no risk of spurious wakeups in a
     simple case like this and so a timeout from wait_until really means that tstop has
     passed. */
  const dds_time_t tstop = dds_time () + DDS_SECS (10);
  while (dds_waitset_wait_until (waitset, NULL, 0, tstop) > 0)
  {
    if (process_topic (readcond))
      topics_seen = true;

    /* No point in looking for other readers/writers once we know some exist */
    if (!endpoints_exist && process_pubsub (ep_readconds))
    {
      endpoints_exist = true;
      /* The readers used for monitoring the existence of readers/writers are no longer
         useful once some eps have been seen.  Deleting them will also detach the
         read conditions from the waitset and delete them. */
      for (size_t k = 0; k < sizeof (ep_readers) / sizeof (ep_readers[0]); k++)
        (void) dds_delete (ep_readers[k]);
    }
  }
  if (!topics_seen && endpoints_exist)
  {
    fprintf (stderr, "No topics discovered but remote readers/writers exist. Is topic discovery enabled in the configuration?\n");
  }

  dds_delete (participant);
  return 0;
}
