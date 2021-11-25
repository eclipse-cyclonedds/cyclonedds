#include <stdlib.h>

#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/threads.h"
#include "dds/dds.h"

#include "async_listener.h"

enum async_listener_kind {
  ALK_DATA_AVAILABLE,
  ALK_PUBLICATION_MATCHED,
  ALK_SUBSCRIPTION_MATCHED
};

struct async_listener_event {
  struct async_listener_event *next;
  enum async_listener_kind kind;
  dds_entity_t handle;
  void *fn_arg;
  union {
    struct {
      dds_on_data_available_fn fn;
    } da;
    struct {
      dds_on_publication_matched_fn fn;
      dds_publication_matched_status_t st;
    } pm;
    struct {
      dds_on_subscription_matched_fn fn;
      dds_subscription_matched_status_t st;
    } sm;
  } u;
};

struct async_listener {
  ddsrt_mutex_t lock;
  ddsrt_cond_t cond;
  ddsrt_thread_t tid;
  bool stop;
  struct async_listener_event *oldest;
  struct async_listener_event *latest;
};

static uint32_t async_listener_thread (void *val)
{
  struct async_listener * const al = val;
  ddsrt_mutex_lock (&al->lock);
  while (!al->stop || al->oldest != NULL)
  {
    if (al->oldest == NULL)
      ddsrt_cond_wait (&al->cond, &al->lock);
    else
    {
      struct async_listener_event *ev = al->oldest;
      al->oldest = ev->next;
      ddsrt_mutex_unlock (&al->lock);
      switch (ev->kind)
      {
        case ALK_DATA_AVAILABLE:
          ev->u.da.fn (ev->handle, ev->fn_arg);
          break;
        case ALK_PUBLICATION_MATCHED:
          ev->u.pm.fn (ev->handle, ev->u.pm.st, ev->fn_arg);
          break;
        case ALK_SUBSCRIPTION_MATCHED:
          ev->u.sm.fn (ev->handle, ev->u.sm.st, ev->fn_arg);
          break;
      }
      free (ev);
      ddsrt_mutex_lock (&al->lock);
    }
  }
  ddsrt_mutex_unlock (&al->lock);
  return 0;
}

struct async_listener *async_listener_new (void)
{
  struct async_listener *al;
  if ((al = malloc (sizeof (*al))) == NULL)
    return NULL;
  ddsrt_mutex_init (&al->lock);
  ddsrt_cond_init (&al->cond);
  al->stop = 0;
  al->oldest = NULL;
  al->latest = NULL;
  return al;
}

bool async_listener_start (struct async_listener *al)
{
  dds_return_t rc;
  ddsrt_threadattr_t tattr;
  ddsrt_threadattr_init (&tattr);
  rc = ddsrt_thread_create (&al->tid, "al", &tattr, async_listener_thread, al);
  return rc == 0;
}

void async_listener_stop (struct async_listener *al)
{
  ddsrt_mutex_lock (&al->lock);
  al->stop = true;
  ddsrt_cond_signal (&al->cond);
  ddsrt_mutex_unlock (&al->lock);
  (void) ddsrt_thread_join (al->tid, NULL);
  assert (al->oldest == NULL);
}

void async_listener_free (struct async_listener *al)
{
  ddsrt_cond_destroy (&al->cond);
  ddsrt_mutex_destroy (&al->lock);
  free (al);
}

static void async_listener_enqueue (struct async_listener *al, struct async_listener_event ev0)
{
  struct async_listener_event *ev;
  if ((ev = malloc (sizeof (*ev))) == NULL)
    abort (); // if we run out of memory, ddsperf is dead anyway
  *ev = ev0;
  ddsrt_mutex_lock (&al->lock);
  assert (!al->stop);
  ev->next = NULL;
  if (al->oldest)
    al->latest->next = ev;
  else
    al->oldest = ev;
  al->latest = ev;
  ddsrt_cond_signal (&al->cond);
  ddsrt_mutex_unlock (&al->lock);
}

void async_listener_enqueue_data_available (struct async_listener *al, dds_on_data_available_fn fn, dds_entity_t rd, void *arg)
{
  async_listener_enqueue (al, (struct async_listener_event) {
    .kind = ALK_DATA_AVAILABLE,
    .handle = rd,
    .fn_arg = arg,
    .u = { .da = {
      .fn = fn
    } }
  });
}

void async_listener_enqueue_subscription_matched (struct async_listener *al, dds_on_subscription_matched_fn fn, dds_entity_t rd, const dds_subscription_matched_status_t status, void *arg)
{
  async_listener_enqueue (al, (struct async_listener_event) {
    .kind = ALK_SUBSCRIPTION_MATCHED,
    .handle = rd,
    .fn_arg = arg,
    .u = { .sm = {
      .fn = fn,
      .st = status
    } }
  });
}

void async_listener_enqueue_publication_matched (struct async_listener *al, dds_on_publication_matched_fn fn, dds_entity_t wr, const dds_publication_matched_status_t status, void *arg)
{
  async_listener_enqueue (al, (struct async_listener_event) {
    .kind = ALK_PUBLICATION_MATCHED,
    .handle = wr,
    .fn_arg = arg,
    .u = { .pm = {
      .fn = fn,
      .st = status
    } }
  });
}

