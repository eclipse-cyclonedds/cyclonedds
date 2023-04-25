// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>
#include <limits.h>

#include "dds/dds.h"
#include "dds/ddsrt/cdtors.h"
#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/process.h"
#include "dds/ddsrt/threads.h"
#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/time.h"

#include "test_common.h"

#define MAX_ENTITIES_CNT (10)

typedef enum thread_state_t {
  STARTING,
  WAITING,
  STOPPED
} thread_state_t;

typedef struct thread_arg_t {
    ddsrt_thread_t tid;
    ddsrt_atomic_uint32_t state;
    dds_entity_t expected;
} thread_arg_t;

static void waiting_thread_start(struct thread_arg_t *arg, dds_entity_t expected);
static dds_return_t waiting_thread_expect_exit(struct thread_arg_t *arg);

static dds_entity_t participant, topic, writer, reader, waitset, publisher, subscriber, readcond;

static void ddsc_waitset_basic_init (void)
{
  ddsrt_init ();
  participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_FATAL (participant > 0);
  waitset = dds_create_waitset (participant);
  CU_ASSERT_FATAL (waitset > 0);
}

static void ddsc_waitset_basic_fini (void)
{
  (void) dds_delete (waitset);
  (void) dds_delete (participant);
  ddsrt_fini ();
}

static void ddsc_waitset_init (void)
{
  uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
  char name[100];
  ddsc_waitset_basic_init ();
  publisher = dds_create_publisher (participant, NULL, NULL);
  CU_ASSERT_FATAL (publisher > 0);
  subscriber = dds_create_subscriber (participant, NULL, NULL);
  CU_ASSERT_FATAL (subscriber >  0);
  topic = dds_create_topic (participant, &RoundTripModule_DataType_desc, create_unique_topic_name ("ddsc_waitset_test", name, sizeof name), NULL, NULL);
  CU_ASSERT_FATAL (topic >  0);
  reader = dds_create_reader (subscriber, topic, NULL, NULL);
  CU_ASSERT_FATAL (reader >  0);
  writer = dds_create_writer (publisher, topic, NULL, NULL);
  CU_ASSERT_FATAL (writer >  0);
  readcond = dds_create_readcondition (reader, mask);
  CU_ASSERT_FATAL (readcond >  0);
}

static void ddsc_waitset_fini (void)
{
  ddsc_waitset_basic_fini ();
}

static void ddsc_waitset_attached_init (void)
{
  dds_return_t ret;
  ddsc_waitset_init ();
  dds_entity_t es[] = { participant, topic, writer, reader, waitset, publisher, subscriber, 0 };
  for (int i = 0; es[i]; i++)
  {
    // waitset doesn't have a status mask, perhaps that ought to be changed
    if (es[i] != waitset)
    {
      ret = dds_set_status_mask (es[i], 0);
      CU_ASSERT_FATAL (ret == 0);
    }
  }
  for (int i = 0; es[i]; i++)
  {
    ret = dds_waitset_attach (waitset, es[i], es[i]);
    CU_ASSERT_FATAL (ret == 0);
  }
}

static void ddsc_waitset_attached_fini (void)
{
  ddsc_waitset_fini ();
}

CU_Test(ddsc_waitset_create, second, .init=ddsc_waitset_basic_init, .fini=ddsc_waitset_basic_fini)
{
  dds_entity_t ws;
  dds_return_t ret;
  /* Basically, ddsc_waitset_basic_init() already tested the creation of a waitset. But
   * just see if we can create a second one and delete the waitsets. */
  ws = dds_create_waitset (participant);
  CU_ASSERT_FATAL (ws > 0);
  ret = dds_delete (ws);
  CU_ASSERT_FATAL (ret == 0);
  ret = dds_delete (waitset);
  CU_ASSERT_FATAL (ret == 0);
}

CU_Test(ddsc_waitset_create, deleted_participant, .init=ddsc_waitset_basic_init, .fini=ddsc_waitset_basic_fini)
{
  dds_entity_t ws;
  dds_entity_t deleted;
  deleted = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  (void) dds_delete (deleted);
  ws = dds_create_waitset (deleted);
  CU_ASSERT_FATAL(ws == DDS_RETCODE_BAD_PARAMETER);
}

CU_TheoryDataPoints(ddsc_waitset_create, invalid_params) = {
  CU_DataPoints(dds_entity_t, -2, -1, 0, INT_MAX, INT_MIN),
};
CU_Theory((dds_entity_t par), ddsc_waitset_create, invalid_params, .init=ddsc_waitset_basic_init, .fini=ddsc_waitset_basic_fini)
{
  dds_entity_t ws = dds_create_waitset (par);
  CU_ASSERT_FATAL (ws == DDS_RETCODE_BAD_PARAMETER);
}

CU_TheoryDataPoints(ddsc_waitset_create, non_participants) = {
  CU_DataPoints(dds_entity_t*, &topic, &writer, &reader, &waitset, &publisher, &subscriber, &readcond),
};
CU_Theory((dds_entity_t *par), ddsc_waitset_create, non_participants, .init=ddsc_waitset_init, .fini=ddsc_waitset_fini)
{
  dds_entity_t ws = dds_create_waitset (*par);
  CU_ASSERT_FATAL (ws == DDS_RETCODE_ILLEGAL_OPERATION);
}

CU_Test (ddsc_waitset_create, domain)
{
  dds_entity_t par, dom, ws;
  dds_return_t rc;
  par = dds_create_participant (0, NULL, NULL);
  CU_ASSERT_FATAL (par > 0);
  dom = dds_get_parent (par);
  CU_ASSERT_FATAL (dom > 0);
  ws = dds_create_waitset (dom);
  CU_ASSERT_FATAL (ws > 0);
  rc = dds_delete (dom);
  CU_ASSERT_FATAL (rc == 0);
}

CU_Test (ddsc_waitset_create, cyclonedds)
{
  dds_entity_t ws;
  dds_return_t rc;
  /* Expect an uninitialised library */
  rc = dds_get_parent (DDS_CYCLONEDDS_HANDLE);
  CU_ASSERT_FATAL (rc == DDS_RETCODE_PRECONDITION_NOT_MET);
  ws = dds_create_waitset (DDS_CYCLONEDDS_HANDLE);
  CU_ASSERT_FATAL (ws > 0);
  rc = dds_delete (DDS_CYCLONEDDS_HANDLE);
  CU_ASSERT_FATAL (rc == 0);
  /* And the same afterward */
  rc = dds_get_parent (DDS_CYCLONEDDS_HANDLE);
  CU_ASSERT_FATAL (rc == DDS_RETCODE_PRECONDITION_NOT_MET);
}

CU_TheoryDataPoints(ddsc_waitset_attach, invalid_params) = {
  CU_DataPoints(dds_entity_t, -2, -1, 0, INT_MAX, INT_MIN),
  CU_DataPoints(dds_attach_t,  (dds_attach_t)NULL, (dds_attach_t)&reader, (dds_attach_t)3, (dds_attach_t)0, (dds_attach_t)0, (dds_attach_t)0, (dds_attach_t)0),
};
CU_Theory((dds_entity_t e, dds_attach_t a), ddsc_waitset_attach, invalid_params, .init=ddsc_waitset_basic_init, .fini=ddsc_waitset_basic_fini)
{
  dds_return_t ret = dds_waitset_attach (waitset, e, a);
  CU_ASSERT_FATAL (ret == DDS_RETCODE_BAD_PARAMETER);
}

CU_TheoryDataPoints(ddsc_waitset_attach, invalid_waitsets) = {
        CU_DataPoints(dds_entity_t, -2, -1, 0, INT_MAX, INT_MIN),
        CU_DataPoints(dds_attach_t,  (dds_attach_t)NULL, (dds_attach_t)&reader, (dds_attach_t)3, (dds_attach_t)0, (dds_attach_t)0, (dds_attach_t)0, (dds_attach_t)0),
};
CU_Theory((dds_entity_t ws, dds_attach_t a), ddsc_waitset_attach, invalid_waitsets, .init=ddsc_waitset_basic_init, .fini=ddsc_waitset_basic_fini)
{
  dds_return_t ret = dds_waitset_attach (ws, participant, a);
  CU_ASSERT_FATAL (ret == DDS_RETCODE_BAD_PARAMETER);
}

CU_TheoryDataPoints(ddsc_waitset_attach, non_waitsets) = {
  CU_DataPoints(dds_entity_t*, &participant, &topic, &writer, &reader,           &publisher, &subscriber, &readcond),
  CU_DataPoints(dds_entity_t*, &participant, &topic, &writer, &reader, &waitset, &publisher, &subscriber, &readcond),
  CU_DataPoints(dds_attach_t,  (dds_attach_t)NULL, (dds_attach_t)&reader, (dds_attach_t)3, (dds_attach_t)0, (dds_attach_t)0, (dds_attach_t)0, (dds_attach_t)0),
};
CU_Theory((dds_entity_t *ws, dds_entity_t *e, dds_attach_t a), ddsc_waitset_attach, non_waitsets, .init=ddsc_waitset_init, .fini=ddsc_waitset_fini)
{
  dds_return_t ret = dds_waitset_attach (*ws, *e, a);
  CU_ASSERT_FATAL (ret == DDS_RETCODE_ILLEGAL_OPERATION);
}

CU_Test(ddsc_waitset_attach, deleted_waitset, .init=ddsc_waitset_basic_init, .fini=ddsc_waitset_basic_fini)
{
  dds_delete(waitset);
  dds_return_t ret = dds_waitset_attach(waitset, participant, 0);
  CU_ASSERT_FATAL (ret == DDS_RETCODE_BAD_PARAMETER);
}

CU_TheoryDataPoints(ddsc_waitset_attach, scoping) = {
  CU_DataPoints (int, -9, -1, -2,  0,  0,  2), /* owner: -9: lib, -1: dom0, -2: dom1 */
  CU_DataPoints (int,  0,  0,  2,  0,  0,  2), /* ok1: participant one can attach */
  CU_DataPoints (int,  3,  1,  3, -1, -1, -1), /* ok2: other participant one can attach, or -1 */
  CU_DataPoints (int, -1,  2,  0,  1,  2,  0), /* fail: participant that one cannot attach, or -1 */
};
CU_Theory ((int owner, int ok1, int ok2, int fail), ddsc_waitset_attach, scoping)
{
  dds_entity_t par[4], dom[2], ws, ownh;
  dds_return_t rc;
  for (int i = 0; i < 2; i++) {
    for (int j = 0; j < 2; j++) {
      par[2*i+j] = dds_create_participant ((dds_domainid_t) i, NULL, NULL);
      CU_ASSERT_FATAL (par[2*i+j] > 0);
    }
    dom[i] = dds_get_parent (par[2*i]);
    CU_ASSERT_FATAL (dom[i] > 0);
  }
  if (owner == -9) {
    ownh = DDS_CYCLONEDDS_HANDLE;
  } else if (owner < 0) {
    ownh = dom[-owner - 1];
  } else {
    ownh = par[owner];
  }
  printf ("%d %d %d %d | %"PRId32"\n", owner, ok1, ok2, fail, ownh);
  ws = dds_create_waitset (ownh);
  CU_ASSERT_FATAL (ws > 0);
  rc = dds_waitset_attach (ws, par[ok1], 0);
  CU_ASSERT_FATAL (rc == 0);
  if (ok2 >= 0) {
    rc = dds_waitset_attach (ws, par[ok2], 1);
    CU_ASSERT_FATAL (rc == 0);
  }
  if (fail >= 0) {
    rc = dds_waitset_attach (ws, par[fail], 2);
    CU_ASSERT_FATAL (rc == DDS_RETCODE_BAD_PARAMETER);
  }
  rc = dds_delete (DDS_CYCLONEDDS_HANDLE);
  CU_ASSERT_FATAL (rc == 0);
  rc = dds_get_parent (DDS_CYCLONEDDS_HANDLE);
  CU_ASSERT_FATAL (rc == DDS_RETCODE_PRECONDITION_NOT_MET);
}

CU_TheoryDataPoints(ddsc_waitset_attach_detach, valid_entities) = {
  CU_DataPoints(dds_entity_t*, &participant, &topic, &writer, &reader, &waitset, &publisher, &subscriber, &readcond),
  CU_DataPoints(dds_entity_t*, &participant, &topic, &writer, &reader, &waitset, &publisher, &subscriber, &readcond),
  CU_DataPoints(dds_attach_t,  (dds_attach_t)NULL, (dds_attach_t)&reader, (dds_attach_t)3, (dds_attach_t)3, (dds_attach_t)3, (dds_attach_t)3, (dds_attach_t)3, (dds_attach_t)3),
};
CU_Theory((dds_entity_t *ws, dds_entity_t *e, dds_attach_t a), ddsc_waitset_attach_detach, valid_entities, .init=ddsc_waitset_init, .fini=ddsc_waitset_fini)
{
  dds_return_t exp;
  dds_return_t ret;

  if (*ws == waitset) {
    /* Attaching to the waitset should work. */
    exp = DDS_RETCODE_OK;
  } else {
    /* Attaching to every other entity should fail. */
    exp = DDS_RETCODE_ILLEGAL_OPERATION;
  }

  ret = dds_waitset_attach (*ws, *e, a);
  CU_ASSERT_FATAL (ret == exp);
  if (ret == 0) {
    ret = dds_waitset_detach (*ws, *e);
    CU_ASSERT_FATAL (ret == 0);
  }
}

CU_Test(ddsc_waitset_attach_detach, second, .init=ddsc_waitset_basic_init, .fini=ddsc_waitset_basic_fini)
{
  dds_return_t ret;
  ret = dds_waitset_attach (waitset, waitset, 0);
  CU_ASSERT_FATAL(ret == 0);
  ret = dds_waitset_attach (waitset, waitset, 0);
  CU_ASSERT_FATAL(ret == DDS_RETCODE_PRECONDITION_NOT_MET);
  ret = dds_waitset_detach (waitset, waitset);
  CU_ASSERT_FATAL(ret == DDS_RETCODE_OK);
  ret = dds_waitset_detach (waitset, waitset);
  CU_ASSERT_FATAL(ret == DDS_RETCODE_PRECONDITION_NOT_MET);
}

CU_TheoryDataPoints(ddsc_waitset_detach, invalid_params) = {
  CU_DataPoints(dds_entity_t, -2, -1, 0, INT_MAX, INT_MIN),
};
CU_Theory((dds_entity_t e), ddsc_waitset_detach, invalid_params, .init=ddsc_waitset_basic_init, .fini=ddsc_waitset_basic_fini)
{
  dds_return_t ret = dds_waitset_detach(waitset, e);
  CU_ASSERT_FATAL (ret == DDS_RETCODE_BAD_PARAMETER);
}

CU_TheoryDataPoints(ddsc_waitset_detach, invalid_waitsets) = {
  CU_DataPoints(dds_entity_t, -2, -1, 0, INT_MAX, INT_MIN),
};
CU_Theory((dds_entity_t ws), ddsc_waitset_detach, invalid_waitsets, .init=ddsc_waitset_basic_init, .fini=ddsc_waitset_basic_fini)
{
  dds_return_t ret = dds_waitset_detach (ws, participant);
  CU_ASSERT_FATAL(ret == DDS_RETCODE_BAD_PARAMETER);
}

CU_TheoryDataPoints(ddsc_waitset_detach, valid_entities) = {
  CU_DataPoints(dds_entity_t*, &participant, &topic, &writer, &reader, &waitset, &publisher, &subscriber, &readcond),
  CU_DataPoints(dds_entity_t*, &participant, &topic, &writer, &reader, &waitset, &publisher, &subscriber, &readcond),
};
CU_Theory((dds_entity_t *ws, dds_entity_t *e), ddsc_waitset_detach, valid_entities, .init=ddsc_waitset_init, .fini=ddsc_waitset_fini)
{
  dds_return_t exp;
  dds_return_t ret;
  if (*ws == waitset) {
    /* Detaching from an empty waitset should yield 'precondition not met'. */
    exp = DDS_RETCODE_PRECONDITION_NOT_MET;
  } else {
    /* Attaching to every other entity should yield 'illegal operation'. */
    exp = DDS_RETCODE_ILLEGAL_OPERATION;
  }
  ret = dds_waitset_detach (*ws, *e);
  CU_ASSERT_FATAL (ret == exp);
}

CU_Test(ddsc_waitset_attach_detach, various, .init=ddsc_waitset_init, .fini=ddsc_waitset_fini)
{
  const dds_entity_t es[] = { readcond, writer, reader, topic, publisher, subscriber, waitset, participant };
  dds_return_t ret;
  for (size_t i = 0; i < sizeof (es) / sizeof (es[0]); i++)
  {
    ret = dds_waitset_attach (waitset, es[i], 0);
    CU_ASSERT_FATAL (ret == 0);
    // doing it a second time gives a precondition not met
    ret = dds_waitset_attach (waitset, es[i], 0);
    CU_ASSERT_FATAL (ret == DDS_RETCODE_PRECONDITION_NOT_MET);
    ret = dds_waitset_detach (waitset, es[i]);
    CU_ASSERT_FATAL (ret == 0);
  }
}

CU_Test(ddsc_waitset_attach_detach, combinations, .init=ddsc_waitset_init, .fini=ddsc_waitset_fini)
{
  const dds_entity_t entities[] = { readcond, writer, reader, topic, publisher, subscriber, waitset, participant };
  const uint32_t count = (uint32_t) (sizeof (entities) / sizeof (entities[0]));
  dds_return_t ret;
  dds_entity_t es[MAX_ENTITIES_CNT];
  ret = dds_waitset_get_entities (waitset, es, sizeof (es) / sizeof (es[0]));
  CU_ASSERT_FATAL (ret == 0);

  uint32_t prevset = 0;
  for (uint32_t round = 1; round < (2u << count) - 2; round++)
  {
    const uint32_t i = (round < (1u << count)) ? round : ((2u << count) - round - 2);
    const uint32_t set = i ^ (i >> 1);
    const uint32_t flipped = set ^ prevset;
    DDSRT_WARNING_MSVC_OFF(4146);
    assert (flipped && (flipped & -flipped) == flipped);
    DDSRT_WARNING_MSVC_ON(4146);
    uint32_t flipidx = 0;
    while (!(flipped & (1u << flipidx)))
      flipidx++;
    assert (flipidx < count);

    //printf ("%zu %zu %02zx -> %02zx : %02zx %"PRIu32" %s\n", round, i, prevset, set, flipped, flipidx, (prevset & flipped) ? "detach" : "attach");
    if (prevset & flipped)
    {
      ret = dds_waitset_detach (waitset, entities[flipidx]);
      CU_ASSERT_FATAL (ret == 0);
    }
    else
    {
      ret = dds_waitset_attach (waitset, entities[flipidx], 0);
      CU_ASSERT_FATAL (ret == 0);
    }

    // attaching any entity in set must give "precond. not met"
    for (uint32_t j = 0; j < count; j++)
    {
      if (!(set & (1u << j)))
        continue;
      ret = dds_waitset_attach (waitset, entities[j], 0);
      CU_ASSERT_FATAL (ret == DDS_RETCODE_PRECONDITION_NOT_MET);
    }

    // set must match expectations
    ret = dds_waitset_get_entities (waitset, es, sizeof (es) / sizeof (es[0]));
    CU_ASSERT_FATAL (ret > 0 && ret <= (dds_return_t) count);
    uint32_t actset = 0;
    for (dds_return_t j = 0; j < ret; j++)
    {
      uint32_t k;
      for (k = 0; k < count; k++)
        if (es[j] == entities[k])
          break;
      // it must be one of the known entities
      CU_ASSERT_FATAL (k < count);
      // must not be seen yet, must be expected to be seen
      CU_ASSERT_FATAL (!(actset & (1u << k)));
      CU_ASSERT_FATAL ((set & (1u << k)) != 0);
      actset |= 1u << k;
    }

    prevset = set;
  }
}

CU_Test(ddsc_waitset_delete_attached, self, .init=ddsc_waitset_basic_init, .fini=ddsc_waitset_basic_fini)
{
  dds_return_t ret;
  ret = dds_waitset_attach (waitset, waitset, 0);
  CU_ASSERT_FATAL (ret == 0);
  ret = dds_delete (waitset);
  CU_ASSERT_FATAL (ret == 0);
}

CU_Test(ddsc_waitset_delete_attached, participant, .init=ddsc_waitset_basic_init, .fini=ddsc_waitset_basic_fini)
{
  dds_return_t ret;
  // init is supposed to create the waitset in the participant, deleting the participant should therefore
  // also delete tear down everything and de-initialize the library
  assert (dds_get_parent (waitset) == participant);
  ret = dds_waitset_attach (waitset, participant, 0);
  CU_ASSERT_FATAL (ret == 0);
  ret = dds_delete (participant);
  CU_ASSERT_FATAL (ret == 0);
  ret = dds_get_parent (waitset);
  CU_ASSERT_FATAL (ret == DDS_RETCODE_PRECONDITION_NOT_MET);
  ret = dds_get_parent (participant);
  CU_ASSERT_FATAL (ret == DDS_RETCODE_PRECONDITION_NOT_MET);
}

CU_Test(ddsc_waitset_delete_attached, reader, .init=ddsc_waitset_init, .fini=ddsc_waitset_fini)
{
  dds_entity_t es[MAX_ENTITIES_CNT];
  dds_return_t ret;
  ret = dds_waitset_attach (waitset, readcond, 0);
  CU_ASSERT_FATAL (ret == 0);
  ret = dds_waitset_attach (waitset, reader, 1);
  CU_ASSERT_FATAL (ret == 0);
  ret = dds_delete (reader);
  CU_ASSERT_FATAL (ret == 0);
  // deleting the reader also deletes readcond, and so no entities should still be attached to waitset
  ret = dds_waitset_get_entities (waitset, es, sizeof (es) / sizeof (es[0]));
  CU_ASSERT_FATAL (ret == 0);
}

CU_Test(ddsc_waitset_delete_attached, various, .init=ddsc_waitset_init, .fini=ddsc_waitset_fini)
{
  // order matters: deleting the reader will also delete readcond; deleting pub/sub will delete wr/rd
  // this order should be ok, but the number of alive entities will dwindle
  const dds_entity_t es[] = { readcond, writer, reader, topic, publisher, subscriber };
  dds_return_t ret;
  for (size_t i = 0; i < sizeof (es) / sizeof (es[0]); i++)
  {
    ret = dds_waitset_attach (waitset, es[i], 0);
    CU_ASSERT_FATAL(ret == 0);
    ret = dds_delete (es[i]);
    CU_ASSERT_FATAL(ret == 0);
  }
}

CU_Test(ddsc_waitset_set_trigger, deleted_waitset, .init=ddsc_waitset_basic_init, .fini=ddsc_waitset_basic_fini)
{
  dds_delete (waitset);
  dds_return_t ret = dds_waitset_set_trigger (waitset, true);
  CU_ASSERT_FATAL (ret == DDS_RETCODE_BAD_PARAMETER);
}

CU_TheoryDataPoints(ddsc_waitset_set_trigger, invalid_params) = {
  CU_DataPoints(dds_entity_t, -2, -1, 0, INT_MAX, INT_MIN),
};
CU_Theory((dds_entity_t ws), ddsc_waitset_set_trigger, invalid_params, .init=ddsc_waitset_basic_init, .fini=ddsc_waitset_basic_fini)
{
  dds_return_t ret = dds_waitset_set_trigger (ws, true);
  CU_ASSERT_FATAL (ret == DDS_RETCODE_BAD_PARAMETER);
}

CU_TheoryDataPoints(ddsc_waitset_set_trigger, non_waitsets) = {
  CU_DataPoints(dds_entity_t*, &participant, &topic, &writer, &reader, &publisher, &subscriber, &readcond),
};
CU_Theory((dds_entity_t *ws), ddsc_waitset_set_trigger, non_waitsets, .init=ddsc_waitset_init, .fini=ddsc_waitset_fini)
{
  dds_return_t ret = dds_waitset_set_trigger (*ws, true);
  CU_ASSERT_FATAL (ret == DDS_RETCODE_ILLEGAL_OPERATION);
}

CU_Test(ddsc_waitset_wait, deleted_waitset, .init=ddsc_waitset_attached_init, .fini=ddsc_waitset_attached_fini)
{
  dds_delete (waitset);
  dds_attach_t triggered;
  dds_return_t ret = dds_waitset_wait (waitset, &triggered, 1, DDS_SECS (1));
  CU_ASSERT_FATAL (ret == DDS_RETCODE_BAD_PARAMETER);
}

CU_TheoryDataPoints(ddsc_waitset_wait, invalid_waitsets) = {
  CU_DataPoints(dds_entity_t, -2, -1, 0, INT_MAX, INT_MIN),
};
CU_Theory((dds_entity_t ws), ddsc_waitset_wait, invalid_waitsets, .init=ddsc_waitset_basic_init, .fini=ddsc_waitset_basic_fini)
{
  dds_attach_t triggered;
  dds_return_t ret = dds_waitset_wait (ws, &triggered, 1, DDS_SECS (1));
  CU_ASSERT_FATAL (ret == DDS_RETCODE_BAD_PARAMETER);
}

CU_TheoryDataPoints(ddsc_waitset_wait, non_waitsets) = {
  CU_DataPoints(dds_entity_t*, &participant, &topic, &writer, &reader, &publisher, &subscriber, &readcond),
};
CU_Theory((dds_entity_t *ws), ddsc_waitset_wait, non_waitsets, .init=ddsc_waitset_init, .fini=ddsc_waitset_fini)
{
  dds_attach_t triggered;
  dds_return_t ret = dds_waitset_wait (*ws, &triggered, 1, DDS_SECS (1));
  CU_ASSERT_FATAL (ret == DDS_RETCODE_ILLEGAL_OPERATION);
}

CU_TheoryDataPoints(ddsc_waitset_wait, invalid_params) = {
  CU_DataPoints(dds_attach_t *, (dds_attach_t[]){0}, NULL),
  CU_DataPoints(size_t, 0, 1, 100),
  CU_DataPoints(int, -1, 0, 1),
};
CU_Theory((dds_attach_t *a, size_t size, int msec), ddsc_waitset_wait, invalid_params, .init=ddsc_waitset_basic_init, .fini=ddsc_waitset_basic_fini)
{
  dds_return_t ret;
  assert ((a == NULL && size != 0) || (a != NULL && size == 0) || msec < 0);
  ret = dds_waitset_wait (waitset, a, size, DDS_MSECS (msec));
  CU_ASSERT_FATAL (ret == DDS_RETCODE_BAD_PARAMETER);
}

CU_Test(ddsc_waitset_wait_until, deleted_waitset, .init=ddsc_waitset_attached_init, .fini=ddsc_waitset_attached_fini)
{
  (void) dds_delete (waitset);
  dds_attach_t triggered;
  dds_return_t ret = dds_waitset_wait_until (waitset, &triggered, 1, dds_time ());
  CU_ASSERT_FATAL (ret == DDS_RETCODE_BAD_PARAMETER);
}

CU_TheoryDataPoints(ddsc_waitset_wait_until, invalid_waitsets) = {
  CU_DataPoints(dds_entity_t, -2, -1, 0, INT_MAX, INT_MIN),
};
CU_Theory((dds_entity_t ws), ddsc_waitset_wait_until, invalid_waitsets, .init=ddsc_waitset_basic_init, .fini=ddsc_waitset_basic_fini)
{
  dds_attach_t triggered;
  dds_return_t ret = dds_waitset_wait_until (ws, &triggered, 1, dds_time ());
  CU_ASSERT_FATAL (ret == DDS_RETCODE_BAD_PARAMETER);
}

CU_TheoryDataPoints(ddsc_waitset_wait_until, non_waitsets) = {
  CU_DataPoints(dds_entity_t*, &participant, &topic, &writer, &reader, &publisher, &subscriber, &readcond),
};
CU_Theory((dds_entity_t *ws), ddsc_waitset_wait_until, non_waitsets, .init=ddsc_waitset_init, .fini=ddsc_waitset_fini)
{
  dds_attach_t triggered;
  dds_return_t ret = dds_waitset_wait_until (*ws, &triggered, 1, dds_time ());
  CU_ASSERT_FATAL (ret == DDS_RETCODE_ILLEGAL_OPERATION);
}

CU_TheoryDataPoints(ddsc_waitset_wait_until, invalid_params) = {
  CU_DataPoints(dds_attach_t *, (dds_attach_t[]){0}, NULL),
  CU_DataPoints(size_t, 0, 1, 100)
};
CU_Theory((dds_attach_t *a, size_t size), ddsc_waitset_wait_until, invalid_params, .init=ddsc_waitset_basic_init, .fini=ddsc_waitset_basic_fini)
{
  dds_return_t ret;
  assert ((a == NULL && size != 0) || (a != NULL && size == 0));
  ret = dds_waitset_wait_until (waitset, a, size, dds_time ());
  CU_ASSERT_FATAL (ret == DDS_RETCODE_BAD_PARAMETER);
}

CU_Test(ddsc_waitset_wait_until, past, .init=ddsc_waitset_attached_init, .fini=ddsc_waitset_attached_fini)
{
  dds_attach_t triggered;
  dds_return_t ret = dds_waitset_wait_until (waitset, &triggered, 1, dds_time () - 100000);
  CU_ASSERT_FATAL(ret == 0);
}

CU_TheoryDataPoints(ddsc_waitset_get_entities, array_sizes) = {
  CU_DataPoints(size_t, 0, 1, 7, MAX_ENTITIES_CNT),
};
CU_Theory((size_t size), ddsc_waitset_get_entities, array_sizes, .init=ddsc_waitset_attached_init, .fini=ddsc_waitset_attached_fini)
{
  const dds_entity_t expected[] = { participant, topic, writer, reader, waitset, publisher, subscriber };
  uint32_t found = 0;
  dds_return_t ret;
  dds_entity_t es[MAX_ENTITIES_CNT];

  /* Make sure at least one entity is in the waitsets' internal triggered list. */
  ret = dds_waitset_set_trigger (waitset, true);
  CU_ASSERT_FATAL (ret == 0);

  /* Get the actual attached entities, return value is the number of attached entities
     and may be larger than size. */
  ret = dds_waitset_get_entities (waitset, es, size);
  CU_ASSERT_FATAL (ret == (dds_return_t) (sizeof (expected) / sizeof (expected[0])));

  /* If size is too small, a subset of expected must be present, otherwise it must be identical */
  dds_return_t nvalid = ((dds_return_t) size < ret) ? (dds_return_t) size : ret;
  dds_return_t count = 0;
  for (dds_return_t i = 0; i < nvalid; i++) {
    uint32_t flag = 0;
    for (size_t j = 0; j < sizeof (expected) / sizeof (expected[0]); j++) {
      if (expected[j] == es[i]) {
        flag = 1u << j;
        break;
      }
    }
    CU_ASSERT_FATAL (flag != 0);
    CU_ASSERT_FATAL (!(found & flag));
    found |= flag;
    count++;
  }
  CU_ASSERT_FATAL (count == nvalid);
}

CU_Test(ddsc_waitset_get_entities, no_array, .init=ddsc_waitset_attached_init, .fini=ddsc_waitset_attached_fini)
{
  dds_return_t ret;
  DDSRT_WARNING_MSVC_OFF(6387); /* Disable SAL warning on intentional misuse of the API */
  ret = dds_waitset_get_entities (waitset, NULL, 1);
  DDSRT_WARNING_MSVC_ON(6387);
  /* ddsc_waitset_attached_init attached 7 entities. */
  CU_ASSERT_FATAL (ret == 7);
}

CU_Test(ddsc_waitset_get_entities, deleted_waitset, .init=ddsc_waitset_attached_init, .fini=ddsc_waitset_attached_fini)
{
  dds_entity_t entities[MAX_ENTITIES_CNT];
  dds_delete (waitset);
  dds_return_t ret = dds_waitset_get_entities (waitset, entities, MAX_ENTITIES_CNT);
  CU_ASSERT_FATAL (ret == DDS_RETCODE_BAD_PARAMETER);
}

CU_TheoryDataPoints(ddsc_waitset_get_entities, invalid_params) = {
  CU_DataPoints(dds_entity_t, -2, -1, 0, INT_MAX, INT_MIN),
};
CU_Theory((dds_entity_t ws), ddsc_waitset_get_entities, invalid_params, .init=ddsc_waitset_attached_init, .fini=ddsc_waitset_attached_fini)
{
  dds_entity_t entities[MAX_ENTITIES_CNT];
  dds_return_t ret = dds_waitset_get_entities (ws, entities, MAX_ENTITIES_CNT);
  CU_ASSERT_FATAL (ret == DDS_RETCODE_BAD_PARAMETER);
}

CU_TheoryDataPoints(ddsc_waitset_get_entities, non_waitsets) = {
  CU_DataPoints(dds_entity_t*, &participant, &topic, &writer, &reader, &publisher, &subscriber, &readcond),
};
CU_Theory((dds_entity_t *ws), ddsc_waitset_get_entities, non_waitsets, .init=ddsc_waitset_attached_init, .fini=ddsc_waitset_attached_fini)
{
  dds_entity_t entities[MAX_ENTITIES_CNT];
  dds_return_t ret = dds_waitset_get_entities (*ws, entities, MAX_ENTITIES_CNT);
  CU_ASSERT_FATAL (ret == DDS_RETCODE_ILLEGAL_OPERATION);
}

static void cw_trig_write (void)
{
  dds_return_t ret = dds_write (writer, &(RoundTripModule_DataType){0});
  CU_ASSERT_FATAL (ret == 0);
}

static void cw_trig_waitset (void)
{
  dds_return_t ret = dds_waitset_set_trigger (waitset, true);
  CU_ASSERT_FATAL (ret == 0);
}

static void check_waitset_trigger (dds_entity_t e, void (*trig) (void))
{
  thread_arg_t arg;
  dds_return_t ret;

  /* verify the initial state of the entity we're playing with is not-triggered */
  ret = dds_triggered (e);
  CU_ASSERT_FATAL (ret == 0);

  /* start a thread to wait for a trigger */
  waiting_thread_start (&arg, e);

  /* calling trig() should unblock the thread and cause it to exit */
  trig ();
  ret = waiting_thread_expect_exit (&arg);
  CU_ASSERT_FATAL (ret == 0);

  /* The thread doesn't do anything with the entity, so the state should remain triggered */
  ret = dds_triggered (e);
  CU_ASSERT (ret > 0);
  /* and consequently a wait with timeout=0 should return the entity */
  dds_attach_t triggered;
  ret = dds_waitset_wait (waitset, &triggered, 1, 0);
  CU_ASSERT_FATAL (ret == 1);
  CU_ASSERT_FATAL (triggered == (dds_attach_t) e);
}

CU_Test (ddsc_waitset_triggering, on_self, .init=ddsc_waitset_attached_init, .fini=ddsc_waitset_attached_fini)
{
  dds_return_t ret;
  check_waitset_trigger (waitset, cw_trig_waitset);
  ret = dds_waitset_set_trigger (waitset, false);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
  ret = dds_triggered (waitset);
  CU_ASSERT_EQUAL_FATAL (ret, 0);
}

CU_Test(ddsc_waitset_triggering, on_reader, .init=ddsc_waitset_attached_init, .fini=ddsc_waitset_attached_fini)
{
  dds_return_t ret = dds_set_status_mask (reader, DDS_DATA_AVAILABLE_STATUS);
  CU_ASSERT_FATAL (ret == 0);
  check_waitset_trigger (reader, cw_trig_write);
}

CU_Test(ddsc_waitset_triggering, on_readcondition, .init=ddsc_waitset_attached_init, .fini=ddsc_waitset_attached_fini)
{
  dds_return_t ret;
  ret = dds_waitset_attach (waitset, readcond, readcond);
  CU_ASSERT_FATAL (ret == 0);
  check_waitset_trigger (readcond, cw_trig_write);
  ret = dds_waitset_detach (waitset, readcond);
  CU_ASSERT_FATAL (ret == 0);
}

static uint32_t waiting_thread (void *a)
{
  thread_arg_t *arg = (thread_arg_t *) a;
  dds_attach_t triggered;
  dds_return_t ret;
  ddsrt_atomic_st32 (&arg->state, WAITING);
  /* This should block until the main test released all claims. */
  ret = dds_waitset_wait (waitset, &triggered, 1, DDS_SECS (1000));
  CU_ASSERT_FATAL (ret == 1);
  CU_ASSERT_FATAL (arg->expected == (dds_entity_t) (intptr_t) triggered);
  ddsrt_atomic_st32 (&arg->state, STOPPED);
  return 0;
}

static dds_return_t thread_reached_state (ddsrt_atomic_uint32_t *actual, thread_state_t expected, int32_t msec)
{
  while (msec > 0 && (thread_state_t) ddsrt_atomic_ld32 (actual) != expected) {
    dds_sleepfor (DDS_MSECS (10));
    msec -= 10;
  }
  return ((thread_state_t) ddsrt_atomic_ld32 (actual) == expected) ? 0 : DDS_RETCODE_TIMEOUT;
}

static void waiting_thread_start (struct thread_arg_t *arg, dds_entity_t expected)
{
  ddsrt_thread_t thread_id;
  ddsrt_threadattr_t thread_attr;
  dds_return_t rc;

  assert (arg);

  /* Create an other thread that will blocking wait on the waitset. */
  arg->expected = expected;
  ddsrt_atomic_st32 (&arg->state, STARTING);
  ddsrt_threadattr_init (&thread_attr);
  rc = ddsrt_thread_create (&thread_id, "waiting_thread", &thread_attr, waiting_thread, arg);
  CU_ASSERT_FATAL (rc == 0);

  /* The thread should reach 'waiting' state. */
  rc = thread_reached_state (&arg->state, WAITING, 1000);
  CU_ASSERT_FATAL (rc == 0);
  /* But thread should block and thus NOT reach 'stopped' state. */
  rc = thread_reached_state (&arg->state, STOPPED, 100);
  CU_ASSERT_FATAL (rc == DDS_RETCODE_TIMEOUT);
  arg->tid = thread_id;
}

static dds_return_t waiting_thread_expect_exit (struct thread_arg_t *arg)
{
  dds_return_t ret = thread_reached_state (&arg->state, STOPPED, 5000);
  if (ret == 0)
    (void) ddsrt_thread_join (arg->tid, NULL);
  return ret;
}

static void listener_nop (dds_entity_t e, void *vflags)
{
  ddsrt_atomic_uint32_t *flags = vflags;
  uint32_t s;
  dds_return_t rc = dds_read_status (e, &s, DDS_DATA_AVAILABLE_STATUS);
  CU_ASSERT_FATAL (rc == 0);
  CU_ASSERT_FATAL (s == DDS_DATA_AVAILABLE_STATUS);
  ddsrt_atomic_or32 (flags, 1);
}

static void listener_take_expecting_data (dds_entity_t e, void *vflags)
{
  (void) e;
  ddsrt_atomic_uint32_t *flags = vflags;
  void *xs = NULL;
  dds_sample_info_t si;
  dds_return_t n = dds_take (reader, &xs, &si, 1, 1);
  CU_ASSERT_FATAL (n == 1);
  // don't care what we take
  (void) dds_return_loan (reader, &xs, n);
  ddsrt_atomic_or32 (flags, 1);
}

struct listener_waitset_thread_arg {
  ddsrt_atomic_uint32_t *flags;
  dds_entity_t ws;
  dds_time_t abstimeout;
};

static uint32_t listener_waitset_thread (void *varg)
{
  struct listener_waitset_thread_arg * const arg = varg;
  dds_return_t n;
  do {
    // spurious wakeups are a possibility in general (probably not here)
    dds_attach_t xs = 0;
    n = dds_waitset_wait_until (arg->ws, &xs, 1, arg->abstimeout);
    if (n < 0)
      return 0;
  } while (n == 0 && dds_time () < arg->abstimeout);
  if (n > 0)
  {
    uint32_t old = ddsrt_atomic_or32_ov (arg->flags, 2);
    if (old & 1) // listener triggered first
      ddsrt_atomic_or32 (arg->flags, 4);
  }
  return 1;
}

CU_TheoryDataPoints(ddsc_waitset_triggering, after_listener) = {
  CU_DataPoints(bool, true, false)
};
CU_Theory((bool use_nop_listener), ddsc_waitset_triggering, after_listener, .init=ddsc_waitset_attached_init, .fini=ddsc_waitset_attached_fini)
{
  ddsrt_atomic_uint32_t flags;
  dds_listener_t *listener = dds_create_listener (&flags);
  ddsrt_thread_t thread_id;
  ddsrt_threadattr_t thread_attr;
  dds_return_t ret;

  // nop listeners mean we operate without a timeout and can repeat the experiment more often
  // repeating makes sense because it improves our chances of catching an ordering problem
  printf ("ddsc_waitset_triggering after_listener - reader %d\n", reader);
  printf ("ddsc_waitset_triggering after_listener - use_nop_listener = %d\n", use_nop_listener);
  unsigned flags_counts[8] = { 0 };
  const unsigned rounds = (use_nop_listener ? 100 : 10);
  for (unsigned i = 0; i < rounds; i++)
  {
    ddsrt_atomic_st32 (&flags, 0);
    ret = dds_set_status_mask (reader, DDS_DATA_AVAILABLE_STATUS);
    CU_ASSERT_FATAL (ret == 0);
    dds_lset_data_available_arg (listener, use_nop_listener ? listener_nop : listener_take_expecting_data, &flags, false);
    ret = dds_set_listener (reader, listener);
    CU_ASSERT_FATAL (ret == 0);

    // if we're not expecting a trigger, wait only 100ms to speed things up a bit
    dds_duration_t reltimeout = use_nop_listener ? DDS_SECS (1) : DDS_MSECS (100);
    struct listener_waitset_thread_arg arg = {
      .flags = &flags,
      .ws = waitset,
      .abstimeout = dds_time () + reltimeout
    };
    ddsrt_threadattr_init (&thread_attr);
    ret = ddsrt_thread_create (&thread_id, "waiting_thread", &thread_attr, listener_waitset_thread, &arg);
    CU_ASSERT_FATAL (ret == 0);

    // There is now a short window during which wait() can observe the status before
    // a listener runs, and so there is no guarantee that the listener runs before any
    // waitsets are triggered, contrary to the spec. (The spec is totally broken on most
    // points anyway ...)
    dds_sleepfor (DDS_MSECS (10));

    // writing data should trigger the listener; if the listener is a nop, the status
    // should remain set and the waitset should become triggered; else the status
    // should be cleared and the wait timeout
    ret = dds_write (writer, &(RoundTripModule_DataType){0});
    CU_ASSERT_FATAL (ret == 0);

    // listener triggers synchronously in Cyclone, so it must already have set the flag
    CU_ASSERT_FATAL (ddsrt_atomic_ld32 (&flags) & 1);

    uint32_t thread_ret;
    ret = ddsrt_thread_join (thread_id, &thread_ret);
    CU_ASSERT_FATAL (ret == 0);
    CU_ASSERT_FATAL (thread_ret != 0);

    // must clear the data available status or the waitset will trigger even without a
    // write, which means the next attempt will likely fail because of the sleep before
    // the write
    uint32_t status;
    ret = dds_take_status (reader, &status, DDS_DATA_AVAILABLE_STATUS);
    CU_ASSERT_FATAL (ret == 0);

    // If a nop listener, expected observed behaviour to be: first listener, then waitset
    // which maps to 7; else, expected observed behaviour is just listener and no waitset
    // trigger
    //
    // There is no guarantee that the initial evaluation of the conditions by wait() won't
    // happen just before the listener fires, so for the nop listener we expect to see
    // mostly 7 and for the listener doing takes mostly 1, but we can't exclude other
    // values.
    uint32_t exp = use_nop_listener ? 7 : 1;
    uint32_t cur = ddsrt_atomic_ld32 (&flags);
    CU_ASSERT_FATAL (cur <= sizeof (flags_counts) / sizeof (flags_counts[0]));
    flags_counts[cur]++;
    if (cur != exp)
      printf ("current flags: 0x%"PRIx32", expected: 0x%"PRIx32"\n", cur, exp);
  }

  // something must have happened
  CU_ASSERT (flags_counts[0] == 0);
  // listener must have been invoked in all cases
  unsigned sum_of_odd_indices = 0;
  for (size_t i = 1; i < sizeof (flags_counts) / sizeof (flags_counts[0]); i += 2)
    sum_of_odd_indices += flags_counts[i];
  CU_ASSERT (sum_of_odd_indices == rounds);
  // majority of cases (80% is "just a number") must have expected value
  CU_ASSERT (5 * flags_counts[use_nop_listener ? 7 : 1] >= 4 * rounds);

  // clear listener so there'll be no reference &flags left
  dds_set_listener (reader, NULL);
  dds_delete_listener (listener);
}
