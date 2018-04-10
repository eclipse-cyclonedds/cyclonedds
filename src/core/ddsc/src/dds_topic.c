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
#include <ctype.h>
#include "dds__topic.h"
#include "dds__listener.h"
#include "dds__qos.h"
#include "dds__stream.h"
#include "dds__init.h"
#include "dds__domain.h"
#include "dds__err.h"
#include "ddsi/q_entity.h"
#include "ddsi/q_thread.h"
#include "q__osplser.h"
#include "ddsi/q_ddsi_discovery.h"
#include "os/os_atomics.h"
#include "dds__report.h"


#define DDS_TOPIC_STATUS_MASK                                    \
                        DDS_INCONSISTENT_TOPIC_STATUS

const ut_avlTreedef_t dds_topictree_def = UT_AVL_TREEDEF_INITIALIZER_INDKEY
(
  offsetof (struct sertopic, avlnode),
  offsetof (struct sertopic, name_typename),
  (int (*) (const void *, const void *)) strcmp,
  0
);

/* builtin-topic handles */
const dds_entity_t DDS_BUILTIN_TOPIC_DCPSPARTICIPANT = (DDS_KIND_INTERNAL + 1);
const dds_entity_t DDS_BUILTIN_TOPIC_CMPARTICIPANT = (DDS_KIND_INTERNAL + 2);
const dds_entity_t DDS_BUILTIN_TOPIC_DCPSTYPE = (DDS_KIND_INTERNAL + 3);
const dds_entity_t DDS_BUILTIN_TOPIC_DCPSTOPIC = (DDS_KIND_INTERNAL + 4);
const dds_entity_t DDS_BUILTIN_TOPIC_DCPSPUBLICATION = (DDS_KIND_INTERNAL + 5);
const dds_entity_t DDS_BUILTIN_TOPIC_CMPUBLISHER = (DDS_KIND_INTERNAL + 6);
const dds_entity_t DDS_BUILTIN_TOPIC_DCPSSUBSCRIPTION = (DDS_KIND_INTERNAL + 7);
const dds_entity_t DDS_BUILTIN_TOPIC_CMSUBSCRIBER = (DDS_KIND_INTERNAL + 8);
const dds_entity_t DDS_BUILTIN_TOPIC_CMDATAWRITER = (DDS_KIND_INTERNAL + 9);
const dds_entity_t DDS_BUILTIN_TOPIC_CMDATAREADER = (DDS_KIND_INTERNAL + 10);

static bool
is_valid_name(
        _In_ const char *name)
{
    bool valid = false;
    /* DDS Spec:
     *  |  TOPICNAME - A topic name is an identifier for a topic, and is defined as any series of characters
     *  |     'a', ..., 'z',
     *  |     'A', ..., 'Z',
     *  |     '0', ..., '9',
     *  |     '-' but may not start with a digit.
     * It is considered that '-' is an error in the spec and should say '_'. So, that's what we'll check for.
     */
    assert(name);
    if ((name[0] != '\0') && (!isdigit((unsigned char)name[0]))) {
        while (isalnum((unsigned char)*name) || (*name == '_')) {
            name++;
        }
        if (*name == '\0') {
            valid = true;
        }
    }

   return valid;
}


static dds_return_t
dds_topic_status_validate(
        uint32_t mask)
{
    return (mask & ~(DDS_TOPIC_STATUS_MASK)) ?
                     DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER, "Argument mask is invalid") :
                     DDS_RETCODE_OK;
}

/*
  Topic status change callback handler. Supports INCONSISTENT_TOPIC
  status (only defined status on a topic).
*/

static void
dds_topic_status_cb(
        struct dds_topic *cb_t)
{
    dds_topic *topic;
    dds__retcode_t rc;
    void *metrics = NULL;

    DDS_REPORT_STACK();

    if (dds_topic_lock(((dds_entity*)cb_t)->m_hdl, &topic) != DDS_RETCODE_OK) {
        /* There's a deletion or closing going on. */
        DDS_REPORT_FLUSH(false);
        return;
    }
    assert(topic == cb_t);

    /* Reset the status for possible Listener call.
     * When a listener is not called, the status will be set (again). */

    /* Update status metrics. */
    topic->m_inconsistent_topic_status.total_count++;
    topic->m_inconsistent_topic_status.total_count_change++;


    /* The topic needs to be unlocked when propagating the (possible) listener
     * call because the application should be able to call this topic within
     * the callback function. */
    dds_topic_unlock(topic);

    /* Is anybody interested within the entity hierarchy through listeners? */
    rc = dds_entity_listener_propagation((dds_entity*)topic,
                                         (dds_entity*)topic,
                                         DDS_INCONSISTENT_TOPIC_STATUS,
                                         (void*)&(topic->m_inconsistent_topic_status),
                                         true);

    if (rc == DDS_RETCODE_OK) {
        /* Event was eaten by a listener. */
        if (dds_topic_lock(((dds_entity*)cb_t)->m_hdl, &topic) == DDS_RETCODE_OK) {
            /* Reset the change counts of the metrics. */
            topic->m_inconsistent_topic_status.total_count_change = 0;
            dds_topic_unlock(topic);
        }
    } else if (rc == DDS_RETCODE_NO_DATA) {
        /* Nobody was interested through a listener (NO_DATA == NO_CALL): set the status. */
        dds_entity_status_set((dds_entity*)topic, DDS_INCONSISTENT_TOPIC_STATUS);
        /* Notify possible interested observers. */
        dds_entity_status_signal((dds_entity*)topic);
        rc = DDS_RETCODE_OK;
    } else if (rc == DDS_RETCODE_ALREADY_DELETED) {
        /* An entity up the hierarchy is being deleted. */
        rc = DDS_RETCODE_OK;
    } else {
        /* Something went wrong up the hierarchy. */
    }

    DDS_REPORT_FLUSH(rc != DDS_RETCODE_OK);
}

sertopic_t
dds_topic_lookup(
        dds_domain *domain,
        const char *name)
{
    sertopic_t st = NULL;
    ut_avlIter_t iter;

    assert (domain);
    assert (name);

    os_mutexLock (&dds_global.m_mutex);
    st = ut_avlIterFirst (&dds_topictree_def, &domain->m_topics, &iter);
    while (st) {
        if (strcmp (st->name, name) == 0) {
            break;
        }
        st = ut_avlIterNext (&iter);
    }
    os_mutexUnlock (&dds_global.m_mutex);
    return st;
}

void
dds_topic_free(
        dds_domainid_t domainid,
        struct sertopic *st)
{
    dds_domain *domain;

    assert (st);

    os_mutexLock (&dds_global.m_mutex);
    domain = (dds_domain*) ut_avlLookup (&dds_domaintree_def, &dds_global.m_domains, &domainid);
    if (domain != NULL) {
        ut_avlDelete (&dds_topictree_def, &domain->m_topics, st);
    }
    os_mutexUnlock (&dds_global.m_mutex);
    st->status_cb_entity = NULL;
    sertopic_free (st);
}

static void
dds_topic_add(
        dds_domainid_t id,
        sertopic_t st)
{
    dds_domain * dom;
    os_mutexLock (&dds_global.m_mutex);
    dom = dds_domain_find_locked (id);
    assert (dom);
    ut_avlInsert (&dds_topictree_def, &dom->m_topics, st);
    os_mutexUnlock (&dds_global.m_mutex);
}

_Pre_satisfies_((participant & DDS_ENTITY_KIND_MASK) == DDS_KIND_PARTICIPANT)
DDS_EXPORT dds_entity_t
dds_find_topic(
        _In_ dds_entity_t participant,
        _In_z_ const char *name)
{
    dds_entity_t tp;
    dds_entity *p = NULL;
    sertopic_t st;
    dds__retcode_t rc;

    DDS_REPORT_STACK();

    if (name) {
        rc = dds_entity_lock(participant, DDS_KIND_PARTICIPANT, &p);
        if (rc == DDS_RETCODE_OK) {
            st = dds_topic_lookup (p->m_domain, name);
            if (st) {
                dds_entity_add_ref (&st->status_cb_entity->m_entity);
                tp = st->status_cb_entity->m_entity.m_hdl;
            } else {
                tp = DDS_ERRNO(DDS_RETCODE_PRECONDITION_NOT_MET, "Topic is not being created yet");
            }
            dds_entity_unlock(p);
        } else {
            tp = DDS_ERRNO(rc, "Error occurred on locking entity");
        }
    } else {
        tp = DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER, "Argument name is not valid");
    }
    DDS_REPORT_FLUSH(tp <= 0);
    return tp;
}

static dds_return_t
dds_topic_delete(
        dds_entity *e)
{
    dds_topic_free(e->m_domainid, ((dds_topic*) e)->m_stopic);
    return DDS_RETCODE_OK;
}

static dds_return_t
dds_topic_qos_validate(
        const dds_qos_t *qos,
        bool enabled)
{
    dds_return_t ret = DDS_RETCODE_OK;
    assert(qos);


    /* Check consistency. */
    if (!dds_qos_validate_common(qos)) {
        ret = DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER, "Argument QoS is not valid");
    }
    if ((qos->present & QP_GROUP_DATA) && !validate_octetseq (&qos->group_data)) {
        ret = DDS_ERRNO(DDS_RETCODE_INCONSISTENT_POLICY, "Group data QoS policy is inconsistent and caused an error");
    }
    if ((qos->present & QP_DURABILITY_SERVICE) && (validate_durability_service_qospolicy(&qos->durability_service) != 0)) {
        ret = DDS_ERRNO(DDS_RETCODE_INCONSISTENT_POLICY, "Durability service QoS policy is inconsistent and caused an error");
    }
    if ((qos->present & QP_LIFESPAN) && (validate_duration(&qos->lifespan.duration) != 0)) {
        ret = DDS_ERRNO(DDS_RETCODE_INCONSISTENT_POLICY, "Lifespan QoS policy is inconsistent and caused an error");
    }
    if (qos->present & QP_HISTORY && (qos->present & QP_RESOURCE_LIMITS) && (validate_history_and_resource_limits(&qos->history, &qos->resource_limits) != 0)) {
        ret = DDS_ERRNO(DDS_RETCODE_INCONSISTENT_POLICY, "Lifespan QoS policy is inconsistent and caused an error");
    }
    if(ret == DDS_RETCODE_OK && enabled){
        ret = dds_qos_validate_mutable_common(qos);
    }
    return ret;
}


static dds_return_t
dds_topic_qos_set(
        dds_entity *e,
        const dds_qos_t *qos,
        bool enabled)
{
    dds_return_t ret = dds_topic_qos_validate(qos, enabled);
    if (ret == DDS_RETCODE_OK) {
        if (enabled) {
            /* TODO: CHAM-95: DDSI does not support changing QoS policies. */
            ret = DDS_ERRNO(DDS_RETCODE_UNSUPPORTED, "Changing the topic QoS is not supported.");
        }
    }
    return ret;
}

_Pre_satisfies_((participant & DDS_ENTITY_KIND_MASK) == DDS_KIND_PARTICIPANT)
DDS_EXPORT dds_entity_t
dds_create_topic(
        _In_ dds_entity_t participant,
        _In_ const dds_topic_descriptor_t *desc,
        _In_z_ const char *name,
        _In_opt_ const dds_qos_t *qos,
        _In_opt_ const dds_listener_t *listener)
{
    static uint32_t next_topicid = 0;

    char *key = NULL;
    sertopic_t st;
    const char *typename;
    dds__retcode_t rc;
    dds_entity *par;
    dds_topic *top;
    dds_qos_t *new_qos = NULL;
    nn_plist_t plist;
    dds_entity_t hdl;
    struct participant *ddsi_pp;
    struct thread_state1 *const thr = lookup_thread_state ();
    const bool asleep = !vtime_awake_p (thr->vtime);
    uint32_t index;

    DDS_REPORT_STACK();

    if (desc == NULL){
        hdl = DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER, "Topic description is NULL");
        goto bad_param_err;
    }

    if (name == NULL) {
        hdl = DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER, "Topic name is NULL");
        goto bad_param_err;
    }

    if (!is_valid_name(name)) {
        hdl = DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER, "Topic name contains characters that are not allowed.");
        goto bad_param_err;
    }

    rc = dds_entity_lock(participant, DDS_KIND_PARTICIPANT, &par);
    if (rc != DDS_RETCODE_OK) {
        hdl = DDS_ERRNO(rc, "Error occurred on locking entity");
        goto lock_err;
    }

    /* Validate qos */
    if (qos) {
        hdl = dds_topic_qos_validate (qos, false);
        if (hdl != DDS_RETCODE_OK) {
            goto qos_err;
        }
    }

    /* Check if topic already exists with same name */
    if (dds_topic_lookup (par->m_domain, name)) {
        hdl = DDS_ERRNO(DDS_RETCODE_PRECONDITION_NOT_MET, "Precondition not met");
        goto qos_err;
    }

    typename = desc->m_typename;
    key = (char*) dds_alloc (strlen (name) + strlen (typename) + 2);
    strcpy (key, name);
    strcat (key, "/");
    strcat (key, typename);

    if (qos) {
        new_qos = dds_qos_create();
        /* Only returns failure when one of the qos args is NULL, which
         * is not the case here. */
        (void)dds_qos_copy(new_qos, qos);
    }

    /* Create topic */
    top = dds_alloc (sizeof (*top));
    top->m_descriptor = desc;
    hdl = dds_entity_init (&top->m_entity, par, DDS_KIND_TOPIC, new_qos, listener, DDS_TOPIC_STATUS_MASK);
    top->m_entity.m_deriver.delete = dds_topic_delete;
    top->m_entity.m_deriver.set_qos = dds_topic_qos_set;
    top->m_entity.m_deriver.validate_status = dds_topic_status_validate;

    st = dds_alloc (sizeof (*st));
    st->type = (void*) desc;
    os_atomic_st32 (&st->refcount, 1);
    st->status_cb = dds_topic_status_cb;
    st->status_cb_entity = top;
    st->name_typename = key;
    st->name = dds_alloc (strlen (name) + 1);
    strcpy (st->name, name);
    st->typename = dds_alloc (strlen (typename) + 1);
    strcpy (st->typename, typename);
    st->nkeys = desc->m_nkeys;
    st->keys = desc->m_keys;
    st->id = next_topicid++;

#ifdef VXWORKS_RTP
    st->hash = (st->id * UINT64_C (12844332200329132887UL)) >> 32;
#else
    st->hash = (st->id * UINT64_C (12844332200329132887)) >> 32;
#endif

    /* Check if topic cannot be optimised (memcpy marshal) */

    if ((desc->m_flagset & DDS_TOPIC_NO_OPTIMIZE) == 0) {
        st->opt_size = dds_stream_check_optimize (desc);
    }
    top->m_stopic = st;

    /* Add topic to extent */
    dds_topic_add (par->m_domainid, st);

    nn_plist_init_empty (&plist);
    if (new_qos) {
        dds_qos_merge (&plist.qos, new_qos);
    }

    /* Set Topic meta data (for SEDP publication) */
    plist.qos.topic_name = dds_string_dup (st->name);
    plist.qos.type_name = dds_string_dup (st->typename);
    plist.qos.present |= (QP_TOPIC_NAME | QP_TYPE_NAME);
    if (desc->m_meta) {
        plist.type_description = dds_string_dup (desc->m_meta);
        plist.present |= PP_PRISMTECH_TYPE_DESCRIPTION;
    }
    if (desc->m_nkeys) {
        plist.qos.present |= QP_PRISMTECH_SUBSCRIPTION_KEYS;
        plist.qos.subscription_keys.use_key_list = 1;
        plist.qos.subscription_keys.key_list.n = desc->m_nkeys;
        plist.qos.subscription_keys.key_list.strs = dds_alloc (desc->m_nkeys * sizeof (char*));
        for (index = 0; index < desc->m_nkeys; index++) {
            plist.qos.subscription_keys.key_list.strs[index] = dds_string_dup (desc->m_keys[index].m_name);
        }
    }

    /* Publish Topic */
    if (asleep) {
        thread_state_awake (thr);
    }
    ddsi_pp = ephash_lookup_participant_guid (&par->m_guid);
    assert (ddsi_pp);
    sedp_write_topic (ddsi_pp, &plist);
    if (asleep) {
        thread_state_asleep (thr);
    }
    nn_plist_fini (&plist);

qos_err:
    dds_entity_unlock(par);
lock_err:
bad_param_err:
    DDS_REPORT_FLUSH(hdl <= 0);
    return hdl;
}

static bool
dds_topic_chaining_filter(
        const void *sample,
        void *ctx)
{
    dds_topic_filter_fn realf = (dds_topic_filter_fn)ctx;
    return realf (sample);
}

static void
dds_topic_mod_filter(
        dds_entity_t topic,
        dds_topic_intern_filter_fn *filter,
        void **ctx,
        bool set)
{
    dds_topic *t;
    if (dds_topic_lock(topic, &t) == DDS_RETCODE_OK) {
        if (set) {
            t->m_stopic->filter_fn = *filter;
            t->m_stopic->filter_ctx = *ctx;

            /* Create sample for read filtering */

            if (t->m_stopic->filter_sample == NULL) {
                t->m_stopic->filter_sample = dds_alloc (t->m_descriptor->m_size);
            }
        } else {
            *filter = t->m_stopic->filter_fn;
            *ctx = t->m_stopic->filter_ctx;
        }
        dds_topic_unlock(t);
    }
}

_Pre_satisfies_((topic & DDS_ENTITY_KIND_MASK) == DDS_KIND_TOPIC)
void
dds_topic_set_filter(
        dds_entity_t topic,
        dds_topic_filter_fn filter)
{
    dds_topic_intern_filter_fn chaining = dds_topic_chaining_filter;
    void *realf = (void *)filter;
    dds_topic_mod_filter (topic, &chaining, &realf, true);
}

_Pre_satisfies_((topic & DDS_ENTITY_KIND_MASK) == DDS_KIND_TOPIC)
dds_topic_filter_fn
dds_topic_get_filter(
        dds_entity_t topic)
{
    dds_topic_intern_filter_fn filter;
    void *ctx;
    dds_topic_mod_filter (topic, &filter, &ctx, false);
    return
      (filter == dds_topic_chaining_filter) ? (dds_topic_filter_fn)ctx : NULL;
}

void
dds_topic_set_filter_with_ctx(
        dds_entity_t topic,
        dds_topic_intern_filter_fn filter,
        void *ctx)
{
  dds_topic_mod_filter (topic, &filter, &ctx, true);
}

dds_topic_intern_filter_fn
dds_topic_get_filter_with_ctx(
        dds_entity_t topic)
{
  dds_topic_intern_filter_fn filter;
  void *ctx;
  dds_topic_mod_filter (topic, &filter, &ctx, false);
  return (filter == dds_topic_chaining_filter) ? NULL : filter;
}

_Pre_satisfies_((topic & DDS_ENTITY_KIND_MASK) == DDS_KIND_TOPIC)
DDS_EXPORT dds_return_t
dds_get_name(
        _In_ dds_entity_t topic,
        _Out_writes_z_(size) char *name,
        _In_ size_t size)
{
    dds_topic *t;
    dds_return_t ret;
    dds__retcode_t rc;

    DDS_REPORT_STACK();

    if(size <= 0){
        ret = DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER, "Argument size is smaller than 0");
        goto fail;
    }
    if(name == NULL){
        ret = DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER, "Argument name is NULL");
        goto fail;
    }
    name[0] = '\0';
    rc = dds_topic_lock(topic, &t);
    if (rc == DDS_RETCODE_OK) {
        (void)snprintf(name, size, "%s", t->m_stopic->name);
        dds_topic_unlock(t);
        ret = DDS_RETCODE_OK;
    } else {
        ret = DDS_ERRNO(rc, "Error occurred on locking topic");
        goto fail;
    }
fail:
    DDS_REPORT_FLUSH(ret != DDS_RETCODE_OK);
    return ret;
}

_Pre_satisfies_((topic & DDS_ENTITY_KIND_MASK) == DDS_KIND_TOPIC)
DDS_EXPORT dds_return_t
dds_get_type_name(
        _In_ dds_entity_t topic,
        _Out_writes_z_(size) char *name,
        _In_ size_t size)
{
    dds_topic *t;
    dds__retcode_t rc;
    dds_return_t ret;

    DDS_REPORT_STACK();

    if(size <= 0){
        ret = DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER, "Argument size is smaller than 0");
        goto fail;
    }
    if(name == NULL){
        ret = DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER, "Argument name is NULL");
        goto fail;
    }
    name[0] = '\0';
    rc = dds_topic_lock(topic, &t);
    if (rc != DDS_RETCODE_OK) {
        ret = DDS_ERRNO(rc, "Error occurred on locking topic");
        goto fail;
    }
    (void)snprintf(name, size, "%s", t->m_stopic->typename);
    dds_topic_unlock(t);
    ret = DDS_RETCODE_OK;
fail:
    DDS_REPORT_FLUSH(ret != DDS_RETCODE_OK);
    return ret;
}
_Pre_satisfies_((topic & DDS_ENTITY_KIND_MASK) == DDS_KIND_TOPIC)
dds_return_t
dds_get_inconsistent_topic_status(
        _In_ dds_entity_t topic,
        _Out_opt_ dds_inconsistent_topic_status_t *status)
{
    dds__retcode_t rc;
    dds_topic *t;
    dds_return_t ret = DDS_RETCODE_OK;

    DDS_REPORT_STACK();

    rc = dds_topic_lock(topic, &t);
    if (rc != DDS_RETCODE_OK) {
        ret = DDS_ERRNO(rc, "Error occurred on locking topic");
        goto fail;
    }
    /* status = NULL, application do not need the status, but reset the counter & triggered bit */
    if (status) {
        *status = t->m_inconsistent_topic_status;
    }
    if (((dds_entity*)t)->m_status_enable & DDS_INCONSISTENT_TOPIC_STATUS) {
        t->m_inconsistent_topic_status.total_count_change = 0;
        dds_entity_status_reset(t, DDS_INCONSISTENT_TOPIC_STATUS);
    }
    dds_topic_unlock(t);
fail:
    DDS_REPORT_FLUSH(ret != DDS_RETCODE_OK);
    return ret;
}
