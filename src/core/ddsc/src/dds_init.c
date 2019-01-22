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
#include "os/os.h"
#include "dds__init.h"
#include "dds__rhc.h"
#include "dds__domain.h"
#include "dds__err.h"
#include "dds__builtin.h"
#include "dds__whc_builtintopic.h"
#include "ddsi/ddsi_iid.h"
#include "ddsi/ddsi_tkmap.h"
#include "ddsi/ddsi_serdata.h"
#include "ddsi/q_servicelease.h"
#include "ddsi/q_entity.h"
#include "ddsi/q_config.h"
#include "ddsc/ddsc_project.h"

#ifdef _WRS_KERNEL
char *os_environ[] = { NULL };
#endif

#define DOMAIN_ID_MIN 0
#define DOMAIN_ID_MAX 230

struct q_globals gv;

dds_globals dds_global = { .m_default_domain = DDS_DOMAIN_DEFAULT };
static struct cfgst * dds_cfgst = NULL;

dds_return_t
dds_init(dds_domainid_t domain)
{
  dds_return_t ret = DDS_RETCODE_OK;
  const char * uri;
  char progname[50] = "UNKNOWN"; /* FIXME: once retrieving process names is back in */
  char hostname[64];
  uint32_t len;
  os_mutex *init_mutex;

  /* Be sure the DDS lifecycle resources are initialized. */
  os_osInit();
  init_mutex = os_getSingletonMutex();

  os_mutexLock(init_mutex);

  dds_global.m_init_count++;
  if (dds_global.m_init_count > 1)
  {
    goto skip;
  }

  if (ut_handleserver_init() != UT_HANDLE_OK)
  {
    DDS_ERROR("Failed to initialize internal handle server\n");
    ret = DDS_ERRNO(DDS_RETCODE_ERROR);
    goto fail_handleserver;
  }

  gv.tstart = now ();
  gv.exception = false;
  os_mutexInit (&dds_global.m_mutex);
  thread_states_init_static();

  uri = os_getenv (DDSC_PROJECT_NAME_NOSPACE_CAPS"_URI");
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

  ut_avlInit(&dds_domaintree_def, &dds_global.m_domains);

  /* Start monitoring the liveliness of all threads. */
  if (!config.liveliness_monitoring)
    gv.servicelease = NULL;
  else
  {
    gv.servicelease = nn_servicelease_new(0, 0);
    if (gv.servicelease == NULL)
    {
      DDS_ERROR("Failed to create a servicelease\n");
      ret = DDS_ERRNO(DDS_RETCODE_OUT_OF_RESOURCES);
      goto fail_servicelease_new;
    }
  }

  if (rtps_init() < 0)
  {
    DDS_LOG(DDS_LC_CONFIG, "Failed to initialize RTPS\n");
    ret = DDS_ERRNO(DDS_RETCODE_ERROR);
    goto fail_rtps_init;
  }

  dds__builtin_init ();

  if (gv.servicelease && nn_servicelease_start_renewing(gv.servicelease) < 0)
  {
    DDS_ERROR("Failed to start the servicelease\n");
    ret = DDS_ERRNO(DDS_RETCODE_ERROR);
    goto fail_servicelease_start;
  }

  upgrade_main_thread();

  /* Set additional default participant properties */

  gv.default_plist_pp.process_id = (unsigned)os_getpid();
  gv.default_plist_pp.present |= PP_PRISMTECH_PROCESS_ID;
  gv.default_plist_pp.exec_name = dds_string_alloc(32);
  (void) snprintf(gv.default_plist_pp.exec_name, 32, "%s: %u", DDSC_PROJECT_NAME, gv.default_plist_pp.process_id);
  len = (uint32_t) (13 + strlen(gv.default_plist_pp.exec_name));
  gv.default_plist_pp.present |= PP_PRISMTECH_EXEC_NAME;
  if (os_gethostname(hostname, sizeof(hostname)) == os_resultSuccess)
  {
    gv.default_plist_pp.node_name = dds_string_dup(hostname);
    gv.default_plist_pp.present |= PP_PRISMTECH_NODE_NAME;
  }
  gv.default_plist_pp.entity_name = dds_alloc(len);
  (void) snprintf(gv.default_plist_pp.entity_name, len, "%s<%u>", progname,
                  gv.default_plist_pp.process_id);
  gv.default_plist_pp.present |= PP_ENTITY_NAME;

skip:
  os_mutexUnlock(init_mutex);
  return DDS_RETCODE_OK;

fail_servicelease_start:
  if (gv.servicelease)
    nn_servicelease_stop_renewing (gv.servicelease);
  rtps_stop ();
  rtps_fini ();
fail_rtps_init:
  if (gv.servicelease)
  {
    nn_servicelease_free (gv.servicelease);
    gv.servicelease = NULL;
  }
fail_servicelease_new:
  thread_states_fini();
fail_rtps_config:
fail_config_domainid:
  dds_global.m_default_domain = DDS_DOMAIN_DEFAULT;
  config_fini (dds_cfgst);
  dds_cfgst = NULL;
fail_config:
  os_mutexDestroy (&dds_global.m_mutex);
  ut_handleserver_fini();
fail_handleserver:
  dds_global.m_init_count--;
  os_mutexUnlock(init_mutex);
  os_osExit();
  return ret;
}

extern void dds_fini (void)
{
  os_mutex *init_mutex;
  init_mutex = os_getSingletonMutex();
  os_mutexLock(init_mutex);
  assert(dds_global.m_init_count > 0);
  dds_global.m_init_count--;
  if (dds_global.m_init_count == 0)
  {
    if (gv.servicelease)
      nn_servicelease_stop_renewing (gv.servicelease);
    rtps_stop ();
    dds__builtin_fini ();
    rtps_fini ();
    if (gv.servicelease)
      nn_servicelease_free (gv.servicelease);
    gv.servicelease = NULL;
    downgrade_main_thread ();
    thread_states_fini ();

    config_fini (dds_cfgst);
    dds_cfgst = NULL;
    os_mutexDestroy (&dds_global.m_mutex);
    ut_handleserver_fini();
    dds_global.m_default_domain = DDS_DOMAIN_DEFAULT;
  }
  os_mutexUnlock(init_mutex);
  os_osExit();
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
  ddsi_plugin.rhc_plugin.rhc_fini_fn = dds_rhc_fini;
  ddsi_plugin.rhc_plugin.rhc_store_fn = dds_rhc_store;
  ddsi_plugin.rhc_plugin.rhc_unregister_wr_fn = dds_rhc_unregister_wr;
  ddsi_plugin.rhc_plugin.rhc_relinquish_ownership_fn = dds_rhc_relinquish_ownership;
  ddsi_plugin.rhc_plugin.rhc_set_qos_fn = dds_rhc_set_qos;
}



//provides explicit default domain id.
dds_domainid_t dds_domain_default (void)
{
  return  dds_global.m_default_domain;
}


dds_return_t
dds__check_domain(
        _In_ dds_domainid_t domain)
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
