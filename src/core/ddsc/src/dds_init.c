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
#include <stdlib.h>
#include <assert.h>

#include "dds/ddsrt/cdtors.h"
#include "dds/ddsrt/environ.h"
#include "dds/ddsrt/process.h"
#include "dds/ddsrt/heap.h"
#include "dds__init.h"
#include "dds__rhc.h"
#include "dds__domain.h"
#include "dds__err.h"
#include "dds__builtin.h"
#include "dds__whc_builtintopic.h"
#include "dds/ddsi/ddsi_iid.h"
#include "dds/ddsi/ddsi_tkmap.h"
#include "dds/ddsi/ddsi_serdata.h"
#include "dds/ddsi/ddsi_threadmon.h"
#include "dds/ddsi/q_entity.h"
#include "dds/ddsi/q_config.h"
#include "dds/ddsi/q_gc.h"
#include "dds/version.h"

#define DOMAIN_ID_MIN 0
#define DOMAIN_ID_MAX 230

struct q_globals gv;

dds_globals dds_global = { .m_default_domain = DDS_DOMAIN_DEFAULT };
static struct cfgst * dds_cfgst = NULL;

static void free_via_gc_cb (struct gcreq *gcreq)
{
  void *bs = gcreq->arg;
  gcreq_free (gcreq);
  ddsrt_free (bs);
}

static void free_via_gc (void *bs)
{
  struct gcreq *gcreq = gcreq_new (gv.gcreq_queue, free_via_gc_cb);
  gcreq->arg = bs;
  gcreq_enqueue (gcreq);
}

dds_return_t
dds_init(dds_domainid_t domain)
{
  dds_return_t ret = DDS_RETCODE_OK;
  char * uri = NULL;
  char progname[50] = "UNKNOWN"; /* FIXME: once retrieving process names is back in */
  char hostname[64];
  uint32_t len;
  ddsrt_mutex_t *init_mutex;

  /* Be sure the DDS lifecycle resources are initialized. */
  ddsrt_init();
  init_mutex = ddsrt_get_singleton_mutex();

  ddsrt_mutex_lock(init_mutex);

  dds_global.m_init_count++;
  if (dds_global.m_init_count > 1)
  {
    goto skip;
  }

  gv.tstart = now ();
  gv.exception = false;
  ddsrt_mutex_init (&dds_global.m_mutex);
  thread_states_init_static();

  (void)ddsrt_getenv (DDS_PROJECT_NAME_NOSPACE_CAPS"_URI", &uri);
  dds_cfgst = config_init (uri);
  if (dds_cfgst == NULL)
  {
    DDS_LOG(DDS_LC_CONFIG, "Failed to parse configuration XML file %s\n", uri);
    ret = DDS_ERRNO(DDS_RETCODE_ERROR);
    goto fail_config;
  }

  /* if a domain id was explicitly given, check & fix up the configuration */
  if (domain != DDS_DOMAIN_DEFAULT)
  {
    if (domain < 0 || domain > 230)
    {
      DDS_ERROR("requested domain id %d is out of range\n", domain);
      ret = DDS_ERRNO(DDS_RETCODE_ERROR);
      goto fail_config_domainid;
    }
    else if (config.domainId.isdefault)
    {
      config.domainId.value = domain;
    }
    else if (domain != config.domainId.value)
    {
      DDS_ERROR("requested domain id %d is inconsistent with configured value %d\n", domain, config.domainId.value);
      ret = DDS_ERRNO(DDS_RETCODE_ERROR);
      goto fail_config_domainid;
    }
  }

  /* The config.domainId can change internally in DDSI. So, remember what the
   * main configured domain id is. */
  dds_global.m_default_domain = config.domainId.value;

  if (rtps_config_prep(dds_cfgst) != 0)
  {
    DDS_LOG(DDS_LC_CONFIG, "Failed to configure RTPS\n");
    ret = DDS_ERRNO(DDS_RETCODE_ERROR);
    goto fail_rtps_config;
  }

  upgrade_main_thread();
  ut_avlInit(&dds_domaintree_def, &dds_global.m_domains);

  /* Start monitoring the liveliness of all threads. */
  if (!config.liveliness_monitoring)
    gv.threadmon = NULL;
  else
  {
    gv.threadmon = ddsi_threadmon_new ();
    if (gv.threadmon == NULL)
    {
      DDS_ERROR("Failed to create a thread monitor\n");
      ret = DDS_ERRNO(DDS_RETCODE_OUT_OF_RESOURCES);
      goto fail_threadmon_new;
    }
  }

  if (rtps_init () < 0)
  {
    DDS_LOG(DDS_LC_CONFIG, "Failed to initialize RTPS\n");
    ret = DDS_ERRNO(DDS_RETCODE_ERROR);
    goto fail_rtps_init;
  }

  if (dds_handle_server_init (free_via_gc) != DDS_RETCODE_OK)
  {
    DDS_ERROR("Failed to initialize internal handle server\n");
    ret = DDS_ERRNO(DDS_RETCODE_ERROR);
    goto fail_handleserver;
  }

  dds__builtin_init ();

  if (rtps_start () < 0)
  {
    DDS_LOG(DDS_LC_CONFIG, "Failed to start RTPS\n");
    ret = DDS_ERRNO(DDS_RETCODE_ERROR);
    goto fail_rtps_start;
  }

  if (gv.threadmon && ddsi_threadmon_start(gv.threadmon) < 0)
  {
    DDS_ERROR("Failed to start the servicelease\n");
    ret = DDS_ERRNO(DDS_RETCODE_ERROR);
    goto fail_threadmon_start;
  }

  /* Set additional default participant properties */

  gv.default_plist_pp.process_id = (unsigned)ddsrt_getpid();
  gv.default_plist_pp.present |= PP_PRISMTECH_PROCESS_ID;
  gv.default_plist_pp.exec_name = dds_string_alloc(32);
  (void) snprintf(gv.default_plist_pp.exec_name, 32, "%s: %u", DDS_PROJECT_NAME, gv.default_plist_pp.process_id);
  len = (uint32_t) (13 + strlen(gv.default_plist_pp.exec_name));
  gv.default_plist_pp.present |= PP_PRISMTECH_EXEC_NAME;
  if (ddsrt_gethostname(hostname, sizeof(hostname)) == DDS_RETCODE_OK)
  {
    gv.default_plist_pp.node_name = dds_string_dup(hostname);
    gv.default_plist_pp.present |= PP_PRISMTECH_NODE_NAME;
  }
  gv.default_plist_pp.entity_name = dds_alloc(len);
  (void) snprintf(gv.default_plist_pp.entity_name, len, "%s<%u>", progname,
                  gv.default_plist_pp.process_id);
  gv.default_plist_pp.present |= PP_ENTITY_NAME;

skip:
  ddsrt_mutex_unlock(init_mutex);
  return DDS_RETCODE_OK;

fail_threadmon_start:
  if (gv.threadmon)
    ddsi_threadmon_stop (gv.threadmon);
  dds_handle_server_fini();
fail_handleserver:
  rtps_stop ();
fail_rtps_start:
  dds__builtin_fini ();
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
  dds_global.m_default_domain = DDS_DOMAIN_DEFAULT;
  config_fini (dds_cfgst);
  dds_cfgst = NULL;
fail_config:
  ddsrt_mutex_destroy (&dds_global.m_mutex);
  dds_global.m_init_count--;
  ddsrt_mutex_unlock(init_mutex);
  ddsrt_fini();
  return ret;
}

extern void dds_fini (void)
{
  ddsrt_mutex_t *init_mutex;
  init_mutex = ddsrt_get_singleton_mutex();
  ddsrt_mutex_lock(init_mutex);
  assert(dds_global.m_init_count > 0);
  dds_global.m_init_count--;
  if (dds_global.m_init_count == 0)
  {
    if (gv.threadmon)
      ddsi_threadmon_stop (gv.threadmon);
    dds_handle_server_fini();
    rtps_stop ();
    dds__builtin_fini ();
    rtps_fini ();
    if (gv.threadmon)
      ddsi_threadmon_free (gv.threadmon);
    gv.threadmon = NULL;
    downgrade_main_thread ();
    thread_states_fini ();

    config_fini (dds_cfgst);
    dds_cfgst = NULL;
    ddsrt_mutex_destroy (&dds_global.m_mutex);
    dds_global.m_default_domain = DDS_DOMAIN_DEFAULT;
  }
  ddsrt_mutex_unlock(init_mutex);
  ddsrt_fini();
}

static int dds__init_plugin (void)
{
  if (dds_global.m_dur_init) (dds_global.m_dur_init) ();
  return 0;
}

static void dds__fini_plugin (void)
{
  if (dds_global.m_dur_fini) (dds_global.m_dur_fini) ();
}

void ddsi_plugin_init (void)
{
  ddsi_plugin.init_fn = dds__init_plugin;
  ddsi_plugin.fini_fn = dds__fini_plugin;

  ddsi_plugin.builtintopic_is_visible = dds__builtin_is_visible;
  ddsi_plugin.builtintopic_get_tkmap_entry = dds__builtin_get_tkmap_entry;
  ddsi_plugin.builtintopic_write = dds__builtin_write;

  ddsi_plugin.rhc_plugin.rhc_free_fn = dds_rhc_free;
  ddsi_plugin.rhc_plugin.rhc_store_fn = dds_rhc_store;
  ddsi_plugin.rhc_plugin.rhc_unregister_wr_fn = dds_rhc_unregister_wr;
  ddsi_plugin.rhc_plugin.rhc_relinquish_ownership_fn = dds_rhc_relinquish_ownership;
  ddsi_plugin.rhc_plugin.rhc_set_qos_fn = dds_rhc_set_qos;
}

//provides explicit default domain id.
dds_domainid_t dds_domain_default (void)
{
  return dds_global.m_default_domain;
}

dds_return_t
dds__check_domain(
  dds_domainid_t domain)
{
  dds_return_t ret = DDS_RETCODE_OK;
  /* If domain is default: use configured id. */
  if (domain != DDS_DOMAIN_DEFAULT)
  {
    /* Specific domain has to be the same as the configured domain. */
    if (domain != dds_global.m_default_domain)
    {
      DDS_ERROR("Inconsistent domain configuration detected: domain on "
                "configuration: %d, domain %d\n", dds_global.m_default_domain, domain);
      ret = DDS_ERRNO(DDS_RETCODE_ERROR);
    }
  }
  return ret;
}
