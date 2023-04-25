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

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/sockets.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "ddsi__tran.h"
#include "ddsi__udp.h"
#include "ddsi__tcp.h"
#include "CUnit/Theory.h"

static bool prefix_zero (const ddsi_locator_t *loc, size_t n)
{
  assert (n <= sizeof (loc->address));
  for (size_t i = 0; i < n; i++)
    if (loc->address[i] != 0)
      return false;
  return true;
}

static bool check_ipv4_address (const ddsi_locator_t *loc, const uint8_t x[4])
{
  return prefix_zero (loc, 12) && memcmp (loc->address + 12, x, 4) == 0;
}

#if DDSRT_HAVE_IPV6
static bool check_ipv64_address (const ddsi_locator_t *loc, const uint8_t x[4])
{
  return prefix_zero (loc, 10) && loc->address[10] == 0xff && loc->address[11] == 0xff && memcmp (loc->address + 12, x, 4) == 0;
}
#endif

static struct ddsi_tran_factory *init (struct ddsi_domaingv *gv, enum ddsi_transport_selector tr)
{
  memset (gv, 0, sizeof (*gv));
  gv->config.transport_selector = tr;
  ddsi_udp_init (gv);
  ddsi_tcp_init (gv);
  switch (tr)
  {
    case DDSI_TRANS_UDP: return ddsi_factory_find (gv, "udp");
    case DDSI_TRANS_TCP: return ddsi_factory_find (gv, "tcp");
    case DDSI_TRANS_UDP6: return ddsi_factory_find (gv, "udp6");
    case DDSI_TRANS_TCP6: return ddsi_factory_find (gv, "tcp6");
    default: return NULL;
  }
}

static void fini (struct ddsi_domaingv *gv)
{
  while (gv->ddsi_tran_factories)
  {
    struct ddsi_tran_factory *f = gv->ddsi_tran_factories;
    gv->ddsi_tran_factories = f->m_factory;
    ddsi_factory_free (f);
  }
}

CU_Test (ddsi_locator_from_string, bogusproto)
{
  struct ddsi_domaingv gv;
  struct ddsi_tran_factory * const fact = init (&gv, DDSI_TRANS_UDP);
  ddsi_locator_t loc;
  enum ddsi_locator_from_string_result res;
  res = ddsi_locator_from_string (&gv, &loc, "bogusproto/xyz", fact);
  CU_ASSERT_FATAL (res == AFSR_UNKNOWN);
  res = ddsi_locator_from_string (&gv, &loc, "bogusproto/xyz:1234", fact);
  CU_ASSERT_FATAL (res == AFSR_UNKNOWN);
  res = ddsi_locator_from_string (&gv, &loc, "bogusproto/192.0.2.0:1234", fact);
  CU_ASSERT_FATAL (res == AFSR_UNKNOWN);
  fini (&gv);
}

CU_TheoryDataPoints(ddsi_locator_from_string, ipv4_invalid) = {
  CU_DataPoints(enum ddsi_transport_selector, DDSI_TRANS_UDP, DDSI_TRANS_TCP)
};

CU_Theory ((enum ddsi_transport_selector tr), ddsi_locator_from_string, ipv4_invalid)
{
  struct ddsi_domaingv gv;
  struct ddsi_tran_factory * const fact = init (&gv, tr);
  ddsi_locator_t loc;
  enum ddsi_locator_from_string_result res;
  char astr[40];
  assert(fact);
  snprintf (astr, sizeof (astr), "%s/", fact->m_typename);
  res = ddsi_locator_from_string (&gv, &loc, astr, fact);
  CU_ASSERT_FATAL (res == AFSR_INVALID);
  snprintf (astr, sizeof (astr), "%s/:", fact->m_typename);
  res = ddsi_locator_from_string (&gv, &loc, astr, fact);
  CU_ASSERT_FATAL (res == AFSR_INVALID);
  snprintf (astr, sizeof (astr), "%s/1.2:", fact->m_typename);
  res = ddsi_locator_from_string (&gv, &loc, astr, fact);
  CU_ASSERT_FATAL (res == AFSR_INVALID);
  snprintf (astr, sizeof (astr), "%s/1.2:99999", fact->m_typename);
  res = ddsi_locator_from_string (&gv, &loc, astr, fact);
  CU_ASSERT_FATAL (res == AFSR_INVALID);
  // if DNS is supported, a hostname lookup is tried whenever parsing as a numerical address fails
  // which means we may get UNKNOWN
  snprintf (astr, sizeof (astr), "%s/:1234", fact->m_typename);
  res = ddsi_locator_from_string (&gv, &loc, astr, fact);
  CU_ASSERT_FATAL (res == AFSR_INVALID || res == AFSR_UNKNOWN);
  snprintf (astr, sizeof (astr), "%s/[192.0.2.0]", fact->m_typename);
  res = ddsi_locator_from_string (&gv, &loc, astr, fact);
  CU_ASSERT_FATAL (res == AFSR_INVALID || res == AFSR_UNKNOWN);
  snprintf (astr, sizeof (astr), "%s/[192.0.2.0]:1234", fact->m_typename);
  res = ddsi_locator_from_string (&gv, &loc, astr, fact);
  CU_ASSERT_FATAL (res == AFSR_INVALID || res == AFSR_UNKNOWN);
  fini (&gv);
}

CU_TheoryDataPoints(ddsi_locator_from_string, ipv4) = {
  CU_DataPoints(enum ddsi_transport_selector, DDSI_TRANS_UDP, DDSI_TRANS_TCP),
  CU_DataPoints(int32_t, DDSI_LOCATOR_KIND_UDPv4, DDSI_LOCATOR_KIND_TCPv4)
};

CU_Theory ((enum ddsi_transport_selector tr, int32_t loc_kind), ddsi_locator_from_string, ipv4)
{
  struct ddsi_domaingv gv;
  struct ddsi_tran_factory * const fact = init (&gv, tr);
  ddsi_locator_t loc;
  enum ddsi_locator_from_string_result res;
  char astr[40];

  // Coverity warns about memcmps against uninitialized memory
  // I guess it loses its way somewhere in the indirect function
  // calls
  memset (&loc, 0xee, sizeof (loc));

  CU_ASSERT_FATAL (ddsi_factory_supports (fact, loc_kind));

#if DDSRT_HAVE_DNS
  {
    enum ddsi_locator_from_string_result exp;
    struct sockaddr_in localhost;
    ddsrt_hostent_t *hent = NULL;
    memset (&localhost, 0xee, sizeof (localhost));
    if (ddsrt_gethostbyname ("localhost", AF_INET, &hent) != 0)
      exp = AFSR_UNKNOWN;
    else
    {
      CU_ASSERT_FATAL (hent->addrs[0].ss_family == AF_INET);
      memcpy (&localhost, &hent->addrs[0], sizeof (localhost));
      ddsrt_free (hent);
      exp = AFSR_OK;
    }
    res = ddsi_locator_from_string (&gv, &loc, "localhost", fact);
    CU_ASSERT_FATAL (res == exp);
    if (res == AFSR_OK)
    {
      CU_ASSERT_FATAL (loc.kind == loc_kind);
      CU_ASSERT_FATAL (loc.port == DDSI_LOCATOR_PORT_INVALID);
      CU_ASSERT_FATAL (prefix_zero (&loc, 12) && memcmp (loc.address + 12, &localhost.sin_addr.s_addr, 4) == 0);
    }
    res = ddsi_locator_from_string (&gv, &loc, "localhost:1234", fact);
    CU_ASSERT_FATAL (res == exp);
    if (res == AFSR_OK)
    {
      CU_ASSERT_FATAL (loc.kind == loc_kind);
      CU_ASSERT_FATAL (loc.port == 1234);
      CU_ASSERT_FATAL (prefix_zero (&loc, 12) && memcmp (loc.address + 12, &localhost.sin_addr.s_addr, 4) == 0);
    }
  }
#endif

  res = ddsi_locator_from_string (&gv, &loc, "192.0.2.0", fact);
  CU_ASSERT_FATAL (res == AFSR_OK);
  CU_ASSERT_FATAL (loc.kind == loc_kind);
  CU_ASSERT_FATAL (loc.port == DDSI_LOCATOR_PORT_INVALID);
  CU_ASSERT_FATAL (check_ipv4_address (&loc, (uint8_t[]){192,0,2,0}));

  snprintf (astr, sizeof (astr), "%s/192.0.2.0", fact->m_typename);
  res = ddsi_locator_from_string (&gv, &loc, astr, fact);
  CU_ASSERT_FATAL (res == AFSR_OK);
  CU_ASSERT_FATAL (loc.kind == loc_kind);
  CU_ASSERT_FATAL (loc.port == DDSI_LOCATOR_PORT_INVALID);
  CU_ASSERT_FATAL (check_ipv4_address (&loc, (uint8_t[]){192,0,2,0}));

  snprintf (astr, sizeof (astr), "%s/192.0.2.0:1234", fact->m_typename);
  res = ddsi_locator_from_string (&gv, &loc, astr, fact);
  CU_ASSERT_FATAL (res == AFSR_OK);
  CU_ASSERT_FATAL (loc.kind == loc_kind);
  CU_ASSERT_FATAL (loc.port == 1234);
  CU_ASSERT_FATAL (check_ipv4_address (&loc, (uint8_t[]){192,0,2,0}));
  fini (&gv);
}

CU_Test (ddsi_locator_from_string, ipv4_cross1)
{
  struct ddsi_domaingv gv;
  struct ddsi_tran_factory * const fact = init (&gv, DDSI_TRANS_UDP);
  ddsi_locator_t loc;
  enum ddsi_locator_from_string_result res;
  res = ddsi_locator_from_string (&gv, &loc, "tcp/192.0.2.0:1234", fact);
  CU_ASSERT_FATAL (res == AFSR_OK);
  CU_ASSERT_FATAL (loc.kind == DDSI_LOCATOR_KIND_TCPv4);
  CU_ASSERT_FATAL (loc.port == 1234);
  CU_ASSERT_FATAL (check_ipv4_address (&loc, (uint8_t[]){192,0,2,0}));
  fini (&gv);
}

CU_Test (ddsi_locator_from_string, ipv4_cross2)
{
  struct ddsi_domaingv gv;
  struct ddsi_tran_factory * const fact = init (&gv, DDSI_TRANS_TCP);
  ddsi_locator_t loc;
  enum ddsi_locator_from_string_result res;
  res = ddsi_locator_from_string (&gv, &loc, "udp/192.0.2.0:1234", fact);
  CU_ASSERT_FATAL (res == AFSR_OK);
  CU_ASSERT_FATAL (loc.kind == DDSI_LOCATOR_KIND_UDPv4);
  CU_ASSERT_FATAL (loc.port == 1234);
  CU_ASSERT_FATAL (check_ipv4_address (&loc, (uint8_t[]){192,0,2,0}));
  fini (&gv);
}

CU_Test (ddsi_locator_from_string, udpv4mcgen)
{
  struct ddsi_domaingv gv;
  struct ddsi_tran_factory * const fact = init (&gv, DDSI_TRANS_UDP);
  ddsi_locator_t loc;
  enum ddsi_locator_from_string_result res;
  res = ddsi_locator_from_string (&gv, &loc, "239.255.0.1;4;8;1:1234", fact);
  CU_ASSERT_FATAL (res == AFSR_OK);
  CU_ASSERT_FATAL (loc.kind == DDSI_LOCATOR_KIND_UDPv4MCGEN);
  CU_ASSERT_FATAL (loc.port == 1234);
  CU_ASSERT_FATAL (loc.address[0] == 239 && loc.address[1] == 255 && loc.address[2] == 0 && loc.address[3] == 1);
  CU_ASSERT_FATAL (loc.address[4] == 4 && loc.address[5] == 8 && loc.address[6] == 1);
  CU_ASSERT_FATAL (loc.address[7] == 0 && loc.address[8] == 0 && loc.address[9] == 0);
  CU_ASSERT_FATAL (loc.address[10] == 0 && loc.address[11] == 0 && loc.address[12] == 0);
  CU_ASSERT_FATAL (loc.address[13] == 0 && loc.address[14] == 0 && loc.address[15] == 0);

  res = ddsi_locator_from_string (&gv, &loc, "239.255.0.1;4;0;1:1234", fact);
  CU_ASSERT_FATAL (res == AFSR_INVALID || res == AFSR_UNKNOWN);
  res = ddsi_locator_from_string (&gv, &loc, "239.255.0.1;4;0;1:2345", fact);
  CU_ASSERT_FATAL (res == AFSR_INVALID || res == AFSR_UNKNOWN);
  res = ddsi_locator_from_string (&gv, &loc, "239.255.0.1;30;1;1:3456", fact);
  CU_ASSERT_FATAL (res == AFSR_INVALID || res == AFSR_UNKNOWN);
  res = ddsi_locator_from_string (&gv, &loc, "239.255.0.1;4;24;1:4567", fact);
  CU_ASSERT_FATAL (res == AFSR_INVALID || res == AFSR_UNKNOWN);
  res = ddsi_locator_from_string (&gv, &loc, "239.255.0.1;4;3;3:5678", fact);
  CU_ASSERT_FATAL (res == AFSR_INVALID || res == AFSR_UNKNOWN);
  fini (&gv);
}

CU_TheoryDataPoints(ddsi_locator_from_string, ipv6_invalid) = {
  CU_DataPoints(enum ddsi_transport_selector, DDSI_TRANS_UDP6, DDSI_TRANS_TCP6)
};

CU_Theory ((enum ddsi_transport_selector tr), ddsi_locator_from_string, ipv6_invalid)
{
#if DDSRT_HAVE_IPV6
  struct ddsi_domaingv gv;
  struct ddsi_tran_factory * const fact = init (&gv, tr);
  ddsi_locator_t loc;
  enum ddsi_locator_from_string_result res;
  char astr[40];
  assert(fact);
  snprintf (astr, sizeof (astr), "%s/", fact->m_typename);
  res = ddsi_locator_from_string (&gv, &loc, astr, fact);
  CU_ASSERT_FATAL (res == AFSR_INVALID);
  // if DNS is supported, a hostname lookup is tried whenever parsing as a numerical address fails
  // which means we may get UNKNOWN
  snprintf (astr, sizeof (astr), "%s/::1:31415", fact->m_typename);
  res = ddsi_locator_from_string (&gv, &loc, astr, fact);
  CU_ASSERT_FATAL (res == AFSR_INVALID || res == AFSR_UNKNOWN);
  snprintf (astr, sizeof (astr), "%s/:", fact->m_typename);
  res = ddsi_locator_from_string (&gv, &loc, astr, fact);
  CU_ASSERT_FATAL (res == AFSR_INVALID || res == AFSR_UNKNOWN);
  snprintf (astr, sizeof (astr), "%s/1.2:", fact->m_typename);
  res = ddsi_locator_from_string (&gv, &loc, astr, fact);
  CU_ASSERT_FATAL (res == AFSR_INVALID || res == AFSR_UNKNOWN);
  snprintf (astr, sizeof (astr), "%s/]:", fact->m_typename);
  res = ddsi_locator_from_string (&gv, &loc, astr, fact);
  CU_ASSERT_FATAL (res == AFSR_INVALID || res == AFSR_UNKNOWN);
  snprintf (astr, sizeof (astr), "%s/[", fact->m_typename);
  res = ddsi_locator_from_string (&gv, &loc, astr, fact);
  CU_ASSERT_FATAL (res == AFSR_INVALID || res == AFSR_UNKNOWN);
  snprintf (astr, sizeof (astr), "%s/[]", fact->m_typename);
  res = ddsi_locator_from_string (&gv, &loc, astr, fact);
  CU_ASSERT_FATAL (res == AFSR_INVALID || res == AFSR_UNKNOWN);
  snprintf (astr, sizeof (astr), "%s/:1234", fact->m_typename);
  res = ddsi_locator_from_string (&gv, &loc, astr, fact);
  CU_ASSERT_FATAL (res == AFSR_INVALID || res == AFSR_UNKNOWN);
  fini (&gv);
#else
  (void) tr;
  CU_PASS ("No IPv6 support");
#endif
}

CU_TheoryDataPoints(ddsi_locator_from_string, ipv6) = {
  CU_DataPoints(enum ddsi_transport_selector, DDSI_TRANS_UDP6, DDSI_TRANS_TCP6),
  CU_DataPoints(int32_t, DDSI_LOCATOR_KIND_UDPv6, DDSI_LOCATOR_KIND_TCPv6)
};

CU_Theory ((enum ddsi_transport_selector tr, int32_t loc_kind), ddsi_locator_from_string, ipv6)
{
#if DDSRT_HAVE_IPV6
  struct ddsi_domaingv gv;
  struct ddsi_tran_factory * const fact = init (&gv, tr);
  ddsi_locator_t loc;
  enum ddsi_locator_from_string_result res;
  char astr[40];

  // Coverity warns about memcmps against uninitialized memory
  // I guess it loses its way somewhere in the indirect function
  // calls
  memset (&loc, 0xee, sizeof (loc));

  CU_ASSERT_FATAL (ddsi_factory_supports (fact, loc_kind));

#if DDSRT_HAVE_DNS
  {
    enum ddsi_locator_from_string_result exp;
    struct sockaddr_in6 localhost;
    ddsrt_hostent_t *hent = NULL;
    memset (&localhost, 0xee, sizeof (localhost));
    if (ddsrt_gethostbyname ("localhost", AF_INET6, &hent) != 0)
      exp = AFSR_UNKNOWN;
    else
    {
      CU_ASSERT_FATAL (hent->addrs[0].ss_family == AF_INET6);
      memcpy (&localhost, &hent->addrs[0], sizeof (localhost));
      ddsrt_free (hent);
      exp = AFSR_OK;
    }
    res = ddsi_locator_from_string (&gv, &loc, "localhost", fact);
    CU_ASSERT_FATAL (res == exp);
    if (res == AFSR_OK)
    {
      CU_ASSERT_FATAL (loc.kind == loc_kind);
      CU_ASSERT_FATAL (loc.port == DDSI_LOCATOR_PORT_INVALID);
      CU_ASSERT_FATAL (memcmp (loc.address, &localhost.sin6_addr.s6_addr, 16) == 0);
    }
    res = ddsi_locator_from_string (&gv, &loc, "[localhost]", fact);
    CU_ASSERT_FATAL (res == exp);
    if (res == AFSR_OK)
    {
      CU_ASSERT_FATAL (loc.kind == loc_kind);
      CU_ASSERT_FATAL (loc.port == DDSI_LOCATOR_PORT_INVALID);
      CU_ASSERT_FATAL (memcmp (loc.address, &localhost.sin6_addr.s6_addr, 16) == 0);
    }
    res = ddsi_locator_from_string (&gv, &loc, "localhost:1234", fact);
    CU_ASSERT_FATAL (res == exp);
    if (res == AFSR_OK)
    {
      CU_ASSERT_FATAL (loc.kind == loc_kind);
      CU_ASSERT_FATAL (loc.port == 1234);
      CU_ASSERT_FATAL (memcmp (loc.address, &localhost.sin6_addr.s6_addr, 16) == 0);
    }
    res = ddsi_locator_from_string (&gv, &loc, "[localhost]:4567", fact);
    CU_ASSERT_FATAL (res == exp);
    if (res == AFSR_OK)
    {
      CU_ASSERT_FATAL (loc.kind == loc_kind);
      CU_ASSERT_FATAL (loc.port == 4567);
      CU_ASSERT_FATAL (memcmp (loc.address, &localhost.sin6_addr.s6_addr, 16) == 0);
    }
  }
#endif

  res = ddsi_locator_from_string (&gv, &loc, "192.0.2.0", fact);
  CU_ASSERT_FATAL (res == AFSR_OK || res == AFSR_UNKNOWN);
  if (res == AFSR_OK)
  {
    CU_ASSERT_FATAL (loc.kind == loc_kind);
    CU_ASSERT_FATAL (loc.port == DDSI_LOCATOR_PORT_INVALID);
    CU_ASSERT_FATAL (check_ipv64_address (&loc, (uint8_t[]){192,0,2,0}));
  }

  res = ddsi_locator_from_string (&gv, &loc, "[192.0.2.0]", fact);
  CU_ASSERT_FATAL (res == AFSR_OK || res == AFSR_UNKNOWN);
  if (res == AFSR_OK)
  {
    CU_ASSERT_FATAL (loc.kind == loc_kind);
    CU_ASSERT_FATAL (loc.port == DDSI_LOCATOR_PORT_INVALID);
    CU_ASSERT_FATAL (check_ipv64_address (&loc, (uint8_t[]){192,0,2,0}));
  }

  snprintf (astr, sizeof (astr), "%s/192.0.2.0", fact->m_typename);
  res = ddsi_locator_from_string (&gv, &loc, astr, fact);
  CU_ASSERT_FATAL (res == AFSR_OK || res == AFSR_UNKNOWN);  if (res == AFSR_OK)
  {
    CU_ASSERT_FATAL (loc.kind == loc_kind);
    CU_ASSERT_FATAL (loc.port == DDSI_LOCATOR_PORT_INVALID);
    CU_ASSERT_FATAL (check_ipv64_address (&loc, (uint8_t[]){192,0,2,0}));
  }

  snprintf (astr, sizeof (astr), "%s/192.0.2.0:6789", fact->m_typename);
  res = ddsi_locator_from_string (&gv, &loc, astr, fact);
  CU_ASSERT_FATAL (res == AFSR_OK || res == AFSR_UNKNOWN);
  if (res == AFSR_OK)
  {
    CU_ASSERT_FATAL (loc.kind == loc_kind);
    CU_ASSERT_FATAL (loc.port == 6789);
    CU_ASSERT_FATAL (check_ipv64_address (&loc, (uint8_t[]){192,0,2,0}));
  }

  snprintf (astr, sizeof (astr), "%s/[192.0.2.0]:7890", fact->m_typename);
  res = ddsi_locator_from_string (&gv, &loc, astr, fact);
  if (res == AFSR_OK)
  {
    CU_ASSERT_FATAL (res == AFSR_OK || res == AFSR_UNKNOWN);
    CU_ASSERT_FATAL (loc.kind == loc_kind);
    CU_ASSERT_FATAL (loc.port == 7890);
    CU_ASSERT_FATAL (check_ipv64_address (&loc, (uint8_t[]){192,0,2,0}));
  }

  snprintf (astr, sizeof (astr), "%s/[::1]:8901", fact->m_typename);
  res = ddsi_locator_from_string (&gv, &loc, astr, fact);
  CU_ASSERT_FATAL (res == AFSR_OK);
  CU_ASSERT_FATAL (loc.kind == loc_kind);
  CU_ASSERT_FATAL (loc.port == 8901);
  CU_ASSERT_FATAL (prefix_zero (&loc, 15) && loc.address[15] == 1);

  fini (&gv);
#else
  (void) tr; (void) loc_kind;
  CU_PASS ("No IPv6 support");
#endif
}
