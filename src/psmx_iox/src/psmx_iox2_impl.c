// Copyright(c) 2025 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stddef.h>
#include <string.h>

#include "dds/ddsrt/avl.h"
#include "dds/ddsrt/align.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/mh3.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/random.h"
#include "dds/ddsrt/strtol.h"
#include "dds/ddsrt/threads.h"
#include "dds/ddsrt/io.h"
#include "dds/ddsrt/machineid.h"
#include "dds/ddsc/dds_loaned_sample.h"
#include "dds/ddsc/dds_psmx.h"

#include "iox2/iceoryx2.h"

#include "psmx_iox2_impl.h"

// I think we need to track whether the listener thread is supposed to stop or not because we don't
// want to propagate the event through shared memory.  Perhaps creating a separate notification port
// and attaching it separately to the waitset could work.
#define USE_LISTENER_THREAD_STATE 1

// If we can generate a unique event for the service specific to this process, then maybe we can send
// a special stop event and don't need the thread state
#define USE_STOP_EVENT 0

// It seems sometimes the data is published but not taken in the event handler.  I don't know why (yet).
// The PSMX tests do pick this up if the data is delayed by at least 500ms.  Printing samples remaining
// is an easy to check whether it timed out because of this.
#define PRINT_COUNT_OF_REMAINING_SAMPLES 0

// There should be no need for a timeout if notifications are never lost (or processed early -- see
// above).  If they can be lost, we need a timeout just to be able to exit.
#define WAIT_TIMEOUT_ENABLE 1

// Timeout of >= 500ms is needed to make the PSMX tests detect a problem (printing samples remaining
// is an easy way to check), a timeout of 100ms seems to make it work fine.
#define WAIT_TIMEOUT_SEC 0
#define WAIT_TIMEOUT_NSEC 100000000

// the IOX2 psmx instance itself, mirrored by an IOX2 node
typedef struct {
  dds_psmx_t base;
  bool support_keyed_topics;
  bool allow_nondisc_wr;
  dds_psmx_node_identifier_t node_id;
  iox2_node_h node_handle;
} psmx_iox2_t;

// the psmx topic, this is just a dummy, as the partitions are implemented as their own
// IOX2 topic counterparts
typedef struct {
  dds_psmx_topic_t base;
  psmx_iox2_t *parent;
  uint32_t type_size;
  const char *topic_name;
  const char *type_name;
  iox2_type_variant_e type_variant;
  ddsrt_avl_tree_t partitions;
  ddsrt_mutex_t lock;
} psmx_iox2_topic_t;

typedef struct psmx_iox2_endpoint psmx_iox2_endpoint_t;

enum listener_thread_state {
  LISTENER_THREAD_NONE,
  LISTENER_THREAD_INIT,
  LISTENER_THREAD_RUN,
  LISTENER_THREAD_STOPREQ,
};

// the real IOX2 topic counterpart, one exists for each DDS topic + partition combo which
// has endpoints created on it
typedef struct {
  ddsrt_avl_node_t avl_node;
  uint32_t refc;
  char *iox2_topic_name;
  psmx_iox2_topic_t *topic;
  iox2_port_factory_pub_sub_h service_handle;
  iox2_port_factory_event_h factory_event_handle;
  ddsrt_mutex_t lock;
  ddsrt_thread_t listener_thread;
#if USE_LISTENER_THREAD_STATE
  ddsrt_atomic_uint32_t listener_thread_state;
#endif
  iox2_notifier_h notifier;

  //attached readers
  psmx_iox2_endpoint_t **readers;
  uint32_t n_readers;
  uint32_t c_readers;
} psmx_iox2_partition_topic_t;

// the endpoints, will have either an IOX2 publisher or subscriber associated with them
// will be underneath the psmx_iox2_partition_topic_t
struct psmx_iox2_endpoint {
  dds_psmx_endpoint_t base;
  psmx_iox2_partition_topic_t *part_topic;
  union {
    iox2_publisher_h wr;
    iox2_subscriber_h rd;
  } iox2_handle;
  dds_entity_t cdds_endpoint;
  ddsrt_mutex_t lock;
};

// IOX2 loaned sample, will have either a const sample from a subscriber or a non-const
// sample from a publisher associated with it
typedef struct {
  dds_loaned_sample_t base;
  union {
    iox2_sample_mut_h mut;
    iox2_sample_h cnst;
  } iox2_ptr;
} psmx_iox2_loaned_sample_t;

static bool psmx_iox2_type_qos_supported (dds_psmx_t *psmx, dds_psmx_endpoint_type_t forwhat, dds_data_type_properties_t data_type, const dds_qos_t *qos);
static dds_return_t psmx_iox2_delete_topic (dds_psmx_topic_t *psmx_topic);
static dds_psmx_node_identifier_t psmx_iox2_get_node_id (const dds_psmx_t *psmx);
static dds_psmx_features_t psmx_iox2_supported_features (const dds_psmx_t *psmx);
static dds_psmx_topic_t *psmx_iox2_create_topic_w_type (dds_psmx_t *psmx,
    const char *topic_name, const char *type_name, dds_data_type_properties_t data_type_props, const struct ddsi_type *type_definition, uint32_t sizeof_type);
static void psmx_iox2_deinit_v2 (dds_psmx_t *psmx);

static const dds_psmx_ops_t psmx_ops = {
  .type_qos_supported = psmx_iox2_type_qos_supported,
  .create_topic = NULL,
  .delete_topic = psmx_iox2_delete_topic,
  .deinit = NULL,
  .get_node_id = psmx_iox2_get_node_id,
  .supported_features = psmx_iox2_supported_features,
  .create_topic_with_type = psmx_iox2_create_topic_w_type,
  .delete_psmx = psmx_iox2_deinit_v2
};

static dds_psmx_endpoint_t *psmx_iox2_create_endpoint (dds_psmx_topic_t *psmx_topic, const dds_qos_t *qos, dds_psmx_endpoint_type_t endpoint_type);
static dds_return_t psmx_iox2_delete_endpoint (dds_psmx_endpoint_t *psmx_endpoint);

static const dds_psmx_topic_ops_t psmx_topic_ops = {
  .create_endpoint = psmx_iox2_create_endpoint,
  .delete_endpoint = psmx_iox2_delete_endpoint
};

static dds_loaned_sample_t *psmx_iox2_req_loan (dds_psmx_endpoint_t *psmx_endpoint, uint32_t size_requested);
static dds_return_t psmx_iox2_write (dds_psmx_endpoint_t *psmx_endpoint, dds_loaned_sample_t *data);
static dds_loaned_sample_t *psmx_iox2_take_locked (psmx_iox2_endpoint_t *psmx_iox2_endpoint);
static dds_loaned_sample_t *psmx_iox2_take (dds_psmx_endpoint_t *psmx_endpoint);
static dds_return_t psmx_iox2_on_data_available (dds_psmx_endpoint_t *psmx_endpoint, dds_entity_t reader);

static const dds_psmx_endpoint_ops_t psmx_ep_ops = {
  .request_loan = psmx_iox2_req_loan,
  .write = psmx_iox2_write,
  .take = psmx_iox2_take,
  .on_data_available = psmx_iox2_on_data_available,
  // backwards compatibility means the following need not be set,
  // but leaving it out results in a compiler warning here
  .write_with_key = NULL  //!!!TODO!!! check whether this can be implemented in IOX2
};

static void psmx_iox2_loaned_sample_const_free (dds_loaned_sample_t *to_fini);
static void psmx_iox2_loaned_sample_mut_free (dds_loaned_sample_t *to_fini);

static const dds_loaned_sample_ops_t loaned_sample_const_ops = {
  .free = psmx_iox2_loaned_sample_const_free
};
static const dds_loaned_sample_ops_t loaned_sample_mut_ops = {
  .free = psmx_iox2_loaned_sample_mut_free
};

static const iox2_event_id_t data_event = { 0 };
#if USE_STOP_EVENT
static const iox2_event_id_t stop_event = { 1 };
#endif

// logger wrapper
static void log_error (const char *fmt, ...)
{
  va_list ap;
  va_start (ap, fmt);
  fprintf (stderr, "%s", "=== [ICEORYX2]");
  vfprintf (stderr, fmt, ap);
  fprintf (stderr, "\n");
  va_end (ap);
}

// this will stop the listener thread associated with a partition topic
// NOT thread safe
// called on destruction of the partition topic or when the last reader is deleted
static void stop_listener_thread (psmx_iox2_partition_topic_t *part_topic)
{
  assert (part_topic->n_readers == 0);
  
  // set listener_state to STOPREQ so that the listener thread will stop on the next event
  // that arrives, even when the "stop_event" we send a notification for is lost
  //
  // FIXME: it looks like it can get lost, but is that by design or a bug?
#if USE_LISTENER_THREAD_STATE
  assert (ddsrt_atomic_ld32 (&part_topic->listener_thread_state) != LISTENER_THREAD_NONE);
  ddsrt_atomic_st32 (&part_topic->listener_thread_state, LISTENER_THREAD_STOPREQ);
  ddsrt_atomic_fence_rel ();
#endif

  int iox2_ret;
  if ((iox2_ret = iox2_notifier_notify_with_custom_event_id (&part_topic->notifier,
#if USE_STOP_EVENT
                                                             &stop_event,
#else
                                                             &data_event,
#endif
                                                             NULL)) != IOX2_OK)
  {
    // FIXME ... does one need a separate notify?
    log_error ("Notifier error: %s [%d]", iox2_notifier_notify_error_string ((iox2_notifier_notify_error_e)iox2_ret), iox2_ret);
  }
  
  uint32_t success;
  dds_return_t ret = ddsrt_thread_join (part_topic->listener_thread, &success);
  if (ret != DDS_RETCODE_OK)
    log_error ("Listener thread join error error: %s [%d]", dds_strretcode (ret), ret);
  else if (!success)
    log_error ("Listener thread failed");

#if USE_LISTENER_THREAD_STATE
  ddsrt_atomic_st32 (&part_topic->listener_thread_state, LISTENER_THREAD_NONE);
#endif
}

// removes the association of a reader type endpoint with the partition topic
// will stop the listener thread if the number of readers reaches 0
static dds_return_t remove_reader_from_part_topic (psmx_iox2_partition_topic_t *part_topic, psmx_iox2_endpoint_t *ep)
{
  ddsrt_mutex_lock(&part_topic->topic->lock);
  ddsrt_mutex_lock (&part_topic->lock);
  uint32_t i;
  for (i = 0; i < part_topic->n_readers; i++)
    if (part_topic->readers[i] == ep)
      break;
  // reader must be present
  if (i == part_topic->n_readers)
  {
    ddsrt_mutex_unlock (&part_topic->lock);
    ddsrt_mutex_unlock (&part_topic->topic->lock);
    return DDS_RETCODE_ERROR;
  }
  assert (part_topic->n_readers > 0);
  part_topic->readers[i] = part_topic->readers[--part_topic->n_readers];
  ddsrt_mutex_unlock (&part_topic->lock);
  
  if (part_topic->n_readers == 0)
    stop_listener_thread (part_topic);
  ddsrt_mutex_unlock (&part_topic->topic->lock);
  return DDS_RETCODE_OK;
}

// callback context holder, is used to pass data to on_event from thread_listener_func
typedef struct {
  iox2_waitset_guard_h_ref guard;
  iox2_listener_h_ref listener;
  psmx_iox2_partition_topic_t *part_topic;
} on_event_arg;

static void on_event_try_take (psmx_iox2_partition_topic_t * const part_topic)
{
  ddsrt_mutex_lock (&part_topic->lock);
  for (uint32_t n = 0; n < part_topic->n_readers; n++)
  {
    psmx_iox2_endpoint_t * const ep = part_topic->readers[n];

    ddsrt_mutex_lock (&ep->lock);
    dds_loaned_sample_t *loaned_sample = psmx_iox2_take_locked (ep);
    if (loaned_sample)
    {
      if (ep->part_topic->topic->parent->allow_nondisc_wr) {
        // By using dds_reader_store_loaned_sample_wr_metadata, Cyclone will accept data
        // from writers that are not discovered and use the provided defaults for the
        // relevant QoS settings.
        (void) dds_reader_store_loaned_sample_wr_metadata (ep->cdds_endpoint, loaned_sample, 0, false, DDS_INFINITY);
      } else {
        (void) dds_reader_store_loaned_sample (ep->cdds_endpoint, loaned_sample);
      }
    }
    ddsrt_mutex_unlock (&ep->lock);

    // the ownership is either transferred to the reader, if succesfully stored,
    // or it is cleaned up here as to not leak resources
    //
    // must be done after release ep->lock because this may acquire the lock again
    if (loaned_sample)
      dds_loaned_sample_unref (loaned_sample);
  }
  ddsrt_mutex_unlock (&part_topic->lock);
}

static iox2_callback_progression_e on_event_try (psmx_iox2_partition_topic_t * const part_topic, iox2_listener_h_ref listener)
{
  while (true)
  {
#if USE_LISTENER_THREAD_STATE
    if (ddsrt_atomic_ld32 (&part_topic->listener_thread_state) == LISTENER_THREAD_STOPREQ)
      return iox2_callback_progression_e_STOP;
#endif

    bool has_received_event;
    iox2_event_id_t event_id;
    int e = iox2_listener_try_wait_one (listener, &event_id, &has_received_event);
    if (e != IOX2_OK)
    {
      log_error ("Failed to receive event on listener: %s [%d]", iox2_listener_wait_error_string ((iox2_listener_wait_error_e)e), e);
      return iox2_callback_progression_e_STOP;
    }
    else if (!has_received_event)
    {
      break;
    }
#if USE_STOP_EVENT
    else if (event_id.value == stop_event.value)
    {
      retval = iox2_callback_progression_e_STOP;
      break;
    }
#endif
    else
    {
      assert (event_id.value == data_event.value);
      on_event_try_take (part_topic);
    }
  }
  return iox2_callback_progression_e_CONTINUE;
}

// event handler function, is called from thread_listener_func when a writer signals delivery of data
// will iterate over all outstanding writer notifications and attempt to get the loans from all
// subscribers associated with the partition topic, and then insert the obtained loans into the
// readers' histories
static iox2_callback_progression_e on_event (iox2_waitset_attachment_id_h attachment_id, void *varg)
{
  on_event_arg * const arg = varg;
  psmx_iox2_partition_topic_t * const part_topic = arg->part_topic;

#if USE_LISTENER_THREAD_STATE
  if (ddsrt_atomic_ld32 (&part_topic->listener_thread_state) == LISTENER_THREAD_STOPREQ)
    return iox2_callback_progression_e_STOP;
#endif

  iox2_callback_progression_e retval = iox2_callback_progression_e_CONTINUE;
  if (iox2_waitset_attachment_id_has_event_from (&attachment_id, arg->guard))
    retval = on_event_try (part_topic, arg->listener);
  iox2_waitset_attachment_id_drop (attachment_id);

#if USE_LISTENER_THREAD_STATE
  if (ddsrt_atomic_ld32 (&part_topic->listener_thread_state) == LISTENER_THREAD_STOPREQ)
    retval = iox2_callback_progression_e_STOP;
#endif
  return retval;
}

// conversion function of waitset errorcodes to string
// implementing this myself, as IOX2 seems to strangely omit this function
static const char *psmx_iox2_waitset_run_result_string (const iox2_waitset_run_result_e e)
{
  switch(e) {
    case iox2_waitset_run_result_e_TERMINATION_REQUEST:
      return "TERMINATION REQUEST";
    case iox2_waitset_run_result_e_INTERRUPT:
      return "INTERRUPT";
    case iox2_waitset_run_result_e_STOP_REQUEST:
      return "STOP REQUEST";
    case iox2_waitset_run_result_e_ALL_EVENTS_HANDLED:
      return "ALL EVENTS HANDLED";
    default:
      return "UNKNOWN RESULT";
  }
}

// listener thread function, will be started for each partition topic when there are readers present
// creates waitset and listener and then continues to wait for data until signalled not to
static uint32_t thread_listener_func (void *p)
{
  psmx_iox2_partition_topic_t * const part_topic = (psmx_iox2_partition_topic_t *) p;
  uint32_t success = false;
  iox2_listener_h listener_handle = NULL;
  iox2_waitset_h waitset = NULL;
  iox2_waitset_builder_h waitset_builder = NULL;
  iox2_waitset_guard_h guard = NULL;

  iox2_port_factory_listener_builder_h listener_builder_handle = iox2_port_factory_event_listener_builder (&part_topic->factory_event_handle, NULL);
  if (listener_builder_handle == NULL)
  {
    log_error ("Unable to create listener builder");
    goto err;
  }

  int e = iox2_port_factory_listener_builder_create (listener_builder_handle, NULL, &listener_handle);
  if (e != IOX2_OK)
  {
    log_error ("Unable to create listener: %s [%d]", iox2_listener_create_error_string ((iox2_listener_create_error_e)e), e);
    goto err;
  }

  iox2_waitset_builder_new (NULL, &waitset_builder);
  if (waitset_builder == NULL)
  {
    log_error ("Unable to create waitset builder");
    goto fini_cleanup_listener;
  }

  e = iox2_waitset_builder_create (waitset_builder, iox2_service_type_e_IPC, NULL, &waitset);
  if (e != IOX2_OK)
  {
    log_error ("Unable to create waitset: %s [%d]", iox2_waitset_create_error_string ((iox2_waitset_create_error_e)e), e);
    goto fini_cleanup_listener;
  }

  e = iox2_waitset_attach_notification (&waitset, iox2_listener_get_file_descriptor (&listener_handle), NULL, &guard);
  if (e != IOX2_OK)
  {
    log_error ("Unable to attach listener to waitset: %s [%d]", iox2_waitset_attachment_error_string ((iox2_waitset_attachment_error_e)e), e);
    goto fini_cleanup_guard;
  }
#if USE_LISTENER_THREAD_STATE
  // use compare-and-swap so we don't lose a STOPREQ
  ddsrt_atomic_cas32 (&part_topic->listener_thread_state, LISTENER_THREAD_INIT, LISTENER_THREAD_RUN);
#endif

  on_event_arg context = {
    .listener = &listener_handle,
    .part_topic = part_topic,
    .guard = &guard
  };
  iox2_waitset_run_result_e result = iox2_waitset_run_result_e_ALL_EVENTS_HANDLED;
#if WAIT_TIMEOUT_ENABLE
  while (e == IOX2_OK && result == iox2_waitset_run_result_e_ALL_EVENTS_HANDLED
#if USE_LISTENER_THREAD_STATE
         && ddsrt_atomic_ld32 (&part_topic->listener_thread_state) != LISTENER_THREAD_STOPREQ
#endif
         )
  {
    e = iox2_waitset_wait_and_process_once_with_timeout (&waitset, on_event, &context, WAIT_TIMEOUT_SEC,WAIT_TIMEOUT_NSEC, &result);
    
    // Try once more in case a notification was lost and wait_and_process timed out
    if (result == iox2_waitset_run_result_e_ALL_EVENTS_HANDLED)
      on_event_try_take (part_topic);
  }
#else
  e = iox2_waitset_wait_and_process (&waitset, on_event, &context, &result);
#endif
  if (e != IOX2_OK)
    log_error ("Waitset wait and process error: %s [%d]", iox2_waitset_run_error_string ((iox2_waitset_run_error_e)e), e);
  else if (result != iox2_waitset_run_result_e_ALL_EVENTS_HANDLED && result != iox2_waitset_run_result_e_STOP_REQUEST)
    log_error ("Waitset wait and process abnormal result: %s [%d]", psmx_iox2_waitset_run_result_string (result), result);
  else
    success = true;
  iox2_waitset_guard_drop (guard);

fini_cleanup_guard:
  iox2_waitset_drop (waitset);
fini_cleanup_listener:
  iox2_listener_drop (listener_handle);
err:
  return (uint32_t) success;
}

static dds_return_t add_reader_to_part_topic (psmx_iox2_partition_topic_t *part_topic, psmx_iox2_endpoint_t *ep)
{
  dds_return_t ret = DDS_RETCODE_OK;
  ddsrt_mutex_lock (&part_topic->topic->lock);
  ddsrt_mutex_lock (&part_topic->lock);
  if (part_topic->c_readers == part_topic->n_readers)
  {
    const uint32_t oldc = part_topic->c_readers;
    const uint32_t newc = (oldc == 0) ? 1 : oldc * 2;
    psmx_iox2_endpoint_t **new_readers = ddsrt_realloc (part_topic->readers, newc * sizeof (*part_topic->readers));
    if (new_readers == NULL)
    {
      ddsrt_mutex_unlock (&part_topic->lock);
      ddsrt_mutex_unlock (&part_topic->topic->lock);
      return DDS_RETCODE_OUT_OF_RESOURCES;
    }
    part_topic->readers = new_readers;
    part_topic->c_readers = newc;
  }
  part_topic->readers[part_topic->n_readers++] = ep;
  ddsrt_mutex_unlock (&part_topic->lock);

  if (part_topic->n_readers == 1)
  {
#if USE_LISTENER_THREAD_STATE
    assert (ddsrt_atomic_ld32 (&part_topic->listener_thread_state) == LISTENER_THREAD_NONE);
    ddsrt_atomic_st32 (&part_topic->listener_thread_state, LISTENER_THREAD_INIT);
#endif
    ddsrt_threadattr_t tattr;
    ddsrt_threadattr_init (&tattr);
    //sched_info_transfer (part_topic->parent->parent->sched_info, &tattr);
    char thread_name[64];
    snprintf (thread_name, sizeof (thread_name), "%s_listener_thread", part_topic->iox2_topic_name);
    ret = ddsrt_thread_create (&part_topic->listener_thread, thread_name, &tattr, thread_listener_func, part_topic);
    if (ret != DDS_RETCODE_OK)
      part_topic->n_readers--;
  }
  ddsrt_mutex_unlock (&part_topic->topic->lock);
  return ret;
}

static void free_part_topic (psmx_iox2_partition_topic_t *part_topic)
{
  if (part_topic->iox2_topic_name != NULL)
    ddsrt_free (part_topic->iox2_topic_name);
  part_topic->iox2_topic_name = NULL;
  if (part_topic->service_handle != NULL)
    iox2_port_factory_pub_sub_drop (part_topic->service_handle);
  part_topic->service_handle = NULL;
  if (part_topic->factory_event_handle != NULL)
    iox2_port_factory_event_drop (part_topic->factory_event_handle);
  part_topic->factory_event_handle = NULL;
  if (part_topic->notifier != NULL)
    iox2_notifier_drop (part_topic->notifier);
  if (part_topic->readers != NULL)
    ddsrt_free (part_topic->readers);
  ddsrt_free (part_topic);
}

static int compare_iox2_topic (const void *va, const void *vb)
{
  const psmx_iox2_partition_topic_t *a = va, *b = vb;
  return strcmp (a->iox2_topic_name, b->iox2_topic_name);
}

static const ddsrt_avl_treedef_t psmx_iox2_partitions_td = DDSRT_AVL_TREEDEF_INITIALIZER (offsetof (psmx_iox2_partition_topic_t, avl_node), 0, compare_iox2_topic, 0);

static void unref_part_topic (psmx_iox2_partition_topic_t *part_topic)
{
  psmx_iox2_topic_t * const topic = part_topic->topic;
  ddsrt_mutex_lock (&topic->lock);
  if (--part_topic->refc == 0)
  {
    ddsrt_avl_delete (&psmx_iox2_partitions_td, &part_topic->topic->partitions, part_topic);
    free_part_topic (part_topic);
  }
  ddsrt_mutex_unlock (&topic->lock);
}

// conversion function of IOX2 type detail errorcodes to string
// implementing this myself, as IOX2 seems to strangely omit this function
static const char *psmx_iox2_type_detail_error_string (const iox2_type_detail_error_e e)
{
  switch (e) {
    case iox2_type_detail_error_e_INVALID_TYPE_NAME:
      return "INVALID TYPE NAME";
    case iox2_type_detail_error_e_INVALID_SIZE_OR_ALIGNMENT_VALUE:
      return "INVALID SIZE OR ALIGNMENT VALUE";
    default:
      return "UNKNOWN ERROR";
  }
}

// IOX2 partition topic name creation
// will take the topic name and postfix it with a hash of the name itself + all partitions
static char *create_iox2_topic_name (const char *topic_name, const dds_qos_t *qos)
{
  const size_t iox2_topic_name_length = strlen (topic_name) + 10;
  char *iox2_topic_name = ddsrt_malloc (iox2_topic_name_length);
  if (iox2_topic_name == NULL)
  {
    log_error ("Unable to allocate memory for IOX2 service name");
    return NULL;
  }

  static char *default_partition = "";
  uint32_t n_partitions;
  char **partitions;
  if (!dds_qget_partition (qos, &n_partitions, &partitions) || n_partitions == 0)
  {
    n_partitions = 1;
    partitions = &default_partition;
  }
  uint32_t part_topic_hash = ddsrt_mh3 (topic_name, strlen(topic_name) + 1, 0);
  for (uint32_t i = 0; i < n_partitions; i++)
  {
    part_topic_hash = ddsrt_mh3 (partitions[i], strlen (partitions[i]) + 1, part_topic_hash);
    if (partitions[i] != default_partition)
      dds_free (partitions[i]);
  }
  if (partitions != &default_partition)
    dds_free (partitions);
  snprintf (iox2_topic_name, iox2_topic_name_length, "%s_%08x", topic_name, part_topic_hash);
  return iox2_topic_name;
}

// partition topic lazy initialization function
// will only initialize a partition topic if one does not yet exist
// will create the waitset, notifier, service and publisher/subscriber factories
static psmx_iox2_partition_topic_t *get_part_topic (psmx_iox2_topic_t *topic, const dds_qos_t *qos)
{
  char *iox2_name = create_iox2_topic_name (topic->topic_name, qos);
  if (iox2_name == NULL)
    return NULL;

  ddsrt_mutex_lock (&topic->lock);

  // Lookup, if present, return early
  psmx_iox2_partition_topic_t template = { .iox2_topic_name = iox2_name };
  psmx_iox2_partition_topic_t *part_topic = ddsrt_avl_lookup (&psmx_iox2_partitions_td, &topic->partitions, &template);
  if (part_topic != NULL)
  {
    part_topic->refc++;
    ddsrt_mutex_unlock (&topic->lock);
    ddsrt_free (iox2_name);
    return part_topic;
  }

  // create service name
  int ret = IOX2_OK;
  iox2_service_name_h service_name_handle = NULL;
  ret = iox2_service_name_new (NULL, iox2_name, strlen (iox2_name), &service_name_handle);
  if (ret != IOX2_OK) {
    log_error ("Unable to create service name: %s [%d]", iox2_semantic_string_error_string ((iox2_semantic_string_error_e)ret), ret);
    goto cleanup_iox2_name;
  }

  // create service builder
  iox2_service_name_ptr service_name_ptr = iox2_cast_service_name_ptr (service_name_handle);
  if (service_name_ptr == NULL) {
    log_error ("Unable to cast service name");
    goto cleanup_service_name_handle;
  }

  iox2_service_builder_pub_sub_h service_builder_pub_sub_handle = iox2_service_builder_pub_sub(iox2_node_service_builder (&topic->parent->node_handle, NULL, service_name_ptr));
  if (service_builder_pub_sub_handle == NULL) {
    log_error ("Unable to create pubsub service builder");
    goto cleanup_service_name_handle;
  }

  // set pub sub payload type
  ret = iox2_service_builder_pub_sub_set_payload_type_details (&service_builder_pub_sub_handle, topic->type_variant, topic->type_name, strlen(topic->type_name), topic->type_size, 8);
  if (ret != IOX2_OK) {
    log_error ("Unable to set type details: %s [%d]", psmx_iox2_type_detail_error_string ((iox2_type_detail_error_e)ret), ret);
    goto cleanup_service_pub_sub;
  }

  //set header type
  ret = iox2_service_builder_pub_sub_set_user_header_type_details (&service_builder_pub_sub_handle, iox2_type_variant_e_FIXED_SIZE, "dds_psmx_metadata_t", strlen("dds_psmx_metadata_t"), sizeof(dds_psmx_metadata_t), 8);
  if (ret != IOX2_OK) {
    log_error ("Unable to set header details: %s [%d]", psmx_iox2_type_detail_error_string ((iox2_type_detail_error_e)ret), ret);
    goto cleanup_service_pub_sub;
  }

  //iox2_service_builder_pub_sub_set_max_nodes <= only relevant for static topologies?
  //iox2_service_builder_pub_sub_set_max_publishers <= only relevant for static topologies?
  //iox2_service_builder_pub_sub_set_max_subscribers <= only relevant for static topologies?
  //iox2_service_builder_pub_sub_set_payload_alignment <= set through payload type details?
  /*int32_t hd = 0;
  dds_history_kind_t hk = DDS_HISTORY_KEEP_LAST;
  if (dds_qget_history(qos, &hk, &hd)) {
    if (hk == DDS_HISTORY_KEEP_LAST)
      iox2_service_builder_pub_sub_set_history_size(&service_builder_pub_sub_handle, (size_t)hd);  //<== this causes issues with things being received out of order?
  }
  //iox2_service_builder_pub_sub_set_enable_safe_overflow(&service_builder_pub_sub_handle, hk == DDS_HISTORY_KEEP_LAST); // <== this causes issues with blocking write calls after exceeding history */
  //iox2_service_builder_pub_sub_set_subscriber_max_buffer_size <= ???
  //iox2_service_builder_pub_sub_set_subscriber_max_borrowed_samples <= ???

  // create service
  iox2_port_factory_pub_sub_h service_handle = NULL;
  ret = iox2_service_builder_pub_sub_open_or_create (service_builder_pub_sub_handle, NULL, &service_handle);
  if (ret != IOX2_OK) {
    log_error ("Unable to create service: %s [%d]", iox2_pub_sub_open_or_create_error_string ((iox2_pub_sub_open_or_create_error_e)ret), ret);
    goto cleanup_service_pub_sub;
  }

  iox2_service_builder_event_h service_builder_event_handle = iox2_service_builder_event (iox2_node_service_builder (&topic->parent->node_handle, NULL, service_name_ptr));
  if (service_builder_event_handle == NULL) {
    log_error ("Unable to create event service builders");
    goto cleanup_service_pub_sub;
  }

  //iox2_service_builder_event_set_deadline <= related to deadline QoS?
  //iox2_service_builder_event_disable_deadline <= related to deadline QoS?
  //iox2_service_builder_event_set_notifier_dead_event <= related to deadline QoS?
  //iox2_service_builder_event_disable_notifier_dead_event <= related to deadline QoS?
  //iox2_service_builder_event_set_notifier_created_event <= related to deadline QoS?
  //iox2_service_builder_event_disable_notifier_created_event <= related to deadline QoS?
  //iox2_service_builder_event_set_notifier_dropped_event <= related to deadline QoS?
  //iox2_service_builder_event_disable_notifier_dropped_event <= related to deadline QoS?
  //iox2_service_builder_event_set_max_notifiers <= only relevant for static topologies?
  //iox2_service_builder_event_set_max_nodes <= only relevant for static topologies?
  //iox2_service_builder_event_set_event_id_max_value <= we don't use event ids?
  //iox2_service_builder_event_set_max_listeners <= only relevant for static topologies?

  iox2_port_factory_event_h factory_event_handle = NULL;
  ret = iox2_service_builder_event_open_or_create (service_builder_event_handle, NULL, &factory_event_handle);
  if (ret != IOX2_OK) {
    log_error ("Unable to open service builder event factory %s [%d]", iox2_event_open_or_create_error_string ((iox2_event_open_or_create_error_e)ret), ret);
    goto cleanup_service_builder_event;
  }

  // create notifier from writer side
  iox2_port_factory_notifier_builder_h notifier_builder = iox2_port_factory_event_notifier_builder (&factory_event_handle, NULL);
  if (notifier_builder == NULL) {
    log_error ("Unable to create notifier builder");
    goto cleanup_factory_event;
  }
  iox2_port_factory_notifier_builder_set_default_event_id (&notifier_builder, &data_event);

  iox2_notifier_h notifier = NULL;
  ret = iox2_port_factory_notifier_builder_create (notifier_builder, NULL, &notifier);
  if (ret != IOX2_OK) {
    log_error("Unable to create notifier: %s [%d]", iox2_notifier_create_error_string ((iox2_notifier_create_error_e)ret), ret);
    goto cleanup_factory_event;
  }
  
  // Construct the partition-topic object
  part_topic = ddsrt_calloc (1, sizeof (*part_topic));
  if (part_topic == NULL) {
    log_error ("Unable to allocate memory for IOX2 partition topic object");
    goto cleanup_notifier;
  }
  part_topic->refc = 1;
  part_topic->iox2_topic_name = iox2_name;
  part_topic->topic = topic;
  part_topic->service_handle = service_handle;
  part_topic->factory_event_handle = factory_event_handle;
  part_topic->notifier = notifier;
#if USE_LISTENER_THREAD_STATE
  ddsrt_atomic_st32 (&part_topic->listener_thread_state, LISTENER_THREAD_NONE);
#endif
  ddsrt_mutex_init (&part_topic->lock);
  ddsrt_avl_insert (&psmx_iox2_partitions_td, &topic->partitions, part_topic);
  ddsrt_mutex_unlock (&topic->lock);
  return part_topic;

cleanup_notifier:
  iox2_notifier_drop (notifier);
cleanup_factory_event:
  iox2_port_factory_event_drop (factory_event_handle);
cleanup_service_builder_event:
  iox2_port_factory_pub_sub_drop (service_handle);
cleanup_service_pub_sub:
cleanup_service_name_handle:
  iox2_service_name_drop (service_name_handle);
cleanup_iox2_name:
  ddsrt_free (iox2_name);
  ddsrt_mutex_unlock (&topic->lock);
  return NULL;
}

static int is_wildcard_partition (const char *str)
{
  return strchr (str, '*') || strchr (str, '?');
}

// checking function for compatibility of QoS policies with IOX2
static bool psmx_iox2_type_qos_supported (dds_psmx_t *psmx, dds_psmx_endpoint_type_t forwhat, dds_data_type_properties_t data_type_props, const dds_qos_t *qos)
{
  psmx_iox2_t * const psmx_iox2 = (psmx_iox2_t *) psmx;
  if ((data_type_props & DDS_DATA_TYPE_CONTAINS_KEY) != 0U && !psmx_iox2->support_keyed_topics)
    return false;

  // Everything else is really dependent on the endpoint QoS, not the topic QoS,
  // as the lazy initialization uses the endpoint QoSes
  if (forwhat == DDS_PSMX_ENDPOINT_TYPE_UNSET)
    return true;

  uint32_t n_partitions;
  char **partitions;
  if (dds_qget_partition (qos, &n_partitions, &partitions))
  {
    bool supported = n_partitions == 0 || (n_partitions == 1 && !is_wildcard_partition (partitions[0]));
    for (uint32_t i = 0; i < n_partitions; i++)
      dds_free (partitions[i]);
    if (n_partitions > 0)
      dds_free (partitions);
    if (!supported)
      return false;
  }

  dds_durability_kind_t d_kind = DDS_DURABILITY_VOLATILE;
  (void) dds_qget_durability (qos, &d_kind);
  if (d_kind != DDS_DURABILITY_VOLATILE && d_kind != DDS_DURABILITY_TRANSIENT_LOCAL)
    return false;
  dds_liveliness_kind_t liveliness_kind;
  if (dds_qget_liveliness (qos, &liveliness_kind, NULL) && liveliness_kind != DDS_LIVELINESS_AUTOMATIC)
    return false;
  dds_duration_t deadline_duration;
  if (dds_qget_deadline (qos, &deadline_duration) && deadline_duration != DDS_INFINITY)
    return false;
  dds_ignorelocal_kind_t ignore_local;
  if (dds_qget_ignorelocal (qos, &ignore_local) && ignore_local != DDS_IGNORELOCAL_NONE)
    return false;
  return true;
}

static dds_return_t psmx_iox2_delete_topic (dds_psmx_topic_t *psmx_topic)
{
  psmx_iox2_topic_t * const iox2_topic = (psmx_iox2_topic_t *) psmx_topic;
  assert (ddsrt_avl_is_empty (&iox2_topic->partitions));
  ddsrt_mutex_destroy (&iox2_topic->lock);
  ddsrt_free (iox2_topic);
  return DDS_RETCODE_OK;
}

// PSMX topic creation function
static dds_psmx_topic_t *psmx_iox2_create_topic_w_type (dds_psmx_t *psmx, const char *topic_name, const char *type_name, dds_data_type_properties_t data_type_props, const struct ddsi_type *type_definition, uint32_t sizeof_type)
{
  psmx_iox2_t * const psmx_iox2 = (psmx_iox2_t *) psmx;
  (void) type_definition;

  psmx_iox2_topic_t *topic = ddsrt_calloc (1, sizeof (*topic));
  if (topic == NULL)
    return NULL;
  ddsrt_mutex_init (&topic->lock);
  ddsrt_avl_init (&psmx_iox2_partitions_td, &topic->partitions);
  topic->parent = psmx_iox2;
  topic->base.ops = psmx_topic_ops;
  topic->type_size = sizeof_type;
  topic->type_variant = ((data_type_props & DDS_DATA_TYPE_IS_MEMCPY_SAFE) == DDS_DATA_TYPE_IS_MEMCPY_SAFE) ? iox2_type_variant_e_FIXED_SIZE : iox2_type_variant_e_DYNAMIC;
  topic->topic_name = topic_name;
  topic->type_name = type_name;
  return &topic->base;
}

static void psmx_iox2_deinit_v2 (dds_psmx_t *psmx)
{
  psmx_iox2_t * const psmx_iox2 = (psmx_iox2_t *) psmx;
  iox2_node_drop (psmx_iox2->node_handle);
  ddsrt_free (psmx_iox2);
}

// IOX2 unique node id retrieval function
static dds_psmx_node_identifier_t psmx_iox2_get_node_id (const dds_psmx_t * psmx)
{
  return ((const psmx_iox2_t *)psmx)->node_id;
}

// IOX2 supported features function
static dds_psmx_features_t psmx_iox2_supported_features (const dds_psmx_t * psmx)
{
  (void) psmx;
  return DDS_PSMX_FEATURE_SHARED_MEMORY | DDS_PSMX_FEATURE_ZERO_COPY;
}

// IOX2 endpoint creation function
// will initialize the service for the topic/partition combo if it does not
// exist already and then create an endpoint for that topic/partition
static bool psmx_iox2_init_reader (psmx_iox2_endpoint_t *ep, const dds_qos_t *qos)
{
  iox2_port_factory_subscriber_builder_h subscriber_builder_handle = iox2_port_factory_pub_sub_subscriber_builder (&ep->part_topic->service_handle, NULL);
  if (subscriber_builder_handle == NULL) {
    log_error ("Unable to create subscriber builder");
    return false;
  }

  //set subscriber properties
  //iox2_port_factory_subscriber_builder_set_buffer_size <= size in which measure? samples? memory?
  
  // history depth? with the publisher blocking when the history is full, we're always safe
  (void) qos;

  int e = iox2_port_factory_subscriber_builder_create (subscriber_builder_handle, NULL, &ep->iox2_handle.rd);
  if (e != IOX2_OK) {
    log_error ("Unable to create subscriber: %s [%d]", iox2_subscriber_create_error_string ((iox2_subscriber_create_error_e)e), e);
    return false;
  }

  return true;
}

static bool psmx_iox2_init_writer (psmx_iox2_endpoint_t *ep, psmx_iox2_topic_t *iox2_topic, const dds_qos_t *qos)
{
  iox2_port_factory_publisher_builder_h publisher_builder_handle = iox2_port_factory_pub_sub_publisher_builder (&ep->part_topic->service_handle, NULL);
  if (publisher_builder_handle == NULL) {
    log_error ("Unable to create publisher builder");
    return false;
  }
  
  //set publisher properties
  //iox2_port_factory_publisher_builder_set_max_loaned_samples <= qos->resource_limits?
  
  iox2_port_factory_publisher_builder_set_allocation_strategy (&publisher_builder_handle, iox2_topic->type_variant == iox2_type_variant_e_FIXED_SIZE ? iox2_allocation_strategy_e_STATIC : iox2_allocation_strategy_e_BEST_FIT);
  iox2_port_factory_publisher_builder_set_initial_max_slice_len (&publisher_builder_handle, iox2_topic->type_size);
  
  dds_reliability_kind_t rel;
  (void) dds_qget_reliability (qos, &rel, NULL);
  iox2_port_factory_publisher_builder_unable_to_deliver_strategy (&publisher_builder_handle, rel == DDS_RELIABILITY_BEST_EFFORT ? iox2_unable_to_deliver_strategy_e_DISCARD_SAMPLE : iox2_unable_to_deliver_strategy_e_BLOCK);
  
  int e = iox2_port_factory_publisher_builder_create (publisher_builder_handle, NULL, &ep->iox2_handle.wr);
  if (e != IOX2_OK) {
    log_error ("Unable to create publisher: %s [%d]", iox2_publisher_create_error_string ((iox2_publisher_create_error_e)e), e);
    return false;
  }
  return true;
}

static bool psmx_iox2_init_endpoint (psmx_iox2_endpoint_t *ep, psmx_iox2_topic_t *iox2_topic, const dds_qos_t *qos, dds_psmx_endpoint_type_t endpoint_type)
{
  switch (endpoint_type)
  {
    case DDS_PSMX_ENDPOINT_TYPE_READER:
      return psmx_iox2_init_reader (ep, qos);
    case DDS_PSMX_ENDPOINT_TYPE_WRITER:
      return psmx_iox2_init_writer (ep, iox2_topic, qos);
    case DDS_PSMX_ENDPOINT_TYPE_UNSET:
      break;
  }
  return false;
}

static dds_psmx_endpoint_t *psmx_iox2_create_endpoint (dds_psmx_topic_t *psmx_topic, const dds_qos_t *qos, dds_psmx_endpoint_type_t endpoint_type)
{
  psmx_iox2_topic_t * const iox2_topic = (psmx_iox2_topic_t *) psmx_topic;
  psmx_iox2_partition_topic_t * const part_topic = get_part_topic (iox2_topic, qos);
  if (part_topic == NULL)
    goto fail_iox2_service;
  psmx_iox2_endpoint_t *ep = ddsrt_calloc (1, sizeof(*ep));
  if (ep == NULL)
    goto fail_alloc;
  ep->base.ops = psmx_ep_ops;
  ep->part_topic = part_topic;
  ddsrt_mutex_init (&ep->lock);
  if (!psmx_iox2_init_endpoint (ep, iox2_topic, qos, endpoint_type))
    goto fail_iox2_init_endpoint;
  return &ep->base;

fail_iox2_init_endpoint:
  ddsrt_mutex_destroy (&ep->lock);
  ddsrt_free (ep);
fail_iox2_service:
  unref_part_topic (part_topic);
fail_alloc:
  return NULL;
}

static dds_return_t psmx_iox2_delete_endpoint (dds_psmx_endpoint_t *psmx_endpoint)
{
  psmx_iox2_endpoint_t *iox2_endpoint = (psmx_iox2_endpoint_t *) psmx_endpoint;
  dds_return_t ret = DDS_RETCODE_OK;
  switch (iox2_endpoint->base.endpoint_type)
  {
    case DDS_PSMX_ENDPOINT_TYPE_READER:
      ret = remove_reader_from_part_topic (iox2_endpoint->part_topic, iox2_endpoint);
#if PRINT_COUNT_OF_REMAINING_SAMPLES // some tests occasionally fail becauses not all data was taken from the IOX2 reader
      dds_loaned_sample_t *ls;
      int n = 0;
      while ((ls = iox2_take (psmx_endpoint)) != NULL) { n++; dds_loaned_sample_unref (ls); }
      if (n) { printf ("iox2 reader dropped with %d samples still available\n", n); }
#endif
      iox2_subscriber_drop (iox2_endpoint->iox2_handle.rd);
      break;
    case DDS_PSMX_ENDPOINT_TYPE_WRITER:
      iox2_publisher_drop (iox2_endpoint->iox2_handle.wr);
      break;
    default:
      log_error ("PSMX endpoint type (%d) not accepted", iox2_endpoint->base.endpoint_type);;
      ret = DDS_RETCODE_BAD_PARAMETER;
  }
  ddsrt_mutex_destroy (&iox2_endpoint->lock);
  unref_part_topic (iox2_endpoint->part_topic);
  ddsrt_free (iox2_endpoint);
  return ret;
}

static dds_loaned_sample_t *psmx_iox2_req_loan (dds_psmx_endpoint_t *psmx_endpoint, uint32_t size_requested)
{
  psmx_iox2_endpoint_t * const iox2_endpoint = (psmx_iox2_endpoint_t*)psmx_endpoint;
  assert (iox2_endpoint->base.endpoint_type == DDS_PSMX_ENDPOINT_TYPE_WRITER);

  ddsrt_mutex_lock (&iox2_endpoint->lock);
  iox2_sample_mut_h sample;
  int iox2_ret;
  
  if (iox2_endpoint->part_topic->topic->type_variant == iox2_type_variant_e_FIXED_SIZE)
    size_requested = 1;
  if ((iox2_ret = iox2_publisher_loan_slice_uninit (&iox2_endpoint->iox2_handle.wr, NULL, &sample, size_requested)) != IOX2_OK)
  {
    log_error ("Failed to loan sample: %s [%d]", iox2_loan_error_string ((iox2_loan_error_e)iox2_ret), iox2_ret);
    ddsrt_mutex_unlock (&iox2_endpoint->lock);
    return NULL;
  }

  psmx_iox2_loaned_sample_t *loaned_sample = ddsrt_calloc (1, sizeof (*loaned_sample));
  if (loaned_sample == NULL)
  {
    // FIXME: perhaps dropping a mut sample without holding the publisher lock is allowed?
    iox2_sample_mut_drop (sample);
    ddsrt_mutex_unlock (&iox2_endpoint->lock);
    return NULL;
  }
  ddsrt_mutex_unlock (&iox2_endpoint->lock);

  loaned_sample->base.ops = loaned_sample_mut_ops;
  loaned_sample->iox2_ptr.mut = sample;
  iox2_sample_mut_payload_mut (&loaned_sample->iox2_ptr.mut, &loaned_sample->base.sample_ptr, NULL);
  iox2_sample_mut_user_header_mut (&loaned_sample->iox2_ptr.mut, (void **) &loaned_sample->base.metadata);
  return &loaned_sample->base;
}

static dds_return_t psmx_iox2_write (dds_psmx_endpoint_t *psmx_endpoint, dds_loaned_sample_t *data)
{
  psmx_iox2_endpoint_t *iox2_endpoint = (psmx_iox2_endpoint_t *) psmx_endpoint;
  psmx_iox2_loaned_sample_t *iox2_sample = (psmx_iox2_loaned_sample_t *) data;
  int iox2_ret;

  ddsrt_mutex_lock (&iox2_endpoint->lock);
  iox2_ret = iox2_sample_mut_send (iox2_sample->iox2_ptr.mut, NULL);
  // Clear metadata/sample_ptr so that any attempt to use it will cause a crash.  This gives no
  // guarantee whatsoever, but in practice it does help in discovering use of a iox writer loan
  // after publishing it.
  //
  // It also prevents the destructor from freeing it
  iox2_sample->base.metadata = NULL;
  iox2_sample->base.sample_ptr = NULL;
  iox2_sample->iox2_ptr.mut = NULL;
  ddsrt_mutex_unlock (&iox2_endpoint->lock);

  if (iox2_ret != IOX2_OK)
  {
    // FIXME: check ownership of sample
    return DDS_RETCODE_ERROR;
  }
  if ((iox2_ret = iox2_notifier_notify (&iox2_endpoint->part_topic->notifier, NULL)) != IOX2_OK)
  {
    log_error ("Notifier error: %s [%d]", iox2_notifier_notify_error_string ((iox2_notifier_notify_error_e)iox2_ret), iox2_ret);
  }
  return DDS_RETCODE_OK;
}

// IOX2 sample retrieval function; may only be called with iox2_endpoint->lock held
static dds_loaned_sample_t *psmx_iox2_take_locked (psmx_iox2_endpoint_t *iox2_endpoint)
{
  psmx_iox2_loaned_sample_t *loaned_sample = NULL;
  iox2_sample_h sample = NULL;
  int iox2_ret;
  if ((iox2_ret = iox2_subscriber_receive (&iox2_endpoint->iox2_handle.rd, NULL, &sample)) != IOX2_OK)
  {
    log_error ("Failed to receive sample %s [%d]", iox2_receive_error_string ((iox2_receive_error_e)iox2_ret), iox2_ret);
    return NULL;
  }
  if (sample == NULL)
  {
    // FIXME: is this even possible?
    return NULL;
  }
  if ((loaned_sample = ddsrt_malloc (sizeof (*loaned_sample))) == NULL)
  {
    iox2_sample_drop (sample);
    return NULL;
  }
  loaned_sample->base.ops = loaned_sample_const_ops;
  loaned_sample->base.loan_origin.origin_kind = DDS_LOAN_ORIGIN_KIND_PSMX;
  loaned_sample->base.loan_origin.psmx_endpoint = &iox2_endpoint->base;
  ddsrt_atomic_st32 (&loaned_sample->base.refc, 1);
  loaned_sample->iox2_ptr.cnst = sample;
  iox2_sample_payload (&loaned_sample->iox2_ptr.cnst, (const void **) &loaned_sample->base.sample_ptr, NULL);
  iox2_sample_user_header (&loaned_sample->iox2_ptr.cnst, (const void **) &loaned_sample->base.metadata);
  return &loaned_sample->base;
}

static dds_loaned_sample_t *psmx_iox2_take (dds_psmx_endpoint_t *psmx_endpoint)
{
  psmx_iox2_endpoint_t *iox2_endpoint = (psmx_iox2_endpoint_t*) psmx_endpoint;
  ddsrt_mutex_lock (&iox2_endpoint->lock);
  dds_loaned_sample_t *loaned_sample = psmx_iox2_take_locked (iox2_endpoint);
  ddsrt_mutex_unlock (&iox2_endpoint->lock);
  return loaned_sample;
}

static dds_return_t psmx_iox2_on_data_available (dds_psmx_endpoint_t *psmx_endpoint, dds_entity_t reader)
{
  psmx_iox2_endpoint_t * const iox2_endpoint = (psmx_iox2_endpoint_t *) psmx_endpoint;
  iox2_endpoint->cdds_endpoint = reader;
  return add_reader_to_part_topic (iox2_endpoint->part_topic, iox2_endpoint);
}

static void psmx_iox2_loaned_sample_const_free (dds_loaned_sample_t *loan)
{
  psmx_iox2_loaned_sample_t * const iox2_loan = (psmx_iox2_loaned_sample_t *) loan;
  if (iox2_loan->iox2_ptr.cnst)
  {
    psmx_iox2_endpoint_t * const iox2_ep = (psmx_iox2_endpoint_t *) loan->loan_origin.psmx_endpoint;
    ddsrt_mutex_lock (&iox2_ep->lock);
    iox2_sample_drop (iox2_loan->iox2_ptr.cnst);
    ddsrt_mutex_unlock (&iox2_ep->lock);
  }
}

static void psmx_iox2_loaned_sample_mut_free (dds_loaned_sample_t *loan)
{
  psmx_iox2_loaned_sample_t * const iox2_loan = (psmx_iox2_loaned_sample_t *) loan;
  if (iox2_loan->iox2_ptr.mut)
  {
    psmx_iox2_endpoint_t * const iox2_ep = (psmx_iox2_endpoint_t *) loan->loan_origin.psmx_endpoint;
    ddsrt_mutex_lock (&iox2_ep->lock);
    iox2_sample_mut_drop (iox2_loan->iox2_ptr.mut);
    ddsrt_mutex_unlock (&iox2_ep->lock);
  }
}

// --------------------------------------------------------------------------------- //

static bool get_loglevel_opt (const char *configstr, iox2_log_level_e *out)
{
  char *valstr = dds_psmx_get_config_option_value (configstr, "LOGLEVEL");
  if (valstr == NULL)
  {
    *out = iox2_log_level_e_FATAL;
    return true;
  }
  else
  {
    static const struct {
      const char *name;
      iox2_log_level_e level;
    } tab[] = {
      { "off", iox2_log_level_e_FATAL },
      { "fatal", iox2_log_level_e_FATAL },
      { "error", iox2_log_level_e_ERROR },
      { "warn", iox2_log_level_e_WARN },
      { "info", iox2_log_level_e_INFO },
      { "debug", iox2_log_level_e_DEBUG },
      { "trace", iox2_log_level_e_TRACE }
    };
    for (size_t i = 0; i < sizeof (tab) / sizeof (*tab); i++)
    {
      if (ddsrt_strcasecmp (valstr, tab[i].name))
      {
        *out = tab[i].level;
        ddsrt_free (valstr);
        return true;
      }
    }
    log_error ("Invalid value for LOGLEVEL: \"%s\"", valstr);
    ddsrt_free (valstr);
    return false;
  }
}

static bool to_node_identifier (const char* str, dds_psmx_node_identifier_t *id)
{
  if (strlen (str) != 2 * sizeof (id->x))
    return false;
  for (uint32_t n = 0; n < 2 * sizeof (id->x); n++)
  {
    int32_t num;
    if ((num = ddsrt_todigit (str[n])) < 0 || num >= 16)
      return false;
    if ((n % 2) == 0)
      id->x[n / 2] = (uint8_t) (num << 4);
    else
      id->x[n / 2] |= (uint8_t) num;
  }
  return true;
}

static bool get_node_id_opt (const char *configstr, dds_psmx_node_identifier_t *node_id)
{
  char *opt_node_id = dds_psmx_get_config_option_value (configstr, "LOCATOR");
  bool valid_node_id;
  if (opt_node_id != NULL)
  {
    valid_node_id = to_node_identifier (opt_node_id, node_id);
    if (!valid_node_id)
      log_error ("Invalid LOCATOR: \"%s\"", opt_node_id);
    ddsrt_free (opt_node_id);
  }
  else
  {
    ddsrt_machineid_t machine_id;
    valid_node_id = ddsrt_get_machineid (&machine_id);
    if (!valid_node_id)
      log_error ("Could not determine machine id");
    DDSRT_STATIC_ASSERT (sizeof (machine_id) == sizeof (*node_id));
    memcpy (node_id, &machine_id, sizeof (*node_id));
  }
  return valid_node_id;
}

static bool get_bool_opt (const char *configstr, const char *option, bool def, bool *val)
{
  char *valstr = dds_psmx_get_config_option_value (configstr, option);
  if (valstr == NULL)
  {
    *val = def;
    return true;
  }
  else
  {
    if (ddsrt_strcasecmp (valstr, "false") == 0)
      *val = false;
    else if (ddsrt_strcasecmp (valstr, "true") == 0)
      *val = true;
    else
    {
      log_error ("Invalid value for %s: \"%s\"", option, valstr);
      ddsrt_free (valstr);
      return false;
    }
    ddsrt_free (valstr);
    return true;
  }
}

// IOX2 psmx instance creation function
// attempts to create an IOX2 PSMX instance based on the supplied config
// if all config settings present are valid, memory is assigned for the psmx instance
// and an IOX2 node with the global (domain) name from the SERVICE_NAME config item
// is created
dds_return_t iox2_create_psmx (dds_psmx_t **psmx, dds_psmx_instance_id_t instance_id, const char *configstr)
{
  (void) instance_id;

  iox2_log_level_e log_level;
  dds_psmx_node_identifier_t node_id;
  bool keyed_topics, allow_nondisc_wr;

  if (!get_loglevel_opt (configstr, &log_level))
    goto err_trivial;
  if (!get_node_id_opt (configstr, &node_id))
    goto err_trivial;
  if (!get_bool_opt (configstr, "KEYED_TOPICS", true, &keyed_topics))
    goto err_trivial;
  if (!get_bool_opt (configstr, "ALLOW_NONDISCOVERED_WRITERS", false, &allow_nondisc_wr))
    goto err_trivial;

#if 0
  sched_info_t si;
  char *opt_sched_prio = get_config_option_val(configstr, "PRIORITY", false);
  if (opt_sched_prio != NULL) {
    if (!sched_info_setpriority (&si, opt_sched_prio)) {
      log_error("Invalid value for PRIORITY: \"%s\"", opt_sched_prio);
      ret = DDS_RETCODE_ERROR;
    }

    dds_free(opt_sched_prio);
    if (ret != DDS_RETCODE_OK)
      goto err;
  }

  char *opt_sched_affinity = get_config_option_val(configstr, "AFFINITY", false);
  if (opt_sched_affinity != NULL) {
    if (!sched_info_setaffinity (&si, opt_sched_affinity)) {
      log_error("Invalid value for AFFINITY: \"%s\"", opt_sched_affinity);
      ret = DDS_RETCODE_ERROR;
    }

    dds_free(opt_sched_affinity);
    if (ret != DDS_RETCODE_OK)
      goto err;
  }
#endif

  iox2_set_log_level (log_level);
  psmx_iox2_t * const psmx_iox2 = ddsrt_calloc (1, sizeof (*psmx_iox2));
  if (psmx_iox2 == NULL)
    return DDS_RETCODE_ERROR;

  // create a new config based on the global config
  iox2_config_ptr config_ptr = iox2_config_global_config ();
  iox2_config_h config = NULL;
  iox2_config_from_ptr (config_ptr, NULL, &config);
  if (config == NULL)
  {
    log_error ("Unable to create IOX2 config");
    goto err_config_from_ptr;
  }

  // The domain name becomes the prefix for all resources.
  // Therefore, different domain names never share the same resources.
  char *instance_name = dds_psmx_get_config_option_value (configstr, "INSTANCE_NAME");
  assert (instance_name);
  if (iox2_config_global_set_prefix (&config, instance_name) != IOX2_OK)
  {
    log_error ("Unable to set IOX2 domain name in IOX2 config");
    goto err_instance_name;
  }

  iox2_node_builder_h node_builder_handle = iox2_node_builder_new (NULL);
  if (node_builder_handle == NULL)
  {
    log_error ("Unable to create IOX2 node builder");
    goto err_instance_name;
  }
  iox2_node_builder_set_config (&node_builder_handle, &config);
  
  // FIXME: no idea what Iceoryx2 signal handling does exactly. This is probably wrong ...
  // iox2_signal_handling_mode_e_HANDLE_TERMINATION_REQUESTS
  iox2_node_builder_set_signal_handling_mode (&node_builder_handle, iox2_signal_handling_mode_e_DISABLED);

  int iox2_ret;
  if ((iox2_ret = iox2_node_builder_create (node_builder_handle, NULL, iox2_service_type_e_IPC, &psmx_iox2->node_handle)) != IOX2_OK)
  {
    log_error ("Error during IOX2 node creation: %s [%d]", iox2_node_creation_failure_string ((iox2_node_creation_failure_e)iox2_ret), iox2_ret);
    goto err_instance_name;
  }
  iox2_config_drop (config);
  ddsrt_free (instance_name);

  psmx_iox2->base.ops = psmx_ops;
  psmx_iox2->support_keyed_topics = keyed_topics;
  psmx_iox2->allow_nondisc_wr = allow_nondisc_wr;
  psmx_iox2->node_id = node_id;
#if 0
  psmx_iox2->sched_info = si;
#endif
  *psmx = &psmx_iox2->base;
  return DDS_RETCODE_OK;

err_instance_name:
  iox2_config_drop (config);
  ddsrt_free (instance_name);
err_config_from_ptr:
  ddsrt_free (psmx);
err_trivial:
  return DDS_RETCODE_ERROR;
}
