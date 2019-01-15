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
#include "ddsi/ddsi_sertopic.h"
#include "ddsi/q_ddsi_discovery.h"
#include "os/os_atomics.h"
#include "ddsi/ddsi_iid.h"

DECL_ENTITY_LOCK_UNLOCK(extern inline, dds_topic)

#define DDS_TOPIC_STATUS_MASK                                    \
                        DDS_INCONSISTENT_TOPIC_STATUS

const ut_avlTreedef_t dds_topictree_def = UT_AVL_TREEDEF_INITIALIZER_INDKEY
(
  offsetof (struct ddsi_sertopic, avlnode),
  offsetof (struct ddsi_sertopic, name_typename),
  (int (*) (const void *, const void *)) strcmp,
  0
);

/* builtin-topic handles */
const dds_entity_t DDS_BUILTIN_TOPIC_DCPSPARTICIPANT = (DDS_KIND_INTERNAL + 1);
const dds_entity_t DDS_BUILTIN_TOPIC_DCPSTOPIC = (DDS_KIND_INTERNAL + 2);
const dds_entity_t DDS_BUILTIN_TOPIC_DCPSPUBLICATION = (DDS_KIND_INTERNAL + 3);
const dds_entity_t DDS_BUILTIN_TOPIC_DCPSSUBSCRIPTION = (DDS_KIND_INTERNAL + 4);

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
     *  |     '/' got added for ROS2
     */
    assert(name);
    if ((name[0] != '\0') && (!isdigit((unsigned char)name[0]))) {
        while (isalnum((unsigned char)*name) || (*name == '_') || (*name == '/')) {
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
    dds_return_t ret = DDS_RETCODE_OK;

    if (mask & ~(DDS_TOPIC_STATUS_MASK)) {
        DDS_ERROR("Argument mask is invalid\n");
        ret = DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER);
    }

    return ret;
}

/*
  Topic status change callback handler. Supports INCONSISTENT_TOPIC
  status (only defined status on a topic).
*/

static void dds_topic_status_cb (struct dds_topic *tp)
{
  struct dds_listener const * const lst = &tp->m_entity.m_listener;

  os_mutexLock (&tp->m_entity.m_observers_lock);
  while (tp->m_entity.m_cb_count > 0)
    os_condWait (&tp->m_entity.m_observers_cond, &tp->m_entity.m_observers_lock);
  tp->m_entity.m_cb_count++;

  tp->m_inconsistent_topic_status.total_count++;
  tp->m_inconsistent_topic_status.total_count_change++;
  if (lst->on_inconsistent_topic)
  {
    os_mutexUnlock (&tp->m_entity.m_observers_lock);
    dds_entity_invoke_listener(&tp->m_entity, DDS_INCONSISTENT_TOPIC_STATUS_ID, &tp->m_inconsistent_topic_status);
    os_mutexLock (&tp->m_entity.m_observers_lock);
    tp->m_inconsistent_topic_status.total_count_change = 0;
  }

  dds_entity_status_set(&tp->m_entity, DDS_INCONSISTENT_TOPIC_STATUS);
  tp->m_entity.m_cb_count--;
  os_condBroadcast (&tp->m_entity.m_observers_cond);
  os_mutexUnlock (&tp->m_entity.m_observers_lock);
}

struct ddsi_sertopic *
dds_topic_lookup_locked(
        dds_domain *domain,
        const char *name)
{
    struct ddsi_sertopic *st = NULL;
    ut_avlIter_t iter;

    assert (domain);
    assert (name);

    st = ut_avlIterFirst (&dds_topictree_def, &domain->m_topics, &iter);
    while (st) {
        if (strcmp (st->name, name) == 0) {
            break;
        }
        st = ut_avlIterNext (&iter);
    }
    return st;
}

struct ddsi_sertopic *
dds_topic_lookup(
        dds_domain *domain,
        const char *name)
{
    struct ddsi_sertopic *st;
    os_mutexLock (&dds_global.m_mutex);
    st = dds_topic_lookup_locked(domain, name);
    os_mutexUnlock (&dds_global.m_mutex);
    return st;
}

void
dds_topic_free(
        dds_domainid_t domainid,
        struct ddsi_sertopic *st)
{
    dds_domain *domain;

    assert (st);

    os_mutexLock (&dds_global.m_mutex);
    domain = ut_avlLookup (&dds_domaintree_def, &dds_global.m_domains, &domainid);
    if (domain != NULL) {
        ut_avlDelete (&dds_topictree_def, &domain->m_topics, st);
    }
    os_mutexUnlock (&dds_global.m_mutex);
    st->status_cb_entity = NULL;
    ddsi_sertopic_unref (st);
}

static void
dds_topic_add_locked(
        dds_domainid_t id,
        struct ddsi_sertopic *st)
{
    dds_domain * dom;
    dom = dds_domain_find_locked (id);
    assert (dom);
    ut_avlInsert (&dds_topictree_def, &dom->m_topics, st);
}

_Pre_satisfies_((participant & DDS_ENTITY_KIND_MASK) == DDS_KIND_PARTICIPANT)
DDS_EXPORT dds_entity_t
dds_find_topic(
        _In_ dds_entity_t participant,
        _In_z_ const char *name)
{
    dds_entity_t tp;
    dds_entity *p = NULL;
    struct ddsi_sertopic *st;
    dds__retcode_t rc;

    if (name) {
        rc = dds_entity_lock(participant, DDS_KIND_PARTICIPANT, &p);
        if (rc == DDS_RETCODE_OK) {
            os_mutexLock (&dds_global.m_mutex);
            st = dds_topic_lookup_locked (p->m_domain, name);
            if (st) {
                dds_entity_add_ref (&st->status_cb_entity->m_entity);
                tp = st->status_cb_entity->m_entity.m_hdl;
            } else {
                DDS_ERROR("Topic is not being created yet\n");
                tp = DDS_ERRNO(DDS_RETCODE_PRECONDITION_NOT_MET);
            }
            os_mutexUnlock (&dds_global.m_mutex);
            dds_entity_unlock(p);
        } else {
            DDS_ERROR("Error occurred on locking entity\n");
            tp = DDS_ERRNO(rc);
        }
    } else {
        DDS_ERROR("Argument name is not valid\n");
        tp = DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER);
    }

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
        DDS_ERROR("Argument QoS is not valid\n");
        ret = DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER);
    }
    if ((qos->present & QP_GROUP_DATA) && !validate_octetseq (&qos->group_data)) {
        DDS_ERROR("Group data QoS policy is inconsistent and caused an error\n");
        ret = DDS_ERRNO(DDS_RETCODE_INCONSISTENT_POLICY);
    }
    if ((qos->present & QP_DURABILITY_SERVICE) && (validate_durability_service_qospolicy(&qos->durability_service) != 0)) {
        DDS_ERROR("Durability service QoS policy is inconsistent and caused an error\n");
        ret = DDS_ERRNO(DDS_RETCODE_INCONSISTENT_POLICY);
    }
    if ((qos->present & QP_LIFESPAN) && (validate_duration(&qos->lifespan.duration) != 0)) {
        DDS_ERROR("Lifespan QoS policy is inconsistent and caused an error\n");
        ret = DDS_ERRNO(DDS_RETCODE_INCONSISTENT_POLICY);
    }
    if (qos->present & QP_HISTORY && (qos->present & QP_RESOURCE_LIMITS) && (validate_history_and_resource_limits(&qos->history, &qos->resource_limits) != 0)) {
        DDS_ERROR("Lifespan QoS policy is inconsistent and caused an error\n");
        ret = DDS_ERRNO(DDS_RETCODE_INCONSISTENT_POLICY);
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
    (void)e;
    if (ret == DDS_RETCODE_OK) {
        if (enabled) {
            /* TODO: CHAM-95: DDSI does not support changing QoS policies. */
            DDS_ERROR("Changing the topic QoS is not supported\n");
            ret = DDS_ERRNO(DDS_RETCODE_UNSUPPORTED);
        }
    }
    return ret;
}

static bool dupdef_qos_ok(const dds_qos_t *qos, const struct ddsi_sertopic *st)
{
    if ((qos == NULL) != (st->status_cb_entity->m_entity.m_qos == NULL)) {
        return false;
    } else if (qos == NULL) {
        return true;
    } else {
        return dds_qos_equal(st->status_cb_entity->m_entity.m_qos, qos);
    }
}

static bool sertopic_equivalent (const struct ddsi_sertopic *a, const struct ddsi_sertopic *b)
{
  if (strcmp (a->name_typename, b->name_typename) != 0)
    return false;
  if (a->serdata_basehash != b->serdata_basehash)
    return false;
  if (a->ops != b->ops)
    return false;
  if (a->serdata_ops != b->serdata_ops)
    return false;
  return true;
}

_Pre_satisfies_((participant & DDS_ENTITY_KIND_MASK) == DDS_KIND_PARTICIPANT)
DDS_EXPORT dds_entity_t
dds_create_topic_arbitrary (
        _In_ dds_entity_t participant,
        _In_ struct ddsi_sertopic *sertopic,
        _In_z_ const char *name,
        _In_opt_ const dds_qos_t *qos,
        _In_opt_ const dds_listener_t *listener,
        _In_opt_ const nn_plist_t *sedp_plist)
{
    struct ddsi_sertopic *stgeneric;
    dds__retcode_t rc;
    dds_entity *par;
    dds_topic *top;
    dds_qos_t *new_qos = NULL;
    dds_entity_t hdl;
    struct participant *ddsi_pp;
    struct thread_state1 *const thr = lookup_thread_state ();
    const bool asleep = !vtime_awake_p (thr->vtime);

    if (sertopic == NULL){
        DDS_ERROR("Topic description is NULL\n");
        hdl = DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER);
        goto bad_param_err;
    }

    if (name == NULL) {
        DDS_ERROR("Topic name is NULL\n");
        hdl = DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER);
        goto bad_param_err;
    }

    if (!is_valid_name(name)) {
        DDS_ERROR("Topic name contains characters that are not allowed\n");
        hdl = DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER);
        goto bad_param_err;
    }

    rc = dds_entity_lock(participant, DDS_KIND_PARTICIPANT, &par);
    if (rc != DDS_RETCODE_OK) {
        DDS_ERROR("Error occurred on locking entity\n");
        hdl = DDS_ERRNO(rc);
        goto lock_err;
    }

    /* Validate qos */
    if (qos) {
        hdl = dds_topic_qos_validate (qos, false);
        if (hdl != DDS_RETCODE_OK) {
            goto qos_err;
        }
    }

    /* FIXME: I find it weird that qos may be NULL in the entity */

    /* Check if topic already exists with same name */
    os_mutexLock (&dds_global.m_mutex);
    if ((stgeneric = dds_topic_lookup_locked (par->m_domain, name)) != NULL) {
        if (!sertopic_equivalent (stgeneric,sertopic)) {
            /* FIXME: should copy the type, perhaps? but then the pointers will no longer be the same */
            DDS_ERROR("Create topic with mismatching type\n");
            hdl = DDS_ERRNO(DDS_RETCODE_PRECONDITION_NOT_MET);
        } else if (!dupdef_qos_ok(qos, stgeneric)) {
            /* FIXME: should copy the type, perhaps? but then the pointers will no longer be the same */
            DDS_ERROR("Create topic with mismatching qos\n");
            hdl = DDS_ERRNO(DDS_RETCODE_INCONSISTENT_POLICY);
        } else {
            dds_entity_add_ref (&stgeneric->status_cb_entity->m_entity);
            hdl = stgeneric->status_cb_entity->m_entity.m_hdl;
        }
        os_mutexUnlock (&dds_global.m_mutex);
    } else {
        if (qos) {
            new_qos = dds_create_qos();
            /* Only returns failure when one of the qos args is NULL, which
             * is not the case here. */
            (void)dds_copy_qos(new_qos, qos);
        }

        /* Create topic */
        top = dds_alloc (sizeof (*top));
        hdl = dds_entity_init (&top->m_entity, par, DDS_KIND_TOPIC, new_qos, listener, DDS_TOPIC_STATUS_MASK);
        top->m_entity.m_deriver.delete = dds_topic_delete;
        top->m_entity.m_deriver.set_qos = dds_topic_qos_set;
        top->m_entity.m_deriver.validate_status = dds_topic_status_validate;
        top->m_stopic = ddsi_sertopic_ref (sertopic);
        sertopic->status_cb_entity = top;

        /* Add topic to extent */
        dds_topic_add_locked (par->m_domainid, sertopic);
        os_mutexUnlock (&dds_global.m_mutex);

        /* Publish Topic */
        if (asleep) {
            thread_state_awake (thr);
        }
        ddsi_pp = ephash_lookup_participant_guid (&par->m_guid);
        assert (ddsi_pp);
        if (sedp_plist) {
            sedp_write_topic (ddsi_pp, sedp_plist);
        }
        if (asleep) {
            thread_state_asleep (thr);
        }
    }

qos_err:
    dds_entity_unlock(par);
lock_err:
bad_param_err:
    return hdl;
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
    char *key = NULL;
    struct ddsi_sertopic_default *st;
    const char *typename;
    dds_qos_t *new_qos = NULL;
    nn_plist_t plist;
    dds_entity_t hdl;
    uint32_t index;
    size_t keysz;

    if (desc == NULL){
        DDS_ERROR("Topic description is NULL");
        hdl = DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER);
        goto bad_param_err;
    }

    if (name == NULL) {
        DDS_ERROR("Topic name is NULL");
        hdl = DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER);
        goto bad_param_err;
    }

    if (!is_valid_name(name)) {
        DDS_ERROR("Topic name contains characters that are not allowed.");
        hdl = DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER);
        goto bad_param_err;
    }

    typename = desc->m_typename;
    keysz = strlen (name) + strlen (typename) + 2;
    key = (char*) dds_alloc (keysz);
    (void) snprintf(key, keysz, "%s/%s", name, typename);

    st = dds_alloc (sizeof (*st));

    os_atomic_st32 (&st->c.refc, 1);
    st->c.iid = ddsi_iid_gen ();
    st->c.status_cb = dds_topic_status_cb;
    st->c.status_cb_entity = NULL; /* set by dds_create_topic_arbitrary */
    st->c.name_typename = key;
    st->c.name = dds_string_dup (name);
    st->c.typename = dds_string_dup (typename);
    st->c.ops = &ddsi_sertopic_ops_default;
    st->c.serdata_ops = desc->m_nkeys ? &ddsi_serdata_ops_cdr : &ddsi_serdata_ops_cdr_nokey;
    st->c.serdata_basehash = ddsi_sertopic_compute_serdata_basehash (st->c.serdata_ops);
    st->native_encoding_identifier = (PLATFORM_IS_LITTLE_ENDIAN ? CDR_LE : CDR_BE);

    st->type = (void*) desc;
    st->nkeys = desc->m_nkeys;
    st->keys = desc->m_keys;

    /* Check if topic cannot be optimised (memcpy marshal) */
    if ((desc->m_flagset & DDS_TOPIC_NO_OPTIMIZE) == 0) {
        st->opt_size = dds_stream_check_optimize (desc);
    }

    nn_plist_init_empty (&plist);
    if (new_qos) {
        dds_merge_qos (&plist.qos, new_qos);
    }

    /* Set Topic meta data (for SEDP publication) */
    plist.qos.topic_name = dds_string_dup (st->c.name);
    plist.qos.type_name = dds_string_dup (st->c.typename);
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

    hdl = dds_create_topic_arbitrary(participant, &st->c, name, qos, listener, &plist);
    ddsi_sertopic_unref (&st->c);
    nn_plist_fini (&plist);

bad_param_err:
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
            t->filter_fn = *filter;
            t->filter_ctx = *ctx;
        } else {
            *filter = t->filter_fn;
            *ctx = t->filter_ctx;
        }
        dds_topic_unlock(t);
    } else {
        *filter = 0;
        *ctx = NULL;
    }
}

_Pre_satisfies_((topic & DDS_ENTITY_KIND_MASK) == DDS_KIND_TOPIC)
void
dds_set_topic_filter(
        dds_entity_t topic,
        dds_topic_filter_fn filter)
{
    dds_topic_intern_filter_fn chaining = dds_topic_chaining_filter;
    void *realf = (void *)filter;
    dds_topic_mod_filter (topic, &chaining, &realf, true);
}

_Pre_satisfies_((topic & DDS_ENTITY_KIND_MASK) == DDS_KIND_TOPIC)
void
dds_topic_set_filter(
        dds_entity_t topic,
        dds_topic_filter_fn filter)
{
    dds_set_topic_filter(topic, filter);
}

_Pre_satisfies_((topic & DDS_ENTITY_KIND_MASK) == DDS_KIND_TOPIC)
dds_topic_filter_fn
dds_get_topic_filter(
        dds_entity_t topic)
{
    dds_topic_intern_filter_fn filter;
    void *ctx;
    dds_topic_mod_filter (topic, &filter, &ctx, false);
    return (filter == dds_topic_chaining_filter) ? (dds_topic_filter_fn)ctx : 0;
}

_Pre_satisfies_((topic & DDS_ENTITY_KIND_MASK) == DDS_KIND_TOPIC)
dds_topic_filter_fn
dds_topic_get_filter(
        dds_entity_t topic)
{
    return dds_get_topic_filter(topic);
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
  return (filter == dds_topic_chaining_filter) ? 0 : filter;
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

    if(size <= 0){
        DDS_ERROR("Argument size is smaller than 0\n");
        ret = DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER);
        goto fail;
    }
    if(name == NULL){
        DDS_ERROR("Argument name is NULL\n");
        ret = DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER);
        goto fail;
    }
    name[0] = '\0';
    rc = dds_topic_lock(topic, &t);
    if (rc == DDS_RETCODE_OK) {
        (void)snprintf(name, size, "%s", t->m_stopic->name);
        dds_topic_unlock(t);
        ret = DDS_RETCODE_OK;
    } else {
        DDS_ERROR("Error occurred on locking topic\n");
        ret = DDS_ERRNO(rc);
        goto fail;
    }
fail:
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

    if(size <= 0){
        DDS_ERROR("Argument size is smaller than 0\n");
        ret = DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER);
        goto fail;
    }
    if(name == NULL){
        DDS_ERROR("Argument name is NULL\n");
        ret = DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER);
        goto fail;
    }
    name[0] = '\0';
    rc = dds_topic_lock(topic, &t);
    if (rc != DDS_RETCODE_OK) {
        DDS_ERROR("Error occurred on locking topic\n");
        ret = DDS_ERRNO(rc);
        goto fail;
    }
    (void)snprintf(name, size, "%s", t->m_stopic->typename);
    dds_topic_unlock(t);
    ret = DDS_RETCODE_OK;
fail:
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

    rc = dds_topic_lock(topic, &t);
    if (rc != DDS_RETCODE_OK) {
        DDS_ERROR("Error occurred on locking topic\n");
        ret = DDS_ERRNO(rc);
        goto fail;
    }
    /* status = NULL, application do not need the status, but reset the counter & triggered bit */
    if (status) {
        *status = t->m_inconsistent_topic_status;
    }
    if (t->m_entity.m_status_enable & DDS_INCONSISTENT_TOPIC_STATUS) {
        t->m_inconsistent_topic_status.total_count_change = 0;
        dds_entity_status_reset(&t->m_entity, DDS_INCONSISTENT_TOPIC_STATUS);
    }
    dds_topic_unlock(t);
fail:
    return ret;
}
