// Copyright(c) 2019 to 2021 ZettaScale Technology and others
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
#include "CUnit/Theory.h"
#include "RoundTrip.h"

#include "dds/ddsrt/cdtors.h"
#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/process.h"
#include "dds/ddsrt/threads.h"
#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/time.h"
#include "dds/ddsrt/random.h"

#define N_WAITSETS 20
#define N_ENTITIES 32

#define N_GUARDCONDS 20
#define N_SUBSCRIBERS 4
#define N_READERS 4
#define N_READCONDS 4

static dds_entity_t ppant;
static dds_entity_t topic;

static ddsrt_atomic_uint32_t terminate;
static ddsrt_atomic_uint32_t waitsets[N_WAITSETS];
static ddsrt_atomic_uint32_t entities[N_ENTITIES], signalled;

static ddsrt_atomic_uint32_t attach_ok, detach_ok, settrig_ok;
static ddsrt_atomic_uint32_t create_ent_ok[5], delete_ent_ok[5];
static ddsrt_atomic_uint32_t create_ws_ok, delete_ws_ok;

#define RESERVED      ((uint32_t) 0xffffffffu)

static void init_prng (ddsrt_prng_t *prng)
{
  ddsrt_prng_seed_t prng_seed;
  for (size_t i = 0; i < sizeof (prng_seed.key) / sizeof (prng_seed.key[0]); i++)
    prng_seed.key[i] = ddsrt_random ();
  ddsrt_prng_init (prng, &prng_seed);
}

static void choose_index (uint32_t *p_idx, uint32_t *p_handle, ddsrt_prng_t *prng, ddsrt_atomic_uint32_t elems[], size_t nelems)
{
  uint32_t idx, h, h1;
retry_idx:
  idx = ddsrt_prng_random (prng) % (uint32_t) nelems;
retry_cas:
  h = ddsrt_atomic_ld32 (&elems[idx]);
  if (h == 0)
    h1 = RESERVED;
  else if ((int32_t) h > 0)
    h1 = 0;
  else
    goto retry_idx;
  if (!ddsrt_atomic_cas32 (&elems[idx], h, h1))
    goto retry_cas;
  *p_idx = idx;
  *p_handle = h;
}

static dds_entity_t pick_a_subscriber (void)
{
  uint32_t idx = ddsrt_random () % N_SUBSCRIBERS;
  uint32_t x = ddsrt_atomic_ld32 (&entities[N_GUARDCONDS + idx]);
  return (dds_entity_t) x;
}

static dds_entity_t pick_a_reader (void)
{
  uint32_t idx = ddsrt_random () % N_READERS;
  uint32_t x = ddsrt_atomic_ld32 (&entities[N_GUARDCONDS + N_SUBSCRIBERS + idx]);
  return (dds_entity_t) x;
}

static int index_to_counter_index (uint32_t idx)
{
  if (idx < N_GUARDCONDS)
    return 0;
  else if (idx < N_GUARDCONDS + N_SUBSCRIBERS)
    return 1;
  else if (idx < N_GUARDCONDS + N_SUBSCRIBERS + N_READERS)
    return 2;
  else
    return 4;
}

static uint32_t guardcond_create_delete_thread (void *varg)
{
  (void) varg;
  ddsrt_prng_t prng;
  init_prng (&prng);
  while (!ddsrt_atomic_ld32 (&terminate))
  {
    uint32_t idx, handle;
    choose_index (&idx, &handle, &prng, entities, N_ENTITIES);
    if (handle == 0)
    {
      dds_entity_t ent = 0, parent = 0;

      if (idx < N_GUARDCONDS)
        ent = dds_create_guardcondition (DDS_CYCLONEDDS_HANDLE);
      else if (idx < N_GUARDCONDS + N_SUBSCRIBERS)
        ent = dds_create_subscriber (ppant, NULL, NULL);
      else if (idx < N_GUARDCONDS + N_SUBSCRIBERS + N_READERS)
      {
        if ((parent = pick_a_subscriber ()) == 0)
          parent = ppant;
        ent = dds_create_reader (parent, topic, NULL, NULL);
      }
      else if ((parent = pick_a_reader ()) != 0)
      {
        ent = dds_create_readcondition (parent, DDS_ANY_STATE);
      }

      if (ent > 0)
      {
        ddsrt_atomic_inc32 (&create_ent_ok[index_to_counter_index (idx) + (parent == ppant)]);
        ddsrt_atomic_st32 (&entities[idx], (uint32_t) ent);
      }
      else if (ent < 0 && idx < N_GUARDCONDS)
      {
        fprintf (stderr, "dds_create_guardcondition failed: %s\n", dds_strretcode (ent));
        ddsrt_atomic_st32 (&terminate, 1);
        return 1;
      }
    }
    else
    {
      dds_return_t rc = dds_delete ((dds_entity_t) handle);
      if (rc == 0)
        ddsrt_atomic_inc32 (&delete_ent_ok[index_to_counter_index (idx)]);
    }
  }
  return 0;
}

static uint32_t waitset_create_delete_thread (void *varg)
{
  (void) varg;
  ddsrt_prng_t prng;
  init_prng (&prng);
  while (!ddsrt_atomic_ld32 (&terminate))
  {
    uint32_t idx, handle;
    choose_index (&idx, &handle, &prng, waitsets, N_WAITSETS);
    if (handle == 0)
    {
      dds_entity_t ws = dds_create_waitset (DDS_CYCLONEDDS_HANDLE);
      if (ws < 0)
      {
        fprintf (stderr, "dds_create_waitset failed: %s\n", dds_strretcode (ws));
        ddsrt_atomic_st32 (&terminate, 1);
        return 1;
      }
      ddsrt_atomic_inc32 (&create_ws_ok);
      ddsrt_atomic_st32 (&waitsets[idx], (uint32_t) ws);
    }
    else
    {
      dds_return_t rc = dds_delete ((dds_entity_t) handle);
      if (rc == 0)
        ddsrt_atomic_inc32 (&delete_ws_ok);
    }
  }
  return 0;
}

static uint32_t guardcond_trigger_thread (void *varg)
{
  (void) varg;
  ddsrt_prng_t prng;
  init_prng (&prng);
  while (!ddsrt_atomic_ld32 (&terminate))
  {
    uint32_t idx = ddsrt_prng_random (&prng) % N_ENTITIES;
    uint32_t h = ddsrt_atomic_ld32 (&entities[idx]);
    if ((int32_t) h <= 0)
      continue;
    else
    {
      uint32_t s, s1;
      do {
        s = ddsrt_atomic_ld32 (&signalled);
        s1 = s ^ (1u << idx);
      } while (!ddsrt_atomic_cas32 (&signalled, s, s1));
      dds_return_t rc = dds_set_guardcondition ((dds_entity_t) h, (s & (1u << idx)) ? false : true);
      if (rc == 0)
        ddsrt_atomic_inc32 (&settrig_ok);
    }
  }
  return 0;
}

static uint32_t waitset_attach_detach_thread (void *varg)
{
  (void) varg;
  ddsrt_prng_t prng;
  init_prng (&prng);
  while (!ddsrt_atomic_ld32 (&terminate))
  {
    uint32_t wsidx = ddsrt_prng_random (&prng) % N_WAITSETS;
    uint32_t wsh = ddsrt_atomic_ld32 (&waitsets[wsidx]);
    if ((int32_t) wsh <= 0)
      continue;

    uint32_t gcidx = ddsrt_prng_random (&prng) % N_ENTITIES;
    uint32_t gch = ddsrt_atomic_ld32 (&entities[gcidx]);
    if ((int32_t) gch <= 0)
      continue;

    dds_return_t rc;
    rc = dds_waitset_detach ((dds_entity_t) wsh, (dds_entity_t) gch);
    if (rc == 0)
    {
      ddsrt_atomic_inc32 (&detach_ok);
    }
    else if (rc != DDS_RETCODE_PRECONDITION_NOT_MET && rc != DDS_RETCODE_BAD_PARAMETER)
    {
      /* attempts at attaching a guard condition twice or detaching an unattached
         one are expected, and those result in a PRECONDITION_NOT_MET */
      fprintf (stderr, "dds_waitset_detach 0x%"PRIx32" 0x%"PRIx32" failed: %s\n", (dds_entity_t) wsh, (dds_entity_t) gch, dds_strretcode (rc));
      ddsrt_atomic_st32 (&terminate, 1);
      return 1;
    }
    else
    {
      /* should imply it is already attached, so try detaching */
      rc = dds_waitset_attach ((dds_entity_t) wsh, (dds_entity_t) gch, 0);
      if (rc == 0)
      {
        ddsrt_atomic_inc32 (&attach_ok);
      }
      else if (rc != DDS_RETCODE_PRECONDITION_NOT_MET && rc != DDS_RETCODE_BAD_PARAMETER)
      {
        fprintf (stderr, "dds_waitset_attach 0x%"PRIx32" 0x%"PRIx32" failed: %s\n", (dds_entity_t) wsh, (dds_entity_t) gch, dds_strretcode (rc));
        ddsrt_atomic_st32 (&terminate, 1);
        return 1;
      }
    }
  }
  return 0;
}

CU_Test (ddsc_waitset, torture)
{
  dds_return_t rc;
  ddsrt_thread_t tids[8];
  ddsrt_threadattr_t tattr;
  ddsrt_threadattr_init (&tattr);

  /* This keeps the library initialised -- it shouldn't be necessary */
  ppant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_FATAL (ppant > 0);
  topic = dds_create_topic (ppant, &RoundTripModule_DataType_desc, "waitset_torture_topic", NULL, NULL);
  CU_ASSERT_FATAL (topic > 0);

  rc = ddsrt_thread_create (&tids[0], "gc_cd", &tattr, guardcond_create_delete_thread, 0);
  CU_ASSERT_FATAL (rc == 0);
  rc = ddsrt_thread_create (&tids[1], "gc_cd", &tattr, guardcond_create_delete_thread, 0);
  CU_ASSERT_FATAL (rc == 0);
  rc = ddsrt_thread_create (&tids[2], "ws_cd", &tattr, waitset_create_delete_thread, 0);
  CU_ASSERT_FATAL (rc == 0);
  rc = ddsrt_thread_create (&tids[3], "ws_cd", &tattr, waitset_create_delete_thread, 0);
  CU_ASSERT_FATAL (rc == 0);
  rc = ddsrt_thread_create (&tids[4], "gc_t", &tattr, guardcond_trigger_thread, 0);
  CU_ASSERT_FATAL (rc == 0);
  rc = ddsrt_thread_create (&tids[5], "gc_t", &tattr, guardcond_trigger_thread, 0);
  CU_ASSERT_FATAL (rc == 0);
  rc = ddsrt_thread_create (&tids[6], "ws_ad", &tattr, waitset_attach_detach_thread, 0);
  CU_ASSERT_FATAL (rc == 0);
  rc = ddsrt_thread_create (&tids[7], "ws_ad", &tattr, waitset_attach_detach_thread, 0);
  CU_ASSERT_FATAL (rc == 0);

  uint32_t wait_err = 0, wait_ok[N_ENTITIES + 1] = { 0 };
  dds_time_t tstop = dds_time () + DDS_SECS (5);
  while (dds_time () < tstop && !ddsrt_atomic_ld32 (&terminate))
  {
    /* Try waiting on the waitset in slot 0 if it exists (it shouldn't make much
       difference which waitset we use; this is easy).  There are never more than
       N_ENTITIES guard conditions, so there are also never more than that many
       triggering entities, and so we can easily do a small histogram.  (The longer
       you run it, the longer the tail of triggering entities one expects.)

       Error handling: the waitset may be deleted in between loading the handle
       and pinning it wait(), so BAD_PARAMETER is to be expected.  If the "extragc"
       isn't there to ensure the library stays initialised, it is even possible
       that we get PRECONDITION_NOT_MET if it just so happened that with the
       deleting of that waitset, no entities remain at all. */
    dds_entity_t ws = (dds_entity_t) ddsrt_atomic_ld32 (&waitsets[0]);
    if (ws > 0)
    {
      int32_t n = dds_waitset_wait (ws, NULL, 0, DDS_MSECS (10));
      if (!((rc >= 0 && rc <= N_ENTITIES) || rc == DDS_RETCODE_BAD_PARAMETER))
      {
        fprintf (stderr, "dds_waitset_wait failed: %s\n", dds_strretcode (rc));
        ddsrt_atomic_st32 (&terminate, 1);
        rc = DDS_RETCODE_ERROR;
      }
      else
      {
        if (n >= 0)
          wait_ok[n]++;
        else
          wait_err++;
      }
    }
  }
  ddsrt_atomic_st32 (&terminate, 1);
  CU_ASSERT (rc != DDS_RETCODE_ERROR);

  for (size_t i = 0; i < sizeof (tids) / sizeof (tids[0]); i++)
  {
    uint32_t retval;
    rc = ddsrt_thread_join (tids[i], &retval);
    CU_ASSERT_FATAL (rc == 0);
    CU_ASSERT (retval == 0);
  }

  /* The threads don't bother to clean up, so delete whatever guard conditions and
     waitsets happen to still exist.  Passing garbage into dds_delete is supposed
     to work, so don't bother with any validation or error checking. */
  for (uint32_t i = 0; i < N_ENTITIES; i++)
  {
    if (dds_delete ((dds_entity_t)  ddsrt_atomic_ld32 (&entities[i])) == DDS_RETCODE_OK)
      ddsrt_atomic_inc32 (&delete_ent_ok[index_to_counter_index (i)]);
  }
  for (uint32_t i = 0; i < N_WAITSETS; i++)
  {
    if (dds_delete ((dds_entity_t) ddsrt_atomic_ld32 (&waitsets[i])) == DDS_RETCODE_OK)
      ddsrt_atomic_inc32 (&delete_ws_ok);
  }

  /* All we should be left within the participant is the topic */
  rc = dds_delete (topic);
  CU_ASSERT_FATAL (rc == DDS_RETCODE_OK);
  rc = dds_get_children (ppant, NULL, 0);
  CU_ASSERT_FATAL (rc == 0);
  rc = dds_delete (ppant);
  CU_ASSERT_FATAL (rc == DDS_RETCODE_OK);

  printf ("attach %"PRIu32" detach %"PRIu32" settrig %"PRIu32"\n", ddsrt_atomic_ld32 (&attach_ok), ddsrt_atomic_ld32 (&detach_ok), ddsrt_atomic_ld32 (&settrig_ok));
  printf ("create/delete ent");
  uint32_t create_ent_ok_sum = 0;
  for (size_t i = 0; i < sizeof (create_ent_ok) / sizeof (create_ent_ok[0]); i++)
  {
    uint32_t c = ddsrt_atomic_ld32 (&create_ent_ok[i]);
    create_ent_ok_sum += c;
    printf (" %"PRIu32"/%"PRIu32, c, ddsrt_atomic_ld32 (&delete_ent_ok[i]));
  }
  printf ("\n");

  {
    uint32_t rd_cr_sub = ddsrt_atomic_ld32 (&create_ent_ok[2]);
    uint32_t rd_cr_ppant = ddsrt_atomic_ld32 (&create_ent_ok[3]);
    uint32_t rd_del = ddsrt_atomic_ld32 (&delete_ent_ok[2]);
    uint32_t sub_del = ddsrt_atomic_ld32 (&delete_ent_ok[1]);
    CU_ASSERT (rd_del <= rd_cr_sub + rd_cr_ppant); /* can't have deleted more readers than were created */
    CU_ASSERT (rd_del >= rd_cr_ppant); /* readers created with ppant as owner must have been deleted explicitly */
    CU_ASSERT (rd_del - rd_cr_ppant <= sub_del); /* other readers may have been deleted by deleting a sub */
  }

  printf ("create/delete ws %"PRIu32"/%"PRIu32"\n", ddsrt_atomic_ld32 (&create_ws_ok), ddsrt_atomic_ld32 (&delete_ws_ok));
  printf ("wait {err %"PRIu32"}", wait_err);
  uint32_t wait_ok_sum = 0;
  for (size_t i = 0; i < sizeof (wait_ok) / sizeof (wait_ok[0]); i++)
  {
    wait_ok_sum += wait_ok[i];
    printf (" %"PRIu32, wait_ok[i]);
  }
  printf ("\n");

  /* Running on Windows on the CI infrastructure has very little concurrency, but Linux
     and macOS seem ok.  The thresholds here appear to be sufficiently low to not give
     many spurious failures, while still being sanity check that at least something
     happened. */
  CU_ASSERT (ddsrt_atomic_ld32 (&attach_ok) +
             ddsrt_atomic_ld32 (&settrig_ok) +
             ddsrt_atomic_ld32 (&create_ws_ok) +
             create_ent_ok_sum +
             wait_ok_sum > 1000);

  /* Library should be de-initialized at this point.  That ordinarily means the handle
     table is gone and PRECONDITION_NOT_MET is returned.  For some reason, on Windows x64
     Release builds the thread cleanup doesn't fully take place, ultimately causing the
     handle table to remain in existence (this is by design: those threads may still be
     reading from it) and BAD_PARAMETER to be returned if the library is otherwise
     properly deinitialized. */
  rc = dds_get_parent (DDS_CYCLONEDDS_HANDLE);
  CU_ASSERT_FATAL (rc == DDS_RETCODE_PRECONDITION_NOT_MET || rc == DDS_RETCODE_BAD_PARAMETER);
}
