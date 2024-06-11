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

#include "dds/durability/dds_durability_public.h"
#include "dds/ddsi/ddsi_log.h"
#include <string.h>

void dds_durability_fini (dds_durability_t* dc)
{
#ifdef DDS_HAS_DURABILITY
  /*
  In the cleanup phase, the durability client's participant will always be the last remaining participant on the domain.
  Thus if the domain is implicit, it will trigger deletion of the domain, which in turn unloads the durability plugin.

  So if the participant were to be deleted from inside the plugin, it would result in a fatal error when
  the program tries to return (as it unwinds the call stack) to the plugin function that called `dds_delete()` on the participant.

  Therefore, in order to safely delete the participant of the durability client,
  its handle needs to be smuggled out so it can be deleted by a function that isn't part of the plugin itself.
  */
  assert(dc->_dds_durability_fini);
  dds_entity_t pp = dc->_dds_durability_fini();
  if ( pp != 0 ) {
    dds_delete(pp);
  }
#endif
}

dds_return_t dds_durability_load (dds_durability_t* dc, const struct ddsi_domaingv* gv)
{
  memset(dc, 0x0, sizeof(dds_durability_t));
#ifdef DDS_HAS_DURABILITY
  dds_return_t (*creator)(dds_durability_t* dc);
  ddsrt_dynlib_t handle = NULL;
  dds_return_t ret;
  if ((ret = ddsrt_dlopen("durability", true, &handle)) != DDS_RETCODE_OK) {
    char buf[256];
    ddsrt_dlerror(buf, sizeof(buf));
    GVERROR("dlopen: %s\n", buf);
    return ret;
  }
  if ((ret = ddsrt_dlsym(handle, "dds_durability_creator", (void**)&creator)) != DDS_RETCODE_OK) {
    char buf[256];
    ddsrt_dlerror(buf, sizeof(buf));
    GVERROR("dlsym: %s\n", buf);
    (void)ddsrt_dlclose(handle);
    return ret;
  }
  creator(dc);
  dc->lib_handle = handle;
#endif
  return DDS_RETCODE_OK;
}

void dds_durability_unload (dds_durability_t* dc)
{
#ifdef DDS_HAS_DURABILITY
  assert(dc->lib_handle);
  (void)ddsrt_dlclose(dc->lib_handle);
#endif
}
