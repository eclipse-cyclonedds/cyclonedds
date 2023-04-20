// Copyright(c) 2006 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>
#include <stdlib.h>

#include "dds/ddsrt/log.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/avl.h"
#include "dds/ddsi/ddsi_sertype.h"
#include "dds/ddsi/ddsi_serdata.h"
#include "dds/ddsi/ddsi_tkmap.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "ddsi__entity_index.h"
#include "ddsi__entity.h"
#include "ddsi__deliver_locally.h"
#include "ddsi__endpoint.h"
#include "ddsi__rhc.h"

#define TYPE_SAMPLE_CACHE_SIZE 4

struct ddsi_sertype;
struct ddsi_serdata;
struct ddsi_tkmap_instance;

struct type_sample_cache_entry {
  struct ddsi_serdata *sample;
  struct ddsi_tkmap_instance *tk;
};

struct type_sample_cache_large_entry {
  ddsrt_avl_node_t avlnode;
  const struct ddsi_sertype *type;
  struct ddsi_serdata *sample;
  struct ddsi_tkmap_instance *tk;
};

struct type_sample_cache {
  uint32_t n;
  const struct ddsi_sertype *types[TYPE_SAMPLE_CACHE_SIZE];
  struct type_sample_cache_entry samples[TYPE_SAMPLE_CACHE_SIZE];
  ddsrt_avl_tree_t overflow;
};

static int cmp_type_ptrs (const void *va, const void *vb)
{
  uintptr_t a = (uintptr_t) va;
  uintptr_t b = (uintptr_t) vb;
  return (a == b) ? 0 : (a < b) ? -1 : 1;
}

static const ddsrt_avl_treedef_t tsc_large_td = DDSRT_AVL_TREEDEF_INITIALIZER_INDKEY (offsetof (struct type_sample_cache_large_entry, avlnode), offsetof (struct type_sample_cache_large_entry, type), cmp_type_ptrs, 0);

static void free_sample_after_store (struct ddsi_domaingv *gv, struct ddsi_serdata *sample, struct ddsi_tkmap_instance *tk)
{
  if (sample)
  {
    ddsi_tkmap_instance_unref (gv->m_tkmap, tk);
    ddsi_serdata_unref (sample);
  }
}

static void type_sample_cache_init (struct type_sample_cache * __restrict tsc)
{
  tsc->n = 0;
  ddsrt_avl_init (&tsc_large_td, &tsc->overflow);
}

static void free_large_entry (void *vnode, void *varg)
{
  struct type_sample_cache_large_entry *e = vnode;
  struct ddsi_domaingv *gv = varg;
  free_sample_after_store (gv, e->sample, e->tk);
  ddsrt_free (e);
}

static void type_sample_cache_fini (struct type_sample_cache * __restrict tsc, struct ddsi_domaingv *gv)
{
  for (uint32_t i = 0; i < tsc->n && i < TYPE_SAMPLE_CACHE_SIZE; i++)
    if (tsc->types[i] && tsc->samples[i].tk)
      free_sample_after_store (gv, tsc->samples[i].sample, tsc->samples[i].tk);

  ddsrt_avl_free_arg (&tsc_large_td, &tsc->overflow, free_large_entry, gv);
}

static bool type_sample_cache_lookup (struct ddsi_serdata ** __restrict sample, struct ddsi_tkmap_instance ** __restrict tk, struct type_sample_cache * __restrict tsc, const struct ddsi_sertype *type)
{
  /* linear scan of an array of pointers should be pretty fast */
  for (uint32_t i = 0; i < tsc->n && i < TYPE_SAMPLE_CACHE_SIZE; i++)
  {
    if (tsc->types[i] == type)
    {
      *tk = tsc->samples[i].tk;
      *sample = tsc->samples[i].sample;
      return true;
    }
  }

  struct type_sample_cache_large_entry *e;
  if ((e = ddsrt_avl_lookup (&tsc_large_td, &tsc->overflow, type)) != NULL)
  {
    *tk = e->tk;
    *sample = e->sample;
    return true;
  }
  return false;
}

static void type_sample_cache_store (struct type_sample_cache * __restrict tsc, const struct ddsi_sertype *type, struct ddsi_serdata *sample, struct ddsi_tkmap_instance *tk)
{
  if (tsc->n < TYPE_SAMPLE_CACHE_SIZE)
  {
    tsc->types[tsc->n] = type;
    tsc->samples[tsc->n].tk = tk;
    tsc->samples[tsc->n].sample = sample;
  }
  else
  {
    struct type_sample_cache_large_entry *e = ddsrt_malloc (sizeof (*e));
    e->type = type;
    e->tk = tk;
    e->sample = sample;
    ddsrt_avl_insert (&tsc_large_td, &tsc->overflow, e);
  }
  tsc->n++;
}

dds_return_t ddsi_deliver_locally_one (struct ddsi_domaingv *gv, struct ddsi_entity_common *source_entity, bool source_entity_locked, const ddsi_guid_t *rdguid, const struct ddsi_writer_info *wrinfo, const struct ddsi_deliver_locally_ops * __restrict ops, void *vsourceinfo)
{
  struct ddsi_reader *rd = ddsi_entidx_lookup_reader_guid (gv->entity_index, rdguid);
  if (rd == NULL)
    return DDS_RETCODE_OK;

  struct ddsi_serdata *payload;
  struct ddsi_tkmap_instance *tk;
  if ((payload = ops->makesample (&tk, gv, rd->type, vsourceinfo)) != NULL)
  {
    EETRACE (source_entity, " =>"PGUIDFMT"\n", PGUID (*rdguid));
    /* FIXME: why look up rd,pwr again? Their states remains valid while the thread stays
       "awake" (although a delete can be initiated), and blocking like this is a stopgap
       anyway -- quite possibly to abort once either is deleted */
    while (!ddsi_rhc_store (rd->rhc, wrinfo, payload, tk))
    {
      if (source_entity_locked)
        ddsrt_mutex_unlock (&source_entity->lock);
      dds_sleepfor (DDS_MSECS (1));
      if (source_entity_locked)
        ddsrt_mutex_lock (&source_entity->lock);
      if (ddsi_entidx_lookup_reader_guid (gv->entity_index, rdguid) == NULL ||
          ddsi_entidx_lookup_guid_untyped (gv->entity_index, &source_entity->guid) == NULL)
      {
        /* give up when reader or proxy writer no longer accessible */
        break;
      }
    }
    free_sample_after_store (gv, payload, tk);
  }
  return DDS_RETCODE_OK;
}

static dds_return_t deliver_locally_slowpath (struct ddsi_domaingv *gv, struct ddsi_entity_common *source_entity, bool source_entity_locked, const struct ddsi_writer_info *wrinfo, const struct ddsi_deliver_locally_ops * __restrict ops, void *vsourceinfo)
{
  /* When deleting, pwr is no longer accessible via the hash
     tables, and consequently, a reader may be deleted without
     it being possible to remove it from rdary. The primary
     reason rdary exists is to avoid locking the proxy writer
     but this is less of an issue when we are deleting it, so
     we fall back to using the GUIDs so that we can deliver all
     samples we received from it. As writer being deleted any
     reliable samples that are rejected are simply discarded. */
  struct type_sample_cache tsc;
  ddsrt_avl_iter_t it;
  struct ddsi_reader *rd;
  type_sample_cache_init (&tsc);
  if (!source_entity_locked)
    ddsrt_mutex_lock (&source_entity->lock);
  rd = ops->first_reader (gv->entity_index, source_entity, &it);
  if (rd != NULL)
    EETRACE (source_entity, " =>");
  while (rd != NULL)
  {
#ifdef DDS_HAS_SHM
    if (rd->has_iceoryx) {
      rd = ops->next_reader(gv->entity_index, &it);
      continue; // skip iceoryx readers
    }
#endif

    struct ddsi_serdata *payload;
    struct ddsi_tkmap_instance *tk;
    if (!type_sample_cache_lookup (&payload, &tk, &tsc, rd->type))
    {
      payload = ops->makesample (&tk, gv, rd->type, vsourceinfo);
      type_sample_cache_store (&tsc, rd->type, payload, tk);
    }
    /* check payload to allow for deserialisation failures */
    if (payload)
    {
      EETRACE (source_entity, " "PGUIDFMT, PGUID (rd->e.guid));
      (void) ddsi_rhc_store (rd->rhc, wrinfo, payload, tk);
    }
    rd = ops->next_reader (gv->entity_index, &it);
  }
  EETRACE (source_entity, "\n");
  if (!source_entity_locked)
    ddsrt_mutex_unlock (&source_entity->lock);
  type_sample_cache_fini (&tsc, gv);
  return DDS_RETCODE_OK;
}

static dds_return_t deliver_locally_fastpath (struct ddsi_domaingv *gv, struct ddsi_entity_common *source_entity, bool source_entity_locked, struct ddsi_local_reader_ary *fastpath_rdary, const struct ddsi_writer_info *wrinfo, const struct ddsi_deliver_locally_ops * __restrict ops, void *vsourceinfo)
{
  struct ddsi_reader ** const rdary = fastpath_rdary->rdary;
  uint32_t i = 0;
  while (rdary[i])
  {
    struct ddsi_sertype const * const type = rdary[i]->type;
    struct ddsi_serdata *payload;
    struct ddsi_tkmap_instance *tk;
    if ((payload = ops->makesample (&tk, gv, type, vsourceinfo)) == NULL)
    {
      /* malformed payload: skip all readers with the same type */
      while (rdary[++i] && rdary[i]->type == type)
        ; /* do nothing */
    }
    else
    {
      do {
        dds_return_t rc;
        while (!ddsi_rhc_store (rdary[i]->rhc, wrinfo, payload, tk))
        {
          if ((rc = ops->on_failure_fastpath (source_entity, source_entity_locked, fastpath_rdary, vsourceinfo)) != DDS_RETCODE_OK)
          {
            free_sample_after_store (gv, payload, tk);
            return rc;
          }
        }
      } while (rdary[++i] && rdary[i]->type == type);
      free_sample_after_store (gv, payload, tk);
    }
  }
  return DDS_RETCODE_OK;
}

dds_return_t ddsi_deliver_locally_allinsync (struct ddsi_domaingv *gv, struct ddsi_entity_common *source_entity, bool source_entity_locked, struct ddsi_local_reader_ary *fastpath_rdary, const struct ddsi_writer_info *wrinfo, const struct ddsi_deliver_locally_ops * __restrict ops, void *vsourceinfo)
{
  dds_return_t rc;
  /* FIXME: Retry loop for re-delivery of rejected reliable samples is a bad hack
     should instead throttle back the writer by skipping acknowledgement and retry */
  do {
    ddsrt_mutex_lock (&fastpath_rdary->rdary_lock);
    if (fastpath_rdary->fastpath_ok)
    {
      EETRACE (source_entity, " => EVERYONE\n");
      if (fastpath_rdary->rdary[0])
        rc = deliver_locally_fastpath (gv, source_entity, source_entity_locked, fastpath_rdary, wrinfo, ops, vsourceinfo);
      else
        rc = DDS_RETCODE_OK;
      ddsrt_mutex_unlock (&fastpath_rdary->rdary_lock);
    }
    else
    {
      ddsrt_mutex_unlock (&fastpath_rdary->rdary_lock);
      rc = deliver_locally_slowpath (gv, source_entity, source_entity_locked, wrinfo, ops, vsourceinfo);
    }
  } while (rc == DDS_RETCODE_TRY_AGAIN);
  return rc;
}
