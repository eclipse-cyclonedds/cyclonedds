/*
 * Copyright(c) 2021 ADLINK Technology Limited and others
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
#include <assert.h>
#include <stdlib.h>
#include <errno.h>

#include "dds/ddsi/ddsi_tran.h"
#include "dds/ddsi/ddsi_vnet.h"
#include "dds/ddsi/q_config.h"
#include "dds/ddsi/q_log.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/log.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/io.h"

typedef struct ddsi_vnet_conn {
  struct ddsi_tran_conn m_base;
} *ddsi_vnet_conn_t;

static char *ddsi_vnet_to_string (char *dst, size_t sizeof_dst, const ddsi_locator_t *loc, ddsi_tran_conn_t conn, int with_port)
{
  // FIXME: fixme
  (void) conn;
  if (with_port)
    (void) snprintf(dst, sizeof_dst, "[%02x:%02x:%02x:%02x:%02x:%02x]:%u",
                    loc->address[0], loc->address[1], loc->address[2],
                    loc->address[3], loc->address[4], loc->address[5], loc->port);
  else
    (void) snprintf(dst, sizeof_dst, "[%02x:%02x:%02x:%02x:%02x:%02x]",
                    loc->address[0], loc->address[1], loc->address[2],
                    loc->address[3], loc->address[4], loc->address[5]);
  return dst;
}

static bool ddsi_vnet_supports (const struct ddsi_tran_factory *fact, int32_t kind)
{
  return (kind == fact->m_kind);
}

static ddsrt_socket_t ddsi_vnet_conn_handle (ddsi_tran_base_t conn)
{
  (void) conn;
  return DDSRT_INVALID_SOCKET;
}

static int ddsi_vnet_conn_locator (ddsi_tran_factory_t fact, ddsi_tran_base_t base, ddsi_locator_t *loc)
{
  (void) fact; (void) base; (void) loc;
  // FIXME: not sure I can get away with this
  return -1;
}

static dds_return_t ddsi_vnet_create_conn (ddsi_tran_conn_t *conn_out, ddsi_tran_factory_t fact, uint32_t port, const struct ddsi_tran_qos *qos)
{
  (void) port;
  struct ddsi_vnet_conn *x = ddsrt_malloc (sizeof (*x));
  memset (x, 0, sizeof (*x));
  
  ddsi_factory_conn_init (fact, qos->m_interface, &x->m_base);
  x->m_base.m_base.m_trantype = DDSI_TRAN_CONN;
  x->m_base.m_base.m_multicast = false;
  x->m_base.m_base.m_handle_fn = ddsi_vnet_conn_handle;
  x->m_base.m_locator_fn = ddsi_vnet_conn_locator;
  x->m_base.m_read_fn = 0;
  x->m_base.m_write_fn = 0;
  x->m_base.m_disable_multiplexing_fn = 0;

  DDS_CTRACE (&fact->gv->logconfig, "ddsi_vnet_create_conn intf %s kind %s\n", x->m_base.m_interf->name, fact->m_typename);
  *conn_out = &x->m_base;
  return 0;
}

static void ddsi_vnet_release_conn (ddsi_tran_conn_t conn)
{
  ddsi_vnet_conn_t x = (ddsi_vnet_conn_t) conn;
  DDS_CTRACE (&conn->m_base.gv->logconfig, "ddsi_vnet_release_conn intf %s kind %s\n", x->m_base.m_interf->name, x->m_base.m_factory->m_typename);
  ddsrt_free (conn);
}

static int ddsi_vnet_is_not (const struct ddsi_tran_factory *tran, const ddsi_locator_t *loc)
{
  (void) tran;
  (void) loc;
  return 0;
}

static enum ddsi_nearby_address_result ddsi_vnet_is_nearby_address (const ddsi_locator_t *loc, size_t ninterf, const struct nn_interface interf[], size_t *interf_idx)
{
  for (size_t i = 0; i < ninterf; i++)
  {
    if (interf[i].loc.kind != loc->kind)
      continue;
    if (memcmp (interf[i].loc.address, loc->address, sizeof (loc->address)) == 0 && interf[i].loc.port == loc->port)
    {
      *interf_idx = i;
      return DNAR_LOCAL;
    }
  }
  return DNAR_DISTANT;
}

static enum ddsi_locator_from_string_result ddsi_vnet_address_from_string (const struct ddsi_tran_factory *tran, ddsi_locator_t *loc, const char *str)
{
  bool bracketed = false;
  int i = 0;
  loc->kind = tran->m_kind;
  loc->port = NN_LOCATOR_PORT_INVALID;
  memset (loc->address, 0, sizeof (loc->address));
  if (*str == '[')
  {
    str++;
    bracketed = true;
  }
  while (i < 6 && *str != 0)
  {
    unsigned o;
    int p;
    if (sscanf (str, "%x%n", &o, &p) != 1 || o > 255)
      return AFSR_INVALID;
    loc->address[10 + i++] = (unsigned char) o;
    str += p;
    if (i < 6)
    {
      if (*str != ':')
        return AFSR_INVALID;
      str++;
    }
  }
  if (bracketed && *str++ != ']')
    return AFSR_INVALID;
  return (*str == 0) ? AFSR_OK : AFSR_INVALID;
}

static int ddsi_vnet_enumerate_interfaces (ddsi_tran_factory_t fact, enum ddsi_transport_selector transport_selector, ddsrt_ifaddrs_t **ifs)
{
  (void) fact; (void) transport_selector;
  *ifs = NULL;
  return DDS_RETCODE_UNSUPPORTED;
}

static int ddsi_vnet_is_valid_port (const struct ddsi_tran_factory *fact, uint32_t port)
{
  (void) fact;
  return (port == 0);
}

static uint32_t ddsi_vnet_receive_buffer_size (const struct ddsi_tran_factory *fact)
{
  (void) fact;
  return 0;
}

static void ddsi_vnet_deinit (ddsi_tran_factory_t fact)
{
  DDS_CLOG (DDS_LC_CONFIG, &fact->gv->logconfig, "vnet %s de-initialized\n", fact->m_typename);
  ddsrt_free ((char *) fact->m_typename);
  ddsrt_free ((char *) fact->m_default_spdp_address);
  ddsrt_free (fact);
}

int ddsi_vnet_init (struct ddsi_domaingv *gv, const char *name, int32_t locator_kind)
{
  struct ddsi_tran_factory *fact = ddsrt_malloc (sizeof (*fact));
  memset (fact, 0, sizeof (*fact));
  fact->gv = gv;
  fact->m_free_fn = ddsi_vnet_deinit;
  fact->m_kind = locator_kind;
  fact->m_typename = ddsrt_strdup (name);
  char *def_spdp; // I think I don't even need this
  (void) ddsrt_asprintf (&def_spdp, "%s/00:00:00:00:00:00", name);
  fact->m_default_spdp_address = def_spdp;
  fact->m_connless = 1;
  fact->m_adv_spdp = 0;
  fact->m_supports_fn = ddsi_vnet_supports;
  fact->m_create_conn_fn = ddsi_vnet_create_conn;
  fact->m_release_conn_fn = ddsi_vnet_release_conn;
  fact->m_join_mc_fn = 0;
  fact->m_leave_mc_fn = 0;
  fact->m_is_loopbackaddr_fn = ddsi_vnet_is_not;
  fact->m_is_mcaddr_fn = ddsi_vnet_is_not;
  fact->m_is_ssm_mcaddr_fn = ddsi_vnet_is_not;
  fact->m_is_nearby_address_fn = ddsi_vnet_is_nearby_address;
  fact->m_locator_from_string_fn = ddsi_vnet_address_from_string;
  fact->m_locator_to_string_fn = ddsi_vnet_to_string;
  fact->m_enumerate_interfaces_fn = ddsi_vnet_enumerate_interfaces;
  fact->m_is_valid_port_fn = ddsi_vnet_is_valid_port;
  fact->m_receive_buffer_size_fn = ddsi_vnet_receive_buffer_size;
  ddsi_factory_add (gv, fact);
  GVLOG (DDS_LC_CONFIG, "vnet %s initialized\n", name);
  return 0;
}
