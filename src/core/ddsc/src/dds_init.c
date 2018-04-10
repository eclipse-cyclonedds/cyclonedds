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
#include <os/os.h>
#include <os/os_report.h>
#include "dds__init.h"
#include "dds__rhc.h"
#include "dds__tkmap.h"
#include "dds__iid.h"
#include "dds__domain.h"
#include "dds__err.h"
#include "dds__builtin.h"
#include "dds__report.h"
#include "ddsi/ddsi_ser.h"
#include "ddsi/q_servicelease.h"
#include "ddsi/q_entity.h"
#include <ddsi/q_config.h>
#include "ddsc/ddsc_project.h"

#ifdef _WRS_KERNEL
char *os_environ[] = { NULL };
#endif

#define DOMAIN_ID_MIN 0
#define DOMAIN_ID_MAX 230

struct q_globals gv;

dds_globals dds_global =
{
  DDS_DOMAIN_DEFAULT, 0,
  NULL, NULL, NULL, NULL
};

static struct cfgst * dds_cfgst = NULL;




os_mutex dds__init_mutex;

static void
dds__fini_once(void)
{
  os_mutexDestroy(&dds__init_mutex);
  os_osExit();
}

static void
dds__init_once(void)
{
  os_osInit();
  os_mutexInit(&dds__init_mutex);
  os_procAtExit(dds__fini_once);
}



void
dds__startup(void)
{
    static os_once_t dds__init_control = OS_ONCE_T_STATIC_INIT;
    os_once(&dds__init_control, dds__init_once);
}


dds_return_t
dds_init(void)
{
  dds_return_t ret = DDS_RETCODE_OK;
  const char * uri;
  char progname[50];
  char hostname[64];
  uint32_t len;

  /* Be sure the DDS lifecycle resources are initialized. */
  dds__startup();

  DDS_REPORT_STACK();

  os_mutexLock(&dds__init_mutex);

  dds_global.m_init_count++;
  if (dds_global.m_init_count > 1)
  {
    goto skip;
  }

  if (ut_handleserver_init() != UT_HANDLE_OK)
  {
    ret = DDS_ERRNO(DDS_RETCODE_ERROR, "Failed to initialize internal handle server");
    goto fail_handleserver;
  }

  gv.tstart = now ();
  gv.exception = false;
  gv.static_logbuf_lock_inited = 0;
  logbuf_init (&gv.static_logbuf);
  os_mutexInit (&gv.static_logbuf_lock);
  gv.static_logbuf_lock_inited = 1;
  os_mutexInit (&dds_global.m_mutex);

  uri = os_getenv (DDSC_PROJECT_NAME_NOSPACE_CAPS"_URI");
  dds_cfgst = config_init (uri);
  if (dds_cfgst == NULL)
  {
    ret = DDS_ERRNO(DDS_RETCODE_ERROR, "Failed to parse configuration XML file %s", uri);
    goto fail_config;
  }
  /* The config.domainId can change internally in DDSI. So, remember what the
   * main configured domain id is. */
  dds_global.m_default_domain = config.domainId;

  dds__builtin_init();

  if (rtps_config_prep(dds_cfgst) != 0)
  {
    ret = DDS_ERRNO(DDS_RETCODE_ERROR, "Failed to configure RTPS.");
    goto fail_rtps_config;
  }

  ut_avlInit(&dds_domaintree_def, &dds_global.m_domains);

  /* Start monitoring the liveliness of all threads and renewing the
     service lease if everything seems well. */

  gv.servicelease = nn_servicelease_new(0, 0);
  if (gv.servicelease == NULL)
  {
    ret = DDS_ERRNO(DDS_RETCODE_OUT_OF_RESOURCES, "Failed to create a servicelease.");
    goto fail_servicelease_new;
  }
  if (nn_servicelease_start_renewing(gv.servicelease) < 0)
  {
    ret = DDS_ERRNO(DDS_RETCODE_ERROR, "Failed to start the servicelease.");
    goto fail_servicelease_start;
  }

  if (rtps_init() < 0)
  {
    ret = DDS_ERRNO(DDS_RETCODE_ERROR, "Failed to initialize RTPS.");
    goto fail_rtps_init;
  }
  upgrade_main_thread();

  /* Set additional default participant properties */

  gv.default_plist_pp.process_id = (unsigned)os_procIdSelf();
  gv.default_plist_pp.present |= PP_PRISMTECH_PROCESS_ID;
  if (os_procName(progname, sizeof(progname)) > 0)
  {
    gv.default_plist_pp.exec_name = dds_string_dup(progname);
  }
  else
  {
    gv.default_plist_pp.exec_name = dds_string_alloc(32);
    (void) snprintf(gv.default_plist_pp.exec_name, 32, "%s: %u", DDSC_PROJECT_NAME, gv.default_plist_pp.process_id);
  }
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
  os_mutexUnlock(&dds__init_mutex);
  DDS_REPORT_FLUSH(false);
  return DDS_RETCODE_OK;

fail_rtps_init:
fail_servicelease_start:
  nn_servicelease_free (gv.servicelease);
  gv.servicelease = NULL;
fail_servicelease_new:
  thread_states_fini();
fail_rtps_config:
  dds__builtin_fini();
  dds_global.m_default_domain = DDS_DOMAIN_DEFAULT;
  config_fini (dds_cfgst);
  dds_cfgst = NULL;
fail_config:
  gv.static_logbuf_lock_inited = 0;
  os_mutexDestroy (&gv.static_logbuf_lock);
  os_mutexDestroy (&dds_global.m_mutex);
  ut_handleserver_fini();
fail_handleserver:
  dds_global.m_init_count--;
  os_mutexUnlock(&dds__init_mutex);
  DDS_REPORT_FLUSH(true);
  return ret;
}



extern void dds_fini (void)
{
  os_mutexLock(&dds__init_mutex);
  assert(dds_global.m_init_count > 0);
  dds_global.m_init_count--;
  if (dds_global.m_init_count == 0)
  {
    dds__builtin_fini();

    ut_handleserver_fini();
    rtps_term ();
    nn_servicelease_free (gv.servicelease);
    gv.servicelease = NULL;
    downgrade_main_thread ();
    thread_states_fini ();

    config_fini (dds_cfgst);
    dds_cfgst = NULL;
    os_mutexDestroy (&gv.static_logbuf_lock);
    os_mutexDestroy (&dds_global.m_mutex);
    dds_global.m_default_domain = DDS_DOMAIN_DEFAULT;
  }
  os_mutexUnlock(&dds__init_mutex);
}





static int dds__init_plugin (void)
{
  os_mutexInit (&gv.attach_lock);
  dds_iid_init ();
  if (dds_global.m_dur_init) (dds_global.m_dur_init) ();
  return 0;
}

static void dds__fini_plugin (void)
{
  os_mutexDestroy (&gv.attach_lock);
  if (dds_global.m_dur_fini) (dds_global.m_dur_fini) ();
  dds_iid_fini ();
}

void ddsi_plugin_init (void)
{
  /* Register initialization/clean functions */

  ddsi_plugin.init_fn = dds__init_plugin;
  ddsi_plugin.fini_fn = dds__fini_plugin;

  /* Register read cache functions */

  ddsi_plugin.rhc_free_fn = dds_rhc_free;
  ddsi_plugin.rhc_fini_fn = dds_rhc_fini;
  ddsi_plugin.rhc_store_fn = dds_rhc_store;
  ddsi_plugin.rhc_unregister_wr_fn = dds_rhc_unregister_wr;
  ddsi_plugin.rhc_relinquish_ownership_fn = dds_rhc_relinquish_ownership;
  ddsi_plugin.rhc_set_qos_fn = dds_rhc_set_qos;
  ddsi_plugin.rhc_lookup_fn = dds_tkmap_lookup_instance_ref;
  ddsi_plugin.rhc_unref_fn = dds_tkmap_instance_unref;

  /* Register iid generator */

  ddsi_plugin.iidgen_fn = dds_iid_gen;
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
      ret = DDS_ERRNO(DDS_RETCODE_ERROR,
                      "Inconsistent domain configuration detected: domain on configuration: %d, domain %d",
                      dds_global.m_default_domain, domain);
    }
  }
  return ret;
}
