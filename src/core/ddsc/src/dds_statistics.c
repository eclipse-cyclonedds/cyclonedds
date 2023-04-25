// Copyright(c) 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>
#include <string.h>

#include "dds/ddsc/dds_statistics.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/log.h"
#include "dds__entity.h"
#include "dds__statistics.h"

struct dds_statistics *dds_alloc_statistics (const struct dds_entity *e, const struct dds_stat_descriptor *d)
{
  struct dds_statistics *s = ddsrt_malloc (sizeof (*s) + d->count * sizeof (s->kv[0]));
  s->entity = e->m_hdllink.hdl;
  s->opaque = e->m_iid;
  s->time = 0;
  s->count = d->count;
  memset (s->kv, 0, d->count * sizeof (s->kv[0]));
  for (size_t i = 0; i < s->count; i++)
  {
    s->kv[i].kind = d->kv[i].kind;
    s->kv[i].name = d->kv[i].name;
  }
  return s;
}

struct dds_statistics *dds_create_statistics (dds_entity_t entity)
{
  dds_entity *e;
  struct dds_statistics *s;
  if (dds_entity_pin (entity, &e) != DDS_RETCODE_OK)
    return NULL;
  struct ddsi_thread_state * const thrst = ddsi_lookup_thread_state ();
  ddsi_thread_state_awake (thrst, &e->m_domain->gv);
  if ((s = dds_entity_deriver_create_statistics (e)) != NULL)
    dds_entity_deriver_refresh_statistics (e, s);
  ddsi_thread_state_asleep (thrst);
  dds_entity_unpin (e);
  return s;
}

dds_return_t dds_refresh_statistics (struct dds_statistics *stat)
{
  dds_return_t rc;
  dds_entity *e;
  if (stat == NULL)
    return DDS_RETCODE_BAD_PARAMETER;
  if ((rc = dds_entity_pin (stat->entity, &e)) != DDS_RETCODE_OK)
    return rc;
  if (stat->opaque != e->m_iid)
  {
    dds_entity_unpin (e);
    return DDS_RETCODE_BAD_PARAMETER;
  }
  struct ddsi_thread_state * const thrst = ddsi_lookup_thread_state ();
  ddsi_thread_state_awake (thrst, &e->m_domain->gv);
  stat->time = dds_time ();
  dds_entity_deriver_refresh_statistics (e, stat);
  ddsi_thread_state_asleep (thrst);
  dds_entity_unpin (e);
  return DDS_RETCODE_OK;
}

const struct dds_stat_keyvalue *dds_lookup_statistic (const struct dds_statistics *stat, const char *name)
{
  if (stat == NULL)
    return NULL;
  for (size_t i = 0; i < stat->count; i++)
    if (strcmp (stat->kv[i].name, name) == 0)
      return &stat->kv[i];
  return NULL;
}

void dds_delete_statistics (struct dds_statistics *stat)
{
  ddsrt_free (stat);
}
