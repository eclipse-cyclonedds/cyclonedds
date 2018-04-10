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
#include "dds__writer.h"
#include "dds__write.h"
#include "dds__tkmap.h"
#include "ddsi/q_error.h"
#include "ddsi/q_thread.h"
#include "q__osplser.h"
#include "dds__err.h"
#include "ddsi/q_transmit.h"
#include "ddsi/q_ephash.h"
#include "ddsi/q_config.h"
#include "ddsi/q_entity.h"
#include "dds__report.h"
#include "ddsi/q_radmin.h"
#include <string.h>


#if OS_ATOMIC64_SUPPORT
typedef os_atomic_uint64_t fake_seq_t;
uint64_t fake_seq_next (fake_seq_t *x) { return os_atomic_inc64_nv (x); }
#else /* HACK */
typedef os_atomic_uint32_t fake_seq_t;
uint64_t fake_seq_next (fake_seq_t *x) { return os_atomic_inc32_nv (x); }
#endif

_Pre_satisfies_((writer & DDS_ENTITY_KIND_MASK) == DDS_KIND_WRITER)
dds_return_t
dds_write(
        _In_ dds_entity_t writer,
        _In_ const void *data)
{
    dds_return_t ret;
    dds__retcode_t rc;
    dds_writer *wr;

    DDS_REPORT_STACK();

    if (data != NULL) {
        rc = dds_writer_lock(writer, &wr);
        if (rc == DDS_RETCODE_OK) {
            ret = dds_write_impl(wr, data, dds_time(), 0);
            dds_writer_unlock(wr);
        } else {
            ret = DDS_ERRNO(rc, "Error occurred on locking entity");
        }
    } else {
        ret = DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER, "No data buffer provided");
    }
    DDS_REPORT_FLUSH(ret != DDS_RETCODE_OK);
    return ret;
}

_Pre_satisfies_((writer & DDS_ENTITY_KIND_MASK) == DDS_KIND_WRITER)
int
dds_writecdr(
        dds_entity_t writer,
        const void *cdr,
        size_t size)
{
    dds_return_t ret;
    dds__retcode_t rc;
    dds_writer *wr;
    if (cdr != NULL) {
        rc = dds_writer_lock(writer, &wr);
        if (rc == DDS_RETCODE_OK) {
            ret = dds_writecdr_impl (wr, cdr, size, dds_time (), 0);
            dds_writer_unlock(wr);
        } else {
            ret = DDS_ERRNO(rc, "Error occurred on locking writer");
        }
    } else{
        ret = DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER, "Given cdr has NULL value");
    }
    return ret;
}

_Pre_satisfies_((writer & DDS_ENTITY_KIND_MASK) == DDS_KIND_WRITER)
dds_return_t
dds_write_ts(
        _In_ dds_entity_t writer,
        _In_ const void *data,
        _In_ dds_time_t timestamp)
{
    dds_return_t ret;
    dds__retcode_t rc;
    dds_writer *wr;

    DDS_REPORT_STACK();

    if(data == NULL){
        ret = DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER, "Argument data has NULL value");
        goto err;
    }
    if(timestamp < 0){
        ret = DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER, "Argument timestamp has negative value");
        goto err;
    }
    rc = dds_writer_lock(writer, &wr);
    if (rc == DDS_RETCODE_OK) {
        ret = dds_write_impl(wr, data, timestamp, 0);
        dds_writer_unlock(wr);
    } else {
        ret = DDS_ERRNO(rc, "Error occurred on locking writer");
    }
err:
    DDS_REPORT_FLUSH(ret != DDS_RETCODE_OK);
    return ret;
}

static void
init_sampleinfo(
        _Out_ struct nn_rsample_info *sampleinfo,
        _In_  struct writer *wr,
        _In_  int64_t seq,
        _In_  serdata_t payload)
{
    memset(sampleinfo, 0, sizeof(*sampleinfo));
    sampleinfo->bswap = 0;
    sampleinfo->complex_qos = 0;
    sampleinfo->hashash = 0;
    sampleinfo->seq = seq;
    sampleinfo->reception_timestamp = payload->v.msginfo.timestamp;
    sampleinfo->statusinfo = payload->v.msginfo.statusinfo;
    sampleinfo->pwr_info.iid = 1;
    sampleinfo->pwr_info.auto_dispose = 0;
    sampleinfo->pwr_info.guid = wr->e.guid;
    sampleinfo->pwr_info.ownership_strength = 0;
}

static int
deliver_locally(
        _In_ struct writer *wr,
        _In_ int64_t seq,
        _In_ serdata_t payload,
        _In_ struct tkmap_instance *tk)
{
    dds_return_t ret = DDS_RETCODE_OK;
    os_mutexLock (&wr->rdary.rdary_lock);
    if (wr->rdary.fastpath_ok) {
        struct reader ** const rdary = wr->rdary.rdary;
        if (rdary[0]) {
            struct nn_rsample_info sampleinfo;
            unsigned i;
            init_sampleinfo(&sampleinfo, wr, seq, payload);
            for (i = 0; rdary[i]; i++) {
                bool stored;
                TRACE (("reader %x:%x:%x:%x\n", PGUID (rdary[i]->e.guid)));
                dds_duration_t max_block_ms = nn_from_ddsi_duration(wr->xqos->reliability.max_blocking_time) / DDS_NSECS_IN_MSEC;
                do {
                    stored = (ddsi_plugin.rhc_store_fn) (rdary[i]->rhc, &sampleinfo, payload, tk);
                    if (!stored) {
                        if (max_block_ms <= 0) {
                            ret = DDS_ERRNO(DDS_RETCODE_TIMEOUT, "The writer could not deliver data on time, probably due to a local reader resources being full.");
                        } else {
                            dds_sleepfor(DDS_MSECS(DDS_HEADBANG_TIMEOUT_MS));
                        }
                        /* Decreasing the block time after the sleep, let's us possibly
                         * wait a bit too long. But that's preferable compared to waiting
                         * a bit too short. */
                        max_block_ms -= DDS_HEADBANG_TIMEOUT_MS;
                    }
                } while ((!stored) && (ret == DDS_RETCODE_OK));
            }
        }
        os_mutexUnlock (&wr->rdary.rdary_lock);
    } else {
        /* When deleting, pwr is no longer accessible via the hash
           tables, and consequently, a reader may be deleted without
           it being possible to remove it from rdary. The primary
           reason rdary exists is to avoid locking the proxy writer
           but this is less of an issue when we are deleting it, so
           we fall back to using the GUIDs so that we can deliver all
           samples we received from it. As writer being deleted any
           reliable samples that are rejected are simply discarded. */
        ut_avlIter_t it;
        struct pwr_rd_match *m;
        struct nn_rsample_info sampleinfo;
        os_mutexUnlock (&wr->rdary.rdary_lock);
        init_sampleinfo(&sampleinfo, wr, seq, payload);
        os_mutexLock (&wr->e.lock);
        for (m = ut_avlIterFirst (&wr_local_readers_treedef, &wr->local_readers, &it); m != NULL; m = ut_avlIterNext (&it)) {
            struct reader *rd;
            if ((rd = ephash_lookup_reader_guid (&m->rd_guid)) != NULL) {
                TRACE (("reader-via-guid %x:%x:%x:%x\n", PGUID (rd->e.guid)));
                /* Copied the return value ignore from DDSI deliver_user_data() function. */
                (void)(ddsi_plugin.rhc_store_fn) (rd->rhc, &sampleinfo, payload, tk);
            }
        }
        os_mutexUnlock (&wr->e.lock);
    }
    return ret;
}

int
dds_write_impl(
        _In_ dds_writer *wr,
        _In_ const void * data,
        _In_ dds_time_t tstamp,
        _In_ dds_write_action action)
{
    static fake_seq_t fake_seq;
    dds_return_t ret = DDS_RETCODE_OK;
    int w_rc;

    assert (wr);
    assert (dds_entity_kind(((dds_entity*)wr)->m_hdl) == DDS_KIND_WRITER);

    struct thread_state1 * const thr = lookup_thread_state ();
    const bool asleep = !vtime_awake_p (thr->vtime);
    const bool writekey = action & DDS_WR_KEY_BIT;
    dds_writer * writer = (dds_writer*) wr;
    struct writer * ddsi_wr = writer->m_wr;
    struct tkmap_instance * tk;
    serdata_t d;

    if (data == NULL) {
        return DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER, "No data buffer provided");
    }

    /* Check for topic filter */
    if (ddsi_wr->topic->filter_fn && ! writekey) {
        if (!(ddsi_wr->topic->filter_fn) (data, ddsi_wr->topic->filter_ctx)) {
            goto filtered;
        }
    }

    if (asleep) {
        thread_state_awake (thr);
    }

    /* Serialize and write data or key */
    if (writekey) {
        d = serialize_key (gv.serpool, ddsi_wr->topic, data);
    } else {
        d = serialize (gv.serpool, ddsi_wr->topic, data);
    }

    /* Set if disposing or unregistering */
    d->v.msginfo.statusinfo = ((action & DDS_WR_DISPOSE_BIT   ) ? NN_STATUSINFO_DISPOSE    : 0) |
                              ((action & DDS_WR_UNREGISTER_BIT) ? NN_STATUSINFO_UNREGISTER : 0) ;
    d->v.msginfo.timestamp.v = tstamp;
    os_mutexLock (&writer->m_call_lock);
    ddsi_serdata_ref(d);
    tk = (ddsi_plugin.rhc_lookup_fn) (d);
    w_rc = write_sample_gc (writer->m_xp, ddsi_wr, d, tk);

    if (w_rc >= 0) {
        /* Flush out write unless configured to batch */
        if (! config.whc_batch){
            nn_xpack_send (writer->m_xp, false);
        }
        ret = DDS_RETCODE_OK;
    } else if (w_rc == ERR_TIMEOUT) {
        ret = DDS_ERRNO(DDS_RETCODE_TIMEOUT, "The writer could not deliver data on time, probably due to a reader resources being full.");
    } else if (w_rc == ERR_INVALID_DATA) {
        ret = DDS_ERRNO(DDS_RETCODE_ERROR, "Invalid data provided");
    } else {
        ret = DDS_ERRNO(DDS_RETCODE_ERROR, "Internal error");
    }
    os_mutexUnlock (&writer->m_call_lock);

    if (ret == DDS_RETCODE_OK) {
        ret = deliver_locally (ddsi_wr, fake_seq_next(&fake_seq), d, tk);
    }
    ddsi_serdata_unref(d);
    (ddsi_plugin.rhc_unref_fn) (tk);

    if (asleep) {
        thread_state_asleep (thr);
    }

filtered:
    return ret;
}

int
dds_writecdr_impl(
        _In_ dds_writer *wr,
        _In_ const void *cdr,
        _In_ size_t sz,
        _In_ dds_time_t tstamp,
        _In_ dds_write_action action)
{
    static fake_seq_t fake_seq;
    int ret = DDS_RETCODE_OK;
    int w_rc;

    assert (wr);
    assert (cdr);

    struct thread_state1 * const thr = lookup_thread_state ();
    const bool asleep = !vtime_awake_p (thr->vtime);
    const bool writekey = action & DDS_WR_KEY_BIT;
    struct writer * ddsi_wr = wr->m_wr;
    struct tkmap_instance * tk;
    serdata_t d;

    /* Check for topic filter */
    if (ddsi_wr->topic->filter_fn && ! writekey) {
        abort();
    }

    if (asleep) {
        thread_state_awake (thr);
    }

    /* Serialize and write data or key */
    if (writekey) {
        abort();
        //d = serialize_key (gv.serpool, ddsi_wr->topic, data);
    } else {
        serstate_t st = ddsi_serstate_new (gv.serpool, ddsi_wr->topic);
        if (ddsi_wr->topic->nkeys) {
            abort();
            //dds_key_gen ((const dds_topic_descriptor_t*) tp->type, &st->data->v.keyhash, (char*) sample);
        }
        ddsi_serstate_append_blob(st, 1, sz, cdr);
        d = ddsi_serstate_fix(st);
    }

    /* Set if disposing or unregistering */
    d->v.msginfo.statusinfo = ((action & DDS_WR_DISPOSE_BIT   ) ? NN_STATUSINFO_DISPOSE    : 0) |
                              ((action & DDS_WR_UNREGISTER_BIT) ? NN_STATUSINFO_UNREGISTER : 0) ;
    d->v.msginfo.timestamp.v = tstamp;
    os_mutexLock (&wr->m_call_lock);
    ddsi_serdata_ref(d);
    tk = (ddsi_plugin.rhc_lookup_fn) (d);
    w_rc = write_sample_gc (wr->m_xp, ddsi_wr, d, tk);

    if (w_rc >= 0) {
        /* Flush out write unless configured to batch */
        if (! config.whc_batch) {
            nn_xpack_send (wr->m_xp, false);
        }
        ret = DDS_RETCODE_OK;
    } else if (w_rc == ERR_TIMEOUT) {
        ret = DDS_ERRNO(DDS_RETCODE_TIMEOUT, "The writer could not deliver data on time, probably due to a reader resources being full.");
    } else if (w_rc == ERR_INVALID_DATA) {
        ret = DDS_ERRNO(DDS_RETCODE_ERROR, "Invalid data provided");
    } else {
        ret = DDS_ERRNO(DDS_RETCODE_ERROR, "Internal error");
    }
    os_mutexUnlock (&wr->m_call_lock);

    if (ret == DDS_RETCODE_OK) {
        ret = deliver_locally (ddsi_wr, fake_seq_next(&fake_seq), d, tk);
    }
    ddsi_serdata_unref(d);
    (ddsi_plugin.rhc_unref_fn) (tk);

    if (asleep) {
        thread_state_asleep (thr);
    }

    return ret;
}

void
dds_write_set_batch(
        bool enable)
{
    config.whc_batch = enable ? 1 : 0;
}

_Pre_satisfies_((writer & DDS_ENTITY_KIND_MASK) == DDS_KIND_WRITER)
void
dds_write_flush(
        dds_entity_t writer)
{
    dds_return_t ret = DDS_RETCODE_OK;
    dds__retcode_t rc;
    DDS_REPORT_STACK();

    struct thread_state1 * const thr = lookup_thread_state ();
    const bool asleep = !vtime_awake_p (thr->vtime);
    dds_writer *wr;

    if (asleep) {
        thread_state_awake (thr);
    }
    rc = dds_writer_lock(writer, &wr);
    if (rc == DDS_RETCODE_OK) {
        nn_xpack_send (wr->m_xp, true);
        dds_writer_unlock(wr);
        ret = DDS_RETCODE_OK;
    } else{
        ret = DDS_ERRNO(rc, "Error occurred on locking writer");
    }

    if (asleep) {
        thread_state_asleep (thr);
    }
    DDS_REPORT_FLUSH(ret < 0);
    return ;
}
