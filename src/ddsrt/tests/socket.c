// Copyright(c) 2006 to 2019 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include "dds/ddsrt/sockets.h"
#include "dds/ddsrt/cdtors.h"
#include "dds/ddsrt/endian.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/string.h"
#include "CUnit/Theory.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

DDSRT_WARNING_MSVC_OFF(4305)
#if DDSRT_ENDIAN == DDSRT_BIG_ENDIAN
static const struct sockaddr_in ipv4_loopback =
  { .sin_family = AF_INET, .sin_addr = { .s_addr = 0x7f000001 } };
#else
static const struct sockaddr_in ipv4_loopback =
  { .sin_family = AF_INET, .sin_addr = { .s_addr = 0x0100007f } };
#endif /* DDSRT_ENDIAN */
DDSRT_WARNING_MSVC_ON(4305)

#if DDSRT_HAVE_IPV6
static const struct sockaddr_in6 ipv6_loopback =
  { .sin6_family = AF_INET6, .sin6_addr = IN6ADDR_LOOPBACK_INIT };
#endif

static void setup(void)
{
  ddsrt_init();
}

static void teardown(void)
{
  ddsrt_fini();
}

CU_Test(ddsrt_sockaddrfromstr, bad_family)
{
  dds_return_t rc;
  struct sockaddr_storage sa;
  rc = ddsrt_sockaddrfromstr(AF_UNSPEC, "127.0.0.1", &sa);
  CU_ASSERT_EQUAL(rc, DDS_RETCODE_BAD_PARAMETER);
}

static void sockaddrfromstr_test(char *str, int af, dds_return_t exp)
{
  dds_return_t rc;
  struct sockaddr_storage ss;
  rc = ddsrt_sockaddrfromstr(af, str, &ss);
  CU_ASSERT_EQUAL(rc, exp);
  if (rc == DDS_RETCODE_OK) {
    CU_ASSERT_EQUAL(ss.ss_family, af);
  }
}

CU_TheoryDataPoints(ddsrt_sockaddrfromstr, ipv4) = {
  CU_DataPoints(char *, "127.0.0.1", "0.0.0.0",
                        "nip"),
  CU_DataPoints(int, AF_INET, AF_INET,
                     AF_INET),
  CU_DataPoints(dds_return_t, DDS_RETCODE_OK, DDS_RETCODE_OK,
                               DDS_RETCODE_BAD_PARAMETER)
};

CU_Theory((char *str, int af, dds_return_t exp), ddsrt_sockaddrfromstr, ipv4, .init=setup, .fini=teardown)
{
  sockaddrfromstr_test(str, af, exp);
}

CU_TheoryDataPoints(ddsrt_sockaddrfromstr, ipv6) = {
#if DDSRT_HAVE_IPV6
  CU_DataPoints(char *, "127.0.0.1", "::1",
                        "::1",       "::",
                        "nip"),
  CU_DataPoints(int, AF_INET6, AF_INET6,
                     AF_INET,  AF_INET6,
                     AF_INET6),
  CU_DataPoints(dds_return_t, DDS_RETCODE_BAD_PARAMETER, DDS_RETCODE_OK,
                               DDS_RETCODE_BAD_PARAMETER, DDS_RETCODE_OK,
                               DDS_RETCODE_BAD_PARAMETER)
#endif /* DDSRT_HAVE_IPV6 */
};

CU_Theory((char *str, int af, dds_return_t exp), ddsrt_sockaddrfromstr, ipv6, .init=setup, .fini=teardown)
{
#if DDSRT_HAVE_IPV6
  sockaddrfromstr_test(str, af, exp);
#else
  (void)str;
  (void)af;
  (void)exp;
  CU_PASS("IPV6 is not supported");
#endif /* DDSRT_HAVE_IPV6 */
}

CU_Test(ddsrt_sockaddrtostr, bad_sockaddr, .init=setup, .fini=teardown)
{
  dds_return_t rc;
  char buf[128] = { 0 };
  struct sockaddr_in sa;
  memcpy(&sa, &ipv4_loopback, sizeof(ipv4_loopback));
  sa.sin_family = AF_UNSPEC;
  rc = ddsrt_sockaddrtostr(&sa, buf, sizeof(buf));
  CU_ASSERT_EQUAL(rc, DDS_RETCODE_BAD_PARAMETER);
}

CU_Test(ddsrt_sockaddrtostr, no_space, .init=setup, .fini=teardown)
{
  dds_return_t rc;
  char buf[1] = { 0 };
  rc = ddsrt_sockaddrtostr(&ipv4_loopback, buf, sizeof(buf));
  CU_ASSERT_EQUAL(rc, DDS_RETCODE_NOT_ENOUGH_SPACE);
}

CU_Test(ddsrt_sockaddrtostr, ipv4)
{
  dds_return_t rc;
  char buf[128] = { 0 };
  rc = ddsrt_sockaddrtostr(&ipv4_loopback, buf, sizeof(buf));
  CU_ASSERT_EQUAL(rc, DDS_RETCODE_OK);
  CU_ASSERT_STRING_EQUAL(buf, "127.0.0.1");
}

CU_Test(ddsrt_sockaddrtostr, ipv6)
{
#if DDSRT_HAVE_IPV6
  dds_return_t rc;
  char buf[128] = { 0 };
  rc = ddsrt_sockaddrtostr(&ipv6_loopback, buf, sizeof(buf));
  CU_ASSERT_EQUAL(rc, DDS_RETCODE_OK);
  CU_ASSERT_STRING_EQUAL(buf, "::1");
#else
  CU_PASS("IPv6 is not supported");
#endif
}

CU_Test(ddsrt_sockets, gethostname)
{
  dds_return_t rc;
  char sysbuf[200], buf[200];

  buf[0] = '\0';
  rc = ddsrt_gethostname(buf, sizeof(buf));
  CU_ASSERT_EQUAL(rc, DDS_RETCODE_OK);

  sysbuf[0] = '\0';
#if LWIP_SOCKET
  (void) ddsrt_strlcpy(sysbuf, "localhost", sizeof(sysbuf));
#else
  int ret = gethostname(sysbuf, sizeof(sysbuf));
  CU_ASSERT_EQUAL(ret, 0);
#endif
  CU_ASSERT(strcmp(buf, sysbuf) == 0);

  rc = ddsrt_gethostname(buf, strlen(buf) - 1);
  CU_ASSERT_EQUAL(rc, DDS_RETCODE_NOT_ENOUGH_SPACE);
}

#if DDSRT_HAVE_DNS
static void gethostbyname_test(char *name, int af, dds_return_t exp)
{
  dds_return_t rc;
  ddsrt_hostent_t *hent = NULL;
  rc = ddsrt_gethostbyname(name, af, &hent);
  CU_ASSERT_EQUAL(rc, exp);
  if (rc == DDS_RETCODE_OK) {
    CU_ASSERT_FATAL(hent->naddrs > 0);
    if (af != AF_UNSPEC) {
      CU_ASSERT_EQUAL(hent->addrs[0].ss_family, af);
    }
  }
  ddsrt_free(hent);
}
#endif

CU_TheoryDataPoints(ddsrt_gethostbyname, ipv4) = {
  CU_DataPoints(char *,        "",                         "127.0.0.1",    "127.0.0.1"),
  CU_DataPoints(int,           AF_UNSPEC,                  AF_INET,        AF_UNSPEC),
  CU_DataPoints(dds_return_t, DDS_RETCODE_HOST_NOT_FOUND, DDS_RETCODE_OK, DDS_RETCODE_OK)
};

CU_Theory((char *name, int af, dds_return_t exp), ddsrt_gethostbyname, ipv4, .init=setup, .fini=teardown)
{
#if DDSRT_HAVE_DNS
  gethostbyname_test(name, af, exp);
#else
  (void)name;
  (void)af;
  (void)exp;
  CU_PASS("DNS is not supported");
#endif
}

/* Lookup of IPv4 address and specifying AF_INET6 is not invalid as it may
   return an IPV4-mapped IPv6 address. */
CU_TheoryDataPoints(ddsrt_gethostbyname, ipv6) = {
#if DDSRT_HAVE_IPV6 && DDSRT_HAVE_DNS
  CU_DataPoints(char *,        "::1",                      "::1",          "::1"),
  CU_DataPoints(int,           AF_INET,                    AF_INET6,       AF_UNSPEC),
  CU_DataPoints(dds_return_t, DDS_RETCODE_HOST_NOT_FOUND, DDS_RETCODE_OK, DDS_RETCODE_OK)
#endif /* DDSRT_HAVE_IPV6 */
};

CU_Theory((char *name, int af, dds_return_t exp), ddsrt_gethostbyname, ipv6, .init=setup, .fini=teardown)
{
#if DDSRT_HAVE_IPV6 && DDSRT_HAVE_DNS
  gethostbyname_test(name, af, exp);
#else
  (void)name;
  (void)af;
  (void)exp;
  CU_PASS("DNS and IPv6 are not supported");
#endif /* DDSRT_HAVE_IPV6 */
}
