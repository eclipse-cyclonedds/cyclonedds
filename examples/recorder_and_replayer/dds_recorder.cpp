/*
 * Copyright(c) 2025 ZettaScale Technology and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <locale.h>
#include <signal.h>
#include <string>
#include <chrono>
#include <iostream>

#include "dds/dds.h"
#include "mcap/writer.hpp"

// most part is copied from dynsub.c, but ignore xtypeobj.
// we do not have a reliable way with c api to obtain idl at runtime, so we have to
// offer it as input.

#include "dds/ddsrt/threads.h"
#include "dds/ddsi/ddsi_serdata.h"
#include "dds_topic_descriptor_serde.h"

// For convenience, the DDS participant is global
static dds_entity_t participant;

static dds_entity_t termcond;

// Helper function to wait for a DCPSPublication/DCPSSubscription to show up with the desired topic name,
// and its descriptor
static dds_return_t get_topic_and_desc (const char *topic_name, dds_duration_t timeout, dds_entity_t *topic,
                                        dds_topic_descriptor_t **descriptor)
{
  *descriptor = NULL;
  const dds_entity_t waitset = dds_create_waitset (participant);
  // we only care publisher as we are recorder
  const dds_entity_t dcpspublication_reader = dds_create_reader (participant, DDS_BUILTIN_TOPIC_DCPSPUBLICATION, NULL, NULL);
  const dds_entity_t dcpspublication_readcond = dds_create_readcondition (dcpspublication_reader, DDS_ANY_STATE);
  (void)dds_waitset_attach (waitset, dcpspublication_readcond, dcpspublication_reader);
  const dds_time_t abstimeout = (timeout == DDS_INFINITY) ? DDS_NEVER : dds_time () + timeout;
  dds_return_t ret = DDS_RETCODE_OK;
  dds_attach_t triggered_reader_x;
  while (dds_waitset_wait_until (waitset, &triggered_reader_x, 1, abstimeout) > 0)
  {
    void *epraw = NULL;
    dds_sample_info_t si;
    dds_entity_t triggered_reader = (dds_entity_t)triggered_reader_x;
    if (dds_take (triggered_reader, &epraw, &si, 1, 1) <= 0)
      continue;
    dds_builtintopic_endpoint_t *ep = (dds_builtintopic_endpoint_t *)epraw;
    const dds_typeinfo_t *typeinfo = NULL;

    // we need the topic descriptor so we do not use dds_find_topic
    if (ep->topic_name != NULL && strcmp (ep->topic_name, topic_name) == 0 &&
        dds_builtintopic_get_endpoint_type_info (ep, &typeinfo) == 0 && typeinfo)
    {
      if ((ret = dds_create_topic_descriptor (DDS_FIND_SCOPE_GLOBAL, participant, typeinfo, DDS_SECS (10), descriptor)) < 0)
      {
        fprintf (stderr, "dds_create_topic_descriptor: %s\n", dds_strretcode (ret));
        dds_return_loan (triggered_reader, &epraw, 1);
        goto error;
      }
      dds_qset_data_representation (ep->qos, 0, NULL);
      if ((*topic = dds_create_topic (participant, *descriptor, ep->topic_name, ep->qos, NULL)) < 0)
      {
        fprintf (stderr, "dds_create_topic_descriptor: %s (be sure to enable topic discovery in the configuration)\n",
                 dds_strretcode (*topic));
        dds_delete_topic_descriptor (*descriptor);
        dds_return_loan (triggered_reader, &epraw, 1);
        goto error;
      }
    }
    dds_return_loan (triggered_reader, &epraw, 1);
  }

error:
  dds_delete (dcpspublication_reader);
  dds_delete (waitset);
  return (*descriptor != NULL) ? DDS_RETCODE_OK : DDS_RETCODE_TIMEOUT;
}

static bool prepare_mcap (const char *topic_name, const char *type_name, const char *idl_file, const dds_topic_descriptor_t *desc,
                          mcap::McapWriter &writer, mcap::Channel &channel)
{
  auto opts = mcap::McapWriterOptions ("");
  std::string outPath = std::string (topic_name) + ".mcap";
  // default chunking is fine; keep CRCs on for safety
  auto status = writer.open (outPath, opts);
  if (!status.ok ())
  {
    std::cerr << "Failed to open MCAP: " << status.message << "\n";
    return false;
  }

  // as it's not very easy to obtain idl text in runtime, we have to offer it manually...
  // if it's not presented, tools such as foxglove/lichtblick won't be able to use it.
  // but we still can replay with our dds_replayer.
  //
  // Note from mcap documents:
  // the idl text must be the text of a single, self-contained OMG IDL source file.
  // That is, all referenced type definitions must be present, and there must be no
  // preprocessor directives, i.e. #include "another.idl".

  mcap::Schema schema;
  schema.id = 0;
  if (idl_file != NULL && idl_file[0] != '\0' && type_name != NULL && type_name[0] != '\0')
  {
    std::vector<std::byte> idl_text;
    FILE *fp = fopen (idl_file, "rb");
    if (fp)
    {
      if (fseek (fp, 0, SEEK_END) == 0)
      {
        long len = ftell (fp);
        if (len > 0)
        {
          idl_text.resize (static_cast<size_t> (len));
          rewind (fp);
          size_t n = fread (idl_text.data (), 1, idl_text.size (), fp);
          idl_text.resize (n);
        }
      }
      fclose (fp);
    } else
    {
      std::cerr << "Warning: failed to open IDL file: " << idl_file << "\n";
      return false;
    }

    schema.name = type_name;
    schema.encoding = "omgidl";
    schema.data = std::move (idl_text);
    writer.addSchema (schema);
  }

  // fill channel(i.e. topic in mcap)
  channel.topic = topic_name;
  channel.messageEncoding = "cdr";
  channel.schemaId = schema.id;

  // we need to save the desc in the metadata of mcap file,
  // so we can use it to build the topic when we replay it.
  const size_t buffer_sz = dds_topic_descriptor_serialized_size (desc);
  void *buffer = (void *)malloc (buffer_sz);
  dds_topic_descriptor_serialize (desc, buffer, buffer_sz);
  channel.metadata.emplace ("topic_descriptor", std::string (reinterpret_cast<char *> (buffer), buffer_sz));
  free (buffer);
  writer.addChannel (channel);
  return true;
}

static bool write_to_mcap (const dds_entity_t reader, mcap::McapWriter *writer, mcap::Channel *channel)
{
  const size_t MAX_SAMPLE_SIZE = 10;

  struct ddsi_serdata *sd[MAX_SAMPLE_SIZE] = {NULL};
  dds_sample_info_t si[MAX_SAMPLE_SIZE];
  dds_return_t n = dds_takecdr (reader, sd, MAX_SAMPLE_SIZE, si, 0);
  if (n < 0)
  {
    fprintf (stderr, "dds_takecdr: %s\n", dds_strretcode (n));
    return false;
  }

  if (n != 0)
  {
    // save raw CDR into mcap
    for (int32_t i = 0; i < n; i++)
    {
      if (si[i].valid_data)
      {
        size_t size = ddsi_serdata_size (sd[i]);
        std::vector<std::byte> payload (size);
        ddsi_serdata_to_ser (sd[i], 0, size, payload.data ()); // Extract the data from the buffer
        ddsi_serdata_unref (sd[i]);

        // Build MCAP message
        mcap::Message msg;
        msg.channelId = channel->id;
        const uint64_t ts =
            (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds> (std::chrono::system_clock::now ().time_since_epoch ()).count ();
        msg.logTime = ts;
        msg.publishTime = si->source_timestamp;
        msg.data = payload.data ();
        msg.dataSize = payload.size ();

        const auto st = writer->write (msg);
        if (st.ok ())
        {
          printf ("one msg written to mcap..\n");
        } else
        {
          std::cerr << "MCAP write failed: " << st.message << "\n";
          return false;
        }
      }
    }
  }

  return true;
}
#if !DDSRT_WITH_FREERTOS && !__ZEPHYR__
static void signal_handler (int sig)
{
  (void)sig;
  dds_set_guardcondition (termcond, true);
}
#endif

#if !_WIN32 && !DDSRT_WITH_FREERTOS && !__ZEPHYR__
static uint32_t sigthread (void *varg)
{
  sigset_t *set = (sigset_t *)varg;
  int sig;
  if (sigwait (set, &sig) == 0)
    signal_handler (sig);
  return 0;
}
#endif

int main (int argc, char **argv)
{
  dds_return_t ret = 0;
  dds_entity_t topic = 0;
  const char *topic_name = NULL;
  const char *idl_file = NULL;
  const char *type_name = NULL;

  if (argc == 2)
  {
    topic_name = argv[1];
  } else if (argc == 6 && strcmp (argv[2], "-i") == 0 && strcmp (argv[4], "-t") == 0)
  {
    topic_name = argv[1];
    idl_file = argv[3];
    type_name = argv[5];
  } else
  {
    fprintf (stderr,
             "Usage:\n"
             "%s <topic_name> [-i <topic.idl> -t <type_name>]\n"
             "\n"
             "For example:\n"
             "./bin/recorder HelloWorldData_Msg -i ../examples/helloworld/HelloWorldData.idl -t HelloWorldData::Msg\n"
             "\n"
             "Note:\n"
             "  - To stop recording, just press ctrl+c.\n"
             "  - The IDL should contain the complete type definition of <type_name>.\n"
             "  - The IDL must not contain '#include' directives.\n"
             "  - if idl is not provided, the output mcap won't be able be visualized by tools like foxglove/lichtblick,\n"
             "    but it always can be replayed by our dds_replayer.\n",
             argv[0]);
    return 1;
  }

  participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  if (participant < 0)
  {
    fprintf (stderr, "dds_create_participant: %s\n", dds_strretcode (participant));
    return 1;
  }

  // get a topic and topic desc ...
  dds_topic_descriptor_t *desc;
  if ((ret = get_topic_and_desc (topic_name, DDS_SECS (10), &topic, &desc)) < 0)
  {
    fprintf (stderr, "get_topic_and_desc: %s\n", dds_strretcode (ret));
    dds_delete (participant);
    return 1;
  }
  printf ("found topic and desc..\n");

  // prepare mcap writer
  mcap::McapWriter writer;
  mcap::Channel channel;
  if (!prepare_mcap (topic_name, type_name, idl_file, desc, writer, channel))
  {
    fprintf (stderr, "prepare mcap failed\n");
    dds_delete (participant);
    return 1;
  }
  dds_delete_topic_descriptor (desc);
  printf ("mcap is ready\n");


  // ... given those, we can create a reader just like we do normally ...
  const dds_entity_t reader = dds_create_reader (participant, topic, NULL, NULL);
  // ... and create a waitset that allows us to wait for any incoming data ...
  const dds_entity_t waitset = dds_create_waitset (participant);
  const dds_entity_t readcond = dds_create_readcondition (reader, DDS_ANY_STATE);
  (void)dds_waitset_attach (waitset, readcond, 0);

  termcond = dds_create_guardcondition (participant);
  (void)dds_waitset_attach (waitset, termcond, 0);

#ifdef _WIN32
  signal (SIGINT, signal_handler);
#elif !DDSRT_WITH_FREERTOS && !__ZEPHYR__
  ddsrt_thread_t sigtid;
  sigset_t sigset, osigset;
  sigemptyset (&sigset);
#ifdef __APPLE__
  DDSRT_WARNING_GNUC_OFF (sign - conversion)
#endif
  sigaddset (&sigset, SIGHUP);
  sigaddset (&sigset, SIGINT);
  sigaddset (&sigset, SIGTERM);
#ifdef __APPLE__
  DDSRT_WARNING_GNUC_ON (sign - conversion)
#endif
  sigprocmask (SIG_BLOCK, &sigset, &osigset);
  {
    ddsrt_threadattr_t tattr;
    ddsrt_threadattr_init (&tattr);
    ddsrt_thread_create (&sigtid, "sigthread", &tattr, sigthread, &sigset);
  }
#endif

  bool termflag = false;
  while (!termflag)
  {
    (void)dds_waitset_wait (waitset, NULL, 0, DDS_INFINITY);
    dds_read_guardcondition (termcond, &termflag);

    if (!write_to_mcap (reader, &writer, &channel))
    {
      fprintf (stderr, "write_to_mcap failed, exiting..\n");
      break;
    }
  }

  if (termflag)
  {
    fprintf (stderr, "signal received, exiting..\n");
  }

#if _WIN32
  signal_handler (SIGINT);
#elif !DDSRT_WITH_FREERTOS && !__ZEPHYR__
  {
    /* get the attention of the signal handler thread */
    void (*osigint) (int);
    void (*osigterm) (int);
    kill (getpid (), SIGTERM);
    ddsrt_thread_join (sigtid, NULL);
    osigint = signal (SIGINT, SIG_IGN);
    osigterm = signal (SIGTERM, SIG_IGN);
    sigprocmask (SIG_SETMASK, &osigset, NULL);
    signal (SIGINT, osigint);
    signal (SIGINT, osigterm);
  }
#endif

  writer.close ();
  dds_delete (participant);
  return 0;
}
