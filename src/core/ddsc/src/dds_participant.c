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

#include "dds/ddsrt/cdtors.h"
#include "dds/ddsi/q_entity.h"
#include "dds/ddsi/q_thread.h"
#include "dds/ddsi/q_config.h"
#include "dds__init.h"
#include "dds__qos.h"
#include "dds__domain.h"
#include "dds__participant.h"
#include "dds__err.h"
#include "dds__builtin.h"

DECL_ENTITY_LOCK_UNLOCK(extern inline, dds_participant)

#define DDS_PARTICIPANT_STATUS_MASK    0u

/* List of created participants */

static dds_entity * dds_pp_head = NULL;

static dds_return_t
dds_participant_status_validate(
    uint32_t mask)
{
    dds_return_t ret = DDS_RETCODE_OK;

    if (mask & ~(DDS_PARTICIPANT_STATUS_MASK)) {
        ret = DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER);
    }

    return ret;
}

static dds_return_t
dds_participant_delete(
    dds_entity *e)
{
    dds_entity *prev = NULL;
    dds_entity *iter;

    assert(e);
    assert(dds_entity_kind(e) == DDS_KIND_PARTICIPANT);

    thread_state_awake (lookup_thread_state ());

    dds_domain_free (e->m_domain);

    ddsrt_mutex_lock (&dds_global.m_mutex);
    iter = dds_pp_head;
    while (iter) {
        if (iter == e) {
            if (prev) {
                prev->m_next = iter->m_next;
            } else {
                  dds_pp_head = iter->m_next;
            }
            break;
        }
        prev = iter;
        iter = iter->m_next;
    }
    ddsrt_mutex_unlock (&dds_global.m_mutex);

    assert (iter);

    thread_state_asleep (lookup_thread_state ());

    /* Every dds_init needs a dds_fini. */
    dds_fini();

    return DDS_RETCODE_OK;
}

static dds_return_t
dds_participant_instance_hdl(
    dds_entity *e,
    dds_instance_handle_t *i)
{
    assert(e);
    assert(i);
    *i = (dds_instance_handle_t)participant_instance_id(&e->m_guid);
    return DDS_RETCODE_OK;
}

static dds_return_t
dds_participant_qos_validate(
    const dds_qos_t *qos,
    bool enabled)
{
    dds_return_t ret = DDS_RETCODE_OK;
    assert(qos);
    (void)enabled;

    /* Check consistency. */
    if ((qos->present & QP_USER_DATA) && !validate_octetseq(&qos->user_data)) {
        DDS_ERROR("User data QoS policy is inconsistent and caused an error\n");
        ret = DDS_ERRNO(DDS_RETCODE_INCONSISTENT_POLICY);
    }
    if ((qos->present & QP_PRISMTECH_ENTITY_FACTORY) && !validate_entityfactory_qospolicy(&qos->entity_factory)) {
        DDS_ERROR("Prismtech entity factory QoS policy is inconsistent and caused an error\n");
        ret = DDS_ERRNO(DDS_RETCODE_INCONSISTENT_POLICY);
    }
    return ret;
}


static dds_return_t
dds_participant_qos_set(
    dds_entity *e,
    const dds_qos_t *qos,
    bool enabled)
{
    dds_return_t ret = dds_participant_qos_validate(qos, enabled);
    (void)e;
    if (ret == DDS_RETCODE_OK) {
        if (enabled) {
            /* TODO: CHAM-95: DDSI does not support changing QoS policies. */
            DDS_ERROR("Changing the participant QoS is not supported\n");
            ret = DDS_ERRNO(DDS_RETCODE_UNSUPPORTED);
        }
    }
    return ret;
}

dds_entity_t
dds_create_participant(
    const dds_domainid_t domain,
    const dds_qos_t *qos,
    const dds_listener_t *listener)
{
    int q_rc;
    dds_return_t ret;
    dds_entity_t e;
    nn_guid_t guid;
    dds_participant * pp;
    nn_plist_t plist;
    dds_qos_t * new_qos = NULL;

    /* Make sure DDS instance is initialized. */
    ret = dds_init(domain);
    if (ret != DDS_RETCODE_OK) {
        e = (dds_entity_t)ret;
        goto fail_dds_init;
    }

    /* Check domain id */
    ret = dds__check_domain (domain);
    if (ret != DDS_RETCODE_OK) {
        e = (dds_entity_t)ret;
        goto fail_domain_check;
    }

    /* Validate qos */
    if (qos) {
        ret = dds_participant_qos_validate (qos, false);
        if (ret != DDS_RETCODE_OK) {
            e = (dds_entity_t)ret;
            goto fail_qos_validation;
        }
        new_qos = dds_create_qos ();
        /* Only returns failure when one of the qos args is NULL, which
         * is not the case here. */
        (void)dds_copy_qos(new_qos, qos);
    } else {
        /* Use default qos. */
        new_qos = dds_create_qos ();
    }

    /* Translate qos */
    nn_plist_init_empty(&plist);
    dds_merge_qos (&plist.qos, new_qos);

    thread_state_awake (lookup_thread_state ());
    q_rc = new_participant (&guid, 0, &plist);
    thread_state_asleep (lookup_thread_state ());
    nn_plist_fini (&plist);
    if (q_rc != 0) {
        DDS_ERROR("Internal error");
        e = DDS_ERRNO(DDS_RETCODE_ERROR);
        goto fail_new_participant;
    }

    pp = dds_alloc (sizeof (*pp));
    e = dds_entity_init (&pp->m_entity, NULL, DDS_KIND_PARTICIPANT, new_qos, listener, DDS_PARTICIPANT_STATUS_MASK);
    if (e < 0) {
        goto fail_entity_init;
    }

    pp->m_entity.m_guid = guid;
    pp->m_entity.m_domain = dds_domain_create (dds_domain_default());
    pp->m_entity.m_domainid = dds_domain_default();
    pp->m_entity.m_deriver.delete = dds_participant_delete;
    pp->m_entity.m_deriver.set_qos = dds_participant_qos_set;
    pp->m_entity.m_deriver.get_instance_hdl = dds_participant_instance_hdl;
    pp->m_entity.m_deriver.validate_status = dds_participant_status_validate;
    pp->m_builtin_subscriber = 0;

    /* Add participant to extent */
    ddsrt_mutex_lock (&dds_global.m_mutex);
    pp->m_entity.m_next = dds_pp_head;
    dds_pp_head = &pp->m_entity;
    ddsrt_mutex_unlock (&dds_global.m_mutex);

    return e;

fail_entity_init:
    dds_free(pp);
fail_new_participant:
    dds_delete_qos(new_qos);
fail_qos_validation:
fail_domain_check:
    dds_fini();
fail_dds_init:
    return e;
}

dds_entity_t
dds_lookup_participant(
    dds_domainid_t domain_id,
    dds_entity_t *participants,
    size_t size)
{
    dds_return_t ret = 0;
    ddsrt_mutex_t *init_mutex;

    /* Be sure the DDS lifecycle resources are initialized. */
    ddsrt_init();
    init_mutex = ddsrt_get_singleton_mutex();

    if ((participants != NULL) && ((size <= 0) || (size >= INT32_MAX))) {
        DDS_ERROR("Array is given, but with invalid size\n");
        ret = DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER);
        goto err;
    }
    if ((participants == NULL) && (size != 0)) {
        DDS_ERROR("Size is given, but no array\n");
        ret = DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER);
        goto err;
    }

    if(participants){
        participants[0] = 0;
    }

    ddsrt_mutex_lock (init_mutex);

    /* Check if dds is intialized. */
    if (dds_global.m_init_count > 0) {
        dds_entity* iter;
        ddsrt_mutex_lock (&dds_global.m_mutex);
        iter = dds_pp_head;
        while (iter) {
            if (iter->m_domainid == domain_id) {
                if ((size_t)ret < size) {
                    participants[ret] = iter->m_hdllink.hdl;
                }
                ret++;
            }
            iter = iter->m_next;
        }
        ddsrt_mutex_unlock (&dds_global.m_mutex);
    }

    ddsrt_mutex_unlock (init_mutex);

err:
    ddsrt_fini();
    return ret;
}
