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
#include <string.h>
#include "dds__entity.h"
#include "dds__reader.h"
#include "dds/ddsi/ddsi_tkmap.h"
#include "dds/ddsc/dds_rhc.h"
#include "dds/ddsi/ddsi_thread.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/ddsi_entity.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_sertype.h"

#include "dds/ddsc/dds_loan_api.h"

/*
  dds_read_impl: Core read/take function. Usually maxs is size of buf and si
  into which samples/status are written, when set to zero is special case
  indicating that size set from number of samples in cache and also that cache
  has been locked. This is used to support C++ API reading length unlimited
  which is interpreted as "all relevant samples in cache".
*/
static dds_return_t dds_read_impl (bool take, dds_entity_t reader_or_condition, void **buf, size_t bufsz, uint32_t maxs, dds_sample_info_t *si, uint32_t mask, dds_instance_handle_t hand, bool lock, bool only_reader)
{
  dds_return_t ret = DDS_RETCODE_OK;
  struct dds_entity *entity;
  struct dds_reader *rd;
  struct dds_readcond *cond;
  unsigned nodata_cleanups = 0;
#define NC_CLEAR_LOAN_OUT 1u
#define NC_FREE_BUF 2u
#define NC_RESET_BUF 4u

  if (buf == NULL || si == NULL || maxs == 0 || bufsz == 0 || bufsz < maxs || maxs > INT32_MAX)
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

  struct ddsi_thread_state * const thrst = ddsi_lookup_thread_state ();
  ddsi_thread_state_awake (thrst, &entity->m_domain->gv);

  /* Allocate samples if not provided (assuming all or none provided) */
  if (buf[0] == NULL)
  {
    /* Allocate, use or reallocate loan cached on reader */
    ddsrt_mutex_lock (&rd->m_entity.m_mutex);
    if (rd->m_loan_out)
    {
      ddsi_sertype_realloc_samples (buf, rd->m_topic->m_stype, NULL, 0, maxs);
      nodata_cleanups = NC_FREE_BUF | NC_RESET_BUF;
    }
    else
    {
      if (rd->m_loan)
      {
        if (rd->m_loan_size >= maxs)
        {
          /* This ensures buf is properly initialized */
          ddsi_sertype_realloc_samples (buf, rd->m_topic->m_stype, rd->m_loan, rd->m_loan_size, rd->m_loan_size);
        }
        else
        {
          ddsi_sertype_realloc_samples (buf, rd->m_topic->m_stype, rd->m_loan, rd->m_loan_size, maxs);
          rd->m_loan_size = maxs;
        }
      }
      else
      {
        ddsi_sertype_realloc_samples (buf, rd->m_topic->m_stype, NULL, 0, maxs);
        rd->m_loan_size = maxs;
      }
      rd->m_loan = buf[0];
      rd->m_loan_out = true;
      nodata_cleanups = NC_RESET_BUF | NC_CLEAR_LOAN_OUT;
    }
    ddsrt_mutex_unlock (&rd->m_entity.m_mutex);
  }

  /* read/take resets data available status -- must reset before reading because
     the actual writing is protected by RHC lock, not by rd->m_entity.m_lock */
  const uint32_t sm_old = dds_entity_status_reset_ov (&rd->m_entity, DDS_DATA_AVAILABLE_STATUS);
  /* reset DATA_ON_READERS status on subscriber after successful read/take if materialized */
  if (sm_old & (DDS_DATA_ON_READERS_STATUS << SAM_ENABLED_SHIFT))
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
      ddsi_sertype_free_samples (rd->m_topic->m_stype, buf, maxs, DDS_FREE_ALL);
    if (nodata_cleanups & NC_RESET_BUF)
      buf[0] = NULL;
    ddsrt_mutex_unlock (&rd->m_entity.m_mutex);
  }
  dds_entity_unpin (entity);
  ddsi_thread_state_asleep (thrst);
  return ret;

#undef NC_CLEAR_LOAN_OUT
#undef NC_FREE_BUF
#undef NC_RESET_BUF

fail_pinned:
  dds_entity_unpin (entity);
fail:
  return ret;
}

static dds_return_t dds_readcdr_impl (bool take, dds_entity_t reader_or_condition, struct ddsi_serdata **buf, uint32_t maxs, dds_sample_info_t *si, uint32_t mask, dds_instance_handle_t hand, bool lock)
{
  dds_return_t ret = DDS_RETCODE_OK;
  struct dds_reader *rd;
  struct dds_entity *entity;

  if (buf == NULL || si == NULL || maxs == 0 || maxs > INT32_MAX)
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

  struct ddsi_thread_state * const thrst = ddsi_lookup_thread_state ();
  ddsi_thread_state_awake (thrst, &entity->m_domain->gv);

  /* read/take resets data available status -- must reset before reading because
     the actual writing is protected by RHC lock, not by rd->m_entity.m_lock */
  const uint32_t sm_old = dds_entity_status_reset_ov (&rd->m_entity, DDS_DATA_AVAILABLE_STATUS);
  /* reset DATA_ON_READERS status on subscriber after successful read/take if materialized */
  if (sm_old & (DDS_DATA_ON_READERS_STATUS << SAM_ENABLED_SHIFT))
    dds_entity_status_reset (rd->m_entity.m_parent, DDS_DATA_ON_READERS_STATUS);

  if (take)
    ret = dds_rhc_takecdr (rd->m_rhc, lock, buf, si, maxs, mask & DDS_ANY_SAMPLE_STATE, mask & DDS_ANY_VIEW_STATE, mask & DDS_ANY_INSTANCE_STATE, hand);
  else
    ret = dds_rhc_readcdr (rd->m_rhc, lock, buf, si, maxs, mask & DDS_ANY_SAMPLE_STATE, mask & DDS_ANY_VIEW_STATE, mask & DDS_ANY_INSTANCE_STATE, hand);

  dds_entity_unpin (entity);
  ddsi_thread_state_asleep (thrst);
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

dds_return_t dds_readcdr (dds_entity_t rd_or_cnd, struct ddsi_serdata **buf, uint32_t maxs, dds_sample_info_t *si, uint32_t mask)
{
  bool lock = true;
  if (maxs == DDS_READ_WITHOUT_LOCK)
  {
    lock = false;
    /* FIXME: Fix the interface. */
    maxs = 100;
  }
  return dds_readcdr_impl (false, rd_or_cnd, buf, maxs, si, mask, DDS_HANDLE_NIL, lock);
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
    maxs = (uint32_t)bufsz;
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

dds_return_t dds_readcdr_instance (dds_entity_t rd_or_cnd, struct ddsi_serdata **buf, uint32_t maxs, dds_sample_info_t *si, dds_instance_handle_t handle, uint32_t mask)
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
  return dds_readcdr_impl(false, rd_or_cnd, buf, maxs, si, mask, handle, lock);
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
    maxs = (uint32_t)bufsz;
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

dds_return_t dds_takecdr_instance (dds_entity_t rd_or_cnd, struct ddsi_serdata **buf, uint32_t maxs, dds_sample_info_t *si, dds_instance_handle_t handle, uint32_t mask)
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
  return dds_readcdr_impl(true, rd_or_cnd, buf, maxs, si, mask, handle, lock);
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

dds_return_t dds_return_reader_loan (dds_reader *rd, void **buf, int32_t bufsz)
{
  if (bufsz <= 0)
  {
    /* No data whatsoever, or an invocation following a failed read/take call.  Read/take
       already take care of restoring the state prior to their invocation if they return
       no data.  Return late so invalid handles can be detected. */
    return DDS_RETCODE_OK;
  }
  assert (buf[0] != NULL);

  const struct ddsi_sertype *st = rd->m_topic->m_stype;

  /* The potentially time consuming part of what happens here (freeing samples)
     can safely be done without holding the reader lock, but that particular
     lock is not used during insertion of data & triggering waitsets (that's
     the observer_lock), so holding it for a bit longer in return for simpler
     code is a fair trade-off. */
  ddsrt_mutex_lock (&rd->m_entity.m_mutex);
  if (buf[0] != rd->m_loan)
  {
    /* Not so much a loan as a buffer allocated by the middleware on behalf of the
       application.  So it really is no more than a sophisticated variant of "free". */
    ddsi_sertype_free_samples (st, buf, (size_t) bufsz, DDS_FREE_ALL);
    buf[0] = NULL;
  }
  else if (!rd->m_loan_out)
  {
    /* Trying to return a loan that has been returned already */
    ddsrt_mutex_unlock (&rd->m_entity.m_mutex);
    return DDS_RETCODE_PRECONDITION_NOT_MET;
  }
  else
  {
    /* Free only the memory referenced from the samples, not the samples themselves.
       Zero them to guarantee the absence of dangling pointers that might cause
       trouble on a following operation.  FIXME: there's got to be a better way */
    ddsi_sertype_free_samples (st, buf, (size_t) bufsz, DDS_FREE_CONTENTS);
    ddsi_sertype_zero_samples (st, rd->m_loan, rd->m_loan_size);
    rd->m_loan_out = false;
    buf[0] = NULL;
  }
  ddsrt_mutex_unlock (&rd->m_entity.m_mutex);
  return DDS_RETCODE_OK;
}
