/*
 * Copyright(c) 2006 to 2019 ADLINK Technology Limited and others
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

#include "cyclonedds/ddsrt/process.h"
#include "cyclonedds/ddsrt/heap.h"
#include "dds__init.h"
#include "cyclonedds/ddsc/dds_rhc.h"
#include "dds__domain.h"
#include "dds__builtin.h"
#include "dds__whc_builtintopic.h"
#include "dds__entity.h"
#include "cyclonedds/ddsi/ddsi_iid.h"
#include "cyclonedds/ddsi/ddsi_tkmap.h"
#include "cyclonedds/ddsi/ddsi_serdata.h"
#include "cyclonedds/ddsi/ddsi_threadmon.h"
#include "cyclonedds/ddsi/q_entity.h"
#include "cyclonedds/ddsi/q_config.h"
#include "cyclonedds/ddsi/q_gc.h"
#include "cyclonedds/ddsi/q_globals.h"

static dds_return_t dds_domain_free (dds_entity *vdomain);

const struct dds_entity_deriver dds_entity_deriver_domain = {
  .interrupt = dds_entity_deriver_dummy_interrupt,
  .close = dds_entity_deriver_dummy_close,
  .delete = dds_domain_free,
  .set_qos = dds_entity_deriver_dummy_set_qos,
  .validate_status = dds_entity_deriver_dummy_validate_status
};

static int dds_domain_compare (const void *va, const void *vb)
{
  const dds_domainid_t *a = va;
  const dds_domainid_t *b = vb;
  return (*a == *b) ? 0 : (*a < *b) ? -1 : 1;
}

static const ddsrt_avl_treedef_t dds_domaintree_def = DDSRT_AVL_TREEDEF_INITIALIZER (
  offsetof (dds_domain, m_node), offsetof (dds_domain, m_id), dds_domain_compare, 0);

static dds_return_t dds_domain_init (dds_domain *domain, dds_domainid_t domain_id, const char *config)
{
  dds_return_t ret = DDS_RETCODE_OK;
  uint32_t len;
  dds_entity_t domain_handle;

  if ((domain_handle = dds_entity_init (&domain->m_entity, &dds_global.m_entity, DDS_KIND_DOMAIN, NULL, NULL, 0)) < 0)
    return domain_handle;
  domain->m_entity.m_domain = domain;
  domain->m_entity.m_flags |= DDS_ENTITY_IMPLICIT;
  domain->m_entity.m_iid = ddsi_iid_gen ();

  domain->gv.tstart = now ();
  ddsrt_avl_init (&dds_topictree_def, &domain->m_topics);

  /* | domain_id | domain id in config | result
     +-----------+---------------------+----------
     | DEFAULT   | any (or absent)     | 0
     | DEFAULT   | n                   | n
     | n         | any (or absent)     | n
     | n         | m = n               | n
     | n         | m /= n              | n, entire config ignored

     Config models:
     1: <CycloneDDS>
          <Domain id="X">...</Domain>
          <Domain .../>
        </CycloneDDS>
        where ... is all that can today be set in children of CycloneDDS
        with the exception of the id
     2: <CycloneDDS>
          <Domain><Id>X</Id></Domain>
          ...
        </CycloneDDS>
        legacy form, domain id must be the first element in the file with
        a value (if nothing has been set previously, it a warning is good
        enough) */

  domain->cfgst = config_init (config, &domain->gv.config, domain_id);
  if (domain->cfgst == NULL)
  {
    DDS_ILOG (DDS_LC_CONFIG, domain_id, "Failed to parse configuration\n");
    ret = DDS_RETCODE_ERROR;
    goto fail_config;
  }

  assert (domain_id == DDS_DOMAIN_DEFAULT || domain_id == domain->gv.config.domainId);
  domain->m_id = domain->gv.config.domainId;

  if (rtps_config_prep (&domain->gv, domain->cfgst) != 0)
  {
    DDS_ILOG (DDS_LC_CONFIG, domain->m_id, "Failed to configure RTPS\n");
    ret = DDS_RETCODE_ERROR;
    goto fail_rtps_config;
  }

  if (rtps_init (&domain->gv) < 0)
  {
    DDS_ILOG (DDS_LC_CONFIG, domain->m_id, "Failed to initialize RTPS\n");
    ret = DDS_RETCODE_ERROR;
    goto fail_rtps_init;
  }

  /* Start monitoring the liveliness of threads if this is the first
     domain to configured to do so. */
  if (domain->gv.config.liveliness_monitoring)
  {
    if (dds_global.threadmon_count++ == 0)
    {
      /* FIXME: configure settings */
      dds_global.threadmon = ddsi_threadmon_new (DDS_MSECS (333), true);
      if (dds_global.threadmon == NULL)
      {
        DDS_ILOG (DDS_LC_CONFIG, domain->m_id, "Failed to create a thread liveliness monitor\n");
        ret = DDS_RETCODE_OUT_OF_RESOURCES;
        goto fail_threadmon_new;
      }
      /* FIXME: thread properties */
      if (ddsi_threadmon_start (dds_global.threadmon, "threadmon") < 0)
      {
        DDS_ILOG (DDS_LC_ERROR, domain->m_id, "Failed to start the thread liveliness monitor\n");
        ret = DDS_RETCODE_ERROR;
        goto fail_threadmon_start;
      }
    }
  }

  dds__builtin_init (domain);

  /* Set additional default participant properties */

  char progname[50] = "UNKNOWN"; /* FIXME: once retrieving process names is back in */
  char hostname[64];
  domain->gv.default_local_plist_pp.process_id = (unsigned) ddsrt_getpid();
  domain->gv.default_local_plist_pp.present |= PP_PRISMTECH_PROCESS_ID;
  domain->gv.default_local_plist_pp.exec_name = dds_string_alloc(32);
  (void) snprintf (domain->gv.default_local_plist_pp.exec_name, 32, "CycloneDDS: %u", domain->gv.default_local_plist_pp.process_id);
  len = (uint32_t) (13 + strlen (domain->gv.default_local_plist_pp.exec_name));
  domain->gv.default_local_plist_pp.present |= PP_PRISMTECH_EXEC_NAME;
  if (ddsrt_gethostname (hostname, sizeof (hostname)) == DDS_RETCODE_OK)
  {
    domain->gv.default_local_plist_pp.node_name = dds_string_dup (hostname);
    domain->gv.default_local_plist_pp.present |= PP_PRISMTECH_NODE_NAME;
  }
  domain->gv.default_local_plist_pp.entity_name = dds_alloc (len);
  (void) snprintf (domain->gv.default_local_plist_pp.entity_name, len, "%s<%u>", progname, domain->gv.default_local_plist_pp.process_id);
  domain->gv.default_local_plist_pp.present |= PP_ENTITY_NAME;

  if (rtps_start (&domain->gv) < 0)
  {
    DDS_ILOG (DDS_LC_CONFIG, domain->m_id, "Failed to start RTPS\n");
    ret = DDS_RETCODE_ERROR;
    goto fail_rtps_start;
  }

  if (domain->gv.config.liveliness_monitoring)
    ddsi_threadmon_register_domain (dds_global.threadmon, &domain->gv);
  dds_entity_init_complete (&domain->m_entity);
  return DDS_RETCODE_OK;

fail_rtps_start:
  if (domain->gv.config.liveliness_monitoring && dds_global.threadmon_count == 1)
    ddsi_threadmon_stop (dds_global.threadmon);
fail_threadmon_start:
  if (domain->gv.config.liveliness_monitoring && --dds_global.threadmon_count == 0)
  {
    ddsi_threadmon_free (dds_global.threadmon);
    dds_global.threadmon = NULL;
  }
fail_threadmon_new:
  rtps_fini (&domain->gv);
fail_rtps_init:
fail_rtps_config:
  config_fini (domain->cfgst);
fail_config:
  dds_handle_delete (&domain->m_entity.m_hdllink);
  return ret;
}

dds_domain *dds_domain_find_locked (dds_domainid_t id)
{
  return ddsrt_avl_lookup (&dds_domaintree_def, &dds_global.m_domains, &id);
}

dds_return_t dds_domain_create_internal (dds_domain **domain_out, dds_domainid_t id, bool use_existing, const char *config)
{
  struct dds_domain *dom;
  dds_return_t ret;

  /* FIXME: should perhaps lock parent object just like everywhere */
  ddsrt_mutex_lock (&dds_global.m_mutex);
 retry:
  if (id != DDS_DOMAIN_DEFAULT)
  {
    if ((dom = dds_domain_find_locked (id)) == NULL)
      ret = DDS_RETCODE_NOT_FOUND;
    else
      ret = DDS_RETCODE_OK;
  }
  else
  {
    if ((dom = ddsrt_avl_find_min (&dds_domaintree_def, &dds_global.m_domains)) != NULL)
      ret = DDS_RETCODE_OK;
    else
      ret = DDS_RETCODE_NOT_FOUND;
  }

  switch (ret)
  {
    case DDS_RETCODE_OK:
      if (!use_existing)
      {
        ret = DDS_RETCODE_PRECONDITION_NOT_MET;
        break;
      }
      ddsrt_mutex_lock (&dom->m_entity.m_mutex);
      if (dds_handle_is_closed (&dom->m_entity.m_hdllink))
      {
        ddsrt_mutex_unlock (&dom->m_entity.m_mutex);
        ddsrt_cond_wait (&dds_global.m_cond, &dds_global.m_mutex);
        goto retry;
      }
      else
      {
        dds_entity_add_ref_locked (&dom->m_entity);
        ddsrt_mutex_unlock (&dom->m_entity.m_mutex);
        *domain_out = dom;
      }
      break;
    case DDS_RETCODE_NOT_FOUND:
      dom = dds_alloc (sizeof (*dom));
      if ((ret = dds_domain_init (dom, id, config)) < 0)
        dds_free (dom);
      else
      {
        ddsrt_mutex_lock (&dom->m_entity.m_mutex);
        ddsrt_avl_insert (&dds_domaintree_def, &dds_global.m_domains, dom);
        dds_entity_register_child (&dds_global.m_entity, &dom->m_entity);
        dds_entity_add_ref_locked (&dom->m_entity);
        ddsrt_mutex_unlock (&dom->m_entity.m_mutex);
        *domain_out = dom;
      }
      break;
  }
  ddsrt_mutex_unlock (&dds_global.m_mutex);
  return ret;
}

dds_return_t dds_create_domain(const dds_domainid_t domain, const char *config)
{
  dds_domain *dom;
  dds_entity_t ret;

  if (domain == DDS_DOMAIN_DEFAULT || config == NULL)
    return DDS_RETCODE_BAD_PARAMETER;

  /* Make sure DDS instance is initialized. */
  if ((ret = dds_init ()) < 0)
    goto err_dds_init;

  if ((ret = dds_domain_create_internal (&dom, domain, false, config)) < 0)
    goto err_domain_create;

  return DDS_RETCODE_OK;

err_domain_create:
  dds_delete_impl_pinned (&dds_global.m_entity, DIS_EXPLICIT);
err_dds_init:
  return ret;
}

static dds_return_t dds_domain_free (dds_entity *vdomain)
{
  struct dds_domain *domain = (struct dds_domain *) vdomain;
  rtps_stop (&domain->gv);
  dds__builtin_fini (domain);

  if (domain->gv.config.liveliness_monitoring)
    ddsi_threadmon_unregister_domain (dds_global.threadmon, &domain->gv);

  rtps_fini (&domain->gv);

  /* tearing down the top-level object has more consequences, so it waits until signalled that all
     domains have been removed */
  ddsrt_mutex_lock (&dds_global.m_mutex);
  if (domain->gv.config.liveliness_monitoring && --dds_global.threadmon_count == 0)
  {
    ddsi_threadmon_stop (dds_global.threadmon);
    ddsi_threadmon_free (dds_global.threadmon);
  }

  ddsrt_avl_delete (&dds_domaintree_def, &dds_global.m_domains, domain);
  dds_entity_final_deinit_before_free (vdomain);
  config_fini (domain->cfgst);
  dds_free (vdomain);
  ddsrt_cond_broadcast (&dds_global.m_cond);
  ddsrt_mutex_unlock (&dds_global.m_mutex);
  return DDS_RETCODE_NO_DATA;
}

#include "dds__entity.h"
static void pushdown_set_batch (struct dds_entity *e, bool enable)
{
  /* e is pinned, no locks held */
  dds_instance_handle_t last_iid = 0;
  struct dds_entity *c;
  ddsrt_mutex_lock (&e->m_mutex);
  while ((c = ddsrt_avl_lookup_succ (&dds_entity_children_td, &e->m_children, &last_iid)) != NULL)
  {
    struct dds_entity *x;
    last_iid = c->m_iid;
    if (dds_entity_pin (c->m_hdllink.hdl, &x) < 0)
      continue;
    assert (x == c);
    ddsrt_mutex_unlock (&e->m_mutex);
    if (c->m_kind == DDS_KIND_PARTICIPANT)
      pushdown_set_batch (c, enable);
    else if (c->m_kind == DDS_KIND_WRITER)
    {
      struct dds_writer *w = (struct dds_writer *) c;
      w->whc_batch = enable;
    }
    ddsrt_mutex_lock (&e->m_mutex);
    dds_entity_unpin (c);
  }
  ddsrt_mutex_unlock (&e->m_mutex);
}

void dds_write_set_batch (bool enable)
{
  /* FIXME: get channels + latency budget working and get rid of this; in the mean time, any ugly hack will do.  */
  struct dds_domain *dom;
  dds_domainid_t next_id = 0;
  if (dds_init () < 0)
    return;
  ddsrt_mutex_lock (&dds_global.m_mutex);
  while ((dom = ddsrt_avl_lookup_succ_eq (&dds_domaintree_def, &dds_global.m_domains, &next_id)) != NULL)
  {
    /* Must be sure that the compiler doesn't reload curr_id from dom->m_id */
    dds_domainid_t curr_id = *((volatile dds_domainid_t *) &dom->m_id);
    next_id = curr_id + 1;
    dom->gv.config.whc_batch = enable;

    dds_instance_handle_t last_iid = 0;
    struct dds_entity *e;
    while (dom && (e = ddsrt_avl_lookup_succ (&dds_entity_children_td, &dom->m_entity.m_children, &last_iid)) != NULL)
    {
      struct dds_entity *x;
      last_iid = e->m_iid;
      if (dds_entity_pin (e->m_hdllink.hdl, &x) < 0)
        continue;
      assert (x == e);
      ddsrt_mutex_unlock (&dds_global.m_mutex);
      pushdown_set_batch (e, enable);
      ddsrt_mutex_lock (&dds_global.m_mutex);
      dds_entity_unpin (e);
      dom = ddsrt_avl_lookup (&dds_domaintree_def, &dds_global.m_domains, &curr_id);
    }
  }
  ddsrt_mutex_unlock (&dds_global.m_mutex);
  dds_delete_impl_pinned (&dds_global.m_entity, DIS_EXPLICIT);
}
