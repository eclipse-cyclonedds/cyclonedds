// Copyright(c) 2019 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stdio.h>
#include "CUnit/Test.h"

#include "dds/dds.h"
#include "dds__entity.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "ddsi__xevent.h"

#include "dds/security/core/dds_security_timed_cb.h"
#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/heap.h"

#define SEQ_SIZE (16)

typedef enum test_event_kind {
  EVENT_ARMED,
  EVENT_TRIGGERED,
  EVENT_DELETED
} test_event_kind_t;

struct test_data
{
  test_event_kind_t kind;
  dds_time_t trigger_time;
  dds_security_time_event_handle_t timer;
  dds_time_t time;
};

struct test_sequence_data {
  uint32_t size;
  uint32_t index;
  struct test_data *expected;
  struct test_data *received;
};

struct timer_argument
{
  dds_security_time_event_handle_t id;
  struct test_sequence_data *seq;
};


static dds_entity_t pp = 0;
static struct ddsi_xeventq *xeventq = NULL;

static struct ddsi_xeventq *get_xeventq (dds_entity_t e)
{
  struct ddsi_xeventq *evq;
  dds_return_t r;
  dds_entity *x;

  r = dds_entity_pin (e, &x);
  CU_ASSERT_FATAL(r >= 0);
  evq = x->m_domain->gv.xevents;
  dds_entity_unpin (x);
  return evq;
}

static void setup(void)
{
  pp = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_FATAL(pp > 0);
  xeventq = get_xeventq(pp);
  CU_ASSERT_FATAL(xeventq != NULL);
}

static void teardown(void)
{
  dds_delete(pp);
}

static void simple_callback(dds_security_time_event_handle_t timer, dds_time_t trigger_time, dds_security_timed_cb_kind_t kind, void *arg)
{
  DDSRT_UNUSED_ARG(timer);
  DDSRT_UNUSED_ARG(kind);
  DDSRT_UNUSED_ARG(trigger_time);
  *((bool *)arg) = !(*((bool *)arg));
}

static int g_order_callback_idx = 0;
static void *g_order_callback[2] = {(void *)NULL, (void *)NULL};
static void order_callback(dds_security_time_event_handle_t timer, dds_time_t trigger_time, dds_security_timed_cb_kind_t kind, void *arg)
{
  DDSRT_UNUSED_ARG(timer);
  DDSRT_UNUSED_ARG(kind);
  DDSRT_UNUSED_ARG(trigger_time);
  g_order_callback[g_order_callback_idx] = arg;
  g_order_callback_idx++;
}

static void test_callback(dds_security_time_event_handle_t timer, dds_time_t trigger_time, dds_security_timed_cb_kind_t kind, void *arg)
{
  struct test_sequence_data *test_seq = arg;

  DDSRT_UNUSED_ARG(timer);

  printf("event %"PRIu64" triggered\n", timer);

  if (test_seq->index < test_seq->size)
  {
    test_seq->received[test_seq->index].trigger_time = trigger_time;
    test_seq->received[test_seq->index].timer = timer;
    test_seq->received[test_seq->index].kind = (kind == DDS_SECURITY_TIMED_CB_KIND_TIMEOUT) ? EVENT_TRIGGERED : EVENT_DELETED;
    test_seq->received[test_seq->index].time = dds_time();
  }
  test_seq->index++;
}

CU_Test(ddssec_timed_cb, simple_test, .init = setup, .fini = teardown)
{
  static bool test_var = false;
  dds_time_t future = dds_time() + DDS_SECS(2);
  struct dds_security_timed_dispatcher *d1 = dds_security_timed_dispatcher_new(xeventq);
  CU_ASSERT_PTR_NOT_NULL_FATAL(d1);
  dds_security_timed_dispatcher_add(d1, simple_callback, future, (void *)&test_var);
  dds_security_timed_dispatcher_enable(d1);
  CU_ASSERT_FALSE_FATAL(test_var);
  dds_sleepfor(DDS_MSECS(500));
  CU_ASSERT_FALSE_FATAL(test_var);
  dds_sleepfor(DDS_SECS(2));
  CU_ASSERT_TRUE_FATAL(test_var);
  dds_security_timed_dispatcher_free(d1);
}

CU_Test(ddssec_timed_cb, simple_order, .init = setup, .fini = teardown)
{
  struct dds_security_timed_dispatcher *d1 = dds_security_timed_dispatcher_new(xeventq);
  CU_ASSERT_PTR_NOT_NULL_FATAL(d1);
  dds_time_t future = dds_time() + DDS_MSECS(20), future2 = future;
  dds_security_timed_dispatcher_add(d1, order_callback, future, (void *)1);
  dds_security_timed_dispatcher_add(d1, order_callback, future2, (void *)2);
  dds_security_timed_dispatcher_enable(d1);
  dds_sleepfor(DDS_MSECS(10));
  dds_security_timed_dispatcher_free(d1);
  CU_ASSERT_EQUAL_FATAL(g_order_callback[0], (void *)1);
  CU_ASSERT_EQUAL_FATAL(g_order_callback[1], (void *)2);
}

CU_Test(ddssec_timed_cb, test_enabled_and_disabled, .init = setup, .fini = teardown)
{
  static bool test_var = false;
  dds_time_t future = dds_time() + DDS_SECS(2);
  struct dds_security_timed_dispatcher *d1 = dds_security_timed_dispatcher_new(xeventq);
  CU_ASSERT_PTR_NOT_NULL_FATAL(d1);
  dds_security_timed_dispatcher_add(d1, simple_callback, future, (void *)&test_var);
  dds_security_timed_dispatcher_enable(d1);
  CU_ASSERT_FALSE(test_var);
  (void) dds_security_timed_dispatcher_disable(d1);
  dds_sleepfor(DDS_MSECS(500));
  CU_ASSERT_FALSE(test_var);
  dds_sleepfor(DDS_SECS(2));
  CU_ASSERT_FALSE(test_var);
  dds_security_timed_dispatcher_free(d1);
}

CU_Test(ddssec_timed_cb, simple_test_with_future, .init = setup, .fini = teardown)
{
  static bool test_var = false;
  dds_time_t now = dds_time(), future = now + DDS_SECS(2), far_future = now + DDS_SECS(10);
  struct dds_security_timed_dispatcher *d1 = dds_security_timed_dispatcher_new(xeventq);
  CU_ASSERT_PTR_NOT_NULL_FATAL(d1);
  dds_security_timed_dispatcher_enable(d1);
  dds_security_timed_dispatcher_add(d1, simple_callback, future, (void *)&test_var);
  dds_security_timed_dispatcher_add(d1, simple_callback, far_future, (void *)&test_var);
  CU_ASSERT_FALSE_FATAL(test_var);
  dds_sleepfor(DDS_MSECS(500));
  CU_ASSERT_FALSE_FATAL(test_var);
  dds_sleepfor(DDS_SECS(2));
  CU_ASSERT_TRUE_FATAL(test_var);
  dds_security_timed_dispatcher_free(d1);
}

CU_Test(ddssec_timed_cb, test_multiple_dispatchers, .init = setup, .fini = teardown)
{
  static bool test_var = false;
  dds_time_t now = dds_time(), future = now + DDS_SECS(2), far_future = now + DDS_SECS(10);
  struct dds_security_timed_dispatcher *d1 = dds_security_timed_dispatcher_new(xeventq);
  struct dds_security_timed_dispatcher *d2 = dds_security_timed_dispatcher_new(xeventq);
  CU_ASSERT_PTR_NOT_NULL_FATAL(d1);
  dds_security_timed_dispatcher_enable(d1);
  dds_security_timed_dispatcher_enable(d2);
  dds_security_timed_dispatcher_free(d2);
  dds_security_timed_dispatcher_add(d1, simple_callback, future, (void *)&test_var);
  dds_security_timed_dispatcher_add(d1, simple_callback, far_future, (void *)&test_var);
  CU_ASSERT_FALSE_FATAL(test_var);
  dds_sleepfor(DDS_MSECS(500));
  CU_ASSERT_FALSE_FATAL(test_var);
  dds_sleepfor(DDS_SECS(2));
  CU_ASSERT_TRUE_FATAL(test_var);
  dds_security_timed_dispatcher_free(d1);
}

CU_Test(ddssec_timed_cb, test_create_dispatcher, .init = setup, .fini = teardown)
{
#define NUM_TIMERS 5
  dds_time_t now = dds_time();
  dds_time_t past = now - DDS_SECS(1);
  dds_time_t present = now;
  dds_time_t future1 = now + DDS_MSECS(500);
  dds_time_t future2 = now + DDS_SECS(1);
  dds_time_t future3 = now + DDS_SECS(10);
  struct {
    dds_time_t expire;
    uint32_t rank;
    test_event_kind_t trigger_kind;
  } timers[NUM_TIMERS] = {
      { future2, 3, EVENT_TRIGGERED },
      { present, 1, EVENT_TRIGGERED },
      { past,    0, EVENT_TRIGGERED },
      { future3, 4, EVENT_ARMED     },
      { future1, 2, EVENT_TRIGGERED }
  };
  struct dds_security_timed_dispatcher *d = NULL;
  struct test_data expected[NUM_TIMERS];
  struct test_data received[NUM_TIMERS];
  struct test_sequence_data test_seq_data = { .size = NUM_TIMERS, .index = 0, .expected = expected, .received = received };
  uint32_t i;

  d = dds_security_timed_dispatcher_new(xeventq);
  CU_ASSERT_PTR_NOT_NULL_FATAL(d);

  memset(received, 0, NUM_TIMERS * sizeof(struct test_data));

  for (i = 0; i < NUM_TIMERS; i++)
  {
    uint32_t rank = timers[i].rank;
    expected[rank].kind = timers[i].trigger_kind;
    expected[rank].trigger_time = timers[i].expire;
    expected[rank].timer = dds_security_timed_dispatcher_add(d, test_callback, expected[rank].trigger_time, (void *)&test_seq_data);
  }

  /* The sleeps are added to get the timing between 'present' and 'past' callbacks right. */
  printf("before enable\n");
  dds_sleepfor(DDS_MSECS(300));
  dds_security_timed_dispatcher_enable(d);
  dds_sleepfor(DDS_MSECS(900));
  (void) dds_security_timed_dispatcher_disable(d);

  if (test_seq_data.index >= test_seq_data.size)
  {
    printf("Unexpected number of triggers: %u vs %u\n", test_seq_data.index, test_seq_data.size);
    CU_FAIL_FATAL("Unexpected number of triggers");
  }

  for (i = 0; i < NUM_TIMERS; i++)
  {
    if (expected[i].kind != received[i].kind)
    {
      printf("Unexpected kind at %u: %d vs %d\n", i, received[i].kind, expected[i].kind);
      CU_FAIL_FATAL("Unexpected kind");
    }
    if (expected[i].kind == EVENT_TRIGGERED)
    {
      if (expected[i].timer != received[i].timer)
      {
        printf("Unexpected ordering at %u: %"PRIu64" vs %"PRIu64"\n", i, received[i].timer, expected[i].timer);
        CU_FAIL_FATAL("Unexpected ordering");
      }
      if (expected[i].trigger_time != received[i].trigger_time)
      {
        printf("Unexpected trigger_time at %u: %"PRId64" vs %"PRId64"\n", i, received[i].trigger_time, expected[i].trigger_time);
        CU_FAIL_FATAL("Unexpected trigger_time");
      }
    }
  }

  printf("before disable\n");
  dds_security_timed_dispatcher_free(d);
  dds_sleepfor(DDS_MSECS(200));

  i = NUM_TIMERS-1;
  expected[i].kind = EVENT_DELETED;

  if (expected[i].kind != received[i].kind)
  {
    printf("Unexpected kind at %u: %d vs %d\n", i, received[i].kind, expected[i].kind);
    CU_FAIL_FATAL("Unexpected kind");
  }
  if (expected[i].kind == EVENT_DELETED)
  {
    if (expected[i].timer != received[i].timer)
    {
      printf("Unexpected ordering at %u: %"PRIu64" vs %"PRIu64"\n", i, received[i].timer, expected[i].timer);
      CU_FAIL_FATAL("Unexpected ordering");
    }
    if (expected[i].trigger_time != received[i].trigger_time)
    {
      printf("Unexpected trigger_time at %u: %"PRId64" vs %"PRId64"\n", i, received[i].trigger_time, expected[i].trigger_time);
      CU_FAIL_FATAL("Unexpected trigger_time");
    }
  }
#undef NUM_TIMERS
}

CU_Test(ddssec_timed_cb, test_remove_timer, .init = setup, .fini = teardown)
{
#define NUM_TIMERS 5
  dds_time_t now = dds_time();
  dds_time_t t1 = now + DDS_SECS(1);
  struct {
    dds_time_t expire;
    uint32_t rank;
    test_event_kind_t trigger_kind;
  } timers[NUM_TIMERS] = {
      { t1,                 2, EVENT_TRIGGERED },
      { t1 + DDS_MSECS(10), 0, EVENT_DELETED   },
      { t1 + DDS_MSECS(20), 3, EVENT_TRIGGERED },
      { t1 + DDS_MSECS(30), 1, EVENT_DELETED   },
      { t1 + DDS_MSECS(40), 4, EVENT_TRIGGERED }
  };
  struct dds_security_timed_dispatcher *d = NULL;
  struct test_data expected[NUM_TIMERS];
  struct test_data received[NUM_TIMERS];
  struct test_sequence_data test_seq_data = { .size = NUM_TIMERS, .index = 0, .expected = expected, .received = received };
  uint32_t i;

  d = dds_security_timed_dispatcher_new(xeventq);
  CU_ASSERT_PTR_NOT_NULL_FATAL(d);

  memset(received, 0, NUM_TIMERS * sizeof(struct test_data));

  for (i = 0; i < NUM_TIMERS; i++)
  {
    uint32_t rank = timers[i].rank;
    expected[rank].kind = timers[i].trigger_kind;
    expected[rank].trigger_time = timers[i].expire;
    expected[rank].timer = dds_security_timed_dispatcher_add(d, test_callback, expected[rank].trigger_time, (void *)&test_seq_data);
  }

  dds_security_timed_dispatcher_enable(d);
  dds_sleepfor(DDS_MSECS(500));

  dds_security_timed_dispatcher_remove(d, expected[0].timer);
  dds_security_timed_dispatcher_remove(d, expected[1].timer);

  dds_sleepfor(DDS_SECS(1));

  for (i = 0; i < NUM_TIMERS; i++)
  {
    if (expected[i].kind != received[i].kind)
    {
      printf("Unexpected kind at %u: %d vs %d\n", i, received[i].kind, expected[i].kind);
      CU_FAIL_FATAL("Unexpected kind");
    }
    if (expected[i].kind == EVENT_TRIGGERED)
    {
      if (expected[i].timer != received[i].timer)
      {
        printf("Unexpected ordering at %u: %"PRIu64" vs %"PRIu64"\n", i, received[i].timer, expected[i].timer);
        CU_FAIL_FATAL("Unexpected ordering");
      }
      if (expected[i].trigger_time != received[i].trigger_time)
      {
        printf("Unexpected trigger_time at %u: %"PRId64" vs %"PRId64"\n", i, received[i].trigger_time, expected[i].trigger_time);
        CU_FAIL_FATAL("Unexpected trigger_time");
      }
    }
  }

  dds_security_timed_dispatcher_free(d);

#undef NUM_TIMERS
}
