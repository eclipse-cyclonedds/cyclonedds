// Copyright(c) 2020 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <string.h>
#include <assert.h>
#include <stdlib.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/log.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/io.h"
#include "dds/ddsi/ddsi_log.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "ddsi__tran.h"
#include "ddsi__vnet.h"

typedef struct ddsi_vnet_conn {
  struct ddsi_tran_conn m_base;
  int32_t m_kind;
} *ddsi_vnet_conn_t;

typedef struct ddsi_vnet_tran_factory {
  struct ddsi_tran_factory m_base;
  int32_t m_kind;
} *ddsi_vnet_tran_factory_t;

static char *ddsi_vnet_to_string (char *dst, size_t sizeof_dst, const ddsi_locator_t *loc, struct ddsi_tran_conn * conn, int with_port)
{
  (void) conn;
  const unsigned char * const x = loc->address;
  int pos;
  pos = snprintf (dst, sizeof_dst, "[%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x]",
                  x[0], x[1], x[2], x[3], x[4], x[5], x[6], x[7], x[8], x[9], x[10], x[11], x[12], x[13], x[14], x[15]);
  if (with_port && pos >= 0 && (size_t) pos < sizeof_dst)
    (void) snprintf (dst + pos, sizeof_dst - (size_t) pos, ":%"PRIu32, loc->port);
  return dst;
}

static bool ddsi_vnet_supports (const struct ddsi_tran_factory *fact_cmn, int32_t kind)
{
  struct ddsi_vnet_tran_factory const * const fact = (const struct ddsi_vnet_tran_factory *) fact_cmn;
  return (kind == fact->m_kind);
}

static ddsrt_socket_t ddsi_vnet_conn_handle (struct ddsi_tran_base * conn)
{
  (void) conn;
  return DDSRT_INVALID_SOCKET;
}

static int ddsi_vnet_conn_locator (struct ddsi_tran_factory * vfact, struct ddsi_tran_base * base, ddsi_locator_t *loc)
{
  (void) base; (void) loc;
  const struct ddsi_vnet_tran_factory *fact = (const struct ddsi_vnet_tran_factory *) vfact;
  memset (loc, 0, sizeof (*loc));
  loc->kind = fact->m_kind;
  return 0;
}

static dds_return_t ddsi_vnet_create_conn (struct ddsi_tran_conn **conn_out, struct ddsi_tran_factory * fact_cmn, uint32_t port, const struct ddsi_tran_qos *qos)
{
  (void) port;
  struct ddsi_vnet_tran_factory *fact = (struct ddsi_vnet_tran_factory *) fact_cmn;
  struct ddsi_domaingv const * const gv = fact->m_base.gv;
  struct ddsi_vnet_conn *x = ddsrt_malloc (sizeof (*x));
  struct ddsi_network_interface const * const intf = qos->m_interface ? qos->m_interface : &gv->interfaces[0];
  memset (x, 0, sizeof (*x));

  ddsi_factory_conn_init (&fact->m_base, intf, &x->m_base);
  x->m_base.m_base.m_trantype = DDSI_TRAN_CONN;
  x->m_base.m_base.m_multicast = false;
  x->m_base.m_base.m_handle_fn = ddsi_vnet_conn_handle;
  x->m_base.m_locator_fn = ddsi_vnet_conn_locator;
  x->m_base.m_read_fn = 0;
  x->m_base.m_write_fn = 0;
  x->m_base.m_disable_multiplexing_fn = 0;

  DDS_CTRACE (&fact->m_base.gv->logconfig, "ddsi_vnet_create_conn intf %s kind %s\n", x->m_base.m_interf->name, fact->m_base.m_typename);
  *conn_out = &x->m_base;
  return 0;
}

static void ddsi_vnet_release_conn (struct ddsi_tran_conn * conn)
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

static enum ddsi_nearby_address_result ddsi_vnet_is_nearby_address (const ddsi_locator_t *loc, size_t ninterf, const struct ddsi_network_interface interf[], size_t *interf_idx)
{
  enum ddsi_nearby_address_result default_result = DNAR_UNREACHABLE;
  for (size_t i = 0; i < ninterf; i++)
  {
    if (interf[i].loc.kind != loc->kind)
      continue;
    default_result = DNAR_DISTANT;
    if (memcmp (interf[i].loc.address, loc->address, sizeof (loc->address)) == 0 && interf[i].loc.port == loc->port)
    {
      if (interf_idx)
        *interf_idx = i;
      return DNAR_SELF;
    }
  }
  return default_result;
}

static enum ddsi_locator_from_string_result ddsi_vnet_address_from_string (const struct ddsi_tran_factory *tran_cmn, ddsi_locator_t *loc, const char *str)
{
  struct ddsi_vnet_tran_factory const * const tran = (const struct ddsi_vnet_tran_factory *) tran_cmn;
  bool bracketed = false;
  int i = 0;
  loc->kind = tran->m_kind;
  loc->port = DDSI_LOCATOR_PORT_INVALID;
  memset (loc->address, 0, sizeof (loc->address));
  if (*str == '[')
  {
    str++;
    bracketed = true;
  }
  while (i < (int) sizeof (loc->address) && *str != 0)
  {
    unsigned o;
    int p;
    DDSRT_WARNING_MSVC_OFF(4996);
    if (sscanf (str, "%x%n", &o, &p) != 1 || o > 255)
      return AFSR_INVALID;
    DDSRT_WARNING_MSVC_ON(4996);
    loc->address[i++] = (unsigned char) o;
    str += p;
    if (i < (int) sizeof (loc->address))
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

static int ddsi_vnet_enumerate_interfaces (struct ddsi_tran_factory * fact, enum ddsi_transport_selector transport_selector, ddsrt_ifaddrs_t **ifs)
{
  (void) transport_selector;
  *ifs = ddsrt_malloc (sizeof (**ifs));
  (*ifs)->next = NULL;
  (*ifs)->type = DDSRT_IFTYPE_UNKNOWN;
  (*ifs)->name = ddsrt_strdup (fact->m_typename);
  (*ifs)->index = 0;
  (*ifs)->flags = IFF_UP | IFF_MULTICAST;
  (*ifs)->addr = ddsrt_malloc (sizeof (struct sockaddr_storage));
  memset ((*ifs)->addr, 0, sizeof (struct sockaddr_storage));
  ((char*)(*ifs)->addr)[sizeof ((*ifs)->addr->sa_family)] = 1;
  (*ifs)->netmask = NULL;
  (*ifs)->broadaddr = NULL;
  return 0;
}

static int ddsi_vnet_is_valid_port (const struct ddsi_tran_factory *fact, uint32_t port)
{
  (void) fact; (void) port;
  return true;
}

static uint32_t ddsi_vnet_receive_buffer_size (const struct ddsi_tran_factory *fact)
{
  (void) fact;
  return 0;
}

static int ddsi_vnet_locator_from_sockaddr (const struct ddsi_tran_factory *tran, ddsi_locator_t *loc, const struct sockaddr *sockaddr)
{
  (void) sockaddr;
  const struct ddsi_vnet_tran_factory *fact = (const struct ddsi_vnet_tran_factory *) tran;
  memset (loc, 0, sizeof (*loc));
  loc->kind = fact->m_kind;
  loc->port = 0;
  memcpy (loc->address, &((char*)sockaddr)[sizeof (sockaddr->sa_family)], sizeof (loc->address));
  return 0;
}

static void ddsi_vnet_deinit (struct ddsi_tran_factory * fact)
{
  DDS_CLOG (DDS_LC_CONFIG, &fact->gv->logconfig, "vnet %s de-initialized\n", fact->m_typename);
  ddsrt_free ((char *) fact->m_typename);
  ddsrt_free (fact);
}

int ddsi_vnet_init (struct ddsi_domaingv *gv, const char *name, int32_t locator_kind)
{
  struct ddsi_vnet_tran_factory *fact = ddsrt_malloc (sizeof (*fact));
  memset (fact, 0, sizeof (*fact));
  fact->m_kind = locator_kind;
  fact->m_base.gv = gv;
  fact->m_base.m_free_fn = ddsi_vnet_deinit;
  fact->m_base.m_typename = ddsrt_strdup (name);
  fact->m_base.m_default_spdp_address = NULL;
  fact->m_base.m_connless = 1;
  fact->m_base.m_enable_spdp = 0;
  fact->m_base.m_supports_fn = ddsi_vnet_supports;
  fact->m_base.m_create_conn_fn = ddsi_vnet_create_conn;
  fact->m_base.m_release_conn_fn = ddsi_vnet_release_conn;
  fact->m_base.m_join_mc_fn = 0;
  fact->m_base.m_leave_mc_fn = 0;
  fact->m_base.m_is_loopbackaddr_fn = ddsi_vnet_is_not;
  fact->m_base.m_is_mcaddr_fn = ddsi_vnet_is_not;
  fact->m_base.m_is_ssm_mcaddr_fn = ddsi_vnet_is_not;
  fact->m_base.m_is_nearby_address_fn = ddsi_vnet_is_nearby_address;
  fact->m_base.m_locator_from_string_fn = ddsi_vnet_address_from_string;
  fact->m_base.m_locator_to_string_fn = ddsi_vnet_to_string;
  fact->m_base.m_enumerate_interfaces_fn = ddsi_vnet_enumerate_interfaces;
  fact->m_base.m_is_valid_port_fn = ddsi_vnet_is_valid_port;
  fact->m_base.m_receive_buffer_size_fn = ddsi_vnet_receive_buffer_size;
  fact->m_base.m_locator_from_sockaddr_fn = ddsi_vnet_locator_from_sockaddr;
  ddsi_factory_add (gv, &fact->m_base);
  GVLOG (DDS_LC_CONFIG, "vnet %s initialized\n", name);
  return 0;
}
