// Copyright(c) 2006 to 2019 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stdio.h>

#include "CUnit/Theory.h"
#include "RoundTrip.h"
#include "Space.h"
#include "test_oneliner.h"
#include "test_util.h"

#include "dds/dds.h"
#include "dds/ddsrt/io.h"
#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/heap.h"

/* Tests in this file only concern themselves with very basic api tests of
   dds_write and dds_write_ts */

static const uint32_t payloadSize = 32;
static RoundTripModule_DataType data;

static dds_entity_t participant = 0;
static dds_entity_t topic = 0;
static dds_entity_t publisher = 0;
static dds_entity_t writer = 0;

static void
setup(void)
{
    participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    CU_ASSERT_GT_FATAL (participant, 0);
    char topicname[100];
    create_unique_topic_name ("RoundTrip", topicname, sizeof (topicname));
    topic = dds_create_topic(participant, &RoundTripModule_DataType_desc, topicname, NULL, NULL);
    CU_ASSERT_GT_FATAL (topic, 0);
    publisher = dds_create_publisher(participant, NULL, NULL);
    CU_ASSERT_GT_FATAL (publisher, 0);
    writer = dds_create_writer(participant, topic, NULL, NULL);
    CU_ASSERT_GT_FATAL (writer, 0);

    memset(&data, 0, sizeof(data));
    data.payload._length = payloadSize;
    data.payload._buffer = dds_alloc (payloadSize);
    memset(data.payload._buffer, 'a', payloadSize);
    data.payload._release = true;
    data.payload._maximum = 0;
}

static void
teardown(void)
{
    RoundTripModule_DataType_free (&data, DDS_FREE_CONTENTS);
    memset(&data, 0, sizeof(data));

    dds_delete(writer);
    dds_delete(publisher);
    dds_delete(topic);
    dds_delete(participant);
}

CU_Test(ddsc_write, basic, .init = setup, .fini = teardown)
{
    dds_return_t status;

    status = dds_write(writer, &data);
    CU_ASSERT_EQ_FATAL (status, DDS_RETCODE_OK);
}

CU_Test(ddsc_write, null_writer, .init = setup, .fini = teardown)
{
    dds_return_t status;

    /* Disable warning related to improper API usage by passing incompatible parameter. */
    DDSRT_WARNING_MSVC_OFF(28020);
    status = dds_write(0, &data);
    DDSRT_WARNING_MSVC_ON(28020);
    CU_ASSERT_EQ_FATAL (status, DDS_RETCODE_BAD_PARAMETER);
}

CU_Test(ddsc_write, bad_writer, .init = setup, .fini = teardown)
{
    dds_return_t status;

    status = dds_write(publisher, &data);
    CU_ASSERT_EQ_FATAL (status, DDS_RETCODE_ILLEGAL_OPERATION);
}

CU_Test(ddsc_write, closed_writer, .init = setup, .fini = teardown)
{
    dds_return_t status;

    status = dds_delete(writer);
    CU_ASSERT_EQ_FATAL (status, DDS_RETCODE_OK);
    status = dds_write(writer, &data);
    writer = 0;
    CU_ASSERT_EQ_FATAL (status, DDS_RETCODE_BAD_PARAMETER);
}

CU_Test(ddsc_write, null_sample, .init = setup, .fini = teardown)
{
    dds_return_t status;

    /* Disable warning related to improper API usage by passing NULL to a non-NULL parameter. */
    DDSRT_WARNING_MSVC_OFF(6387);
    status = dds_write(writer, NULL);
    DDSRT_WARNING_MSVC_ON(6387);

    CU_ASSERT_EQ_FATAL (status, DDS_RETCODE_BAD_PARAMETER);
}

CU_Test(ddsc_write_ts, basic, .init = setup, .fini = teardown)
{
    dds_return_t status;

    status = dds_write_ts(writer, &data, dds_time());
    CU_ASSERT_EQ_FATAL (status, DDS_RETCODE_OK);
}

CU_Test(ddsc_write_ts, bad_timestamp, .init = setup, .fini = teardown)
{
    dds_return_t status;

    status = dds_write_ts(writer, &data, -1);
    CU_ASSERT_EQ_FATAL (status, DDS_RETCODE_BAD_PARAMETER);
}

CU_Test(ddsc_write, simpletypes)
{
    dds_return_t status;
    dds_entity_t par, top, wri;
    const Space_simpletypes st_data = {
        .l = -1,
        .ll = -1,
        .us = 1,
        .ul = 1,
        .ull = 1,
        .f = 1.0f,
        .d = 1.0f,
        .c = '1',
        .b = true,
        .o = 1,
        .s = "This string is exactly so long that it would previously trigger CHAM-405. If this string is shortened exactly one character, all is well. Since it is fixed now, there doesn't need to be any further investigation."
    };

    par = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    CU_ASSERT_GT_FATAL (par, 0);
    char topicname[100];
    create_unique_topic_name ("RoundTrip", topicname, sizeof (topicname));
    top = dds_create_topic(par, &Space_simpletypes_desc, topicname, NULL, NULL);
    CU_ASSERT_GT_FATAL (top, 0);
    wri = dds_create_writer(par, top, NULL, NULL);
    CU_ASSERT_GT_FATAL (wri, 0);

    status = dds_write(wri, &st_data);
    CU_ASSERT_EQ_FATAL (status, DDS_RETCODE_OK);

    dds_delete(wri);
    dds_delete(top);
    dds_delete(par);
}

CU_Test(ddsc_write, invalid_data)
{
    const union { Space_invalid_data_enum ide; int i; } invalid_data_enum0 = { .i = 0 };
    const union { Space_invalid_data_enum ide; int i; } invalid_data_enum1 = { .i = 1 };
    const struct { Space_invalid_data x; bool invalidkey; } tests[] = {
        {
            .x = { .o1={._length=2, ._buffer=(uint8_t[]){1,2}}, .e1=invalid_data_enum0.ide, .bm1=0, .exto=&(uint8_t){0} },
            .invalidkey = false
        },
        {
            .x = { .o1={._length=1, ._buffer=NULL}, .e1=invalid_data_enum0.ide, .bm1=0, .exto=&(uint8_t){0} },
            .invalidkey = false
        },
        {
            .x = { .o1={._length=1, ._buffer=(uint8_t[]){1}}, .e1=invalid_data_enum1.ide, .bm1=0, .exto=&(uint8_t){0} },
            .invalidkey = true
        },
        {
            .x = { .o1 = {._length=1, ._buffer=(uint8_t[]){1}}, .e1=invalid_data_enum0.ide, .bm1=2, .exto=&(uint8_t){0} },
            .invalidkey = true
        },
        {
            .x = { .o1={._length=1, ._buffer=(uint8_t[]){1}}, .e1=invalid_data_enum0.ide, .bm1=0, .exto=NULL },
            .invalidkey = true
        },
        {
            .x = { .o1={._length=1, ._buffer=(uint8_t[]){1}}, .e1=invalid_data_enum0.ide, .bm1=0, .exto=NULL },
            .invalidkey = true
        }
    };
    dds_return_t status;
    dds_entity_t par, top, wri;

    par = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    CU_ASSERT_GT_FATAL (par, 0);
    char topicname[100];
    create_unique_topic_name ("RoundTrip", topicname, sizeof (topicname));
    top = dds_create_topic(par, &Space_invalid_data_desc, topicname, NULL, NULL);
    CU_ASSERT_GT_FATAL (top, 0);
    wri = dds_create_writer(par, top, NULL, NULL);
    CU_ASSERT_GT_FATAL (wri, 0);

    for (size_t i = 0; i < sizeof (tests) / sizeof (tests[0]); i++)
    {
        status = dds_write(wri, &tests[i].x);
        CU_ASSERT_EQ_FATAL (status, DDS_RETCODE_BAD_PARAMETER);

        if (tests[i].invalidkey)
        {
            status = dds_dispose(wri, &tests[i].x);
            CU_ASSERT_EQ_FATAL (status, DDS_RETCODE_BAD_PARAMETER);
        }
    }

    dds_delete(wri);
    dds_delete(top);
    dds_delete(par);
}

CU_Test(ddsc_write, relwr_unrelrd_network)
{
  // Avoid shared memory because we need the debugging tricks in DDSI
  // Use a special port number to reduce interference from other tests, because
  // we depend on best-effort data making it through
  const char *config_override =
  "<Domain id=\"any\">"
  "  <Discovery><Ports><Base>7350</Base></Ports></Discovery>"
  "</Domain>";

  // Relying on unreliable communication leads to a really flaky test, so
  // try a few times
  int result = 0;
  for (int attempt = 0; result <= 0 && attempt < 10; attempt++)
  {
    // We don't now why it failed if it failed, so let's sleep a while before
    // trying again
    if (attempt > 0)
      dds_sleepfor (DDS_MSECS (100));

    result = test_oneliner_with_config
      ("pm w(r=r) "
       "dor R' " // subscriber with data-on-readers
       "sm r'(r=be) "
       "?pm w ?sm r' " // mutual discovery complete
       "wr w 0 ?dor R' " // data should arrive (likelihood of loss in practice is low here)
       "s'(r=be) " // create 2nd reader, no need to worry about discovery because of w+r'
       "setflags(d) w wr w 1 wr w 2 setflags() w " // lose some data on the network
       "wr w 3 ?dor(2) R' " // 4th sample should arrive (like 1st), both readers should trigger
       "take{(0,0,0),(3,0,0)} r' take{(3,0,0)} s'", // r' should now have 2 samples, s' one
       config_override);
  }

  // It really should have succeeded after several attempts
  CU_ASSERT_GT_FATAL (result, 0);
}

CU_Test(ddsc_write, batch_flush)
{
  static const struct { const char *flush; const char *exp; } x[] = {
    { "flush w", "(0,0,0)" },
    { "flush x", "(1,0,0)" },
    { "flush w flush x", "(0,0,0),(1,0,0)" },
    { "flush W", "(0,0,0),(1,0,0)" },
    { "flush P", "(0,0,0),(1,0,0)" }
  };
  // oneliner doesn't do multiple publishers for a single participant
  // but the iteration logic is the same regardless of the type of
  // entity, so if it handles multiple writers in a publisher, it should
  // equally handle multiple publishers in a participant if it handles
  // one
  for (size_t i = 0; i < sizeof (x) / sizeof (x[0]); i++)
  {
    char *prog = NULL;
    ddsrt_asprintf (&prog,
      "pm w(r=r,wb=y) " // writer with batching
      "sm da r(r=r) "   // local reader (not subject to batching)
      "?pm w "          // consume match event so we can rely ...
      "sm da r'(r=r) "  // remote reader (subject to batching)
      "?pm w ?sm r' "   // ... on it here to wait for mutual discovery
      "pm x(r=r,wb=y) " // second writer with batching
      // ?pm x is tricky because the publication matched event fires
      // for both w and x and "oneliner" isn't too smart about such
      // cases.  Fortunately, w has discovered r', therefore x will
      // have been matched during creation of the writer and we only
      // need to worry about r' discovering x if we want to avoid
      // risking r' initially dropping data from x
      "?sm r' ?ack w ?ack x "
      // The heartbeat/flushing logic ordinarily forces a flushes when
      // piggy-backing a heartbeat if "enough" time has passed since
      // the previous write, but that makes this test code far too
      // sensitive to timing.  Set a flag that suppresses this
      "setflags(s) w setflags(s) x "
      // The data should now not get pushed out to the network
      // except by an explicit flush.  The local reader gets it
      // immediately but the remote reader doesn't.
      //
      // 200ms sleep to cover latency of getting the data to the
      // remote reader.  That should be enough most of the time
      // (it even allows for a heartbeat/nack/retransmit).  If it
      // isn't the test will incorrectly pass.
      "wr w 0 wr x 1 "
      "  take{(0,0,0),(1,0,0)} r "
      "  sleep 0.2 take{} r' "
      // flush one or both writers: now the data should arrive in
      // in the remote reader (but not in the local one a second
      // time)
      "%s "
      "  take{}r "
      "  take!{%s} r'",
      x[i].flush, x[i].exp);
    int result = test_oneliner_no_shm (prog);
    ddsrt_free (prog);
    CU_ASSERT_GT_FATAL (result, 0);
  }
}

CU_Test(ddsc_write, async_one_unrel_sample)
{
  // Avoid shared memory because we need the debugging tricks in DDSI
  // Use a special port number to reduce interference from other tests, because
  // we depend on best-effort data making it through
  const char *config_override =
  "<Domain id=\"any\">"
  "  <Discovery><Ports><Base>7350</Base></Ports></Discovery>"
  "</Domain>";

  // Relying on unreliable communication leads to a really flaky test, so
  // try a few times
  int result = 0;
  for (int attempt = 0; result <= 0 && attempt < 10; attempt++)
  {
    // We don't now why it failed if it failed, so let's sleep a while before
    // trying again
    if (attempt > 0)
      dds_sleepfor (DDS_MSECS (100));
    result = test_oneliner_with_config
      ("pm w(lb=0.01,r=be) "
       "sm r'(lb=0.01,r=be) "
       "?pm w ?sm r' "
       "wr w (3,4,5) " // exactly 1 packet ever into the queue must arrive
       "take!{(3,4,5)} r'", // (except it is best-effort ...)
       config_override);
  }

  // It really should have succeeded after several attempts
  CU_ASSERT_GT_FATAL (result, 0);
}

CU_Test(ddsc_write, old_sample_registration)
{
  int result = test_oneliner
    ("w(do=s,ad=n) x(do=s,ad=n) "
     "r(do=s,h=all) "
     // write/unregister with source timestamp T using writer with
     // lowest GUID
     "wr w 1@1 unreg w 1@1 "
     // write sample with source timestamp T using writer with highest
     // GUID => this gets rejected by the instance to guarantee eventual
     // consistency with by-source destination order. The writer
     // does get registered
     "wr x 1@1 "
     "read{fan(1,0,0)w@1} r "
     // second rejected write should not cause live writer count to be
     // incremented to 2
     "wr x 1@1 "
     // unregister decrements by 1
     "unreg x 1@1 "
     // so after this we must see a not-alive-no-writers instance; if the
     // second rejected write incremented the live writer count to 2, we
     // would have received an alive instance
     "read{suo(1,0,0)w@1,fuo1x@1#u1} r");
  CU_ASSERT_GT_FATAL (result, 0);
}

CU_Test(ddsc_write, old_sample_ownership)
{
  // like above test "ddsc_write, old_sample_registration"
  // verify that the registration without accepting a sample
  // by the higher strength writer does not claim ownership
  int result = test_oneliner
    ("w(do=s,ad=n,o=x:1) x(do=s,ad=n,o=x:2) "
     "r(do=s,o=x,h=1) "
     "wr w 1@1 unreg w 1@1 "
     "wr x 1@1 "
     "wr w 1@2 "
     "take{fan(1,0,0)w@2#u1} r "
     "wr x 1@3 "
     "take{fao(1,0,0)x@3#u1} r");
  CU_ASSERT_GT_FATAL (result, 0);
}
