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
#include <string.h>
#include <assert.h>

#include "dds/ddsrt/cdtors.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/sync.h"
#include "dds/util/ut_handleserver.h"

/* Arbitrarily number of max handles. Should be enough for the mock. */
#define MAX_NR_OF_HANDLES  (1000)

#define HDL_FLAG_NONE      (0x00)
#define HDL_FLAG_CLOSED    (0x01)

typedef struct ut_handlelink {
    ut_handle_t hdl;
    void *arg;
    uint32_t cnt;
    uint8_t flags;
} ut_handlelink;

typedef struct ut_handleserver {
    ut_handlelink *hdls[MAX_NR_OF_HANDLES];
    int32_t last;
    ddsrt_mutex_t mutex;
} ut_handleserver;


/* Singleton handle server. */
static ut_handleserver *hs = NULL;


static ut_handle_retcode_t
lookup_handle(ut_handle_t hdl, int32_t kind, ut_handlelink **link);

static ut_handle_t
check_handle(ut_handle_t hdl, int32_t kind);

static void
delete_handle(int32_t idx);


ut_handle_retcode_t
ut_handleserver_init(void)
{
    ddsrt_init();
    /* TODO Allow re-entry (something like ddsrt_init()). */
    assert(hs == NULL);
    hs = ddsrt_malloc(sizeof(ut_handleserver));
    hs->last = 0;
    ddsrt_mutex_init(&hs->mutex);
    return UT_HANDLE_OK;
}


void
ut_handleserver_fini(void)
{
    int32_t i;

    /* TODO Only destroy when this is the last fini (something like ddsrt_fini()). */
    assert(hs);

    /* Every handle should have been deleted, but make sure. */
    for (i = 0; i < hs->last; i++) {
        if (hs->hdls[i] != NULL) {
            /* TODO CHAM-138: Print warning. */
            ddsrt_free(hs->hdls[i]);
        }
    }
    ddsrt_mutex_destroy(&hs->mutex);
    ddsrt_free(hs);
    hs = NULL;
    ddsrt_fini();
}


ut_handle_t
ut_handle_create(
    int32_t kind,
    void *arg)
{
    ut_handle_t hdl = (ut_handle_t)UT_HANDLE_OUT_OF_RESOURCES;

    /* A kind is obligatory. */
    assert(kind & UT_HANDLE_KIND_MASK);
    /* The kind should extent outside its boundaries. */
    assert(!(kind & ~UT_HANDLE_KIND_MASK));

    if (hs == NULL) {
        return (ut_handle_t)UT_HANDLE_NOT_INITALIZED;
    }

    ddsrt_mutex_lock(&hs->mutex);

    /* TODO CHAM-138: Improve the creation and management of handles. */
    if (hs->last < MAX_NR_OF_HANDLES) {
        hdl  = hs->last;
        hdl |= kind;
        hs->hdls[hs->last] = ddsrt_malloc(sizeof(ut_handlelink));
        hs->hdls[hs->last]->cnt   = 0;
        hs->hdls[hs->last]->arg   = arg;
        hs->hdls[hs->last]->hdl   = hdl;
        hs->hdls[hs->last]->flags = HDL_FLAG_NONE;
        hs->last++;
    }

    ddsrt_mutex_unlock(&hs->mutex);

    return hdl;
}

void
ut_handle_close(
    ut_handle_t hdl,
    struct ut_handlelink *link)
{
    struct ut_handlelink *info = link;
    ut_handle_retcode_t   ret = UT_HANDLE_OK;

    assert(hs);

    ddsrt_mutex_lock(&hs->mutex);
    if (info == NULL) {
        ret = lookup_handle(hdl, UT_HANDLE_DONTCARE_KIND, &info);
    }
    if (ret == UT_HANDLE_OK) {
        assert(info);
        assert(hdl == info->hdl);
        info->flags |= HDL_FLAG_CLOSED;
    }
    ddsrt_mutex_unlock(&hs->mutex);
}

ut_handle_retcode_t
ut_handle_delete(
    ut_handle_t hdl,
    struct ut_handlelink *link,
    dds_time_t timeout)
{
    struct ut_handlelink *info = link;
    ut_handle_retcode_t   ret = UT_HANDLE_OK;

    assert(hs);

    ddsrt_mutex_lock(&hs->mutex);
    if (info == NULL) {
        ret = lookup_handle(hdl, UT_HANDLE_DONTCARE_KIND, &info);
    }
    if (ret == UT_HANDLE_OK) {
        assert(info);
        assert(hdl == info->hdl);
        info->flags |= HDL_FLAG_CLOSED;

        /* FIXME: Replace this polling with conditional wait. */
        ddsrt_mutex_unlock(&hs->mutex);
        {
            dds_duration_t delay = DDS_MSECS(10);
            while ((info->cnt != 0) && timeout > 0) {
                dds_sleepfor(delay);
                timeout = (delay < timeout ? timeout - delay : 0);
            }
        }
        ddsrt_mutex_lock(&hs->mutex);

        if (info->cnt == 0) {
            delete_handle(hdl & UT_HANDLE_IDX_MASK);
        } else {
            ret = UT_HANDLE_TIMEOUT;
        }
    }
    ddsrt_mutex_unlock(&hs->mutex);

    return ret;
}

ut_handle_retcode_t
ut_handle_status(
    ut_handle_t hdl,
    struct ut_handlelink *link,
    int32_t kind)
{
    struct ut_handlelink *info = link;
    ut_handle_retcode_t   ret = UT_HANDLE_OK;

    if (hs == NULL) {
        return (ut_handle_t)UT_HANDLE_INVALID;
    }

    ddsrt_mutex_lock(&hs->mutex);
    if (info == NULL) {
        ret = lookup_handle(hdl, kind, &info);
    }
    if (ret == UT_HANDLE_OK) {
        assert(info);
        assert(hdl == info->hdl);
        if (info->flags & HDL_FLAG_CLOSED) {
            ret = UT_HANDLE_CLOSED;
        }
    }
    ddsrt_mutex_unlock(&hs->mutex);

    return ret;
}

ut_handle_retcode_t
ut_handle_claim(
    ut_handle_t hdl,
    struct ut_handlelink *link,
    int32_t kind,
    void **arg)
{
    struct ut_handlelink *info = link;
    ut_handle_retcode_t   ret = UT_HANDLE_OK;

    if (arg != NULL) {
        *arg = NULL;
    }

    if (hs == NULL) {
        return (ut_handle_t)UT_HANDLE_INVALID;
    }

    ddsrt_mutex_lock(&hs->mutex);
    if (info == NULL) {
        ret = lookup_handle(hdl, kind, &info);
    }
    if (ret == UT_HANDLE_OK) {
        assert(info);
        assert(hdl == info->hdl);
        if (info->flags & HDL_FLAG_CLOSED) {
            ret = UT_HANDLE_CLOSED;
        }
    }
    if (ret == UT_HANDLE_OK) {
        info->cnt++;
        if (arg != NULL) {
            *arg = info->arg;
        }
    }
    ddsrt_mutex_unlock(&hs->mutex);

    return ret;
}


void
ut_handle_release(
    ut_handle_t hdl,
    struct ut_handlelink *link)
{
    struct ut_handlelink *info = link;
    ut_handle_retcode_t   ret = UT_HANDLE_OK;

    assert(hs);

    ddsrt_mutex_lock(&hs->mutex);
    if (info == NULL) {
        ret = lookup_handle(hdl, UT_HANDLE_DONTCARE_KIND, &info);
    }
    if (ret == UT_HANDLE_OK) {
        assert(info);
        assert(hdl == info->hdl);
        assert(info->cnt > 0);
        info->cnt--;
    }
    ddsrt_mutex_unlock(&hs->mutex);
}

bool
ut_handle_is_closed(
    ut_handle_t hdl,
    struct ut_handlelink *link)
{
    struct ut_handlelink *info = link;
    ut_handle_retcode_t   ret = UT_HANDLE_OK;

    assert(hs);

    ddsrt_mutex_lock(&hs->mutex);
    if (info == NULL) {
        ret = lookup_handle(hdl, UT_HANDLE_DONTCARE_KIND, &info);
    }
    if (ret == UT_HANDLE_OK) {
        assert(info);
        assert(hdl == info->hdl);
        if (info->flags & HDL_FLAG_CLOSED) {
            ret = UT_HANDLE_CLOSED;
        }
    }
    ddsrt_mutex_unlock(&hs->mutex);

    /* Simulate closed for every error. */
    return (ret != UT_HANDLE_OK);
}

struct ut_handlelink*
ut_handle_get_link(
    ut_handle_t hdl)
{
    struct ut_handlelink *info;
    ut_handle_retcode_t   ret;

    assert(hs);

    ddsrt_mutex_lock(&hs->mutex);
    ret = lookup_handle(hdl, UT_HANDLE_DONTCARE_KIND, &info);
    assert(((ret == UT_HANDLE_OK) && (info != NULL)) ||
           ((ret != UT_HANDLE_OK) && (info == NULL)) );
    (void)ret;
    ddsrt_mutex_unlock(&hs->mutex);

    return info;
}


static ut_handle_retcode_t
lookup_handle(
    ut_handle_t hdl,
    int32_t kind,
    ut_handlelink **link)
{
    ut_handle_retcode_t ret;
    *link = NULL;
    ret = check_handle(hdl, kind);
    if (ret == UT_HANDLE_OK) {
        int32_t idx = (hdl & UT_HANDLE_IDX_MASK);
        assert(idx < MAX_NR_OF_HANDLES);
        *link = hs->hdls[idx];
    }
    return ret;
}

static ut_handle_t
check_handle(
    ut_handle_t hdl,
    int32_t kind)
{
    /* When handle is negative, it contains a retcode. */
    ut_handle_retcode_t ret = UT_HANDLE_OK;
    if (hdl > 0) {
        if (hdl & UT_HANDLE_KIND_MASK) {
            int32_t idx = (hdl & UT_HANDLE_IDX_MASK);
            if (idx < hs->last) {
                assert(idx < MAX_NR_OF_HANDLES);
                ut_handlelink *info = hs->hdls[idx];
                if (info != NULL) {
                    if ((info->hdl & UT_HANDLE_KIND_MASK) == (hdl & UT_HANDLE_KIND_MASK)) {
                        if ((kind != UT_HANDLE_DONTCARE_KIND) &&
                            (kind != (hdl & UT_HANDLE_KIND_MASK))) {
                            /* It's a valid handle, but the caller expected a different kind. */
                            ret = UT_HANDLE_UNEQUAL_KIND;
                        }
                    } else {
                        ret = UT_HANDLE_UNEQUAL_KIND;
                    }
                } else {
                    ret = UT_HANDLE_DELETED;
                }
            } else {
                ret = UT_HANDLE_INVALID;
            }
        } else {
            ret = UT_HANDLE_INVALID;
        }
    } else if (hdl == 0) {
        ret = UT_HANDLE_INVALID;
    } else {
        /* When handle is negative, it contains a retcode. */
        ret = (ut_handle_retcode_t)hdl;
    }
    return ret;
}

static void
delete_handle(int32_t idx)
{
    assert(hs);
    assert(idx < MAX_NR_OF_HANDLES);
    ddsrt_free(hs->hdls[idx]);
    hs->hdls[idx] = NULL;
}
