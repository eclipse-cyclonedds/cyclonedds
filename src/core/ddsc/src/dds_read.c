/*
 * Copyright(c) 2006 to 2018 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#include <assert.h>
#include <string.h>
#include "dds__entity.h"
#include "dds__reader.h"
#include "dds/ddsi/ddsi_tkmap.h"
#include "dds/ddsc/dds_rhc.h"
#include "dds/ddsi/q_thread.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/q_entity.h"
#include "dds/ddsi/q_globals.h"
#include "dds/ddsi/ddsi_sertopic.h"

/*
  dds_read_impl: Core read/take function. Usually maxs is size of buf and si
  into which samples/status are written, when set to zero is special case
  indicating that size set from number of samples in cache and also that cache
  has been locked. This is used to support C++ API reading length unlimited
  which is interpreted as "all relevant samples in cache".
*/
static dds_return_t dds_read_impl (bool take, dds_entity_t reader_or_condition, void **buf, size_t bufsz, uint32_t maxs, dds_sample_info_t *si, uint32_t mask, dds_instance_handle_t hand, bool lock, bool only_reader)
{
  struct thread_state1 * const ts1 = lookup_thread_state ();
  dds_return_t ret = DDS_RETCODE_OK;
  struct dds_entity *entity;
  struct dds_reader *rd;
  struct dds_readcond *cond;
  unsigned nodata_cleanups = 0;
#define NC_CLEAR_LOAN_OUT 1u
#define NC_FREE_BUF 2u
#define NC_RESET_BUF 4u

  if (buf == NULL || si == NULL || maxs == 0 || bufsz == 0 || bufsz < maxs)
    return DDS_RETCODE_BAD_PARAMETER;

  if ((ret = dds_entity_pin (reader_or_condition, &entity)) < 0) {
    goto fail;
  } else if (dds_entity_kind (entity) == DDS_KIND_READER) {
    rd = (dds_reader *) entity;
    cond = NULL;
  } else if (only_reader) {
    ret = DDS_RETCODE_ILLEGAL_OPERATION;
    goto fail_pinned;
  } else if (dds_entity_kind (entity) != DDS_KIND_COND_READ && dds_entity_kind (entity) != DDS_KIND_COND_QUERY) {
    ret = DDS_RETCODE_ILLEGAL_OPERATION;
    goto fail_pinned;
  } else {
    rd = (dds_reader *) entity->m_parent;
    cond = (dds_readcond *) entity;
  }

  thread_state_awake (ts1, &entity->m_domain->gv);

  if (hand != DDS_HANDLE_NIL)
  {
    if (ddsi_tkmap_find_by_id (entity->m_domain->gv.m_tkmap, hand) == NULL) {
      ret = DDS_RETCODE_PRECONDITION_NOT_MET;
      goto fail_awake_pinned;
    }
  }

  /* Allocate samples if not provided (assuming all or none provided) */
  if (buf[0] == NULL)
  {
    /* Allocate, use or reallocate loan cached on reader */
    ddsrt_mutex_lock (&rd->m_entity.m_mutex);
    if (rd->m_loan_out)
    {
      ddsi_sertopic_realloc_samples (buf, rd->m_topic->m_stopic, NULL, 0, maxs);
      nodata_cleanups = NC_FREE_BUF | NC_RESET_BUF;
    }
    else
    {
      if (rd->m_loan)
      {
        if (rd->m_loan_size >= maxs)
        {
          /* This ensures buf is properly initialized */
          ddsi_sertopic_realloc_samples (buf, rd->m_topic->m_stopic, rd->m_loan, rd->m_loan_size, rd->m_loan_size);
        }
        else
        {
          ddsi_sertopic_realloc_samples (buf, rd->m_topic->m_stopic, rd->m_loan, rd->m_loan_size, maxs);
          rd->m_loan = buf[0];
          rd->m_loan_size = maxs;
        }
        nodata_cleanups = NC_RESET_BUF;
      }
      else
      {
        ddsi_sertopic_realloc_samples (buf, rd->m_topic->m_stopic, NULL, 0, maxs);
        rd->m_loan = buf[0];
        rd->m_loan_size = maxs;
      }
      rd->m_loan_out = true;
      nodata_cleanups |= NC_CLEAR_LOAN_OUT;
    }
    ddsrt_mutex_unlock (&rd->m_entity.m_mutex);
  }

  /* read/take resets data available status -- must reset before reading because
     the actual writing is protected by RHC lock, not by rd->m_entity.m_lock */
  dds_entity_status_reset (&rd->m_entity, DDS_DATA_AVAILABLE_STATUS);

  /* reset DATA_ON_READERS status on subscriber after successful read/take */
  assert (dds_entity_kind (rd->m_entity.m_parent) == DDS_KIND_SUBSCRIBER);
  dds_entity_status_reset (rd->m_entity.m_parent, DDS_DATA_ON_READERS_STATUS);

  if (take)
    ret = dds_rhc_take (rd->m_rhc, lock, buf, si, maxs, mask, hand, cond);
  else
    ret = dds_rhc_read (rd->m_rhc, lock, buf, si, maxs, mask, hand, cond);

  /* if no data read, restore the state to what it was before the call, with the sole
     exception of holding on to a buffer we just allocated and that is pointed to by
     rd->m_loan */
  if (ret <= 0 && nodata_cleanups)
  {
    ddsrt_mutex_lock (&rd->m_entity.m_mutex);
    if (nodata_cleanups & NC_CLEAR_LOAN_OUT)
      rd->m_loan_out = false;
    if (nodata_cleanups & NC_FREE_BUF)
      ddsi_sertopic_free_samples (rd->m_topic->m_stopic, buf, maxs, DDS_FREE_ALL);
    if (nodata_cleanups & NC_RESET_BUF)
      buf[0] = NULL;
    ddsrt_mutex_unlock (&rd->m_entity.m_mutex);
  }
  dds_entity_unpin (entity);
  thread_state_asleep (ts1);
  return ret;

#undef NC_CLEAR_LOAN_OUT
#undef NC_FREE_BUF
#undef NC_RESET_BUF

fail_awake_pinned:
  thread_state_asleep (ts1);
fail_pinned:
  dds_entity_unpin (entity);
fail:
  return ret;
}

static dds_return_t dds_readcdr_impl (bool take, dds_entity_t reader_or_condition, struct ddsi_serdata **buf, uint32_t maxs, dds_sample_info_t *si, uint32_t mask, dds_instance_handle_t hand, bool lock)
{
  struct thread_state1 * const ts1 = lookup_thread_state ();
  dds_return_t ret = DDS_RETCODE_OK;
  struct dds_reader *rd;
  struct dds_entity *entity;

  assert (take);
  assert (buf);
  assert (si);
  assert (hand == DDS_HANDLE_NIL);
  assert (maxs > 0);
  (void)take;

  if ((ret = dds_entity_pin (reader_or_condition, &entity)) < 0) {
    return ret;
  } else if (dds_entity_kind (entity) == DDS_KIND_READER) {
    rd = (dds_reader *) entity;
  } else if (dds_entity_kind (entity) != DDS_KIND_COND_READ && dds_entity_kind (entity) != DDS_KIND_COND_QUERY) {
    dds_entity_unpin (entity);
    return DDS_RETCODE_ILLEGAL_OPERATION;
  } else {
    rd = (dds_reader *) entity->m_parent;
  }

  thread_state_awake (ts1, &entity->m_domain->gv);

  /* read/take resets data available status -- must reset before reading because
     the actual writing is protected by RHC lock, not by rd->m_entity.m_lock */
  dds_entity_status_reset (&rd->m_entity, DDS_DATA_AVAILABLE_STATUS);

  /* reset DATA_ON_READERS status on subscriber after successful read/take */
  assert (dds_entity_kind (rd->m_entity.m_parent) == DDS_KIND_SUBSCRIBER);
  dds_entity_status_reset (rd->m_entity.m_parent, DDS_DATA_ON_READERS_STATUS);

  ret = dds_rhc_takecdr (rd->m_rhc, lock, buf, si, maxs, mask & DDS_ANY_SAMPLE_STATE, mask & DDS_ANY_VIEW_STATE, mask & DDS_ANY_INSTANCE_STATE, hand);
  dds_entity_unpin (entity);
  thread_state_asleep (ts1);
  return ret;
}

dds_return_t dds_read (dds_entity_t rd_or_cnd, void **buf, dds_sample_info_t *si, size_t bufsz, uint32_t maxs)
{
  bool lock = true;
  if (maxs == DDS_READ_WITHOUT_LOCK)
  {
    lock = false;
    /* FIXME: Fix the interface. */
    maxs = (uint32_t) bufsz;
  }
  return dds_read_impl (false, rd_or_cnd, buf, bufsz, maxs, si, NO_STATE_MASK_SET, DDS_HANDLE_NIL, lock, false);
}

dds_return_t dds_read_wl (dds_entity_t rd_or_cnd, void **buf, dds_sample_info_t *si, uint32_t maxs)
{
  bool lock = true;
  if (maxs == DDS_READ_WITHOUT_LOCK)
  {
    lock = false;
    /* FIXME: Fix the interface. */
    maxs = 100;
  }
  return dds_read_impl (false, rd_or_cnd, buf, maxs, maxs, si, NO_STATE_MASK_SET, DDS_HANDLE_NIL, lock, false);
}

dds_return_t dds_read_mask (dds_entity_t rd_or_cnd, void **buf, dds_sample_info_t *si, size_t bufsz, uint32_t maxs, uint32_t mask)
{
  bool lock = true;
  if (maxs == DDS_READ_WITHOUT_LOCK)
  {
    lock = false;
    /* FIXME: Fix the interface. */
    maxs = (uint32_t)bufsz;
  }
  return dds_read_impl (false, rd_or_cnd, buf, bufsz, maxs, si, mask, DDS_HANDLE_NIL, lock, false);
}

dds_return_t dds_read_mask_wl (dds_entity_t rd_or_cnd, void **buf, dds_sample_info_t *si, uint32_t maxs, uint32_t mask)
{
  bool lock = true;
  if (maxs == DDS_READ_WITHOUT_LOCK)
  {
    lock = false;
    /* FIXME: Fix the interface. */
    maxs = 100;
  }
  return dds_read_impl (false, rd_or_cnd, buf, maxs, maxs, si, mask, DDS_HANDLE_NIL, lock, false);
}

dds_return_t dds_read_instance (dds_entity_t rd_or_cnd, void **buf, dds_sample_info_t *si, size_t bufsz, uint32_t maxs, dds_instance_handle_t handle)
{
  bool lock = true;

  if (handle == DDS_HANDLE_NIL)
    return DDS_RETCODE_PRECONDITION_NOT_MET;

  if (maxs == DDS_READ_WITHOUT_LOCK)
  {
    lock = false;
    /* FIXME: Fix the interface. */
    maxs = 100;
  }
  return dds_read_impl (false, rd_or_cnd, buf, bufsz, maxs, si, NO_STATE_MASK_SET, handle, lock, false);
}

dds_return_t dds_read_instance_wl (dds_entity_t rd_or_cnd, void **buf, dds_sample_info_t *si, uint32_t maxs, dds_instance_handle_t handle)
{
  bool lock = true;

  if (handle == DDS_HANDLE_NIL)
    return DDS_RETCODE_PRECONDITION_NOT_MET;

  if (maxs == DDS_READ_WITHOUT_LOCK)
  {
    lock = false;
    /* FIXME: Fix the interface. */
    maxs = 100;
  }
  return dds_read_impl (false, rd_or_cnd, buf, maxs, maxs, si, NO_STATE_MASK_SET, handle, lock, false);
}

dds_return_t dds_read_instance_mask (dds_entity_t rd_or_cnd, void **buf, dds_sample_info_t *si, size_t bufsz, uint32_t maxs, dds_instance_handle_t handle, uint32_t mask)
{
  bool lock = true;

  if (handle == DDS_HANDLE_NIL)
    return DDS_RETCODE_PRECONDITION_NOT_MET;

  if (maxs == DDS_READ_WITHOUT_LOCK)
  {
    lock = false;
    /* FIXME: Fix the interface. */
    maxs = 100;
  }
  return dds_read_impl (false, rd_or_cnd, buf, bufsz, maxs, si, mask, handle, lock, false);
}

dds_return_t dds_read_instance_mask_wl (dds_entity_t rd_or_cnd, void **buf, dds_sample_info_t *si, uint32_t maxs, dds_instance_handle_t handle, uint32_t mask)
{
  bool lock = true;

  if (handle == DDS_HANDLE_NIL)
    return DDS_RETCODE_PRECONDITION_NOT_MET;

  if (maxs == DDS_READ_WITHOUT_LOCK)
  {
    lock = false;
    /* FIXME: Fix the interface. */
    maxs = 100;
  }
  return dds_read_impl (false, rd_or_cnd, buf, maxs, maxs, si, mask, handle, lock, false);
}

dds_return_t dds_read_next (dds_entity_t reader, void **buf, dds_sample_info_t *si)
{
  uint32_t mask = DDS_NOT_READ_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
  return dds_read_impl (false, reader, buf, 1u, 1u, si, mask, DDS_HANDLE_NIL, true, true);
}

dds_return_t dds_read_next_wl (
                 dds_entity_t reader,
                 void **buf,
                 dds_sample_info_t *si)
{
  uint32_t mask = DDS_NOT_READ_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
  return dds_read_impl (false, reader, buf, 1u, 1u, si, mask, DDS_HANDLE_NIL, true, true);
}

dds_return_t dds_take (dds_entity_t rd_or_cnd, void **buf, dds_sample_info_t *si, size_t bufsz, uint32_t maxs)
{
  bool lock = true;
  if (maxs == DDS_READ_WITHOUT_LOCK)
  {
    lock = false;
    /* FIXME: Fix the interface. */
    maxs = (uint32_t)bufsz;
  }
  return dds_read_impl (true, rd_or_cnd, buf, bufsz, maxs, si, NO_STATE_MASK_SET, DDS_HANDLE_NIL, lock, false);
}

dds_return_t dds_take_wl (dds_entity_t rd_or_cnd, void ** buf, dds_sample_info_t * si, uint32_t maxs)
{
  bool lock = true;
  if (maxs == DDS_READ_WITHOUT_LOCK)
  {
    lock = false;
    /* FIXME: Fix the interface. */
    maxs = 100;
  }
  return dds_read_impl (true, rd_or_cnd, buf, maxs, maxs, si, NO_STATE_MASK_SET, DDS_HANDLE_NIL, lock, false);
}

dds_return_t dds_take_mask (dds_entity_t rd_or_cnd, void **buf, dds_sample_info_t *si, size_t bufsz, uint32_t maxs, uint32_t mask)
{
  bool lock = true;
  if (maxs == DDS_READ_WITHOUT_LOCK)
  {
    lock = false;
    /* FIXME: Fix the interface. */
    maxs = (uint32_t) bufsz;
  }
  return dds_read_impl (true, rd_or_cnd, buf, bufsz, maxs, si, mask, DDS_HANDLE_NIL, lock, false);
}

dds_return_t dds_take_mask_wl (dds_entity_t rd_or_cnd, void **buf, dds_sample_info_t *si, uint32_t maxs, uint32_t mask)
{
  bool lock = true;
  if (maxs == DDS_READ_WITHOUT_LOCK)
  {
    lock = false;
    /* FIXME: Fix the interface. */
    maxs = 100;
  }
  return dds_read_impl (true, rd_or_cnd, buf, maxs, maxs, si, mask, DDS_HANDLE_NIL, lock, false);
}

dds_return_t dds_takecdr (dds_entity_t rd_or_cnd, struct ddsi_serdata **buf, uint32_t maxs, dds_sample_info_t *si, uint32_t mask)
{
  bool lock = true;
  if (maxs == DDS_READ_WITHOUT_LOCK)
  {
    lock = false;
    /* FIXME: Fix the interface. */
    maxs = 100;
  }
  return dds_readcdr_impl (true, rd_or_cnd, buf, maxs, si, mask, DDS_HANDLE_NIL, lock);
}

dds_return_t dds_take_instance (dds_entity_t rd_or_cnd, void **buf, dds_sample_info_t *si, size_t bufsz, uint32_t maxs, dds_instance_handle_t handle)
{
  bool lock = true;

  if (handle == DDS_HANDLE_NIL)
    return DDS_RETCODE_PRECONDITION_NOT_MET;

  if (maxs == DDS_READ_WITHOUT_LOCK)
  {
    lock = false;
    /* FIXME: Fix the interface. */
    maxs = 100;
  }
  return dds_read_impl(true, rd_or_cnd, buf, bufsz, maxs, si, NO_STATE_MASK_SET, handle, lock, false);
}

dds_return_t dds_take_instance_wl (dds_entity_t rd_or_cnd, void **buf, dds_sample_info_t *si, uint32_t maxs, dds_instance_handle_t handle)
{
  bool lock = true;

  if (handle == DDS_HANDLE_NIL)
    return DDS_RETCODE_PRECONDITION_NOT_MET;

  if (maxs == DDS_READ_WITHOUT_LOCK)
  {
    lock = false;
    /* FIXME: Fix the interface. */
    maxs = 100;
  }
  return dds_read_impl(true, rd_or_cnd, buf, maxs, maxs, si, NO_STATE_MASK_SET, handle, lock, false);
}

dds_return_t dds_take_instance_mask (dds_entity_t rd_or_cnd, void **buf, dds_sample_info_t *si, size_t bufsz, uint32_t maxs, dds_instance_handle_t handle, uint32_t mask)
{
  bool lock = true;

  if (handle == DDS_HANDLE_NIL)
    return DDS_RETCODE_PRECONDITION_NOT_MET;

  if (maxs == DDS_READ_WITHOUT_LOCK)
  {
    lock = false;
    /* FIXME: Fix the interface. */
    maxs = 100;
  }
  return dds_read_impl(true, rd_or_cnd, buf, bufsz, maxs, si, mask, handle, lock, false);
}

dds_return_t dds_take_instance_mask_wl (dds_entity_t rd_or_cnd, void **buf, dds_sample_info_t *si, uint32_t maxs, dds_instance_handle_t handle, uint32_t mask)
{
  bool lock = true;

  if (handle == DDS_HANDLE_NIL)
    return DDS_RETCODE_PRECONDITION_NOT_MET;

  if (maxs == DDS_READ_WITHOUT_LOCK)
  {
    lock = false;
    /* FIXME: Fix the interface. */
    maxs = 100;
  }
  return dds_read_impl(true, rd_or_cnd, buf, maxs, maxs, si, mask, handle, lock, false);
}

dds_return_t dds_take_next (dds_entity_t reader, void **buf, dds_sample_info_t *si)
{
  uint32_t mask = DDS_NOT_READ_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
  return dds_read_impl (true, reader, buf, 1u, 1u, si, mask, DDS_HANDLE_NIL, true, true);
}

dds_return_t dds_take_next_wl (dds_entity_t reader, void **buf, dds_sample_info_t *si)
{
  uint32_t mask = DDS_NOT_READ_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
  return dds_read_impl (true, reader, buf, 1u, 1u, si, mask, DDS_HANDLE_NIL, true, true);
}

dds_return_t dds_return_loan (dds_entity_t reader_or_condition, void **buf, int32_t bufsz)
{
  const struct ddsi_sertopic *st;
  dds_reader *rd;
  dds_entity *entity;
  dds_return_t ret = DDS_RETCODE_OK;

  if (buf == NULL || (*buf == NULL && bufsz > 0))
    return DDS_RETCODE_BAD_PARAMETER;

  if ((ret = dds_entity_pin (reader_or_condition, &entity)) < 0) {
    return ret;
  } else if (dds_entity_kind (entity) == DDS_KIND_READER) {
    rd = (dds_reader *) entity;
  } else if (dds_entity_kind (entity) != DDS_KIND_COND_READ && dds_entity_kind (entity) != DDS_KIND_COND_QUERY) {
    dds_entity_unpin (entity);
    return DDS_RETCODE_ILLEGAL_OPERATION;
  } else {
    rd = (dds_reader *) entity->m_parent;
  }

  st = rd->m_topic->m_stopic;
  for (int32_t i = 0; i < bufsz; i++)
    ddsi_sertopic_free_sample (st, buf[i], DDS_FREE_CONTENTS);

  /* If possible return loan buffer to reader */
  ddsrt_mutex_lock (&rd->m_entity.m_mutex);
  if (rd->m_loan != 0 && (buf[0] == rd->m_loan))
  {
    rd->m_loan_out = false;
    ddsi_sertopic_zero_samples (st, rd->m_loan, rd->m_loan_size);
    buf[0] = NULL;
  }
  ddsrt_mutex_unlock (&rd->m_entity.m_mutex);
  dds_entity_unpin (entity);
  return DDS_RETCODE_OK;
}
