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

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/log.h"
#include "dds__entity.h"
#include "dds__querycond.h"
#include "dds__readcond.h"
#include "dds__rhc.h"

DEFINE_ENTITY_LOCK_UNLOCK(static, dds_waitset, DDS_KIND_WAITSET)

static void
dds_waitset_swap(
        dds_attachment **dst,
        dds_attachment **src,
        dds_attachment  *prev,
        dds_attachment  *idx)
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
        dds_entity_t waitset,
        dds_attach_t *xs,
        size_t nxs,
        dds_time_t abstimeout,
        dds_time_t tnow)
{
    dds_waitset *ws;
    dds_return_t ret;
    dds_attachment *idx;
    dds_attachment *next;
    dds_attachment *prev;

    if ((xs == NULL) && (nxs != 0)){
        DDS_ERROR("A size was given, but no array\n");
        return DDS_RETCODE_BAD_PARAMETER;
    }
    if ((xs != NULL) && (nxs == 0)){
        DDS_ERROR("Array is given with an invalid size\n");
        return DDS_RETCODE_BAD_PARAMETER;
    }

    /* Locking the waitset here will delay a possible deletion until it is
     * unlocked. Even when the related mutex is unlocked by a conditioned wait. */
    ret = dds_waitset_lock(waitset, &ws);
    if (ret == DDS_RETCODE_OK) {
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
        while ((ws->observed != NULL) && (ws->triggered == NULL) && (ret == DDS_RETCODE_OK)) {
            if (abstimeout == DDS_NEVER) {
                ddsrt_cond_wait(&ws->m_entity.m_cond, &ws->m_entity.m_mutex);
            } else if (abstimeout <= tnow) {
                ret = DDS_RETCODE_TIMEOUT;
            } else {
                dds_duration_t dt = abstimeout - tnow;
                (void)ddsrt_cond_waitfor(&ws->m_entity.m_cond, &ws->m_entity.m_mutex, dt);
                tnow = dds_time();
            }
        }

        /* Get number of triggered entities
         *   - set attach array when needed
         *   - swap them back to observed */
        if (ret == DDS_RETCODE_OK) {
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
        } else if (ret == DDS_RETCODE_TIMEOUT) {
            ret = 0;
        } else {
            DDS_ERROR("Internal error");
        }

        dds_waitset_unlock(ws);
    } else {
        DDS_ERROR("Error occurred on locking waitset\n");
    }

    return ret;
}

static void
dds_waitset_close_list(
        dds_attachment **list,
        dds_entity_t waitset)
{
    dds_attachment *idx = *list;
    dds_attachment *next;
    while (idx != NULL) {
        next = idx->next;
        (void)dds_entity_observer_unregister(idx->entity->m_hdllink.hdl, waitset);
        ddsrt_free(idx);
        idx = next;
    }
    *list = NULL;
}

static bool
dds_waitset_remove_from_list(
        dds_attachment **list,
        dds_entity_t observed)
{
    dds_attachment *idx = *list;
    dds_attachment *prev = NULL;

    while (idx != NULL) {
        if (idx->entity->m_hdllink.hdl == observed) {
            if (prev == NULL) {
                *list = idx->next;
            } else {
                prev->next = idx->next;
            }
            ddsrt_free(idx);

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

    dds_waitset_close_list(&ws->observed,  e->m_hdllink.hdl);
    dds_waitset_close_list(&ws->triggered, e->m_hdllink.hdl);

    /* Trigger waitset to wake up. */
    ddsrt_cond_broadcast(&e->m_cond);

    return DDS_RETCODE_OK;
}

DDS_EXPORT dds_entity_t
dds_create_waitset(
        dds_entity_t participant)
{
    dds_entity_t hdl;
    dds_entity *par;
    dds_return_t rc;

    rc = dds_entity_lock(participant, DDS_KIND_PARTICIPANT, &par);
    if (rc == DDS_RETCODE_OK) {
        dds_waitset *waitset = dds_alloc(sizeof(*waitset));
        hdl = dds_entity_init(&waitset->m_entity, par, DDS_KIND_WAITSET, NULL, NULL, 0);
        waitset->m_entity.m_deriver.close = dds_waitset_close;
        waitset->observed = NULL;
        waitset->triggered = NULL;
        dds_entity_unlock(par);
    } else {
        hdl = rc;
    }

    return hdl;
}


DDS_EXPORT dds_return_t
dds_waitset_get_entities(
        dds_entity_t waitset,
        dds_entity_t *entities,
        size_t size)
{
    dds_return_t ret;
    dds_waitset *ws;

    ret = dds_waitset_lock(waitset, &ws);
    if (ret == DDS_RETCODE_OK) {
        dds_attachment* iter;

        ret = 0;
        iter = ws->observed;
        while (iter) {
            if (((size_t)ret < size) && (entities != NULL)) {
                entities[ret] = iter->entity->m_hdllink.hdl;
            }
            ret++;
            iter = iter->next;
        }

        iter = ws->triggered;
        while (iter) {
            if (((size_t)ret < size) && (entities != NULL)) {
                entities[ret] = iter->entity->m_hdllink.hdl;
            }
            ret++;
            iter = iter->next;
        }
        dds_waitset_unlock(ws);
    } else {
        DDS_ERROR("Error occurred on locking waitset\n");
    }

    return ret;
}


static void
dds_waitset_move(
        dds_attachment **src,
        dds_attachment **dst,
        dds_entity_t entity)
{
    dds_attachment *idx = *src;
    dds_attachment *prev = NULL;
    while (idx != NULL) {
        if (idx->entity->m_hdllink.hdl == entity) {
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
        ddsrt_cond_broadcast(&ws->m_entity.m_cond);
        dds_waitset_unlock(ws);
    }
}

DDS_EXPORT dds_return_t
dds_waitset_attach(
        dds_entity_t waitset,
        dds_entity_t entity,
        dds_attach_t x)
{
    dds_entity  *e = NULL;
    dds_waitset *ws;
    dds_return_t ret;

    ret = dds_waitset_lock(waitset, &ws);
    if (ret == DDS_RETCODE_OK) {
        if (waitset != entity) {
            ret = dds_entity_lock(entity, DDS_KIND_DONTCARE, &e);
            if (ret != DDS_RETCODE_OK) {
                e = NULL;
            }
        } else {
            e = &ws->m_entity;
        }

        /* This will fail if given entity is already attached (or deleted). */
        if (ret == DDS_RETCODE_OK) {
            ret = dds_entity_observer_register_nl(e, waitset, dds_waitset_observer);
        }

        if (ret == DDS_RETCODE_OK) {
            dds_attachment *a = ddsrt_malloc(sizeof(dds_attachment));
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
        } else if (ret != DDS_RETCODE_PRECONDITION_NOT_MET) {
            DDS_ERROR("Entity is not valid\n");
            ret = DDS_RETCODE_BAD_PARAMETER;
        } else {
            DDS_ERROR("Entity is already attached\n");
        }
        if ((e != NULL) && (waitset != entity)) {
            dds_entity_unlock(e);
        }
        dds_waitset_unlock(ws);
    } else {
        DDS_ERROR("Error occurred on locking waitset\n");
    }

    return ret;
}

DDS_EXPORT dds_return_t
dds_waitset_detach(
        dds_entity_t waitset,
        dds_entity_t entity)
{
    dds_waitset *ws;
    dds_return_t ret;

    ret = dds_waitset_lock(waitset, &ws);
    if (ret == DDS_RETCODE_OK) {
        /* Possibly fails when entity was not attached. */
        if (waitset == entity) {
            ret = dds_entity_observer_unregister_nl(&ws->m_entity, waitset);
        } else {
            ret = dds_entity_observer_unregister(entity, waitset);
        }
        if (ret == DDS_RETCODE_OK) {
            dds_waitset_remove(ws, entity);
        } else if (ret != DDS_RETCODE_PRECONDITION_NOT_MET) {
            DDS_ERROR("The given entity to detach is invalid\n");
            ret = DDS_RETCODE_BAD_PARAMETER;
        } else {
            DDS_ERROR("The given entity to detach was not attached previously\n");
        }
        dds_waitset_unlock(ws);
    } else {
        DDS_ERROR("Error occurred on locking waitset\n");
    }

    return ret;
}

dds_return_t
dds_waitset_wait_until(
        dds_entity_t waitset,
        dds_attach_t *xs,
        size_t nxs,
        dds_time_t abstimeout)
{
    return dds_waitset_wait_impl(waitset, xs, nxs, abstimeout, dds_time());
}

dds_return_t
dds_waitset_wait(
        dds_entity_t waitset,
        dds_attach_t *xs,
        size_t nxs,
        dds_duration_t reltimeout)
{
    dds_entity_t ret;

    if (reltimeout >= 0) {
        dds_time_t tnow = dds_time();
        dds_time_t abstimeout = (DDS_INFINITY - reltimeout <= tnow) ? DDS_NEVER : (tnow + reltimeout);
        ret = dds_waitset_wait_impl(waitset, xs, nxs, abstimeout, tnow);
    } else{
        DDS_ERROR("Negative timeout\n");
        ret = DDS_RETCODE_BAD_PARAMETER;
    }

    return ret;
}

dds_return_t dds_waitset_set_trigger (dds_entity_t waitset, bool trigger)
{
  dds_waitset *ws;
  dds_return_t rc;

  if ((rc = dds_waitset_lock (waitset, &ws)) != DDS_RETCODE_OK)
    return rc;

  ddsrt_mutex_unlock (&ws->m_entity.m_mutex);

  ddsrt_mutex_lock (&ws->m_entity.m_observers_lock);
  if (trigger)
    dds_entity_status_set (&ws->m_entity, DDS_WAITSET_TRIGGER_STATUS);
  else
    dds_entity_status_reset (&ws->m_entity, DDS_WAITSET_TRIGGER_STATUS);
  ddsrt_mutex_unlock (&ws->m_entity.m_observers_lock);

  ddsrt_mutex_lock (&ws->m_entity.m_mutex);
  dds_waitset_unlock (ws);
  return DDS_RETCODE_OK;
}

