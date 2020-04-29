/*
 * Copyright(c) 2006 to 2020 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#include <stdlib.h>

#include "dds/dds.h"
#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/environ.h"
#include "test_common.h"

static ddsrt_mutex_t g_mutex;
static ddsrt_cond_t g_cond;
static uint32_t cb_called;
static dds_entity_t cb_topic, cb_writer, cb_reader, cb_subscriber;

#define DEFINE_STATUS_CALLBACK(name, NAME, kind) \
  static dds_##name##_status_t cb_##name##_status; \
  static void name##_cb (dds_entity_t kind, const dds_##name##_status_t status, void *arg) \
  { \
    (void) arg; \
    ddsrt_mutex_lock (&g_mutex); \
    cb_##kind = kind; \
    cb_##name##_status = status; \
    cb_called |= DDS_##NAME##_STATUS; \
    ddsrt_cond_broadcast (&g_cond); \
    ddsrt_mutex_unlock (&g_mutex); \
  }

DEFINE_STATUS_CALLBACK (inconsistent_topic, INCONSISTENT_TOPIC, topic)
DEFINE_STATUS_CALLBACK (liveliness_changed, LIVELINESS_CHANGED, reader)
DEFINE_STATUS_CALLBACK (liveliness_lost, LIVELINESS_LOST, writer)
DEFINE_STATUS_CALLBACK (offered_deadline_missed, OFFERED_DEADLINE_MISSED, writer)
DEFINE_STATUS_CALLBACK (offered_incompatible_qos, OFFERED_INCOMPATIBLE_QOS, writer)
DEFINE_STATUS_CALLBACK (publication_matched, PUBLICATION_MATCHED, writer)
DEFINE_STATUS_CALLBACK (requested_deadline_missed, REQUESTED_DEADLINE_MISSED, reader)
DEFINE_STATUS_CALLBACK (requested_incompatible_qos, REQUESTED_INCOMPATIBLE_QOS, reader)
DEFINE_STATUS_CALLBACK (sample_lost, SAMPLE_LOST, reader)
DEFINE_STATUS_CALLBACK (sample_rejected, SAMPLE_REJECTED, reader)
DEFINE_STATUS_CALLBACK (subscription_matched, SUBSCRIPTION_MATCHED, reader)

static void data_on_readers_cb (dds_entity_t subscriber, void *arg)
{
  (void) arg;
  ddsrt_mutex_lock (&g_mutex);
  cb_subscriber = subscriber;
  cb_called |= DDS_DATA_ON_READERS_STATUS;
  ddsrt_cond_broadcast (&g_cond);
  ddsrt_mutex_unlock (&g_mutex);
}

static void data_available_cb (dds_entity_t reader, void *arg)
{
  (void)arg;
  ddsrt_mutex_lock (&g_mutex);
  cb_reader = reader;
  cb_called |= DDS_DATA_AVAILABLE_STATUS;
  ddsrt_cond_broadcast (&g_cond);
  ddsrt_mutex_unlock (&g_mutex);
}

static void dummy_data_on_readers_cb (dds_entity_t subscriber, void *arg)
{
  (void)subscriber;
  (void)arg;
}

static void dummy_data_available_cb (dds_entity_t reader, void *arg)
{
  (void)reader;
  (void)arg;
}

static void dummy_subscription_matched_cb (dds_entity_t reader, const dds_subscription_matched_status_t status, void *arg)
{
  (void)reader;
  (void)status;
  (void)arg;
}

static void dummy_liveliness_changed_cb (dds_entity_t reader, const dds_liveliness_changed_status_t status, void *arg)
{
  (void)reader;
  (void)status;
  (void)arg;
}

static void dummy_cb (void)
{
  // Used as a listener function in checking merging of listeners,
  // and for that purpose, casting it to whatever function type is
  // required is ok.  It is not supposed to ever be called.
  abort ();
}

#undef DEFINE_STATUS_CALLBACK

/**************************************************
 ****                                          ****
 ****  create/delete/get/set/copy/merge/reset  ****
 ****                                          ****
 **************************************************/

static void set_all_const (dds_listener_t *l, void (*c) (void))
{
  dds_lset_data_available (l, (dds_on_data_available_fn) c);
  dds_lset_data_on_readers (l, (dds_on_data_on_readers_fn) c);
  dds_lset_inconsistent_topic (l, (dds_on_inconsistent_topic_fn) c);
  dds_lset_liveliness_changed (l, (dds_on_liveliness_changed_fn) c);
  dds_lset_liveliness_lost (l, (dds_on_liveliness_lost_fn) c);
  dds_lset_offered_deadline_missed (l, (dds_on_offered_deadline_missed_fn) c);
  dds_lset_offered_incompatible_qos (l, (dds_on_offered_incompatible_qos_fn) c);
  dds_lset_publication_matched (l, (dds_on_publication_matched_fn) c);
  dds_lset_requested_deadline_missed (l, (dds_on_requested_deadline_missed_fn) c);
  dds_lset_requested_incompatible_qos (l, (dds_on_requested_incompatible_qos_fn) c);
  dds_lset_sample_lost (l, (dds_on_sample_lost_fn) c);
  dds_lset_sample_rejected (l, (dds_on_sample_rejected_fn) c);
  dds_lset_subscription_matched (l, (dds_on_subscription_matched_fn) c);
}

static void set_all (dds_listener_t *l)
{
  dds_lset_data_available (l, data_available_cb);
  dds_lset_data_on_readers (l, data_on_readers_cb);
  dds_lset_inconsistent_topic (l, inconsistent_topic_cb);
  dds_lset_liveliness_changed (l, liveliness_changed_cb);
  dds_lset_liveliness_lost (l, liveliness_lost_cb);
  dds_lset_offered_deadline_missed (l, offered_deadline_missed_cb);
  dds_lset_offered_incompatible_qos (l, offered_incompatible_qos_cb);
  dds_lset_publication_matched (l, publication_matched_cb);
  dds_lset_requested_deadline_missed (l, requested_deadline_missed_cb);
  dds_lset_requested_incompatible_qos (l, requested_incompatible_qos_cb);
  dds_lset_sample_lost (l, sample_lost_cb);
  dds_lset_sample_rejected (l, sample_rejected_cb);
  dds_lset_subscription_matched (l, subscription_matched_cb);
}

#define ASSERT_CALLBACK_EQUAL(fntype, listener, expected) \
  do { \
    dds_on_##fntype##_fn cb; \
    dds_lget_##fntype(listener, &cb); \
    CU_ASSERT_EQUAL(cb, expected); \
  } while (0)

static void check_all_const (const dds_listener_t *l, void (*c) (void))
{
  ASSERT_CALLBACK_EQUAL (data_available, l, (dds_on_data_available_fn) c);
  ASSERT_CALLBACK_EQUAL (data_on_readers, l, (dds_on_data_on_readers_fn) c);
  ASSERT_CALLBACK_EQUAL (inconsistent_topic, l, (dds_on_inconsistent_topic_fn) c);
  ASSERT_CALLBACK_EQUAL (liveliness_changed, l, (dds_on_liveliness_changed_fn) c);
  ASSERT_CALLBACK_EQUAL (liveliness_lost, l, (dds_on_liveliness_lost_fn) c);
  ASSERT_CALLBACK_EQUAL (offered_deadline_missed, l, (dds_on_offered_deadline_missed_fn) c);
  ASSERT_CALLBACK_EQUAL (offered_incompatible_qos, l, (dds_on_offered_incompatible_qos_fn) c);
  ASSERT_CALLBACK_EQUAL (publication_matched, l, (dds_on_publication_matched_fn) c);
  ASSERT_CALLBACK_EQUAL (requested_deadline_missed, l, (dds_on_requested_deadline_missed_fn) c);
  ASSERT_CALLBACK_EQUAL (requested_incompatible_qos, l, (dds_on_requested_incompatible_qos_fn) c);
  ASSERT_CALLBACK_EQUAL (sample_lost, l, (dds_on_sample_lost_fn) c);
  ASSERT_CALLBACK_EQUAL (sample_rejected, l, (dds_on_sample_rejected_fn) c);
  ASSERT_CALLBACK_EQUAL (subscription_matched, l, (dds_on_subscription_matched_fn) c);
}

static void check_all (const dds_listener_t *l)
{
  ASSERT_CALLBACK_EQUAL (data_available, l, data_available_cb);
  ASSERT_CALLBACK_EQUAL (data_on_readers, l, data_on_readers_cb);
  ASSERT_CALLBACK_EQUAL (inconsistent_topic, l, inconsistent_topic_cb);
  ASSERT_CALLBACK_EQUAL (liveliness_changed, l, liveliness_changed_cb);
  ASSERT_CALLBACK_EQUAL (liveliness_lost, l, liveliness_lost_cb);
  ASSERT_CALLBACK_EQUAL (offered_deadline_missed, l, offered_deadline_missed_cb);
  ASSERT_CALLBACK_EQUAL (offered_incompatible_qos, l, offered_incompatible_qos_cb);
  ASSERT_CALLBACK_EQUAL (publication_matched, l, publication_matched_cb);
  ASSERT_CALLBACK_EQUAL (requested_deadline_missed, l, requested_deadline_missed_cb);
  ASSERT_CALLBACK_EQUAL (requested_incompatible_qos, l, requested_incompatible_qos_cb);
  ASSERT_CALLBACK_EQUAL (sample_lost, l, sample_lost_cb);
  ASSERT_CALLBACK_EQUAL (sample_rejected, l, sample_rejected_cb);
  ASSERT_CALLBACK_EQUAL (subscription_matched, l, subscription_matched_cb);
}

CU_Test (ddsc_listener, create_and_delete)
{
  dds_listener_t *listener = dds_create_listener (NULL);
  CU_ASSERT_PTR_NOT_NULL_FATAL (listener);
  check_all_const (listener, 0);
  dds_delete_listener (listener);

  // check delete_listeners handles a null pointer gracefully
  dds_delete_listener (NULL);
}

CU_Test (ddsc_listener, reset)
{
  dds_listener_t *listener = dds_create_listener (NULL);
  CU_ASSERT_PTR_NOT_NULL_FATAL (listener);

  set_all (listener);

  // all callbacks should revert to default after reset
  dds_reset_listener (listener);
  check_all_const (listener, 0);
  dds_delete_listener (listener);

  // check reset_listeners handles a null pointer gracefully
  dds_reset_listener (NULL);
}

CU_Test (ddsc_listener, copy)
{
  dds_listener_t *listener1 = dds_create_listener (NULL);
  CU_ASSERT_PTR_NOT_NULL_FATAL (listener1);
  set_all (listener1);

  dds_listener_t *listener2 = dds_create_listener (NULL);
  CU_ASSERT_PTR_NOT_NULL_FATAL (listener2);
  dds_copy_listener (listener2, listener1);
  check_all (listener2);

  // Calling copy with NULL should not crash and be noops
  dds_copy_listener (listener2, NULL);
  dds_copy_listener (NULL, listener1);
  dds_copy_listener (NULL, NULL);

  dds_delete_listener (listener1);
  dds_delete_listener (listener2);
}

CU_Test (ddsc_listener, merge)
{
  dds_listener_t *listener1 = dds_create_listener (NULL);
  CU_ASSERT_PTR_NOT_NULL_FATAL (listener1);
  set_all (listener1);

  // Merging listener1 into empty listener2 be like a copy
  dds_listener_t *listener2 = dds_create_listener (NULL);
  CU_ASSERT_PTR_NOT_NULL_FATAL (listener2);
  dds_merge_listener (listener2, listener1);
  check_all (listener2);

  // Merging listener into a full listener2 should not overwrite anything
  set_all_const (listener2, dummy_cb);
  dds_merge_listener (listener2, listener1);
  check_all_const (listener2, dummy_cb);

  // Using NULLs shouldn't crash and be noops
  dds_merge_listener (listener2, NULL);
  dds_merge_listener (NULL, listener1);
  dds_merge_listener (NULL, NULL);

  dds_delete_listener (listener1);
  dds_delete_listener (listener2);
}

CU_Test(ddsc_listener, getters_setters)
{
  // test all individual cb get/set methods
  dds_listener_t *listener = dds_create_listener (NULL);
  CU_ASSERT_PTR_NOT_NULL_FATAL (listener);

#define TEST_GET_SET(listener, fntype, cb) \
  do { \
    dds_on_##fntype##_fn dummy = NULL; \
    /* Initially expect DDS_LUNSET on a newly created listener */ \
    ASSERT_CALLBACK_EQUAL (fntype, listener, 0); \
    /* Using listener or callback NULL, shouldn't crash and be noop */ \
    dds_lset_##fntype (NULL, NULL); \
    dds_lget_##fntype (NULL, NULL); \
    dds_lget_##fntype (listener, NULL); \
    dds_lget_##fntype (NULL, &dummy);  \
    CU_ASSERT_EQUAL_FATAL (dummy, NULL); \
    /* Set to NULL, get to confirm it succeeds */ \
    dds_lset_##fntype (listener, NULL); \
    ASSERT_CALLBACK_EQUAL (fntype, listener, NULL); \
    /* Set to a proper cb method, get to confirm it succeeds */ \
    dds_lset_##fntype (listener, cb); \
    ASSERT_CALLBACK_EQUAL (fntype, listener, cb); \
  } while (0)
  TEST_GET_SET (listener, data_available, data_available_cb);
  TEST_GET_SET (listener, data_on_readers, data_on_readers_cb);
  TEST_GET_SET (listener, inconsistent_topic, inconsistent_topic_cb);
  TEST_GET_SET (listener, liveliness_changed, liveliness_changed_cb);
  TEST_GET_SET (listener, liveliness_lost, liveliness_lost_cb);
  TEST_GET_SET (listener, offered_deadline_missed, offered_deadline_missed_cb);
  TEST_GET_SET (listener, offered_incompatible_qos, offered_incompatible_qos_cb);
  TEST_GET_SET (listener, publication_matched, publication_matched_cb);
  TEST_GET_SET (listener, requested_deadline_missed, requested_deadline_missed_cb);
  TEST_GET_SET (listener, requested_incompatible_qos, requested_incompatible_qos_cb);
  TEST_GET_SET (listener, sample_lost, sample_lost_cb);
  TEST_GET_SET (listener, sample_rejected, sample_rejected_cb);
  TEST_GET_SET (listener, subscription_matched, subscription_matched_cb);
#undef TEST_GET_SET

  dds_delete_listener (listener);
}

#undef ASSERT_CALLBACK_EQUAL

/**************************************************
 ****                                          ****
 ****  programmable listener checker           ****
 ****                                          ****
 **************************************************/

// These had better match the corresponding type definitions!
// n   uint32_t ...count
// c   int32_t  ...count_change
// I            instance handle of a data instance
// P   uint32_t QoS policy ID
// E            instance handle of an entity
// R            sample_rejected_status_kind
static const struct {
  size_t size;                   // size of status struct
  const char *desc;              // description of status struct
  uint32_t mask;                 // status mask, bit in "cb_called"
  const dds_entity_t *cb_entity; // which cb_... entity to look at
  const void *cb_status;         // cb_..._status to look at
} lldesc[] = {
  { 0, NULL, DDS_DATA_AVAILABLE_STATUS, &cb_reader, NULL }, // data available
  { 0, NULL, DDS_DATA_ON_READERS_STATUS, &cb_subscriber, NULL }, // data on readers
  { sizeof (dds_inconsistent_topic_status_t), "nc", DDS_INCONSISTENT_TOPIC_STATUS, &cb_topic, &cb_inconsistent_topic_status },
  { sizeof (dds_liveliness_changed_status_t), "nnccE", DDS_LIVELINESS_CHANGED_STATUS, &cb_reader, &cb_liveliness_changed_status },
  { sizeof (dds_liveliness_lost_status_t), "nc", DDS_LIVELINESS_LOST_STATUS, &cb_writer, &cb_liveliness_lost_status },
  { sizeof (dds_offered_deadline_missed_status_t), "ncI", DDS_OFFERED_DEADLINE_MISSED_STATUS, &cb_writer, &cb_offered_deadline_missed_status },
  { sizeof (dds_offered_incompatible_qos_status_t), "ncP", DDS_OFFERED_INCOMPATIBLE_QOS_STATUS, &cb_writer, &cb_offered_incompatible_qos_status },
  { sizeof (dds_publication_matched_status_t), "ncncE", DDS_PUBLICATION_MATCHED_STATUS, &cb_writer, &cb_publication_matched_status },
  { sizeof (dds_requested_deadline_missed_status_t), "ncI", DDS_REQUESTED_DEADLINE_MISSED_STATUS, &cb_reader, &cb_requested_deadline_missed_status },
  { sizeof (dds_requested_incompatible_qos_status_t), "ncP", DDS_REQUESTED_INCOMPATIBLE_QOS_STATUS, &cb_reader, &cb_requested_incompatible_qos_status },
  { sizeof (dds_sample_lost_status_t), "nc", DDS_SAMPLE_LOST_STATUS, &cb_reader, &cb_sample_lost_status },
  { sizeof (dds_sample_rejected_status_t), "ncRI", DDS_SAMPLE_REJECTED_STATUS, &cb_reader, &cb_sample_rejected_status },
  { sizeof (dds_subscription_matched_status_t), "ncncE", DDS_SUBSCRIPTION_MATCHED_STATUS, &cb_reader, &cb_subscription_matched_status }
};

static const void *advance (const void *status, size_t *off, char code)
{
#define alignof(type_) offsetof (struct { char c; type_ d; }, d)
  size_t align = 1, size = 1;
  switch (code)
  {
    case 'n': case 'c': case 'P':
      align = alignof (uint32_t); size = sizeof (uint32_t);
      break;
    case 'E': case 'I':
      align = alignof (dds_instance_handle_t); size = sizeof (dds_instance_handle_t);
      break;
    case 'R':
      align = alignof (dds_sample_rejected_status_kind); size = sizeof (dds_sample_rejected_status_kind);
      break;
    default:
      abort ();
  }
#undef alignof
  *off = (*off + align - 1) & ~(align - 1);
  const void *p = (const char *) status + *off;
  *off += size;
  return p;
}

static void get_status (int ll, dds_entity_t ent, void *status)
{
  dds_return_t ret;
  switch (ll)
  {
    case 2: ret = dds_get_inconsistent_topic_status (ent, status); break;
    case 3: ret = dds_get_liveliness_changed_status (ent, status); break;
    case 4: ret = dds_get_liveliness_lost_status (ent, status); break;
    case 5: ret = dds_get_offered_deadline_missed_status (ent, status); break;
    case 6: ret = dds_get_offered_incompatible_qos_status (ent, status); break;
    case 7: ret = dds_get_publication_matched_status (ent, status); break;
    case 8: ret = dds_get_requested_deadline_missed_status (ent, status); break;
    case 9: ret = dds_get_requested_incompatible_qos_status (ent, status); break;
    case 10: ret = dds_get_sample_lost_status (ent, status); break;
    case 11: ret = dds_get_sample_rejected_status (ent, status); break;
    case 12: ret = dds_get_subscription_matched_status (ent, status); break;
    default: abort ();
  }
  CU_ASSERT_FATAL (ret == 0);
}

static void assert_status_change_fields_are_0 (int ll, dds_entity_t ent)
{
  if (lldesc[ll].desc)
  {
    const char *d = lldesc[ll].desc;
    void *status = malloc (lldesc[ll].size);
    get_status (ll, ent, status);
    size_t off = 0;
    while (*d)
    {
      const uint32_t *p = advance (status, &off, *d);
      if (*d == 'c')
        CU_ASSERT_FATAL (*p == 0);
      d++;
    }
    assert (off <= lldesc[ll].size);
    free (status);
  }
}

static int getentity (const char *tok, bool *isbang, bool *ishash)
{
  static const char *known = "PRWrstwxy";
  const char *p;
  if (isbang)
    *isbang = false;
  if (ishash)
    *ishash = false;
  if ((p = strchr (known, *tok)) == NULL)
    return -1;
  int ent = (int) (p - known);
  if (*++tok == 0)
    return ent;
  if (*tok == '\'')
  {
    ent += (int) strlen (known);
    tok++;
  }
  while (*tok == '!' || *tok == '#')
  {
    if (strchr (known + 3, *p) == NULL)
      return -1; // only readers, writers
    if (*tok == '!' && isbang)
      *isbang = true;
    else if (*tok == '#' && ishash)
      *ishash = true;
    tok++;
  }
  return (*tok == 0) ? ent : -1;
}

static int getlistener (const char *tok, bool *isbang)
{
  // note: sort order is on full name (so sample rejected precedes subscription matched)
  static const char *ls[] = {
    "da", "dor", "it", "lc", "ll", "odm", "oiq", "pm", "rdm", "riq", "sl", "sr", "sm"
  };
  if (isbang)
    *isbang = false;
  for (size_t i = 0; i < sizeof (ls) / sizeof (*ls); i++)
  {
    size_t n = strlen (ls[i]);
    if (strncmp (tok, ls[i], n) == 0 && (tok[n] == 0 || tok[n+1] == ','))
    {
      if (isbang)
        *isbang = (tok[n] == '!');
      return (int) i;
    }
  }
  return -1;
}

struct ents {
  dds_entity_t es[2 * 9];
  dds_entity_t tps[2];
  dds_entity_t doms[2];
  dds_instance_handle_t esi[2 * 9];
  // built-in topic readers for cross-referencing instance handles
  dds_entity_t pubrd[2];
  dds_entity_t subrd[2];
};

static void make_participant (struct ents *es, const char *topicname, int ent, const dds_qos_t *qos, dds_listener_t *list)
{
  const dds_domainid_t domid = (ent < 9) ? 0 : 1;
  char *conf = ddsrt_expand_envvars ("${CYCLONEDDS_URI}${CYCLONEDDS_URI:+,}<Discovery><ExternalDomainId>0</ExternalDomainId></Discovery>", domid);
  printf ("create domain %"PRIu32, domid);
  fflush (stdout);
  es->doms[domid] = dds_create_domain (domid, conf);
  CU_ASSERT_FATAL (es->doms[domid] > 0);
  ddsrt_free (conf);
  printf (" create participant P%s", (ent < 9) ? "" : "'");
  fflush (stdout);
  es->es[ent] = dds_create_participant (domid, NULL, list);
  CU_ASSERT_FATAL (es->es[ent] > 0);
  es->tps[domid] = dds_create_topic (es->es[ent], &Space_Type1_desc, topicname, qos, NULL);
  CU_ASSERT_FATAL (es->tps[domid] > 0);

  // Create the built-in topic readers with a dummy listener to avoid any event (data available comes to mind)
  // from propagating to the normal data available listener, in case it has been set on the participant.
  //
  // - dummy_cb aborts when it is invoked, but all reader-related listeners that can possibly trigger are set
  //   separately (incompatible qos, deadline missed, sample lost and sample rejected are all impossible by
  //   construction)
  // - regarding data_on_readers: Cyclone handles listeners installed on an ancestor by *inheriting* them,
  //   rather than by walking up ancestor chain. Setting data_on_readers on the reader therefore overrides the
  //   listener set on the subscriber. It is a nice feature!
  dds_listener_t *dummylist = dds_create_listener (NULL);
  set_all_const (dummylist, dummy_cb);
  dds_lset_data_available (dummylist, dummy_data_available_cb);
  dds_lset_data_on_readers (dummylist, dummy_data_on_readers_cb);
  dds_lset_subscription_matched (dummylist, dummy_subscription_matched_cb);
  dds_lset_liveliness_changed (dummylist, dummy_liveliness_changed_cb);
  es->pubrd[domid] = dds_create_reader (es->es[ent], DDS_BUILTIN_TOPIC_DCPSPUBLICATION, NULL, dummylist);
  CU_ASSERT_FATAL (es->pubrd[domid] > 0);
  es->subrd[domid] = dds_create_reader (es->es[ent], DDS_BUILTIN_TOPIC_DCPSSUBSCRIPTION, NULL, dummylist);
  CU_ASSERT_FATAL (es->subrd[domid] > 0);
  dds_delete_listener (dummylist);
  printf ("pubrd %"PRId32" subrd %"PRId32" sub %"PRId32"\n", es->pubrd[domid], es->subrd[domid], dds_get_parent (es->pubrd[domid]));
}

static void make_entity1 (struct ents *es, const char *topicname, int ent, bool isbang, bool ishash, const dds_qos_t *qos, dds_qos_t *rwqos, dds_listener_t *list)
{
  dds_return_t ret;
  switch (ent)
  {
    case 0: case 9:
      make_participant (es, topicname, ent, qos, list);
      break;
    case 1: case 10:
      if (es->es[ent-1] == 0)
      {
        printf ("[");
        make_entity1 (es, topicname, ent-1, false, false, qos, rwqos, NULL);
        printf ("] ");
      }
      printf ("create subscriber R%s", (ent < 9) ? "" : "'");
      fflush (stdout);
      es->es[ent] = dds_create_subscriber (es->es[ent-1], NULL, list);
      break;
    case 2: case 11:
      if (es->es[ent-2] == 0)
      {
        printf ("[");
        make_entity1 (es, topicname, ent-2, false, false, qos, rwqos, NULL);
        printf ("] ");
      }
      printf ("create publisher W%s", (ent < 9) ? "" : "'");
      fflush (stdout);
      es->es[ent] = dds_create_publisher (es->es[ent-2], NULL, list);
      break;
    case 3: case 4: case 5: case 12: case 13: case 14:
      if (es->es[ent < 9 ? 1 : 10] == 0)
      {
        printf ("[");
        make_entity1 (es, topicname, ent < 9 ? 1 : 10, false, false, qos, rwqos, NULL);
        printf ("] ");
      }
      printf ("create %s reader %c%s", isbang ? "best-effort" : "reliable", 'r' + (ent < 9 ? ent-3 : ent-12), (ent < 9) ? "" : "'");
      fflush (stdout);
      dds_reset_qos (rwqos);
      if (isbang)
        dds_qset_reliability (rwqos, DDS_RELIABILITY_BEST_EFFORT, DDS_MSECS (100));
      if (ishash)
        dds_qset_resource_limits (rwqos, 1, DDS_LENGTH_UNLIMITED, DDS_LENGTH_UNLIMITED);
      es->es[ent] = dds_create_reader (es->es[ent < 9 ? 1 : 10], es->tps[ent < 9 ? 0 : 1], rwqos, list);
      break;
    case 6: case 7: case 8: case 15: case 16: case 17:
      if (es->es[ent < 9 ? 2 : 11] == 0)
      {
        printf ("[");
        make_entity1 (es, topicname, ent < 9 ? 2 : 11, false, false, qos, rwqos, NULL);
        printf ("] ");
      }
      printf ("create %s writer %c%s", isbang ? "best-effort" : "reliable", 'w' + (ent < 9 ? ent-6 : ent-15), (ent < 9) ? "" : "'");
      fflush (stdout);
      dds_reset_qos (rwqos);
      if (isbang)
        dds_qset_reliability (rwqos, DDS_RELIABILITY_BEST_EFFORT, DDS_MSECS (100));
      if (ishash)
        dds_qset_resource_limits (rwqos, 1, DDS_LENGTH_UNLIMITED, DDS_LENGTH_UNLIMITED);
      es->es[ent] = dds_create_writer (es->es[ent < 9 ? 2 : 11], es->tps[ent < 9 ? 0 : 1], rwqos, list);
      break;
    default:
      abort ();
  }
  printf (" = %"PRId32, es->es[ent]);
  fflush (stdout);
  CU_ASSERT_FATAL (es->es[ent] > 0);
  ret = dds_get_instance_handle (es->es[ent], &es->esi[ent]);
  //printf (" %"PRIx64, es->esi[ent]);
  //fflush (stdout);
  CU_ASSERT_FATAL (ret == 0);
}

static void make_entity (struct ents *es, const char *topicname, int ent, bool isbang, bool ishash, const dds_qos_t *qos, dds_qos_t *rwqos, dds_listener_t *list)
{
  make_entity1 (es, topicname, ent, isbang, ishash, qos, rwqos, list);
  printf ("\n");
}

static char *strsep_noempty (char **cursor, const char *sep)
{
  char *tok;
  while ((tok = ddsrt_strsep (cursor, sep)) != NULL && *tok == 0) { }
  return tok;
}

static dds_instance_handle_t lookup_insthandle (const struct ents *es, int ent, int ent1)
{
  // if both are in the same domain, it's easy
  if (ent / 9 == ent1 / 9)
    return es->esi[ent1];
  else
  {
    // if they aren't ... find GUID from instance handle in the one domain,
    // then find instance handle for GUID in the other
    dds_entity_t rd1 = 0, rd2 = 0;
    switch (ent1)
    {
      case  3: case  4: case  5: rd1 = es->subrd[0]; rd2 = es->subrd[1]; break;
      case 12: case 13: case 14: rd1 = es->subrd[1]; rd2 = es->subrd[0]; break;
      case  6: case  7: case  8: rd1 = es->pubrd[0]; rd2 = es->pubrd[1]; break;
      case 15: case 16: case 17: rd1 = es->pubrd[1]; rd2 = es->pubrd[0]; break;
      default: abort ();
    }

    dds_return_t ret;
    dds_builtintopic_endpoint_t keysample;
    //printf ("(in %"PRId32" %"PRIx64" -> ", rd1, es->esi[ent1]);
    //fflush (stdout);
    ret = dds_instance_get_key (rd1, es->esi[ent1], &keysample);
    CU_ASSERT_FATAL (ret == 0);
    // In principle, only key fields are set in sample returned by get_key;
    // in the case of a built-in topic that is extended to the participant
    // key. The qos and topic/type names should not be set, and there is no
    // (therefore) memory allocated for the sample.
    CU_ASSERT_FATAL (keysample.qos == NULL);
    CU_ASSERT_FATAL (keysample.topic_name == NULL);
    CU_ASSERT_FATAL (keysample.type_name == NULL);
    //for (size_t j = 0; j < sizeof (keysample.key.v); j++)
    //  printf ("%s%02x", (j > 0 && j % 4 == 0) ? ":" : "", keysample.key.v[j]);
    const dds_instance_handle_t ih = dds_lookup_instance (rd2, &keysample);
    CU_ASSERT_FATAL (ih != 0);
    //printf (" -> %"PRIx64")", ih);
    //fflush (stdout);
    return ih;
  }
}

static void checkstatus (int ll, const struct ents *es, int ent, const char *args, const void *status)
{
  DDSRT_WARNING_MSVC_OFF(4996); // use of sscanf triggers a warning
  if (*args == 0)
    return;
  if (*args++ != '(')
    abort ();
  assert (lldesc[ll].desc != NULL);
  const char *d = lldesc[ll].desc;
  const char *sep = "(";
  size_t off = 0;
  while (*d)
  {
    const void *p = advance (status, &off, *d);
    char str[32];
    unsigned u;
    int i, pos = -1;
    switch (*d)
    {
      case 'n':
        if (sscanf (args, "%u%n", &u, &pos) != 1 || (args[pos] != ',' && args[pos] != ')'))
          abort ();
        printf ("%s%"PRIu32" %u", sep, *(uint32_t *)p, u); fflush (stdout);
        CU_ASSERT_FATAL (*(uint32_t *)p == u);
        break;
      case 'c':
        if (sscanf (args, "%d%n", &i, &pos) != 1 || (args[pos] != ',' && args[pos] != ')'))
          abort ();
        printf ("%s%"PRId32" %d", sep, *(int32_t *)p, i); fflush (stdout);
        CU_ASSERT_FATAL (*(int32_t *)p == i);
        break;
      case 'P': // policy id: currently fixed at reliability
        pos = -1; // not actually consuming an argument
        printf ("%s%"PRIu32" %d", sep, *(uint32_t *)p, (int) DDS_RELIABILITY_QOS_POLICY_ID); fflush (stdout);
        CU_ASSERT_FATAL (*(uint32_t *)p == (uint32_t) DDS_RELIABILITY_QOS_POLICY_ID);
        break;
      case 'R':
        if (sscanf (args, "%31[^,)]%n", str, &pos) != 1 || (args[pos] != ',' && args[pos] != ')'))
          abort ();
        if (strcmp (str, "i") == 0)
          i = (int) DDS_REJECTED_BY_INSTANCES_LIMIT;
        else if (strcmp (str, "s") == 0)
          i = (int) DDS_REJECTED_BY_SAMPLES_LIMIT;
        else if (strcmp (str, "spi") == 0)
          i = (int) DDS_REJECTED_BY_SAMPLES_PER_INSTANCE_LIMIT;
        else
          abort ();
        printf ("%s%d %d", sep, (int) *(dds_sample_rejected_status_kind *)p, i); fflush (stdout);
        CU_ASSERT_FATAL (*(dds_sample_rejected_status_kind *)p == (dds_sample_rejected_status_kind) i);
        break;
      case 'I': // instance handle is too complicated
        pos = -1; // not actually consuming an argument
        break;
      case 'E': {
        int ent1 = -1;
        dds_instance_handle_t esi1 = 0;
        if (sscanf (args, "%31[^,)]%n", str, &pos) != 1 || (args[pos] != ',' && args[pos] != ')'))
          abort ();
        if (strcmp (str, "*") != 0 && (ent1 = getentity (str, NULL, NULL)) < 0)
          abort ();
        if (ent1 != -1)
          esi1 = lookup_insthandle (es, ent, ent1);
        printf ("%s%"PRIx64" %"PRIx64, sep, *(dds_instance_handle_t *)p, esi1); fflush (stdout);
        CU_ASSERT_FATAL (ent1 == -1 || *(dds_instance_handle_t *)p == esi1);
        break;
      }
      default: abort ();
    }
    args += pos + 1;
    sep = ", ";
    d++;
  }
  printf (")");
  assert (*args == 0);
  assert (off <= lldesc[ll].size);
  DDSRT_WARNING_MSVC_ON(4996);
}

/** @brief run a "test" consisting of a sequence of simplish operations
 *
 * This operation takes a test description, really a program in a bizarre syntax, and executes it.  Any failures,
 * be it because of error codes coming out of the Cyclone calls or expected values being wrong cause it to fail
 * the test via CU_ASSERT_FATAL. While it is doing this, it outputs the test steps to stdout including some
 * actual values. An invalid program is mostly reported by calling abort(). It is geared towards checking for
 * listener invocations and the effects on statuses.
 *
 * Entities in play:
 *
 * - participants:   P      P'
 * - subscribers:    R      R'
 * - publishers:     W      W'
 * - readers:        r s t  r' s' t'
 * - writers:        w x y  w' x' y'
 *
 * The unprimed ones exist in domain 0, the primed ones in domain 1 (but configured such that it talks to
 * domain 0), so that network-related listener invocations can be checked as well.
 *
 * The first mention of an entity creates it as well as its ancestors.  Implicitly created ancestors always have
 * standard QoS and have no listeners. There is one topic that is created implicitly when the participant is
 * created.
 *
 * Standard QoS is: default + reliable (100ms), by-source-timestamp, keep-all.
 * The QoS of a reader/writer can be altered at the first mention of it by suffixing its name with "!" and/or "#"
 * (the apostrophe is part of the name, so w#! or r'! are valid). Those suffixes are ignored if the entity
 * already exists.
 *
 * A program consists of a sequence of operations separated by whitespace, ';' or '/' (there is no meaning to the
 * separators, they exist to allow visual grouping):
 *
 * PROGRAM     ::= (OP (\s+|[/;])*)*
 *
 * OP          ::= (LISTENER)* ENTITY-NAME
 *                       if entity ENTITY-NAME does not exist:
 *                         creates the entity with the given listeners installed
 *                       else
 *                         changes the entity's listeners to the specified ones
 *                       (see above for the valid ENTITY-NAMEs)
 *               | -ENTITY-NAME
 *                       deletes the specified entity
 *               | WRITE-LIKE[fail][@DT] KEY <entity name>
 *                       writes/disposes/unregisters key KEY (an integer), if "fail" is appended, the
 *                       expectation is that it fails with a timeout, if @DT is appended, the timestamp is the
 *                       start time of the test + <dt>s rather than the current time; DT is a floating-point
 *                       number
 *               | READ-LIKE[(A,B))] <entity name>
 *                       reads/takes at most 10 samples, counting the number of valid and invalid samples seen
 *                       and checking it against A and B if given
 *               | ?LISTENER[(ARGS)] <entity name>
 *                       waits until the specified listener has been invoked on <entity name> using a flag set
 *                       by the listener function, resets the flag and verifies that neither the entity status
 *                       bit nor the "change" fields in the various statuses were set
 *                       ARGS is used to check the status argument most recently passed to the listener:
 *                         it        (A,B) verifies count and change match A and B, policy matches RELIABILITY
 *                         lc        (A,B,C,D,E) verifies that alive and not-alive counts match A and B, that
 *                                   alive and not-alive changes match C and D and that the last handle matches
 *                                   E if an entity name (ignored if E = "*")
 *                         ll        (A,B) verifies count and change match A and B
 *                         odm       (A,B) verifies count and change match A and B, last handle is ignored
 *                         oiq       (A,B) verifies that total count and change match A and B and that the
 *                                   mismatching QoS is reliability (the only one that can for now)
 *                         pm        (A,B,C,D,E) verifies that total count and change match A and B, that
 *                                   current count and change match C and D and that the last handle matches E
 *                                   if an entity name (ignored if E = "*")
 *                         rdm       see odm
 *                         riq       see oiq
 *                         sl        (A,B) verifies that total count and change match A and B
 *                         sr        (A,B,C) verifies total count and change match A and B, and that the reason
 *                                   matches C (one of "s" for samples, "i" for instances, "spi" for samples
 *                                   per instance)
 *                         sm        see pm
 *               | ?!LISTENER
 *                       (not listener) tests that LISTENER has not been invoked since last reset
 *               | sleep D
 *                       delay program execution for D s (D is a floating-point number)
 * WRITE-LIKE  ::= wr    write
 *               | wrdisp  write-dispose
 *               | disp  dispose
 *               | unreg unregister
 * READ-LIKE   ::= read  dds_read (so any state)
 *               | take  dds_take (so any state)
 * LISTENER    ::= da    data available (acts on a reader)
 *               | dor   data on readers (acts on a subcsriber)
 *               | it    incompatible topic (acts on a topic)
 *               | lc    liveliness changed (acts on a reader)
 *               | ll    liveliness lost (acts on a writer)
 *               | odm   offered deadline missed (acts on a writer)
 *               | oiq   offered incompatible QoS (acts on a writer)
 *               | pm    publication matched (acts on a writer)
 *               | rdm   requested deadline missed (acts on a reader)
 *               | riq   requested incompatible QoS (acts on a reader)
 *               | sl    sample lost (acts on a reader)
 *               | sr    sample rejected (acts on a reader)
 *               | sm    subscription matched (acts on a reader)
 *
 * All entities share the listeners with their global state. Only the latest invocation is visible.
 *
 * @param[in]  ops  Program to execute.
 */
static void dotest (const char *ops)
{
  DDSRT_WARNING_MSVC_OFF(4996); // use of sscanf triggers a warning
  static const char *sep = " /;\n\t\r\v";
  char *opscopy = ddsrt_strdup (ops), *cursor = opscopy, *tok;
  struct ents es;
  dds_return_t ret;
  Space_Type1 sample;
  char topicname[100];
  dds_qos_t *qos = dds_create_qos (), *rwqos = dds_create_qos ();
  dds_listener_t *list = dds_create_listener (NULL);
  const dds_time_t tref = dds_time ();
  CU_ASSERT_FATAL (qos != NULL);
  CU_ASSERT_FATAL (rwqos != NULL);
  CU_ASSERT_FATAL (list != NULL);
  dds_qset_reliability (qos, DDS_RELIABILITY_RELIABLE, DDS_MSECS (100));
  dds_qset_destination_order (qos, DDS_DESTINATIONORDER_BY_SOURCE_TIMESTAMP);
  dds_qset_history (qos, DDS_HISTORY_KEEP_ALL, 0);
  memset (&es, 0, sizeof (es));
  memset (&sample, 0, sizeof (sample));

  ddsrt_mutex_init (&g_mutex);
  ddsrt_cond_init (&g_cond);
  ddsrt_mutex_lock (&g_mutex);
  cb_called = 0;
  ddsrt_mutex_unlock (&g_mutex);

  create_unique_topic_name ("ddsc_listener_test", topicname, 100);
  printf ("dotest: %s\n", ops);
  printf ("topic: %s\n", topicname);
  while ((tok = strsep_noempty (&cursor, sep)) != NULL)
  {
    int ent, ll;
    bool isbang, ishash;
    if ((ent = getentity (tok, &isbang, &ishash)) >= 0)
    {
      make_entity (&es, topicname, ent, isbang, ishash, qos, rwqos, NULL);
    }
    else if (*tok == '-' && (ent = getentity (tok + 1, NULL, NULL)) >= 0)
    {
      // delete deliberately leaves the instance handle in place for checking
      // the publication/subscription handle in subscription matched/publication
      // matched for a lost match
      printf ("delete %"PRId32"\n", es.es[ent]);
      ret = dds_delete (es.es[ent]);
      CU_ASSERT_FATAL (ret == 0);
      es.es[ent] = 0;
    }
    else if ((ll = getlistener (tok, &isbang)) >= 0)
    {
      printf ("set listener:");
      dds_reset_listener (list);
      do {
        printf (" %s", tok);
        switch (ll)
        {
          case 0: dds_lset_data_available (list, isbang ? 0 : data_available_cb); break;
          case 1: dds_lset_data_on_readers (list, isbang ? 0 : data_on_readers_cb); break;
          case 2: dds_lset_inconsistent_topic (list, isbang ? 0: inconsistent_topic_cb); break;
          case 3: dds_lset_liveliness_changed (list, isbang ? 0 : liveliness_changed_cb); break;
          case 4: dds_lset_liveliness_lost (list, isbang ? 0 : liveliness_lost_cb); break;
          case 5: dds_lset_offered_deadline_missed (list, isbang ? 0 : offered_deadline_missed_cb); break;
          case 6: dds_lset_offered_incompatible_qos (list, isbang ? 0 : offered_incompatible_qos_cb); break;
          case 7: dds_lset_publication_matched (list, isbang ? 0 : publication_matched_cb); break;
          case 8: dds_lset_requested_deadline_missed (list, isbang ? 0 : requested_deadline_missed_cb); break;
          case 9: dds_lset_requested_incompatible_qos (list, isbang ? 0 : requested_incompatible_qos_cb); break;
          case 10: dds_lset_sample_lost (list, isbang ? 0 : sample_lost_cb); break;
          case 11: dds_lset_sample_rejected (list, isbang ? 0 : sample_rejected_cb); break;
          case 12: dds_lset_subscription_matched (list, isbang ? 0 : subscription_matched_cb); break;
          default: abort ();
        }
      } while ((tok = strsep_noempty (&cursor, sep)) != NULL && (ll = getlistener (tok, &isbang)) >= 0);
      if (tok == NULL || (ent = getentity (tok, &isbang, &ishash)) < 0)
        abort ();
      if (es.es[ent] == 0)
      {
        printf (" for ");
        make_entity (&es, topicname, ent, isbang, ishash, qos, rwqos, list);
      }
      else
      {
        dds_listener_t *tmplist = dds_create_listener (NULL);
        CU_ASSERT_FATAL (tmplist != NULL);
        ret = dds_get_listener (es.es[ent], tmplist);
        CU_ASSERT_FATAL (ret == 0);
        dds_merge_listener (list, tmplist);
        dds_delete_listener (tmplist);
        printf (" on entity %"PRId32"\n", es.es[ent]);
        ret = dds_set_listener (es.es[ent], list);
        CU_ASSERT_FATAL (ret == 0);
      }
    }
    else if (strncmp (tok, "wr", 2) == 0 || strncmp (tok, "disp", 4) == 0 || strncmp (tok, "unreg", 5) == 0)
    {
      dds_return_t (*fn) (dds_entity_t wr, const void *sample, dds_time_t ts) = 0;
      double dt = 0.0;
      dds_time_t ts = dds_time ();
      char *cmd = tok;
      bool expectfail = false;
      int off, pos, key;
      if ((tok = strsep_noempty (&cursor, sep)) == NULL)
        abort ();
      if (sscanf (tok, "%d%n", &key, &pos) != 1 || tok[pos] != 0)
        abort ();
      if ((tok = strsep_noempty (&cursor, sep)) == NULL || (ent = getentity (tok, &isbang, &ishash)) < 0)
        abort ();
      if (es.es[ent] == 0)
        make_entity (&es, topicname, ent, isbang, ishash, qos, rwqos, NULL);
      switch (cmd[0])
      {
        case 'w':
          if (strncmp (cmd + 2, "disp", 4) == 0) {
            off = 6; fn = dds_writedispose_ts;
          } else {
            off = 2; fn = dds_write_ts;
          }
          break;
        case 'd': off = 4; fn = dds_dispose_ts; break;
        case 'u': off = 5; fn = dds_unregister_instance_ts; break;
        default: abort ();
      }
      if (strncmp (cmd + off, "fail", 4) == 0)
      {
        expectfail = true;
        off += 4;
      }
      if (cmd[off] == '@')
      {
        if (sscanf (cmd + off, "@%lf%n", &dt, &pos) != 1 || cmd[off + pos] != 0)
          abort ();
        ts = tref + (dds_time_t) (dt * 1e9);
      }
      sample.long_1 = key;
      printf ("entity %"PRId32": %*.*s@%"PRId64".%09"PRId64" %d\n", es.es[ent], off, off, cmd, ts / DDS_NSECS_IN_SEC, ts % DDS_NSECS_IN_SEC, key);
      ret = fn (es.es[ent], &sample, ts);
      if (expectfail) {
        CU_ASSERT_FATAL (ret == DDS_RETCODE_TIMEOUT);
      } else {
        CU_ASSERT_FATAL (ret == 0);
      }
    }
    else if (strncmp (tok, "take", 4) == 0 || strncmp(tok, "read", 4) == 0)
    {
      char *args = (tok[4] ? tok + 4 : NULL);
      int exp_nvalid = -1, exp_ninvalid = -1, pos;
      dds_return_t (*fn) (dds_entity_t, void **buf, dds_sample_info_t *, size_t, uint32_t);
      fn = (strncmp (tok, "take", 4) == 0) ? dds_take : dds_read;
      assert (args == NULL || *args == '(');
      if (args && (sscanf (args, "(%d,%d)%n", &exp_nvalid, &exp_ninvalid, &pos) != 2 || args[pos] != 0))
        abort ();
      if ((tok = strsep_noempty (&cursor, sep)) == NULL || (ent = getentity (tok, &isbang, &ishash)) < 0)
        abort ();
      if (es.es[ent] == 0)
        make_entity (&es, topicname, ent, isbang, ishash, qos, rwqos, NULL);
      printf ("entity %"PRId32": %s", es.es[ent], (fn == dds_take) ? "take" : "read");
      fflush (stdout);
      void *raw[10] = { NULL };
      dds_sample_info_t si[10];
      const uint32_t maxs = (uint32_t) (sizeof (raw) / sizeof (raw[0]));
      int count[2] = { 0, 0 };
      ret = fn (es.es[ent], raw, si, maxs, maxs);
      CU_ASSERT_FATAL (ret >= 0);
      for (int32_t i = 0; i < ret; i++)
        count[si[i].valid_data]++;
      ret = dds_return_loan (es.es[ent], raw, ret);
      CU_ASSERT_FATAL (ret == 0);
      printf (" valid %d %d invalid %d %d\n", count[1], exp_nvalid, count[0], exp_ninvalid);
      if (exp_nvalid >= 0)
        CU_ASSERT_FATAL (count[1] == exp_nvalid);
      if (exp_ninvalid >= 0)
        CU_ASSERT_FATAL (count[0] == exp_ninvalid);
    }
    else if (tok[0] == '?')
    {
      const bool expectclear = (tok[1] == '!');
      const char *llname = tok + (expectclear ? 2 : 1);
      char *checkargs;
      if ((checkargs = strchr (llname, '(')) != NULL)
        *checkargs = 0; // clear so getlistener groks the input
      if ((ll = getlistener (llname, NULL)) < 0)
        abort ();
      if (expectclear)
      {
        printf ("listener %s: check not called", llname);
        fflush (stdout);
        ddsrt_mutex_lock (&g_mutex);
        printf (" cb_called %"PRIx32" %s\n", cb_called, (cb_called & lldesc[ll].mask) == 0 ? "ok" : "fail");
        CU_ASSERT_FATAL ((cb_called & lldesc[ll].mask) == 0);
        ddsrt_mutex_unlock (&g_mutex);
      }
      else
      {
        bool signalled = true;
        uint32_t status;
        if ((tok = strsep_noempty (&cursor, sep)) == NULL || (ent = getentity (tok, &isbang, &ishash)) < 0)
          abort ();
        if (es.es[ent] == 0)
          make_entity (&es, topicname, ent, isbang, ishash, qos, rwqos, NULL);
        if ((size_t) ll >= sizeof (lldesc) / sizeof (*lldesc))
          abort ();
        printf ("listener %s: check called for entity %"PRId32, llname, es.es[ent]);
        fflush (stdout);
        ddsrt_mutex_lock (&g_mutex);
        while ((cb_called & lldesc[ll].mask) == 0 && signalled)
          signalled = ddsrt_cond_waitfor (&g_cond, &g_mutex, DDS_SECS (5));
        printf (" cb_called %"PRIx32" (%s)", cb_called, (cb_called & lldesc[ll].mask) != 0 ? "ok" : "fail");
        fflush (stdout);
        CU_ASSERT_FATAL ((cb_called & lldesc[ll].mask) != 0);
        printf (" cb_entity %"PRId32" %"PRId32" (%s)", *lldesc[ll].cb_entity, es.es[ent], (*lldesc[ll].cb_entity == es.es[ent]) ? "ok" : "fail");
        fflush (stdout);
        CU_ASSERT_FATAL (*lldesc[ll].cb_entity == es.es[ent]);
        if (!(es.doms[0] && es.doms[1]))
        {
          // FIXME: two domains: listener invocation happens on another thread and we can observe non-0 "change" fields
          // they get updated, listener gets invoked, then they get reset -- pretty sure it is allowed by the spec, but
          // not quite elegant
          assert_status_change_fields_are_0 (ll, es.es[ent]);
        }
        if (checkargs && lldesc[ll].cb_status)
        {
          *checkargs = '('; // restore ( so checkargs function gets a more sensible input
          checkstatus (ll, &es, ent, checkargs, lldesc[ll].cb_status);
        }
        printf ("\n");
        cb_called &= ~lldesc[ll].mask;
        ddsrt_mutex_unlock (&g_mutex);
        ret = dds_get_status_changes (es.es[ent], &status);
        CU_ASSERT_FATAL (ret == 0);
        CU_ASSERT_FATAL ((status & lldesc[ll].mask) == 0);
      }
    }
    else if (strcmp (tok, "sleep") == 0)
    {
      if ((tok = strsep_noempty (&cursor, sep)) == NULL)
        abort ();
      double d; int pos;
      if (sscanf (tok, "%lf%n", &d, &pos) != 1 || tok[pos] != 0)
        abort ();
      printf ("sleep %fs\n", d);
      dds_sleepfor ((dds_duration_t) (d * 1e9));
    }
    else
    {
      printf ("tok '%s': unrecognized\n", tok);
      abort ();
    }
  }

  dds_delete_listener (list);
  dds_delete_qos (rwqos);
  dds_delete_qos (qos);
  // prevent any listeners from being invoked so we can safely delete the
  // mutex and the condition variable -- must do this going down the
  // hierarchy, or listeners may remain set through inheritance
  for (size_t i = 0; i < sizeof (es.es) / sizeof (es.es[0]); i++)
  {
    if (es.es[i])
    {
      ret = dds_set_listener (es.es[i], NULL);
      CU_ASSERT_FATAL (ret == 0);
    }
  }
  ddsrt_mutex_destroy (&g_mutex);
  ddsrt_cond_destroy (&g_cond);
  for (size_t i = 0; i < sizeof (es.doms) / sizeof (es.doms[0]); i++)
  {
    if (es.doms[i])
    {
      ret = dds_delete (es.doms[i]);
      CU_ASSERT_FATAL (ret == 0);
    }
  }
  ddsrt_free (opscopy);
  DDSRT_WARNING_MSVC_ON(4996);
}

/**************************************************
 ****                                          ****
 ****  listener invocation checks              ****
 ****                                          ****
 **************************************************/

CU_Test (ddsc_listener, propagation)
{
  // data-on-readers set on a participant at creation time must not trigger for
  // the readers for DCPSPublication and DCPSSubscription: those events must be
  // invisible for the test logic to work reliably. Installing a dummy listener
  // for it on the reader should prevent that from happening
  dotest ("da dor lc sm P ; ?!dor ?!da ?!sm ?!lc");
  // writing data should trigger data-available unless data-on-readers is set
  dotest ("da lc sm P ; r ; wr 0 w ; ?da r ?sm r ?lc r");
  dotest ("da dor lc sm P ; r ; wr 0 w ; ?!da ; ?dor R ?sm r ?lc r");
  // setting listeners after entity creation should work, too
  dotest ("P W R ; dor P pm W sm R ; r w ; ?sm r ?pm w ; wr 0 w ; ?dor R ; ?!da");
}

CU_Test (ddsc_listener, matched)
{
  // publication & subscription matched must both trigger; note: reader/writer matching inside
  // a process is synchronous, no need to check everywhere
  dotest ("sm r pm w ?pm w ?sm r");
  // across the network it should work just as well (matching happens on different threads for
  // remote & local entity creation, so it is meaningfully different test)
  dotest ("sm r pm w' ?pm w' ?sm r");
}

CU_Test (ddsc_listener, publication_matched)
{
  // regardless of order of creation, the writer should see one reader come & then go
  dotest ("sm r pm w ; ?pm(1,1,1,1,r) w ?sm r ; -r ; ?pm(1,0,0,-1,r) w");
  dotest ("pm w sm r ; ?pm(1,1,1,1,r) w ?sm r ; -r ; ?pm(1,0,0,-1,r) w");

  // regardless of order of creation, the writer should see one reader come & then go, also
  // when a second reader introduced
  dotest ("sm r pm w ; ?pm(1,1,1,1,r) w ?sm r ; t ?pm(2,1,2,1,t) w ; -r ; ?pm(2,0,1,-1,r) w");
  dotest ("pm w sm r ; ?pm(1,1,1,1,r) w ?sm r ; t ?pm(2,1,2,1,t) w ; -t ; ?pm(2,0,1,-1,t) w");

  // same with 2 domains
  dotest ("sm r pm w' ; ?pm(1,1,1,1,r) w' ?sm r ; -r ; ?pm(1,0,0,-1,r) w'");
  dotest ("pm w sm r' ; ?pm(1,1,1,1,r') w ?sm r' ; -r' ; ?pm(1,0,0,-1,r') w");

  dotest ("sm r pm w' ; ?pm(1,1,1,1,r) w' ?sm r ; t ?pm(2,1,2,1,t) w' ; -r ; ?pm(2,0,1,-1,r) w'");
  dotest ("pm w sm r' ; ?pm(1,1,1,1,r') w ?sm r' ; t ?pm(2,1,2,1,t) w ; -t ; ?pm(2,0,1,-1,t) w");
  dotest ("sm r pm w' ; ?pm(1,1,1,1,r) w' ?sm r ; t' ?pm(2,1,2,1,t') w' ; -r ; ?pm(2,0,1,-1,r) w'");
  dotest ("pm w sm r' ; ?pm(1,1,1,1,r') w ?sm r' ; t' ?pm(2,1,2,1,t') w ; -t' ; ?pm(2,0,1,-1,t') w");
}

CU_Test (ddsc_listener, subscription_matched)
{
  // regardless of order of creation, the reader should see one writer come & then go
  dotest ("sm r pm w ; ?pm w ?sm(1,1,1,1,w) r ; -w ; ?sm(1,0,0,-1,w) r");
  dotest ("pm w sm r ; ?pm w ?sm(1,1,1,1,w) r ; -w ; ?sm(1,0,0,-1,w) r");

  // regardless of order of creation, the reader should see one writer come & then go, also
  // when a second writer is introduced
  dotest ("sm r pm w ; ?pm w ?sm(1,1,1,1,w) r ; x ?sm(2,1,2,1,x) r ; -w ; ?sm(2,0,1,-1,w) r");
  dotest ("pm w sm r ; ?pm w ?sm(1,1,1,1,w) r ; x ?sm(2,1,2,1,x) r ; -x ; ?sm(2,0,1,-1,x) r");

  // same with 2 domains
  dotest ("sm r pm w' ; ?pm w' ?sm(1,1,1,1,w') r ; -w' ; ?sm(1,0,0,-1,w') r");
  dotest ("pm w sm r' ; ?pm w ?sm(1,1,1,1,w) r' ; -w ; ?sm(1,0,0,-1,w) r'");

  dotest ("sm r pm w' ; ?pm w' ?sm(1,1,1,1,w') r ; x ?sm(2,1,2,1,x) r ; -w' ; ?sm(2,0,1,-1,w') r");
  dotest ("pm w sm r' ; ?pm w ?sm(1,1,1,1,w) r' ; x ?sm(2,1,2,1,x) r' ; -x ; ?sm(2,0,1,-1,x) r'");
  dotest ("sm r pm w' ; ?pm w' ?sm(1,1,1,1,w') r ; x' ?sm(2,1,2,1,x') r ; -w' ; ?sm(2,0,1,-1,w') r");
  dotest ("pm w sm r' ; ?pm w ?sm(1,1,1,1,w) r' ; x' ?sm(2,1,2,1,x') r' ; -x' ; ?sm(2,0,1,-1,x') r'");
}

CU_Test (ddsc_listener, incompatible_qos)
{
  // best-effort writer & reliable reader: both must trigger incompatible QoS event
  dotest ("oiq w! riq r ; ?oiq(1,1) w ?riq(1,1) r");
  dotest ("riq r oiq w! ; ?oiq(1,1) w ?riq(1,1) r");
}

CU_Test (ddsc_listener, data_available)
{
  // data available on reader
  dotest ("da sm r pm w ?pm w ?sm r wr 0 w ?da r ?!dor");
  // data available set on subscriber
  dotest ("da R sm r pm w ?pm w ?sm r wr 0 w ?da r ?!dor");
  // data available set on participant
  dotest ("da P sm r pm w ?pm w ?sm r wr 0 w ?da r ?!dor");
}

CU_Test (ddsc_listener, data_available_delete_writer)
{
  // unmatching a writer that didn't read anything has no visible effect on RHC
  // subscription-matched event is generated synchronously, so "?sm r" doesn't
  // really add anything (it'd be different if there are two domain instances)
  dotest ("da sm r w ; -w ?sm r ?!da ; take(0,0) r");
  // after writing: auto-dispose should always trigger data available, an invalid
  // sample needs to show up if there isn't an unread sample to use instead
  dotest ("da r w ; wr 0 w ?da r ; -w ?da r ; take(1,0) r");
  dotest ("da r w ; wr 0 w ?da r ; read(1,0) r ; -w ?da r ; take(1,1) r");
  dotest ("da r w ; wr 0 w ?da r ; take(1,0) r ; -w ?da r ; take(0,1) r");
  // same with two writers (no point in doing this also with two domains)
  dotest ("da r w x ; -w ?!da -x ?!da ; take(0,0) r");
  dotest ("da r w x ; wr 0 w ?da r ; -x ?!da ; -w ?da r ; take(1,0) r");
  dotest ("da r w x ; wr 0 w ?da r ; -w ?da r ; take(1,0) r ; -x ?!da ; take(0,0) r");
  dotest ("da r w x ; wr 0 w wr 0 x ?da r ; -w ?!da ; take(2,0) r ; -x ?da r ; take(0,1) r");
  dotest ("da r w x ; wr 0 w wr 0 x ?da r ; read(2,0) r ; -w ?!da -x ?da r ; take(2,1) r");
  dotest ("da r w x ; wr 0 w wr 0 x ?da r ; read(2,0) r ; -x ?!da -w ?da r ; take(2,1) r");
  dotest ("da r w x ; wr 0 w read(1,0) r ; wr 0 x ?da r ; -w ?!da -x ?da r ; take(2,0) r");
  dotest ("da r w x ; wr 0 w read(1,0) r ; wr 0 x ?da r ; -x ?!da -w ?da r ; take(2,0) r");
  dotest ("da r w x ; wr 0 w read(1,0) r ; wr 0 x ?da r ; read(2,0) r ; -w ?!da -x ?da r ; take(2,1) r");
  dotest ("da r w x ; wr 0 w read(1,0) r ; wr 0 x ?da r ; read(2,0) r ; -x ?!da -w ?da r ; take(2,1) r");
  dotest ("da r w x ; wr 0 w wr 0 x ?da r ; take(2,0) r ; -w ?!da -x ?da r ; take(0,1) r");
  dotest ("da r w x ; wr 0 w wr 0 x ?da r ; take(2,0) r ; -x ?!da -w ?da r ; take(0,1) r");
}

CU_Test (ddsc_listener, data_available_delete_writer_disposed)
{
  // same as data_available_delete_writer, but now with the instance disposed first
  dotest ("da r w ; wr 0 w disp 0 w ?da r ; -w ?!da");
  dotest ("da r w ; wr 0 w disp 0 w ?da r ; read(1,0) r ; -w ?!da");
  dotest ("da r w ; wr 0 w disp 0 w ?da r ; take(1,0) r ; -w ?!da");

  dotest ("da r w x ; wr 0 w ?da r ; read(1,0) r ; disp 0 w ?da r ; read(1,1) r ; -w ?!da -x ?!da");
  dotest ("da r w x ; wr 0 w ?da r ; take(1,0) r ; disp 0 w ?da r ; take(0,1) r ; -w ?!da -x ?!da");
  dotest ("da r w x ; wr 0 w ?da r ; read(1,0) r ; disp 0 w ?da r ; read(1,1) r ; -x ?!da -w ?!da");
  dotest ("da r w x ; wr 0 w ?da r ; take(1,0) r ; disp 0 w ?da r ; take(0,1) r ; -x ?!da -w ?!da");

  dotest ("da r w x ; wr 0 w ?da r ; read(1,0) r ; disp 0 x ?da r ; read(1,1) r ; -w ?!da -x ?!da");
  dotest ("da r w x ; wr 0 w ?da r ; take(1,0) r ; disp 0 x ?da r ; take(0,1) r ; -w ?!da -x ?!da");
  dotest ("da r w x ; wr 0 w ?da r ; read(1,0) r ; disp 0 x ?da r ; read(1,1) r ; -x ?!da -w ?!da");
  dotest ("da r w x ; wr 0 w ?da r ; take(1,0) r ; disp 0 x ?da r ; take(0,1) r ; -x ?!da -w ?!da");
}

CU_Test (ddsc_listener, data_on_readers)
{
  // data on readers wins from data available
  dotest ("dor R da r ; wr 0 w ; ?dor R ?!da");
  dotest ("dor P da r ; wr 0 w ; ?dor R ?!da");
}

CU_Test (ddsc_listener, sample_lost)
{
  // FIXME: figure out what really constitutes a "lost sample"
  dotest ("sl r ; wr@0 0 w ?!sl ; wr@-1 0 w ?sl(1,1) r");
}

CU_Test (ddsc_listener, sample_rejected)
{
  // FIXME: rejection counts with retries?
  // reliable: expect timeout on the write when max samples has been reached
  // invalid samples don't count towards resource limits, so dispose should
  // not be blocked
  dotest ("sr r# ; wr 0 w wrfail 0 w wrfail 0 w ; ?sr r");
  dotest ("sr r# ; wr 0 w wrfail 0 w ; read(1,0) r ; disp 0 w ; read(1,1) r ; ?sr r");

  // best-effort: writes should succeed despite not delivering the data adding
  // the data in the RHC, also check number of samples rejected
  dotest ("sr r#! ; wr 0 w! wr 0 w wr 0 w ; ?sr(2,1,s) r");
  dotest ("sr r#! ; wr 0 w! wr 0 w ; read(1,0) r ; disp 0 w ; read(1,1) r ; ?sr(1,1,s) r");
}

CU_Test (ddsc_listener, liveliness_changed)
{
  // liveliness changed should trigger along with matching
  dotest ("pm w lc sm r ; ?pm w ?sm r ; ?lc(1,0,1,0,w) r ; -w ; ?lc(0,0,-1,0,w) r");
  dotest ("pm w lc sm r' ; ?pm w ?sm r' ; ?lc(1,0,1,0,w) r' ; -w ; ?lc(0,0,-1,0,w) r'");
}
