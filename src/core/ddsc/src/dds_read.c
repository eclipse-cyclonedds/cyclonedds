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
#include "dds__tkmap.h"
#include "dds__rhc.h"
#include "dds__err.h"
#include "ddsi/q_thread.h"
#include "ddsi/q_ephash.h"
#include "ddsi/q_entity.h"
#include "dds__report.h"


static _Check_return_ dds__retcode_t
dds_read_lock(
        _In_ dds_entity_t hdl,
        _Out_ dds_reader   **reader,
        _Out_ dds_readcond **condition,
        _In_  bool only_reader)
{
    dds__retcode_t rc = hdl;
    assert(reader);
    assert(condition);
    *reader = NULL;
    *condition = NULL;

    rc = dds_entity_lock(hdl, DDS_KIND_READER, (dds_entity**)reader);
    if (rc == DDS_RETCODE_ILLEGAL_OPERATION) {
        if (!only_reader) {
            if ((dds_entity_kind(hdl) == DDS_KIND_COND_READ ) || (dds_entity_kind(hdl) == DDS_KIND_COND_QUERY) ){
                rc = dds_entity_lock(hdl, DDS_KIND_DONTCARE, (dds_entity**)condition);
                if (rc == DDS_RETCODE_OK) {
                    dds_entity *parent = ((dds_entity*)*condition)->m_parent;
                    assert(parent);
                    rc = dds_entity_lock(parent->m_hdl, DDS_KIND_READER, (dds_entity**)reader);
                    if (rc != DDS_RETCODE_OK) {
                        dds_entity_unlock((dds_entity*)*condition);
                        DDS_ERROR(rc, "Failed to lock condition reader.");
                    }
                } else {
                    DDS_ERROR(rc, "Failed to lock condition.");
                }
            } else {
                DDS_ERROR(rc, "Given entity is not a reader nor a condition.");
            }
        } else {
            DDS_ERROR(rc, "Given entity is not a reader.");
        }
    } else if (rc != DDS_RETCODE_OK) {
        DDS_ERROR(rc, "Failed to lock reader.");
    }
    return rc;
}

static void
dds_read_unlock(
        _In_ dds_reader   *reader,
        _In_ dds_readcond *condition)
{
    assert(reader);
    dds_entity_unlock((dds_entity*)reader);
    if (condition) {
        dds_entity_unlock((dds_entity*)condition);
    }
}
/*
  dds_read_impl: Core read/take function. Usually maxs is size of buf and si
  into which samples/status are written, when set to zero is special case
  indicating that size set from number of samples in cache and also that cache
  has been locked. This is used to support C++ API reading length unlimited
  which is interpreted as "all relevant samples in cache".
*/
static dds_return_t
dds_read_impl(
        _In_  bool take,
        _In_  dds_entity_t reader_or_condition,
        _Inout_ void **buf,
        _In_ size_t bufsz,
        _In_  uint32_t maxs,
        _Out_ dds_sample_info_t *si,
        _In_  uint32_t mask,
        _In_  dds_instance_handle_t hand,
        _In_  bool lock,
        _In_ bool only_reader)
{
    uint32_t i;
    dds_return_t ret = DDS_RETCODE_OK;
    dds__retcode_t rc;
    struct dds_reader * rd;
    struct dds_readcond * cond;
    struct thread_state1 * const thr = lookup_thread_state ();
    const bool asleep = !vtime_awake_p (thr->vtime);

    if (asleep) {
        thread_state_awake (thr);
    }
    if (buf == NULL) {
        ret = DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER, "The provided buffer is NULL");
        goto fail;
    }
    if (si == NULL) {
        ret = DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER, "Provided pointer to an array of dds_sample_info_t is NULL");
        goto fail;
    }
    if (maxs == 0) {
        ret = DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER, "The maximum number of samples to read is zero");
        goto fail;
    }
    if (bufsz == 0) {
        ret = DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER, "The size of buffer is zero");
        goto fail;
    }
    if (bufsz < maxs) {
        ret = DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER, "The provided size of buffer is smaller than the maximum number of samples to read");
        goto fail;
    }

    rc = dds_read_lock(reader_or_condition, &rd, &cond, only_reader);
    if (rc != DDS_RETCODE_OK) {
        ret = DDS_ERRNO(rc, "Error occurred on locking entity");
        goto fail;
    }
    if (hand != DDS_HANDLE_NIL) {
        if (dds_tkmap_find_by_id(gv.m_tkmap, hand) == NULL) {
            ret = DDS_ERRNO(DDS_RETCODE_PRECONDITION_NOT_MET, "Could not find instance");
            dds_read_unlock(rd, cond);
            goto fail;
        }
    }
    /* Allocate samples if not provided (assuming all or none provided) */
    if (buf[0] == NULL) {
        char * loan;
        const size_t sz = rd->m_topic->m_descriptor->m_size;
        const uint32_t loan_size = (uint32_t) (sz * maxs);
        /* Allocate, use or reallocate loan cached on reader */
        if (rd->m_loan_out) {
            loan = dds_alloc (loan_size);
        } else {
            if (rd->m_loan) {
                if (rd->m_loan_size < loan_size) {
                    rd->m_loan = dds_realloc_zero (rd->m_loan, loan_size);
                    rd->m_loan_size = loan_size;
                }
             } else {
                 rd->m_loan = dds_alloc (loan_size);
                 rd->m_loan_size = loan_size;
             }
                 loan = rd->m_loan;
                 rd->m_loan_out = true;
        }
        for (i = 0; i < maxs; i++) {
            buf[i] = loan;
            loan += sz;
        }
    }
    if (take) {
        ret = (dds_return_t)dds_rhc_take(rd->m_rd->rhc, lock, buf, si, maxs, mask, hand, cond);
    } else {
        ret = (dds_return_t)dds_rhc_read(rd->m_rd->rhc, lock, buf, si, maxs, mask, hand, cond);
    }
    /* read/take resets data available status */
    dds_entity_status_reset(rd, DDS_DATA_AVAILABLE_STATUS);
    /* reset DATA_ON_READERS status on subscriber after successful read/take */
    if (dds_entity_kind(((dds_entity*)rd)->m_parent->m_hdl) == DDS_KIND_SUBSCRIBER) {
        dds_entity_status_reset(((dds_entity*)rd)->m_parent, DDS_DATA_ON_READERS_STATUS);
    }
    dds_read_unlock(rd, cond);

fail:
    if (asleep) {
        thread_state_asleep (thr);
    }
    return ret;
}

static dds_return_t
dds_readcdr_impl(
        _In_  bool take,
        _In_  dds_entity_t reader_or_condition,
        _Out_ struct serdata ** buf,
        _In_  uint32_t maxs,
        _Out_ dds_sample_info_t * si,
        _In_  uint32_t mask,
        _In_  dds_instance_handle_t hand,
        _In_  bool lock)
{
  dds_return_t ret = DDS_RETCODE_OK;
  dds__retcode_t rc;
  struct dds_reader * rd;
  struct dds_readcond * cond;
  struct thread_state1 * const thr = lookup_thread_state ();
  const bool asleep = !vtime_awake_p (thr->vtime);

  assert (take);
  assert (buf);
  assert (si);
  assert (hand == DDS_HANDLE_NIL);
  assert (maxs > 0);

  if (asleep)
  {
    thread_state_awake (thr);
  }
  rc = dds_read_lock(reader_or_condition, &rd, &cond, false);
  if (rc >= DDS_RETCODE_OK) {
      ret = dds_rhc_takecdr
        (
         rd->m_rd->rhc, lock, buf, si, maxs,
         mask & DDS_ANY_SAMPLE_STATE,
         mask & DDS_ANY_VIEW_STATE,
         mask & DDS_ANY_INSTANCE_STATE,
         hand
         );

      /* read/take resets data available status */
      dds_entity_status_reset(rd, DDS_DATA_AVAILABLE_STATUS);

      /* reset DATA_ON_READERS status on subscriber after successful read/take */

      if (dds_entity_kind(((dds_entity*)rd)->m_parent->m_hdl) == DDS_KIND_SUBSCRIBER)
      {
        dds_entity_status_reset(((dds_entity*)rd)->m_parent, DDS_DATA_ON_READERS_STATUS);
      }
      dds_read_unlock(rd, cond);
  } else {
      ret = DDS_ERRNO(rc, "Error occurred on locking entity");
  }

  if (asleep)
  {
    thread_state_asleep (thr);
  }

  return ret;
}

_Pre_satisfies_(((rd_or_cnd & DDS_ENTITY_KIND_MASK) == DDS_KIND_READER ) ||\
                ((rd_or_cnd & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_READ ) || \
                ((rd_or_cnd & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_QUERY ))
dds_return_t
dds_read(
        _In_ dds_entity_t rd_or_cnd,
        _Inout_ void ** buf,
        _Out_ dds_sample_info_t * si,
        _In_ size_t bufsz,
        _In_ uint32_t maxs)
{
    bool lock = true;
    dds_return_t ret;

    DDS_REPORT_STACK();
    if (maxs == DDS_READ_WITHOUT_LOCK) {
        lock = false;
        /* Use a more sensible maxs, so use bufsz instead.
         * CHAM-306 will remove this ugly piece of code. */
        maxs = (uint32_t)bufsz;
    }
    ret = dds_read_impl (false, rd_or_cnd, buf, bufsz, maxs, si, NO_STATE_MASK_SET, DDS_HANDLE_NIL, lock, false);
    DDS_REPORT_FLUSH(ret < 0);
    return ret;
}

_Pre_satisfies_(((rd_or_cnd & DDS_ENTITY_KIND_MASK) == DDS_KIND_READER ) ||\
                ((rd_or_cnd & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_READ ) || \
                ((rd_or_cnd & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_QUERY ))
dds_return_t
dds_read_wl(
        _In_ dds_entity_t rd_or_cnd,
        _Inout_ void ** buf,
        _Out_ dds_sample_info_t * si,
        _In_ uint32_t maxs)
{
    bool lock = true;
    dds_return_t ret;

    DDS_REPORT_STACK();

    if (maxs == DDS_READ_WITHOUT_LOCK) {
        lock = false;
        /* Use a more sensible maxs. Just an arbitrarily number.
         * CHAM-306 will remove this ugly piece of code. */
        maxs = 100;
    }
    ret =  dds_read_impl (false, rd_or_cnd, buf, maxs, maxs, si, NO_STATE_MASK_SET, DDS_HANDLE_NIL, lock, false);
    DDS_REPORT_FLUSH(ret < 0);
    return ret;
}

_Pre_satisfies_(((rd_or_cnd & DDS_ENTITY_KIND_MASK) == DDS_KIND_READER ) ||\
                ((rd_or_cnd & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_READ ) || \
                ((rd_or_cnd & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_QUERY ))
dds_return_t
dds_read_mask(
        _In_ dds_entity_t rd_or_cnd,
        _Inout_ void ** buf,
        _Out_ dds_sample_info_t * si,
        _In_ size_t bufsz,
        _In_ uint32_t maxs,
        _In_ uint32_t mask)
{
    bool lock = true;
    dds_return_t ret;

    DDS_REPORT_STACK();

    if (maxs == DDS_READ_WITHOUT_LOCK) {
        lock = false;
        /* Use a more sensible maxs, so use bufsz instead.
         * CHAM-306 will remove this ugly piece of code. */
        maxs = (uint32_t)bufsz;
    }
    ret = dds_read_impl (false, rd_or_cnd, buf, bufsz, maxs, si, mask, DDS_HANDLE_NIL, lock, false);
    DDS_REPORT_FLUSH(ret < 0);
    return ret;
}

_Pre_satisfies_(((rd_or_cnd & DDS_ENTITY_KIND_MASK) == DDS_KIND_READER ) ||\
                ((rd_or_cnd & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_READ ) || \
                ((rd_or_cnd & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_QUERY ))
dds_return_t
dds_read_mask_wl(
        _In_ dds_entity_t rd_or_cnd,
        _Inout_ void ** buf,
        _Out_ dds_sample_info_t * si,
        _In_ uint32_t maxs,
        _In_ uint32_t mask)
{
    bool lock = true;
    dds_return_t ret;

    DDS_REPORT_STACK();

    if (maxs == DDS_READ_WITHOUT_LOCK) {
        lock = false;
        /* Use a more sensible maxs. Just an arbitrarily number.
         * CHAM-306 will remove this ugly piece of code. */
        maxs = 100;
    }
    ret = dds_read_impl (false, rd_or_cnd, buf, maxs, maxs, si, mask, DDS_HANDLE_NIL, lock, false);
    DDS_REPORT_FLUSH(ret < 0 );
    return ret;
}

_Pre_satisfies_(((rd_or_cnd & DDS_ENTITY_KIND_MASK) == DDS_KIND_READER ) ||\
                ((rd_or_cnd & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_READ ) || \
                ((rd_or_cnd & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_QUERY ))
dds_return_t
dds_read_instance(
        _In_ dds_entity_t rd_or_cnd,
        _Inout_ void **buf,
        _Out_ dds_sample_info_t *si,
        _In_ size_t bufsz,
        _In_ uint32_t maxs,
        _In_ dds_instance_handle_t handle)
{
    dds_return_t ret = DDS_RETCODE_OK;
    bool lock = true;
    DDS_REPORT_STACK();

    if (handle == DDS_HANDLE_NIL) {
        ret = DDS_ERRNO(DDS_RETCODE_PRECONDITION_NOT_MET, "DDS_HANDLE_NIL was provided");
        goto fail;
    }

    if (maxs == DDS_READ_WITHOUT_LOCK) {
        lock = false;
        /* Use a more sensible maxs. Just an arbitrarily number.
         * CHAM-306 will remove this ugly piece of code. */
        maxs = 100;
    }
    ret = dds_read_impl(false, rd_or_cnd, buf, bufsz, maxs, si, NO_STATE_MASK_SET, handle, lock, false);
fail:
    DDS_REPORT_FLUSH(ret < 0 );
    return ret;
}

_Pre_satisfies_(((rd_or_cnd & DDS_ENTITY_KIND_MASK) == DDS_KIND_READER ) ||\
                ((rd_or_cnd & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_READ ) || \
                ((rd_or_cnd & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_QUERY ))
dds_return_t
dds_read_instance_wl(
        _In_ dds_entity_t rd_or_cnd,
        _Inout_ void **buf,
        _Out_ dds_sample_info_t *si,
        _In_ uint32_t maxs,
        _In_ dds_instance_handle_t handle)
{
    dds_return_t ret = DDS_RETCODE_OK;
    bool lock = true;
    DDS_REPORT_STACK();

    if (handle == DDS_HANDLE_NIL) {
        ret = DDS_ERRNO(DDS_RETCODE_PRECONDITION_NOT_MET, "DDS_HANDLE_NIL was provided");
        goto fail;
    }

    if (maxs == DDS_READ_WITHOUT_LOCK) {
        lock = false;
        /* Use a more sensible maxs. Just an arbitrarily number.
         * CHAM-306 will remove this ugly piece of code. */
        maxs = 100;
    }
    ret = dds_read_impl(false, rd_or_cnd, buf, maxs, maxs, si, NO_STATE_MASK_SET, handle, lock, false);
fail:
    DDS_REPORT_FLUSH(ret < 0);
    return ret;
}


_Pre_satisfies_(((rd_or_cnd & DDS_ENTITY_KIND_MASK) == DDS_KIND_READER ) ||\
                ((rd_or_cnd & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_READ ) || \
                ((rd_or_cnd & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_QUERY ))
dds_return_t
dds_read_instance_mask(
        _In_ dds_entity_t rd_or_cnd,
        _Inout_ void **buf,
        _Out_ dds_sample_info_t *si,
        _In_ size_t bufsz,
        _In_ uint32_t maxs,
        _In_ dds_instance_handle_t handle,
        _In_ uint32_t mask)
{
    dds_return_t ret = DDS_RETCODE_OK;
    bool lock = true;
    DDS_REPORT_STACK();

    if (handle == DDS_HANDLE_NIL) {
        ret = DDS_ERRNO(DDS_RETCODE_PRECONDITION_NOT_MET, "DDS_HANDLE_NIL was provided");
        goto fail;
    }

    if (maxs == DDS_READ_WITHOUT_LOCK) {
        lock = false;
        /* Use a more sensible maxs. Just an arbitrarily number.
         * CHAM-306 will remove this ugly piece of code. */
        maxs = 100;
    }
    ret = dds_read_impl(false, rd_or_cnd, buf, bufsz, maxs, si, mask, handle, lock, false);
fail:
    DDS_REPORT_FLUSH(ret < 0);
    return ret;
}


_Pre_satisfies_(((rd_or_cnd & DDS_ENTITY_KIND_MASK) == DDS_KIND_READER ) ||\
                ((rd_or_cnd & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_READ ) || \
                ((rd_or_cnd & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_QUERY ))
dds_return_t
dds_read_instance_mask_wl(
        _In_ dds_entity_t rd_or_cnd,
        _Inout_ void **buf,
        _Out_ dds_sample_info_t *si,
        _In_ uint32_t maxs,
        _In_ dds_instance_handle_t handle,
        _In_ uint32_t mask)
{
    dds_return_t ret = DDS_RETCODE_OK;
    bool lock = true;
    DDS_REPORT_STACK();

    if (handle == DDS_HANDLE_NIL) {
        ret = DDS_ERRNO(DDS_RETCODE_PRECONDITION_NOT_MET, "DDS_HANDLE_NIL was provided");
        goto fail;
    }
    if (maxs == DDS_READ_WITHOUT_LOCK) {
        lock = false;
        /* Use a more sensible maxs. Just an arbitrarily number.
         * CHAM-306 will remove this ugly piece of code. */
        maxs = 100;
    }
    ret = dds_read_impl(false, rd_or_cnd, buf, maxs, maxs, si, mask, handle, lock, false);
fail:
    DDS_REPORT_FLUSH(ret < 0);
    return ret;
}

_Pre_satisfies_((reader & DDS_ENTITY_KIND_MASK) == DDS_KIND_READER )
dds_return_t
dds_read_next(
        _In_ dds_entity_t reader,
        _Inout_ void **buf,
        _Out_ dds_sample_info_t *si)
{
    uint32_t mask = DDS_NOT_READ_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t ret;

    DDS_REPORT_STACK();

    ret = dds_read_impl (false, reader, buf, 1u, 1u, si, mask, DDS_HANDLE_NIL, true, true);
    DDS_REPORT_FLUSH(ret != DDS_RETCODE_OK);
    return ret;
}

_Pre_satisfies_((reader & DDS_ENTITY_KIND_MASK) == DDS_KIND_READER )
dds_return_t
dds_read_next_wl(
        _In_ dds_entity_t reader,
        _Inout_ void **buf,
        _Out_ dds_sample_info_t *si)
{
    uint32_t mask = DDS_NOT_READ_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t ret;

    DDS_REPORT_STACK();
    ret = dds_read_impl (false, reader, buf, 1u, 1u, si, mask, DDS_HANDLE_NIL, true, true);
    DDS_REPORT_FLUSH(ret != DDS_RETCODE_OK);
    return ret;
}

_Pre_satisfies_(((rd_or_cnd & DDS_ENTITY_KIND_MASK) == DDS_KIND_READER ) ||\
                ((rd_or_cnd & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_READ ) || \
                ((rd_or_cnd & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_QUERY ))
dds_return_t
dds_take(
        _In_ dds_entity_t rd_or_cnd,
        _Inout_ void ** buf,
        _Out_ dds_sample_info_t * si,
        _In_ size_t bufsz,
        _In_ uint32_t maxs)
{
    bool lock = true;
    dds_return_t ret;

    DDS_REPORT_STACK();

    if (maxs == DDS_READ_WITHOUT_LOCK) {
        lock = false;
        /* Use a more sensible maxs, so use bufsz instead.
         * CHAM-306 will remove this ugly piece of code. */
        maxs = (uint32_t)bufsz;
    }
    ret = dds_read_impl (true, rd_or_cnd, buf, bufsz, maxs, si, NO_STATE_MASK_SET, DDS_HANDLE_NIL, lock, false);
    DDS_REPORT_FLUSH(ret < 0);
    return ret;
}

_Pre_satisfies_(((rd_or_cnd & DDS_ENTITY_KIND_MASK) == DDS_KIND_READER ) ||\
                ((rd_or_cnd & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_READ ) || \
                ((rd_or_cnd & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_QUERY ))
dds_return_t
dds_take_wl(
        _In_ dds_entity_t rd_or_cnd,
        _Inout_ void ** buf,
        _Out_ dds_sample_info_t * si,
        _In_ uint32_t maxs)
{
    bool lock = true;
    dds_return_t ret;

    DDS_REPORT_STACK();

    if (maxs == DDS_READ_WITHOUT_LOCK) {
        lock = false;
        /* Use a more sensible maxs. Just an arbitrarily number.
         * CHAM-306 will remove this ugly piece of code. */
        maxs = 100;
    }
    ret = dds_read_impl (true, rd_or_cnd, buf, maxs, maxs, si, NO_STATE_MASK_SET, DDS_HANDLE_NIL, lock, false);
    DDS_REPORT_FLUSH(ret < 0);
    return ret;
}

_Pre_satisfies_(((rd_or_cnd & DDS_ENTITY_KIND_MASK) == DDS_KIND_READER ) ||\
                ((rd_or_cnd & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_READ ) || \
                ((rd_or_cnd & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_QUERY ))
dds_return_t
dds_take_mask(
        _In_ dds_entity_t rd_or_cnd,
        _Inout_ void ** buf,
        _Out_ dds_sample_info_t * si,
        _In_ size_t bufsz,
        _In_ uint32_t maxs,
        _In_ uint32_t mask)
{
    bool lock = true;
    dds_return_t ret;

    DDS_REPORT_STACK();

    if (maxs == DDS_READ_WITHOUT_LOCK) {
        lock = false;
        /* Use a more sensible maxs, so use bufsz instead.
         * CHAM-306 will remove this ugly piece of code. */
        maxs = (uint32_t)bufsz;
    }
    ret = dds_read_impl (true, rd_or_cnd, buf, bufsz, maxs, si, mask, DDS_HANDLE_NIL, lock, false);
    DDS_REPORT_FLUSH(ret < 0);
    return ret;
}

_Pre_satisfies_(((rd_or_cnd & DDS_ENTITY_KIND_MASK) == DDS_KIND_READER ) ||\
                ((rd_or_cnd & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_READ ) || \
                ((rd_or_cnd & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_QUERY ))
dds_return_t
dds_take_mask_wl(
        _In_ dds_entity_t rd_or_cnd,
        _Inout_ void ** buf,
        _Out_ dds_sample_info_t * si,
        _In_ uint32_t maxs,
        _In_ uint32_t mask)
{
    bool lock = true;
    dds_return_t ret;

    DDS_REPORT_STACK();

    if (maxs == DDS_READ_WITHOUT_LOCK) {
        lock = false;
        /* Use a more sensible maxs. Just an arbitrarily number.
         * CHAM-306 will remove this ugly piece of code. */
        maxs = 100;
    }
    ret = dds_read_impl (true, rd_or_cnd, buf, maxs, maxs, si, mask, DDS_HANDLE_NIL, lock, false);
    DDS_REPORT_FLUSH(ret < 0);
    return ret;
}

int
dds_takecdr(
        dds_entity_t rd_or_cnd,
        struct serdata **buf,
        uint32_t maxs,
        dds_sample_info_t *si,
        uint32_t mask)
{
    bool lock = true;
    if (maxs == DDS_READ_WITHOUT_LOCK) {
        lock = false;
        /* Use a more sensible maxs. Just an arbitrarily number.
         * CHAM-306 will remove this ugly piece of code. */
        maxs = 100;
    }
    return dds_readcdr_impl (true, rd_or_cnd, buf, maxs, si, mask, DDS_HANDLE_NIL, lock);
}


_Pre_satisfies_(((rd_or_cnd & DDS_ENTITY_KIND_MASK) == DDS_KIND_READER ) ||\
                ((rd_or_cnd & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_READ ) || \
                ((rd_or_cnd & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_QUERY ))
dds_return_t
dds_take_instance(
        _In_ dds_entity_t rd_or_cnd,
        _Inout_ void **buf,
        _Out_ dds_sample_info_t *si,
        _In_ size_t bufsz,
        _In_ uint32_t maxs,
        _In_ dds_instance_handle_t handle)
{
    dds_return_t ret = DDS_RETCODE_OK;
    bool lock = true;

    DDS_REPORT_STACK();

    if (handle == DDS_HANDLE_NIL) {
        ret = DDS_ERRNO(DDS_RETCODE_PRECONDITION_NOT_MET, "DDS_HANDLE_NIL was provided");
        goto fail;
    }

    if (maxs == DDS_READ_WITHOUT_LOCK) {
        lock = false;
        /* Use a more sensible maxs. Just an arbitrarily number.
         * CHAM-306 will remove this ugly piece of code. */
        maxs = 100;
    }
    ret = dds_read_impl(true, rd_or_cnd, buf, bufsz, maxs, si, NO_STATE_MASK_SET, handle, lock, false);
fail:
    DDS_REPORT_FLUSH(ret < 0);
    return ret;
}

_Pre_satisfies_(((rd_or_cnd & DDS_ENTITY_KIND_MASK) == DDS_KIND_READER ) ||\
                ((rd_or_cnd & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_READ ) || \
                ((rd_or_cnd & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_QUERY ))
dds_return_t
dds_take_instance_wl(
        _In_ dds_entity_t rd_or_cnd,
        _Inout_ void **buf,
        _Out_ dds_sample_info_t *si,
        _In_ uint32_t maxs,
        _In_ dds_instance_handle_t handle)
{
    dds_return_t ret = DDS_RETCODE_OK;
    bool lock = true;

    DDS_REPORT_STACK();

    if (handle == DDS_HANDLE_NIL) {
        ret = DDS_ERRNO(DDS_RETCODE_PRECONDITION_NOT_MET, "DDS_HANDLE_NIL was provided");
        goto fail;
    }
    if (maxs == DDS_READ_WITHOUT_LOCK) {
        lock = false;
        /* Use a more sensible maxs. Just an arbitrarily number.
         * CHAM-306 will remove this ugly piece of code. */
        maxs = 100;
    }
    ret = dds_read_impl(true, rd_or_cnd, buf, maxs, maxs, si, NO_STATE_MASK_SET, handle, lock, false);
fail:
    DDS_REPORT_FLUSH(ret < 0);
    return ret;
}


_Pre_satisfies_(((rd_or_cnd & DDS_ENTITY_KIND_MASK) == DDS_KIND_READER ) ||\
                ((rd_or_cnd & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_READ ) || \
                ((rd_or_cnd & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_QUERY ))
dds_return_t
dds_take_instance_mask(
        _In_ dds_entity_t rd_or_cnd,
        _Inout_ void **buf,
        _Out_ dds_sample_info_t *si,
        _In_ size_t bufsz,
        _In_ uint32_t maxs,
        _In_ dds_instance_handle_t handle,
        _In_ uint32_t mask)
{
    dds_return_t ret = DDS_RETCODE_OK;
    bool lock = true;

    DDS_REPORT_STACK();

    if (handle == DDS_HANDLE_NIL) {
        ret = DDS_ERRNO(DDS_RETCODE_PRECONDITION_NOT_MET, "DDS_HANDLE_NIL was provided");
        goto fail;
    }
    if (maxs == DDS_READ_WITHOUT_LOCK) {
        lock = false;
        /* Use a more sensible maxs. Just an arbitrarily number.
         * CHAM-306 will remove this ugly piece of code. */
        maxs = 100;
    }
    ret = dds_read_impl(true, rd_or_cnd, buf, bufsz, maxs, si, mask, handle, lock, false);
fail:
    DDS_REPORT_FLUSH(ret < 0);
    return ret;
}


_Pre_satisfies_(((rd_or_cnd & DDS_ENTITY_KIND_MASK) == DDS_KIND_READER ) ||\
                ((rd_or_cnd & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_READ ) || \
                ((rd_or_cnd & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_QUERY ))
dds_return_t
dds_take_instance_mask_wl(
        _In_ dds_entity_t rd_or_cnd,
        _Inout_ void **buf,
        _Out_ dds_sample_info_t *si,
        _In_ uint32_t maxs,
        _In_ dds_instance_handle_t handle,
        _In_ uint32_t mask)
{
    dds_return_t ret = DDS_RETCODE_OK;
    bool lock = true;

    DDS_REPORT_STACK();

    if (handle == DDS_HANDLE_NIL) {
        ret = DDS_ERRNO(DDS_RETCODE_PRECONDITION_NOT_MET, "DDS_HANDLE_NIL was provided");
        goto fail;
    }
    if (maxs == DDS_READ_WITHOUT_LOCK) {
        lock = false;
        /* Use a more sensible maxs. Just an arbitrarily number.
         * CHAM-306 will remove this ugly piece of code. */
        maxs = 100;
    }
    ret = dds_read_impl(true, rd_or_cnd, buf, maxs, maxs, si, mask, handle, lock, false);
fail:
    DDS_REPORT_FLUSH(ret < 0);
    return ret;
}

_Pre_satisfies_((reader & DDS_ENTITY_KIND_MASK) == DDS_KIND_READER )
dds_return_t
dds_take_next(
        _In_ dds_entity_t reader,
        _Inout_ void **buf,
        _Out_ dds_sample_info_t *si)
{
    uint32_t mask = DDS_NOT_READ_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t ret;
    DDS_REPORT_STACK();
    ret = dds_read_impl (true, reader, buf, 1u, 1u, si, mask, DDS_HANDLE_NIL, true, true);
    DDS_REPORT_FLUSH(ret != DDS_RETCODE_OK);
    return ret;
}

_Pre_satisfies_((reader & DDS_ENTITY_KIND_MASK) == DDS_KIND_READER )
dds_return_t
dds_take_next_wl(
        _In_ dds_entity_t reader,
        _Inout_ void **buf,
        _Out_ dds_sample_info_t *si)
{
    uint32_t mask = DDS_NOT_READ_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t ret;
    DDS_REPORT_STACK();
    ret = dds_read_impl (true, reader, buf, 1u, 1u, si, mask, DDS_HANDLE_NIL, true, true);
    DDS_REPORT_FLUSH(ret != DDS_RETCODE_OK);
    return ret;
}

_Pre_satisfies_(((reader_or_condition & DDS_ENTITY_KIND_MASK) == DDS_KIND_READER ) ||\
                ((reader_or_condition & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_READ ) || \
                ((reader_or_condition & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_QUERY ))
_Must_inspect_result_ dds_return_t
dds_return_loan(
        _In_ dds_entity_t reader_or_condition,
        _Inout_updates_(bufsz) void **buf,
        _In_ size_t bufsz)
{
    dds__retcode_t rc;
    const dds_topic_descriptor_t * desc;
    dds_reader *rd;
    dds_readcond *cond;
    dds_return_t ret = DDS_RETCODE_OK;

    DDS_REPORT_STACK();

    if (!buf ) {
        ret = DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER, "Argument buf is NULL");
        goto fail;
    }
    if(*buf == NULL && bufsz > 0){
        ret = DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER, "Argument buf is NULL");
        goto fail;
    }

    rc = dds_read_lock(reader_or_condition, &rd, &cond, false);
    if (rc != DDS_RETCODE_OK) {
        ret = DDS_ERRNO(rc, "Error occurred on locking entity");
        goto fail;
    }
    desc = rd->m_topic->m_descriptor;

    /* Only free sample contents if they have been allocated */
    if (desc->m_flagset & DDS_TOPIC_NO_OPTIMIZE) {
        size_t i = 0;
        for (i = 0; i < bufsz; i++) {
            dds_sample_free(buf[i], desc, DDS_FREE_CONTENTS);
        }
    }

    /* If possible return loan buffer to reader */
    if (rd->m_loan != 0 && (buf[0] == rd->m_loan)) {
        rd->m_loan_out = false;
        memset (rd->m_loan, 0, rd->m_loan_size);
        buf[0] = NULL;
    }

    dds_read_unlock(rd, cond);
fail:
    DDS_REPORT_FLUSH(ret != DDS_RETCODE_OK);
    return ret;
}
