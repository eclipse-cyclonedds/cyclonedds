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
#include "dds__read.h"
#include "dds/ddsi/ddsi_tkmap.h"
#include "dds/ddsc/dds_rhc.h"
#include "dds/ddsi/ddsi_thread.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/ddsi_entity.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_serdata.h"

#include "dds/ddsc/dds_psmx.h"
#include "dds__loaned_sample.h"
#include "dds__heap_loan.h"

void dds_read_collect_sample_arg_init (struct dds_read_collect_sample_arg *arg, void **ptrs, dds_sample_info_t *infos, struct dds_loan_pool *loan_pool, struct dds_loan_pool *heap_loan_cache)
{
  arg->next_idx = 0;
  arg->ptrs = ptrs;
  arg->infos = infos;
  arg->loan_pool = loan_pool;
  arg->heap_loan_cache = heap_loan_cache;
}

dds_return_t dds_read_collect_sample (void *varg, const dds_sample_info_t *si, const struct ddsi_sertype *st, struct ddsi_serdata *sd)
{
  struct dds_read_collect_sample_arg * const arg = varg;
  bool ok;
  arg->infos[arg->next_idx] = *si;

  if (si->valid_data)
    ok = ddsi_serdata_to_sample (sd, arg->ptrs[arg->next_idx], NULL, NULL);
  else
  {
    /* ddsi_serdata_untyped_to_sample just deals with the key value, without paying any attention to attributes;
       but that makes life harder for the user: the attributes of an invalid sample would be garbage, but would
       nonetheless have to be freed in the end.  Zero'ing it explicitly solves that problem. */
    ddsi_sertype_free_sample (st, arg->ptrs[arg->next_idx], DDS_FREE_CONTENTS);
    ddsi_sertype_zero_sample (st, arg->ptrs[arg->next_idx]);
    ok = ddsi_serdata_untyped_to_sample (st, sd, arg->ptrs[arg->next_idx], NULL, NULL);
  }
  arg->next_idx++;
  return ok ? DDS_RETCODE_OK : DDS_RETCODE_ERROR;
}

static dds_return_t dds_read_collect_sample_loan_zerocopy (struct dds_read_collect_sample_arg *arg, const dds_sample_info_t *si, struct ddsi_serdata *sd)
{
  dds_loaned_sample_t * const ls = sd->loan;
  dds_return_t ret;
  if (ls == NULL)
    return 1; // a slightly unusual choice of return value
  else if (ls->metadata->sample_state != DDS_LOANED_SAMPLE_STATE_RAW_DATA && ls->metadata->sample_state != DDS_LOANED_SAMPLE_STATE_RAW_KEY)
    return 1; // same here
  else if ((ret = dds_loan_pool_add_loan (arg->loan_pool, ls)) != DDS_RETCODE_OK)
  {
    assert (ret < 0);
    return ret;
  }
  else
  {
    dds_loaned_sample_ref (ls);
    arg->ptrs[arg->next_idx] = ls->sample_ptr;
    arg->infos[arg->next_idx] = *si;
    arg->next_idx++;
    return 0;
  }
}

dds_return_t dds_read_collect_sample_loan (void *varg, const dds_sample_info_t *si, const struct ddsi_sertype *st, struct ddsi_serdata *sd)
{
  struct dds_read_collect_sample_arg * const arg = varg;
  dds_return_t ret;

  if ((ret = dds_read_collect_sample_loan_zerocopy (arg, si, sd)) <= 0)
    return ret;

  const dds_loaned_sample_state_t state = (si->valid_data ? DDS_LOANED_SAMPLE_STATE_RAW_DATA : DDS_LOANED_SAMPLE_STATE_RAW_KEY);
  dds_loaned_sample_t *ls;
  if (arg->heap_loan_cache && (ls = dds_loan_pool_get_loan (arg->heap_loan_cache)) != NULL) {
    // lucky us, we can reuse a cached loaned_sample
  } else if ((ret = dds_heap_loan (st, state, &ls)) == DDS_RETCODE_OK) {
    // new heap loan, eventually it'll end up in the cache
  } else {
    return ret;
  }

  arg->ptrs[arg->next_idx] = ls->sample_ptr;
  if ((ret = dds_read_collect_sample (arg, si, st, sd)) != DDS_RETCODE_OK ||
      (ret = dds_loan_pool_add_loan (arg->loan_pool, ls)) != DDS_RETCODE_OK)
  {
    dds_loaned_sample_unref (ls);
    // take/read has to assume that all non-null pointers in the input array are valid (if
    // the first one is non-null on entry) and so an application that relies on that (as
    // opposed to calling dds_return_loan explicitly) would end up with a use-after-free
    // or similar if we don't reset it to a null pointer.
    arg->ptrs[arg->next_idx] = NULL;
  }
  return ret;
}

dds_return_t dds_read_collect_sample_refs (void *varg, const dds_sample_info_t *si, const struct ddsi_sertype *st, struct ddsi_serdata *sd)
{
  (void) st;
  struct dds_read_collect_sample_arg * const arg = varg;
  arg->infos[arg->next_idx] = *si;
  arg->ptrs[arg->next_idx] = ddsi_serdata_ref (sd);
  arg->next_idx++;
  return DDS_RETCODE_OK;
}

static dds_return_t dds_read_impl_setup (dds_entity_t reader_or_condition, bool only_reader, struct dds_entity **pentity, struct dds_reader **prd, struct dds_readcond **pcond, uint32_t *mask)
{
  dds_return_t ret;
  if ((ret = dds_entity_pin (reader_or_condition, pentity)) < 0) {
    return ret;
  } else if (dds_entity_kind (*pentity) == DDS_KIND_READER) {
    *prd = (dds_reader *) *pentity;
    *pcond = NULL;
  } else if (only_reader) {
    dds_entity_unpin (*pentity);
    return DDS_RETCODE_ILLEGAL_OPERATION;
  } else if (dds_entity_kind (*pentity) != DDS_KIND_COND_READ && dds_entity_kind (*pentity) != DDS_KIND_COND_QUERY) {
    dds_entity_unpin (*pentity);
    return DDS_RETCODE_ILLEGAL_OPERATION;
  } else {
    *prd = (dds_reader *) (*pentity)->m_parent;
    *pcond = (dds_readcond *) *pentity;
    if (*mask == 0)
      *mask = DDS_RHC_NO_STATE_MASK_SET;
  }
  return DDS_RETCODE_OK;
}

enum dds_read_impl_common_oper {
  READ_OPER_PEEK,
  READ_OPER_READ,
  READ_OPER_TAKE
};

static dds_return_t dds_read_impl_common (enum dds_read_impl_common_oper oper, struct dds_reader *rd, struct dds_readcond *cond, uint32_t maxs, uint32_t mask, dds_instance_handle_t hand, dds_read_with_collector_fn_t collect_sample, void *collect_sample_arg)
{
  /* read/take resets data available status -- must reset before reading because
     the actual writing is protected by RHC lock, not by rd->m_entity.m_lock */
  const uint32_t sm_old = dds_entity_status_reset_ov (&rd->m_entity, DDS_DATA_AVAILABLE_STATUS);
  /* reset DATA_ON_READERS status on subscriber after successful read/take if materialized */
  if (sm_old & (DDS_DATA_ON_READERS_STATUS << SAM_ENABLED_SHIFT))
    dds_entity_status_reset (rd->m_entity.m_parent, DDS_DATA_ON_READERS_STATUS);

  dds_return_t ret = DDS_RETCODE_ERROR;
  assert (maxs <= INT32_MAX);
  switch (oper)
  {
    case READ_OPER_PEEK:
      ret = dds_rhc_peek (rd->m_rhc, (int32_t) maxs, mask, hand, cond, collect_sample, collect_sample_arg);
      break;
    case READ_OPER_READ:
      ret = dds_rhc_read (rd->m_rhc, (int32_t) maxs, mask, hand, cond, collect_sample, collect_sample_arg);
      break;
    case READ_OPER_TAKE:
      ret = dds_rhc_take (rd->m_rhc, (int32_t) maxs, mask, hand, cond, collect_sample, collect_sample_arg);
      break;
  }
  return ret;
}

static dds_return_t dds_read_with_collector_impl (enum dds_read_impl_common_oper oper, dds_entity_t reader_or_condition, uint32_t maxs, uint32_t mask, dds_instance_handle_t hand, bool only_reader, dds_read_with_collector_fn_t collect_sample, void *collect_sample_arg)
{
  dds_return_t ret;
  struct dds_entity *entity;
  struct dds_reader *rd;
  struct dds_readcond *cond;

  if (collect_sample == 0 || maxs == 0 || maxs > INT32_MAX)
    return DDS_RETCODE_BAD_PARAMETER;

  if ((ret = dds_read_impl_setup (reader_or_condition, only_reader, &entity, &rd, &cond, &mask)) < 0)
    return ret;

  struct ddsi_thread_state * const thrst = ddsi_lookup_thread_state ();
  ddsi_thread_state_awake (thrst, &entity->m_domain->gv);
  ret = dds_read_impl_common (oper, rd, cond, maxs, mask, hand, collect_sample, collect_sample_arg);
  ddsi_thread_state_asleep (thrst);
  dds_entity_unpin (entity);
  return ret;
}

static dds_return_t dds_readcdr_impl (enum dds_read_impl_common_oper oper, dds_entity_t reader_or_condition, struct ddsi_serdata **buf, uint32_t maxs, dds_sample_info_t *si, uint32_t mask, dds_instance_handle_t hand)
{
  if (buf == NULL || si == NULL)
    return DDS_RETCODE_BAD_PARAMETER;
  struct dds_read_collect_sample_arg collect_arg;
  DDSRT_STATIC_ASSERT (sizeof (struct ddsi_serdata *) == sizeof (void *));
  dds_read_collect_sample_arg_init (&collect_arg, (void **) buf, si, NULL, NULL);
  const dds_return_t ret = dds_read_with_collector_impl (oper, reader_or_condition, maxs, mask, hand, true, dds_read_collect_sample_refs, &collect_arg);
  return ret;
}

static dds_return_t return_reader_loan_locked (dds_reader *rd, void **buf, int32_t bufsz)
  ddsrt_nonnull_all ddsrt_attribute_warn_unused_result;

static dds_return_t dds_read_impl (enum dds_read_impl_common_oper oper, dds_entity_t reader_or_condition, void **buf, size_t bufsz, uint32_t maxs, dds_sample_info_t *si, uint32_t mask, dds_instance_handle_t hand, bool only_reader)
{
  if (buf == NULL || si == NULL || maxs == 0 || bufsz == 0 || bufsz < maxs || maxs > INT32_MAX)
    return DDS_RETCODE_BAD_PARAMETER;

  dds_return_t ret;
  struct dds_entity *entity;
  struct dds_reader *rd;
  struct dds_readcond *cond;
  if ((ret = dds_read_impl_setup (reader_or_condition, only_reader, &entity, &rd, &cond, &mask)) < 0)
    return ret;

  struct ddsi_thread_state * const thrst = ddsi_lookup_thread_state ();
  ddsi_thread_state_awake (thrst, &entity->m_domain->gv);

  ddsrt_mutex_lock (&rd->m_entity.m_mutex);

  // Using either user-supplied memory or loans (not a mixture of the two) and we expect the
  // array is fully initialized.  We assume no non-null pointers following the first null
  // pointer.
  //
  // Failure modes:
  // - A non-loan followed by a loan: we assume only non-loans and so we'll start overwriting
  //   the contents of loans.  This may cause big trouble, it may also be of minor consequence
  //   (heap loans and no other references).
  // - A loan followed by a non-loan: these we can detect and report an error, after which
  //   memory leaks become quite likely.  At least we *did* report API abuse as soon as
  //   detected it ...
  if (buf[0] != NULL && (ret = return_reader_loan_locked (rd, buf, (int32_t) bufsz)) < 0)
    goto err_return_reader_loan_locked;

  struct dds_read_collect_sample_arg collect_arg;
  dds_read_collect_sample_arg_init (&collect_arg, buf, si, rd->m_loans, rd->m_heap_loan_cache);
  const bool use_loan = (buf[0] == NULL);
  const dds_read_with_collector_fn_t collect_sample = use_loan ? dds_read_collect_sample_loan : dds_read_collect_sample;
  ret = dds_read_impl_common (oper, rd, cond, maxs, mask, hand, collect_sample, &collect_arg);

  // If use_loan, make sure the `buf` is either fully initialized or ends on a null pointer
  // so the various paths returning loans know when to stop.  (If no data returned and using
  // loans, buf[0] is a null pointer, no point in updating it again.)
  if (use_loan && ret > 0 && (size_t) ret < bufsz - 1)
    buf[ret] = NULL;

  // Drop any remaining cached samples.  We have to be prepared to drop *some* because of the
  // path where PSMX delivers a serialized sample, which then gets converted into a heap loan
  // of a deserialized sample. Simply pushing any loans into this cache will cause it to grow
  // (effectively) without bounds in this case.
  dds_loaned_sample_t *loan;
  while ((loan = dds_loan_pool_get_loan (rd->m_heap_loan_cache)) != NULL)
    dds_loaned_sample_unref (loan);

err_return_reader_loan_locked:
  ddsrt_mutex_unlock (&rd->m_entity.m_mutex);
  ddsi_thread_state_asleep (thrst);
  dds_entity_unpin (entity);
  return ret;
}

dds_return_t dds_peek (dds_entity_t reader_or_condition, void **buf, dds_sample_info_t *si, size_t bufsz, uint32_t maxs)
{
  return dds_read_impl (READ_OPER_PEEK, reader_or_condition, buf, bufsz, maxs, si, 0, DDS_HANDLE_NIL, false);
}

dds_return_t dds_peek_mask (dds_entity_t reader_or_condition, void **buf, dds_sample_info_t *si, size_t bufsz, uint32_t maxs, uint32_t mask)
{
  return dds_read_impl (READ_OPER_PEEK, reader_or_condition, buf, bufsz, maxs, si, mask, DDS_HANDLE_NIL, false);
}

dds_return_t dds_peek_instance (dds_entity_t reader_or_condition, void **buf, dds_sample_info_t *si, size_t bufsz, uint32_t maxs, dds_instance_handle_t handle)
{
  if (handle == DDS_HANDLE_NIL)
    return DDS_RETCODE_PRECONDITION_NOT_MET;
  return dds_read_impl (READ_OPER_PEEK, reader_or_condition, buf, bufsz, maxs, si, 0, handle, false);
}

dds_return_t dds_peek_instance_mask (dds_entity_t reader_or_condition, void **buf, dds_sample_info_t *si, size_t bufsz, uint32_t maxs, dds_instance_handle_t handle, uint32_t mask)
{
  if (handle == DDS_HANDLE_NIL)
    return DDS_RETCODE_PRECONDITION_NOT_MET;
  return dds_read_impl (READ_OPER_PEEK, reader_or_condition, buf, bufsz, maxs, si, mask, handle, false);
}

dds_return_t dds_peek_next (dds_entity_t reader, void **buf, dds_sample_info_t *si)
{
  uint32_t mask = DDS_NOT_READ_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
  return dds_read_impl (READ_OPER_PEEK, reader, buf, 1u, 1u, si, mask, DDS_HANDLE_NIL, true);
}

dds_return_t dds_read (dds_entity_t reader_or_condition, void **buf, dds_sample_info_t *si, size_t bufsz, uint32_t maxs)
{
  return dds_read_impl (READ_OPER_READ, reader_or_condition, buf, bufsz, maxs, si, 0, DDS_HANDLE_NIL, false);
}

dds_return_t dds_read_wl (dds_entity_t reader_or_condition, void **buf, dds_sample_info_t *si, uint32_t maxs)
{
  return dds_read (reader_or_condition, buf, si, maxs, maxs);
}

dds_return_t dds_read_mask (dds_entity_t reader_or_condition, void **buf, dds_sample_info_t *si, size_t bufsz, uint32_t maxs, uint32_t mask)
{
  return dds_read_impl (READ_OPER_READ, reader_or_condition, buf, bufsz, maxs, si, mask, DDS_HANDLE_NIL, false);
}

dds_return_t dds_read_mask_wl (dds_entity_t reader_or_condition, void **buf, dds_sample_info_t *si, uint32_t maxs, uint32_t mask)
{
  return dds_read_mask (reader_or_condition, buf, si, maxs, maxs, mask);
}

dds_return_t dds_read_instance (dds_entity_t reader_or_condition, void **buf, dds_sample_info_t *si, size_t bufsz, uint32_t maxs, dds_instance_handle_t handle)
{
  if (handle == DDS_HANDLE_NIL)
    return DDS_RETCODE_PRECONDITION_NOT_MET;
  return dds_read_impl (READ_OPER_READ, reader_or_condition, buf, bufsz, maxs, si, 0, handle, false);
}

dds_return_t dds_read_instance_wl (dds_entity_t reader_or_condition, void **buf, dds_sample_info_t *si, uint32_t maxs, dds_instance_handle_t handle)
{
  return dds_read_instance (reader_or_condition, buf, si, maxs, maxs, handle);
}

dds_return_t dds_read_instance_mask (dds_entity_t reader_or_condition, void **buf, dds_sample_info_t *si, size_t bufsz, uint32_t maxs, dds_instance_handle_t handle, uint32_t mask)
{
  if (handle == DDS_HANDLE_NIL)
    return DDS_RETCODE_PRECONDITION_NOT_MET;
  return dds_read_impl (READ_OPER_READ, reader_or_condition, buf, bufsz, maxs, si, mask, handle, false);
}

dds_return_t dds_read_instance_mask_wl (dds_entity_t reader_or_condition, void **buf, dds_sample_info_t *si, uint32_t maxs, dds_instance_handle_t handle, uint32_t mask)
{
  return dds_read_instance_mask (reader_or_condition, buf, si, maxs, maxs, handle, mask);
}

dds_return_t dds_read_next (dds_entity_t reader, void **buf, dds_sample_info_t *si)
{
  uint32_t mask = DDS_NOT_READ_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
  return dds_read_impl (READ_OPER_READ, reader, buf, 1u, 1u, si, mask, DDS_HANDLE_NIL, true);
}

dds_return_t dds_read_next_wl (dds_entity_t reader, void **buf, dds_sample_info_t *si)
{
  return dds_read_next (reader, buf, si);
}

dds_return_t dds_take (dds_entity_t reader_or_condition, void **buf, dds_sample_info_t *si, size_t bufsz, uint32_t maxs)
{
  return dds_read_impl (READ_OPER_TAKE, reader_or_condition, buf, bufsz, maxs, si, 0, DDS_HANDLE_NIL, false);
}

dds_return_t dds_take_wl (dds_entity_t reader_or_condition, void **buf, dds_sample_info_t *si, uint32_t maxs)
{
  return dds_take (reader_or_condition, buf, si, maxs, maxs);
}

dds_return_t dds_take_mask (dds_entity_t reader_or_condition, void **buf, dds_sample_info_t *si, size_t bufsz, uint32_t maxs, uint32_t mask)
{
  return dds_read_impl (READ_OPER_TAKE, reader_or_condition, buf, bufsz, maxs, si, mask, DDS_HANDLE_NIL, false);
}

dds_return_t dds_take_mask_wl (dds_entity_t reader_or_condition, void **buf, dds_sample_info_t *si, uint32_t maxs, uint32_t mask)
{
  return dds_take_mask (reader_or_condition, buf, si, maxs, maxs, mask);
}

dds_return_t dds_take_instance (dds_entity_t reader_or_condition, void **buf, dds_sample_info_t *si, size_t bufsz, uint32_t maxs, dds_instance_handle_t handle)
{
  if (handle == DDS_HANDLE_NIL)
    return DDS_RETCODE_PRECONDITION_NOT_MET;
  return dds_read_impl (READ_OPER_TAKE, reader_or_condition, buf, bufsz, maxs, si, 0, handle, false);
}

dds_return_t dds_take_instance_wl (dds_entity_t reader_or_condition, void **buf, dds_sample_info_t *si, uint32_t maxs, dds_instance_handle_t handle)
{
  return dds_take_instance (reader_or_condition, buf, si, maxs, maxs, handle);
}

dds_return_t dds_take_instance_mask (dds_entity_t reader_or_condition, void **buf, dds_sample_info_t *si, size_t bufsz, uint32_t maxs, dds_instance_handle_t handle, uint32_t mask)
{
  if (handle == DDS_HANDLE_NIL)
    return DDS_RETCODE_PRECONDITION_NOT_MET;
  return dds_read_impl (READ_OPER_TAKE, reader_or_condition, buf, bufsz, maxs, si, mask, handle, false);
}

dds_return_t dds_take_instance_mask_wl (dds_entity_t reader_or_condition, void **buf, dds_sample_info_t *si, uint32_t maxs, dds_instance_handle_t handle, uint32_t mask)
{
  return dds_take_instance_mask (reader_or_condition, buf, si, maxs, maxs, handle, mask);
}

dds_return_t dds_take_next (dds_entity_t reader, void **buf, dds_sample_info_t *si)
{
  uint32_t mask = DDS_NOT_READ_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
  return dds_read_impl (READ_OPER_TAKE, reader, buf, 1u, 1u, si, mask, DDS_HANDLE_NIL, true);
}

dds_return_t dds_take_next_wl (dds_entity_t reader, void **buf, dds_sample_info_t *si)
{
  return dds_take_next (reader, buf, si);
}

dds_return_t dds_peekcdr (dds_entity_t reader_or_condition, struct ddsi_serdata **buf, uint32_t maxs, dds_sample_info_t *si, uint32_t mask)
{
  return dds_readcdr_impl (READ_OPER_PEEK, reader_or_condition, buf, maxs, si, mask, DDS_HANDLE_NIL);
}

dds_return_t dds_peekcdr_instance (dds_entity_t reader_or_condition, struct ddsi_serdata **buf, uint32_t maxs, dds_sample_info_t *si, dds_instance_handle_t handle, uint32_t mask)
{
  if (handle == DDS_HANDLE_NIL)
    return DDS_RETCODE_PRECONDITION_NOT_MET;
  return dds_readcdr_impl (READ_OPER_PEEK, reader_or_condition, buf, maxs, si, mask, handle);
}

dds_return_t dds_readcdr (dds_entity_t reader_or_condition, struct ddsi_serdata **buf, uint32_t maxs, dds_sample_info_t *si, uint32_t mask)
{
  return dds_readcdr_impl (READ_OPER_READ, reader_or_condition, buf, maxs, si, mask, DDS_HANDLE_NIL);
}

dds_return_t dds_readcdr_instance (dds_entity_t reader_or_condition, struct ddsi_serdata **buf, uint32_t maxs, dds_sample_info_t *si, dds_instance_handle_t handle, uint32_t mask)
{
  if (handle == DDS_HANDLE_NIL)
    return DDS_RETCODE_PRECONDITION_NOT_MET;
  return dds_readcdr_impl (READ_OPER_READ, reader_or_condition, buf, maxs, si, mask, handle);
}

dds_return_t dds_takecdr (dds_entity_t reader_or_condition, struct ddsi_serdata **buf, uint32_t maxs, dds_sample_info_t *si, uint32_t mask)
{
  return dds_readcdr_impl (READ_OPER_TAKE, reader_or_condition, buf, maxs, si, mask, DDS_HANDLE_NIL);
}

dds_return_t dds_takecdr_instance (dds_entity_t reader_or_condition, struct ddsi_serdata **buf, uint32_t maxs, dds_sample_info_t *si, dds_instance_handle_t handle, uint32_t mask)
{
  if (handle == DDS_HANDLE_NIL)
    return DDS_RETCODE_PRECONDITION_NOT_MET;
  return dds_readcdr_impl (READ_OPER_TAKE, reader_or_condition, buf, maxs, si, mask, handle);
}

dds_return_t dds_peek_with_collector (dds_entity_t reader_or_condition, uint32_t maxs, dds_instance_handle_t handle, uint32_t mask, dds_read_with_collector_fn_t collect_sample, void *collect_sample_arg)
{
  return dds_read_with_collector_impl (READ_OPER_PEEK, reader_or_condition, maxs, mask, handle, false, collect_sample, collect_sample_arg);
}

dds_return_t dds_read_with_collector (dds_entity_t reader_or_condition, uint32_t maxs, dds_instance_handle_t handle, uint32_t mask, dds_read_with_collector_fn_t collect_sample, void *collect_sample_arg)
{
  return dds_read_with_collector_impl (READ_OPER_READ, reader_or_condition, maxs, mask, handle, false, collect_sample, collect_sample_arg);
}

dds_return_t dds_take_with_collector (dds_entity_t reader_or_condition, uint32_t maxs, dds_instance_handle_t handle, uint32_t mask, dds_read_with_collector_fn_t collect_sample, void *collect_sample_arg)
{
  return dds_read_with_collector_impl (READ_OPER_TAKE, reader_or_condition, maxs, mask, handle, false, collect_sample, collect_sample_arg);
}

static void return_reader_loan_locked_onesample (dds_reader *rd, dds_loaned_sample_t *loan, bool reset)
{
  if (loan->loan_origin.origin_kind != DDS_LOAN_ORIGIN_KIND_HEAP || ddsrt_atomic_ld32 (&loan->refc) != 1)
    dds_loaned_sample_unref (loan);
  else
  {
    // Free any memory allocated for sequences, strings, what-have-you: this is not required for
    // correctness because to_sample must be prepared to handle an arbitrary valid sample for its
    // destination, but it does make sense not to hold on to potentially large amounts of
    // application data in a cache.
    if (reset)
      dds_heap_loan_reset (loan);
    if (dds_loan_pool_add_loan (rd->m_heap_loan_cache, loan) != DDS_RETCODE_OK)
      dds_loaned_sample_unref (loan);
  }
}

static dds_return_t return_reader_loan_locked_loop (dds_reader *rd, void **buf, int32_t first, int32_t bufsz, bool reset)
  ddsrt_nonnull_all ddsrt_attribute_warn_unused_result;

static dds_return_t return_reader_loan_locked_loop (dds_reader *rd, void **buf, int32_t first, int32_t bufsz, bool reset)
{
  dds_return_t rc = DDS_RETCODE_OK;
  for (int32_t s = first; s < bufsz && buf[s] != NULL; s++)
  {
    dds_loaned_sample_t *loan;
    if ((loan = dds_loan_pool_find_and_remove_loan (rd->m_loans, buf[s])) == NULL)
    {
      // Not supposed to happen: either all memory is borrowed or none is, and this
      // means the application screwed up.  Continue so that afterward the only
      // non-null pointers are to memory.
      rc = DDS_RETCODE_BAD_PARAMETER;
    }
    else
    {
      return_reader_loan_locked_onesample (rd, loan, reset);
    }
  }
  return rc;
}

static dds_return_t return_reader_loan_locked (dds_reader *rd, void **buf, int32_t bufsz)
{
  dds_loaned_sample_t *loan;
  if ((loan = dds_loan_pool_find_and_remove_loan (rd->m_loans, buf[0])) == NULL)
  {
    // first entry is not a loan, thus: input assumed to consist of application-owned memory
    return DDS_RETCODE_OK;
  }
  else
  {
    buf[0] = NULL;
    return_reader_loan_locked_onesample (rd, loan, false);
    return return_reader_loan_locked_loop (rd, buf, 1, bufsz, false);
  }
}

dds_return_t dds_return_reader_loan (dds_reader *rd, void **buf, int32_t bufsz)
{
  if (bufsz <= 0)
  {
    // No data whatsoever, or an invocation following a failed read/take call.  Read/take
    // already take care of restoring the state prior to their invocation if they return
    // no data.  Return late so invalid handles can be detected.
    return DDS_RETCODE_OK;
  }
  else if (buf[0] == NULL)
  {
    // Nothing to do
    return DDS_RETCODE_OK;
  }

  dds_return_t ret;
  dds_loaned_sample_t *loan;
  ddsrt_mutex_lock (&rd->m_entity.m_mutex);
  if ((loan = dds_loan_pool_find_and_remove_loan (rd->m_loans, buf[0])) == NULL)
  {
    // First entry is not a loan, thus: input assumed to consist of application-owned memory.
    // That's not what this function is for.
    ret = DDS_RETCODE_PRECONDITION_NOT_MET;
  }
  else
  {
    buf[0] = NULL;
    return_reader_loan_locked_onesample (rd, loan, false);
    ret = return_reader_loan_locked_loop (rd, buf, 1, bufsz, false);
  }
  ddsrt_mutex_unlock (&rd->m_entity.m_mutex);
  return ret;
}
