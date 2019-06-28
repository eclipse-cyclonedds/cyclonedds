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

#include "dds/ddsrt/environ.h"
#include "dds/ddsrt/process.h"
#include "dds/ddsrt/heap.h"
#include "dds__init.h"
#include "dds__rhc.h"
#include "dds__domain.h"
#include "dds__builtin.h"
#include "dds__whc_builtintopic.h"
#include "dds/ddsi/ddsi_iid.h"
#include "dds/ddsi/ddsi_tkmap.h"
#include "dds/ddsi/ddsi_serdata.h"
#include "dds/ddsi/ddsi_threadmon.h"
#include "dds/ddsi/q_entity.h"
#include "dds/ddsi/q_config.h"
#include "dds/ddsi/q_gc.h"
#include "dds/ddsi/q_globals.h"
#include "dds/version.h"

static int dds_domain_compare (const void *va, const void *vb)
{
  const dds_domainid_t *a = va;
  const dds_domainid_t *b = vb;
  return (*a == *b) ? 0 : (*a < *b) ? -1 : 1;
}

static const ddsrt_avl_treedef_t dds_domaintree_def = DDSRT_AVL_TREEDEF_INITIALIZER (
  offsetof (dds_domain, m_node), offsetof (dds_domain, m_id), dds_domain_compare, 0);

static dds_return_t dds_domain_init (dds_domain *domain, dds_domainid_t domain_id)
{
  dds_return_t ret = DDS_RETCODE_OK;
  char * uri = NULL;
  uint32_t len;

  domain->m_id = domain_id;
  domain->m_refc = 1;
  ddsrt_avl_init (&dds_topictree_def, &domain->m_topics);

  gv.tstart = now ();

  (void) ddsrt_getenv ("CYCLONEDDS_URI", &uri);
  domain->cfgst = config_init (uri, &config);
  if (domain->cfgst == NULL)
  {
    DDS_LOG (DDS_LC_CONFIG, "Failed to parse configuration XML file %s\n", uri);
    ret = DDS_RETCODE_ERROR;
    goto fail_config;
  }

  /* if a domain id was explicitly given, check & fix up the configuration */
  if (domain_id != DDS_DOMAIN_DEFAULT)
  {
    if (domain_id < 0 || domain_id > 230)
    {
      DDS_ERROR ("requested domain id %"PRId32" is out of range\n", domain_id);
      ret = DDS_RETCODE_ERROR;
      goto fail_config_domainid;
    }
    else if (config.domainId.isdefault)
    {
      config.domainId.value = domain_id;
    }
    else if (domain_id != config.domainId.value)
    {
      DDS_ERROR ("requested domain id %"PRId32" is inconsistent with configured value %"PRId32"\n", domain_id, config.domainId.value);
      ret = DDS_RETCODE_ERROR;
      goto fail_config_domainid;
    }
  }

  /* FIXME: The config.domainId can change internally in DDSI. So, remember what the
   * main configured domain id is. */
  domain->m_id = config.domainId.value;

  if (rtps_config_prep (domain->cfgst) != 0)
  {
    DDS_LOG (DDS_LC_CONFIG, "Failed to configure RTPS\n");
    ret = DDS_RETCODE_ERROR;
    goto fail_rtps_config;
  }

  /* Start monitoring the liveliness of all threads. */
  if (!config.liveliness_monitoring)
    gv.threadmon = NULL;
  else
  {
    gv.threadmon = ddsi_threadmon_new ();
    if (gv.threadmon == NULL)
    {
      DDS_ERROR ("Failed to create a thread monitor\n");
      ret = DDS_RETCODE_OUT_OF_RESOURCES;
      goto fail_threadmon_new;
    }
  }

  if (rtps_init () < 0)
  {
    DDS_LOG (DDS_LC_CONFIG, "Failed to initialize RTPS\n");
    ret = DDS_RETCODE_ERROR;
    goto fail_rtps_init;
  }

  dds__builtin_init (domain);

  if (rtps_start () < 0)
  {
    DDS_LOG (DDS_LC_CONFIG, "Failed to start RTPS\n");
    ret = DDS_RETCODE_ERROR;
    goto fail_rtps_start;
  }

  if (gv.threadmon && ddsi_threadmon_start (gv.threadmon) < 0)
  {
    DDS_ERROR ("Failed to start the servicelease\n");
    ret = DDS_RETCODE_ERROR;
    goto fail_threadmon_start;
  }

  /* Set additional default participant properties */

  char progname[50] = "UNKNOWN"; /* FIXME: once retrieving process names is back in */
  char hostname[64];
  gv.default_plist_pp.process_id = (unsigned) ddsrt_getpid();
  gv.default_plist_pp.present |= PP_PRISMTECH_PROCESS_ID;
  gv.default_plist_pp.exec_name = dds_string_alloc(32);
  (void) snprintf (gv.default_plist_pp.exec_name, 32, "CycloneDDS: %u", gv.default_plist_pp.process_id);
  len = (uint32_t) (13 + strlen (gv.default_plist_pp.exec_name));
  gv.default_plist_pp.present |= PP_PRISMTECH_EXEC_NAME;
  if (ddsrt_gethostname (hostname, sizeof (hostname)) == DDS_RETCODE_OK)
  {
    gv.default_plist_pp.node_name = dds_string_dup (hostname);
    gv.default_plist_pp.present |= PP_PRISMTECH_NODE_NAME;
  }
  gv.default_plist_pp.entity_name = dds_alloc (len);
  (void) snprintf (gv.default_plist_pp.entity_name, len, "%s<%u>", progname, gv.default_plist_pp.process_id);
  gv.default_plist_pp.present |= PP_ENTITY_NAME;

  return DDS_RETCODE_OK;

fail_threadmon_start:
  if (gv.threadmon)
    ddsi_threadmon_stop (gv.threadmon);
  rtps_stop ();
fail_rtps_start:
  rtps_fini ();
fail_rtps_init:
  if (gv.threadmon)
  {
    ddsi_threadmon_free (gv.threadmon);
    gv.threadmon = NULL;
  }
fail_threadmon_new:
  downgrade_main_thread ();
  thread_states_fini();
fail_rtps_config:
fail_config_domainid:
  config_fini (domain->cfgst);
fail_config:
  return ret;
}

static void dds_domain_fini (struct dds_domain *domain)
{
  if (gv.threadmon)
    ddsi_threadmon_stop (gv.threadmon);
  rtps_stop ();
  dds__builtin_fini (domain);
  rtps_fini ();
  if (gv.threadmon)
    ddsi_threadmon_free (gv.threadmon);
  gv.threadmon = NULL;
  config_fini (domain->cfgst);
}

dds_domain *dds_domain_find_locked (dds_domainid_t id)
{
  return ddsrt_avl_lookup (&dds_domaintree_def, &dds_global.m_domains, &id);
}

dds_return_t dds_domain_create (dds_domain **domain_out, dds_domainid_t id)
{
  struct dds_domain *dom = NULL;
  dds_return_t ret;

  if (id != DDS_DOMAIN_DEFAULT && (id < 0 || id > 230))
    return DDS_RETCODE_BAD_PARAMETER;

  ddsrt_mutex_lock (&dds_global.m_mutex);

  /* FIXME: hack around default domain ids, not yet being able to handle multiple domains simultaneously */
  if (id != DDS_DOMAIN_DEFAULT)
  {
    if ((dom = dds_domain_find_locked (id)) == NULL)
      ret = DDS_RETCODE_NOT_FOUND;
    else if (dom->m_id == id)
      ret = DDS_RETCODE_OK;
    else
      ret = DDS_RETCODE_PRECONDITION_NOT_MET;
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
      dom->m_refc++;
      *domain_out = dom;
      break;
    case DDS_RETCODE_NOT_FOUND:
      dom = dds_alloc (sizeof (*dom));
      if ((ret = dds_domain_init (dom, id)) < 0)
        dds_free (dom);
      else
      {
        ddsrt_avl_insert (&dds_domaintree_def, &dds_global.m_domains, dom);
        *domain_out = dom;
      }
      break;
    case DDS_RETCODE_PRECONDITION_NOT_MET:
      DDS_ERROR("Inconsistent domain configuration detected: domain on configuration: %"PRId32", domain %"PRId32"\n", dom->m_id, id);
      break;
  }
  ddsrt_mutex_unlock (&dds_global.m_mutex);
  return ret;
}

void dds_domain_free (dds_domain *domain)
{
  ddsrt_mutex_lock (&dds_global.m_mutex);
  if (--domain->m_refc != 0)
  {
    //fprintf(stderr, "domain_free %d %p refcount > 0\n", (int)domain->m_id, domain);
    ddsrt_mutex_unlock (&dds_global.m_mutex);
  }
  else
  {
    //fprintf(stderr, "domain_free %d %p\n", (int)domain->m_id, domain);
    ddsrt_avl_delete (&dds_domaintree_def, &dds_global.m_domains, domain);
    ddsrt_mutex_unlock (&dds_global.m_mutex);
    dds_domain_fini (domain);
    dds_free (domain);
  }
}
