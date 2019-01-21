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
#include "os/os.h"
#include "dds__entity.h"
#include "dds__querycond.h"
#include "dds__readcond.h"
#include "dds__rhc.h"
#include "dds__err.h"

DEFINE_ENTITY_LOCK_UNLOCK(static, dds_waitset, DDS_KIND_WAITSET)

static void
dds_waitset_swap(
        _Inout_   dds_attachment **dst,
        _In_      dds_attachment **src,
        _In_opt_  dds_attachment  *prev,
        _In_      dds_attachment  *idx)
{
    /* Remove from source. */
    if (prev == NULL) {
        *src = idx->next;
    } else {
        prev->next = idx->next;
    }

    /* Add to destination. */
    idx->next = *dst;
    *dst = idx;
}

static dds_return_t
dds_waitset_wait_impl(
        _In_ dds_entity_t waitset,
        _Out_writes_to_opt_(nxs, return < 0 ? 0 : return) dds_attach_t *xs,
        _In_ size_t nxs,
        _In_ dds_time_t abstimeout,
        _In_ dds_time_t tnow)
{
    dds_waitset *ws;
    dds_return_t ret;
    dds__retcode_t rc;
    dds_attachment *idx;
    dds_attachment *next;
    dds_attachment *prev;

    if ((xs == NULL) && (nxs != 0)){
        DDS_ERROR("A size was given, but no array\n");
        return DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER);
    }
    if ((xs != NULL) && (nxs == 0)){
        DDS_ERROR("Array is given with an invalid size\n");
        return DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER);
    }

    /* Locking the waitset here will delay a possible deletion until it is
     * unlocked. Even when the related mutex is unlocked by a conditioned wait. */
    rc = dds_waitset_lock(waitset, &ws);
    if (rc == DDS_RETCODE_OK) {
        /* Check if any of any previous triggered entities has changed there status
         * and thus it trigger value could be false now. */
        idx = ws->triggered;
        prev = NULL;
        while (idx != NULL) {
            next = idx->next;
            if (idx->entity->m_trigger == 0) {
                /* Move observed entity to triggered list. */
                dds_waitset_swap(&(ws->observed), &(ws->triggered), prev, idx);
            } else {
                prev = idx;
            }
            idx = next;
        }
        /* Check if any of the entities have been triggered. */
        idx = ws->observed;
        prev = NULL;
        while (idx != NULL) {
            next = idx->next;
            if (idx->entity->m_trigger > 0) {
                /* Move observed entity to triggered list. */
                dds_waitset_swap(&(ws->triggered), &(ws->observed), prev, idx);
            } else {
                prev = idx;
            }
            idx = next;
        }

        /* Only wait/keep waiting when whe have something to observer and there aren't any triggers yet. */
        rc = DDS_RETCODE_OK;
        while ((ws->observed != NULL) && (ws->triggered == NULL) && (rc == DDS_RETCODE_OK)) {
            if (abstimeout == DDS_NEVER) {
                os_condWait(&ws->m_entity.m_cond, &ws->m_entity.m_mutex);
            } else if (abstimeout <= tnow) {
                rc = DDS_RETCODE_TIMEOUT;
            } else {
                dds_duration_t dt = abstimeout - tnow;
                os_time to;
                if ((dt / (dds_duration_t)DDS_NSECS_IN_SEC) >= (dds_duration_t)OS_TIME_INFINITE_SEC) {
                    to.tv_sec = OS_TIME_INFINITE_SEC;
                    to.tv_nsec = DDS_NSECS_IN_SEC - 1;
                } else {
                    to.tv_sec = (os_timeSec) (dt / DDS_NSECS_IN_SEC);
                    to.tv_nsec = (int32_t) (dt % DDS_NSECS_IN_SEC);
                }
                (void)os_condTimedWait(&ws->m_entity.m_cond, &ws->m_entity.m_mutex, &to);
                tnow = dds_time();
            }
        }

        /* Get number of triggered entities
         *   - set attach array when needed
         *   - swap them back to observed */
        if (rc == DDS_RETCODE_OK) {
            ret = 0;
            idx = ws->triggered;
            while (idx != NULL) {
                if ((uint32_t)ret < (uint32_t)nxs) {
                    xs[ret] = idx->arg;
                }
                ret++;

                next = idx->next;
                /* The idx is always the first in triggered, so no prev. */
                dds_waitset_swap(&(ws->observed), &(ws->triggered), NULL, idx);
                idx = next;
            }
        } else if (rc == DDS_RETCODE_TIMEOUT) {
            ret = 0;
        } else {
            DDS_ERROR("Internal error");
            ret = DDS_ERRNO(rc);
        }

        dds_waitset_unlock(ws);
    } else {
        DDS_ERROR("Error occurred on locking waitset\n");
        ret = DDS_ERRNO(rc);
    }

    return ret;
}

static void
dds_waitset_close_list(
        _In_ dds_attachment **list,
        _In_ dds_entity_t waitset)
{
    dds_attachment *idx = *list;
    dds_attachment *next;
    while (idx != NULL) {
        next = idx->next;
        (void)dds_entity_observer_unregister(idx->entity->m_hdl, waitset);
        os_free(idx);
        idx = next;
    }
    *list = NULL;
}

static bool
dds_waitset_remove_from_list(
        _In_ dds_attachment **list,
        _In_ dds_entity_t observed)
{
    dds_attachment *idx = *list;
    dds_attachment *prev = NULL;

    while (idx != NULL) {
        if (idx->entity->m_hdl == observed) {
            if (prev == NULL) {
                *list = idx->next;
            } else {
                prev->next = idx->next;
            }
            os_free(idx);

            /* We're done. */
            return true;
        }
        prev = idx;
        idx = idx->next;
    }
    return false;
}

dds_return_t
dds_waitset_close(
        struct dds_entity *e)
{
    dds_waitset *ws = (dds_waitset*)e;

    dds_waitset_close_list(&ws->observed,  e->m_hdl);
    dds_waitset_close_list(&ws->triggered, e->m_hdl);

    /* Trigger waitset to wake up. */
    os_condBroadcast(&e->m_cond);

    return DDS_RETCODE_OK;
}

_Pre_satisfies_((participant & DDS_ENTITY_KIND_MASK) == DDS_KIND_PARTICIPANT)
DDS_EXPORT _Must_inspect_result_ dds_entity_t
dds_create_waitset(
        _In_ dds_entity_t participant)
{
    dds_entity_t hdl;
    dds_entity *par;
    dds__retcode_t rc;

    rc = dds_entity_lock(participant, DDS_KIND_PARTICIPANT, &par);
    if (rc == DDS_RETCODE_OK) {
        dds_waitset *waitset = dds_alloc(sizeof(*waitset));
        hdl = dds_entity_init(&waitset->m_entity, par, DDS_KIND_WAITSET, NULL, NULL, 0);
        waitset->m_entity.m_deriver.close = dds_waitset_close;
        waitset->observed = NULL;
        waitset->triggered = NULL;
        dds_entity_unlock(par);
    } else {
        DDS_ERROR("Error occurred on locking entity\n");
        hdl = DDS_ERRNO(rc);
    }

    return hdl;
}


_Pre_satisfies_((waitset & DDS_ENTITY_KIND_MASK) == DDS_KIND_WAITSET)
DDS_EXPORT dds_return_t
dds_waitset_get_entities(
        _In_ dds_entity_t waitset,
        _Out_writes_to_(size, return < 0 ? 0 : return) dds_entity_t *entities,
        _In_ size_t size)
{
    dds_return_t ret = 0;
    dds__retcode_t rc;
    dds_waitset *ws;

    rc = dds_waitset_lock(waitset, &ws);
    if (rc == DDS_RETCODE_OK) {
        dds_attachment* iter;

        iter = ws->observed;
        while (iter) {
            if (((size_t)ret < size) && (entities != NULL)) {
                entities[ret] = iter->entity->m_hdl;
            }
            ret++;
            iter = iter->next;
        }

        iter = ws->triggered;
        while (iter) {
            if (((size_t)ret < size) && (entities != NULL)) {
                entities[ret] = iter->entity->m_hdl;
            }
            ret++;
            iter = iter->next;
        }
        dds_waitset_unlock(ws);
    } else {
        DDS_ERROR("Error occurred on locking waitset\n");
        ret = DDS_ERRNO(rc);
    }

    return ret;
}


static void
dds_waitset_move(
        _In_    dds_attachment **src,
        _Inout_ dds_attachment **dst,
        _In_    dds_entity_t entity)
{
    dds_attachment *idx = *src;
    dds_attachment *prev = NULL;
    while (idx != NULL) {
        if (idx->entity->m_hdl == entity) {
            /* Swap idx from src to dst. */
            dds_waitset_swap(dst, src, prev, idx);

            /* We're done. */
            return;
        }
        prev = idx;
        idx = idx->next;
    }
}

static void
dds_waitset_remove(
        dds_waitset *ws,
        dds_entity_t observed)
{
    if (!dds_waitset_remove_from_list(&(ws->observed), observed)) {
        (void)dds_waitset_remove_from_list(&(ws->triggered), observed);
    }
}

/* This is called when the observed entity signals a status change. */
void
dds_waitset_observer(
        dds_entity_t observer,
        dds_entity_t observed,
        uint32_t status)
{
    dds_waitset *ws;
    if (dds_waitset_lock(observer, &ws) == DDS_RETCODE_OK) {
        if (status & DDS_DELETING_STATUS) {
            /* Remove this observed entity, which is being deleted, from the waitset. */
            dds_waitset_remove(ws, observed);
            /* Our registration to this observed entity will be removed automatically. */
        } else if (status != 0) {
            /* Move observed entity to triggered list. */
            dds_waitset_move(&(ws->observed), &(ws->triggered), observed);
        } else {
            /* Remove observed entity from triggered list (which it possibly resides in). */
            dds_waitset_move(&(ws->triggered), &(ws->observed), observed);
        }
        /* Trigger waitset to wake up. */
        os_condBroadcast(&ws->m_entity.m_cond);
        dds_waitset_unlock(ws);
    }
}

_Pre_satisfies_((waitset & DDS_ENTITY_KIND_MASK) == DDS_KIND_WAITSET)
DDS_EXPORT dds_return_t
dds_waitset_attach(
        _In_ dds_entity_t waitset,
        _In_ dds_entity_t entity,
        _In_ dds_attach_t x)
{
    dds_entity  *e = NULL;
    dds_waitset *ws;
    dds__retcode_t rc;
    dds_return_t ret;

    rc = dds_waitset_lock(waitset, &ws);
    if (rc == DDS_RETCODE_OK) {
        if (waitset != entity) {
            rc = dds_entity_lock(entity, DDS_KIND_DONTCARE, &e);
            if (rc != DDS_RETCODE_OK) {
                e = NULL;
            }
        } else {
            e = &ws->m_entity;
        }

        /* This will fail if given entity is already attached (or deleted). */
        if (rc == DDS_RETCODE_OK) {
            rc = dds_entity_observer_register_nl(e, waitset, dds_waitset_observer);
        }

        if (rc == DDS_RETCODE_OK) {
            dds_attachment *a = os_malloc(sizeof(dds_attachment));
            a->arg = x;
            a->entity = e;
            if (e->m_trigger > 0) {
                a->next = ws->triggered;
                ws->triggered = a;
            } else {
                a->next = ws->observed;
                ws->observed = a;
            }
            ret = DDS_RETCODE_OK;
        } else if (rc != DDS_RETCODE_PRECONDITION_NOT_MET) {
            DDS_ERROR("Entity is not valid\n");
            ret = DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER);
        } else {
            DDS_ERROR("Entity is already attached\n");
            ret = DDS_ERRNO(rc);
        }
        if ((e != NULL) && (waitset != entity)) {
            dds_entity_unlock(e);
        }
        dds_waitset_unlock(ws);
    } else {
        DDS_ERROR("Error occurred on locking waitset\n");
        ret = DDS_ERRNO(rc);
    }

    return ret;
}

_Pre_satisfies_((waitset & DDS_ENTITY_KIND_MASK) == DDS_KIND_WAITSET)
DDS_EXPORT dds_return_t
dds_waitset_detach(
        _In_ dds_entity_t waitset,
        _In_ dds_entity_t entity)
{
    dds_waitset *ws;
    dds__retcode_t rc;
    dds_return_t ret;

    rc = dds_waitset_lock(waitset, &ws);
    if (rc == DDS_RETCODE_OK) {
        /* Possibly fails when entity was not attached. */
        if (waitset == entity) {
            rc = dds_entity_observer_unregister_nl(&ws->m_entity, waitset);
        } else {
            rc = dds_entity_observer_unregister(entity, waitset);
        }
        if (rc == DDS_RETCODE_OK) {
            dds_waitset_remove(ws, entity);
            ret = DDS_RETCODE_OK;
        } else if (rc != DDS_RETCODE_PRECONDITION_NOT_MET) {
            DDS_ERROR("The given entity to detach is invalid\n");
            ret = DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER);
        } else {
            DDS_ERROR("The given entity to detach was not attached previously\n");
            ret = DDS_ERRNO(rc);
        }
        dds_waitset_unlock(ws);
    } else {
        DDS_ERROR("Error occurred on locking waitset\n");
        ret = DDS_ERRNO(rc);
    }

    return ret;
}

_Pre_satisfies_((waitset & DDS_ENTITY_KIND_MASK) == DDS_KIND_WAITSET)
dds_return_t
dds_waitset_wait_until(
        _In_ dds_entity_t waitset,
        _Out_writes_to_opt_(nxs, return < 0 ? 0 : return) dds_attach_t *xs,
        _In_ size_t nxs,
        _In_ dds_time_t abstimeout)
{
    return dds_waitset_wait_impl(waitset, xs, nxs, abstimeout, dds_time());
}

_Pre_satisfies_((waitset & DDS_ENTITY_KIND_MASK) == DDS_KIND_WAITSET)
dds_return_t
dds_waitset_wait(
        _In_ dds_entity_t waitset,
        _Out_writes_to_opt_(nxs, return < 0 ? 0 : return) dds_attach_t *xs,
        _In_ size_t nxs,
        _In_ dds_duration_t reltimeout)
{
    dds_entity_t ret;

    if (reltimeout >= 0) {
        dds_time_t tnow = dds_time();
        dds_time_t abstimeout = (DDS_INFINITY - reltimeout <= tnow) ? DDS_NEVER : (tnow + reltimeout);
        ret = dds_waitset_wait_impl(waitset, xs, nxs, abstimeout, tnow);
    } else{
        DDS_ERROR("Negative timeout\n");
        ret = DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER);
    }

    return ret;
}

dds_return_t dds_waitset_set_trigger (dds_entity_t waitset, bool trigger)
{
  dds_waitset *ws;
  dds__retcode_t rc;

  if ((rc = dds_waitset_lock (waitset, &ws)) != DDS_RETCODE_OK)
    return DDS_ERRNO (rc);

  os_mutexUnlock (&ws->m_entity.m_mutex);

  os_mutexLock (&ws->m_entity.m_observers_lock);
  if (trigger)
    dds_entity_status_set (&ws->m_entity, DDS_WAITSET_TRIGGER_STATUS);
  else
    dds_entity_status_reset (&ws->m_entity, DDS_WAITSET_TRIGGER_STATUS);
  os_mutexUnlock (&ws->m_entity.m_observers_lock);

  os_mutexLock (&ws->m_entity.m_mutex);
  dds_waitset_unlock (ws);
  return DDS_RETCODE_OK;
}

