// Copyright(c) 2006 to 2019 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>
#include "dds/ddsrt/cdtors.h"
#include "dds/ddsrt/ifaddrs.h"
#include "dds/ddsrt/retcode.h"
#include "CUnit/Test.h"

/* FIXME: It's not possible to predict what network interfaces are available
          on a given host. To properly test all combinations the abstracted
          operating system functions must be mocked. */

/* FIXME: It's possible that IPv6 is available in the network stack, but
          disabled in the kernel. Travis CI for example has build environments
          that do not have IPv6 enabled. */

#ifdef DDSRT_HAVE_IPV6
static int ipv6_enabled = 1;
#endif

CU_Init(ddsrt_getifaddrs)
{
  ddsrt_init();

#ifdef DDSRT_HAVE_IPV6
#ifdef __linux
  FILE *fh;
  const char *const *path;
  static const char *const paths[] = {
    "/proc/sys/net/ipv6/conf/all/disable_ipv6",
    "/proc/sys/net/ipv6/conf/default/disable_ipv6",
    NULL
  };

  for (path = paths; ipv6_enabled == 1 && *path != NULL; path++) {
    if ((fh = fopen(*path, "r")) != NULL) {
      ipv6_enabled = (fgetc(fh) == '0');
      fclose(fh);
      fh = NULL;
    }
  }
#endif /* __linux */
#endif /* DDSRT_HAVE_IPV6 */

    return 0;
}

CU_Clean(ddsrt_getifaddrs)
{
  ddsrt_fini();
  return 0;
}

/* Assume every test machine has at least one IPv4 enabled interface. This
   simple test verifies an interface can at least be found and that the
   IFF_LOOPBACK flags are properly set. */
CU_Test(ddsrt_getifaddrs, ipv4)
{
  dds_return_t ret;
  int seen = 0;
  ddsrt_ifaddrs_t *ifa_root, *ifa;
  const int afs[] = { AF_INET, DDSRT_AF_TERM };

  ret = ddsrt_getifaddrs(&ifa_root, afs);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  for (ifa = ifa_root; ifa; ifa = ifa->next) {
    CU_ASSERT_PTR_NOT_EQUAL_FATAL(ifa->addr, NULL);
    assert (ifa->addr != NULL); /* for the benefit of clang's static analyzer */
    CU_ASSERT_EQUAL(ifa->addr->sa_family, AF_INET);
    if (ifa->addr->sa_family == AF_INET) {
      if (ifa->flags & IFF_LOOPBACK) {
        CU_ASSERT(ddsrt_sockaddr_isloopback(ifa->addr));
      } else {
        CU_ASSERT(!ddsrt_sockaddr_isloopback(ifa->addr));
      }
      seen = 1;
    }
  }

  CU_ASSERT_EQUAL(seen, 1);
  ddsrt_freeifaddrs(ifa_root);
}

CU_Test(ddsrt_getifaddrs, null_filter)
{
  dds_return_t ret;
  int cnt = 0;
  ddsrt_ifaddrs_t *ifa_root, *ifa;

  ret = ddsrt_getifaddrs(&ifa_root, NULL);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  for (ifa = ifa_root; ifa; ifa = ifa->next) {
    CU_ASSERT_PTR_NOT_EQUAL_FATAL(ifa->addr, NULL);
    cnt++;
  }

  CU_ASSERT(cnt > 0);
  ddsrt_freeifaddrs(ifa_root);
}

CU_Test(ddsrt_getifaddrs, empty_filter)
{
  dds_return_t ret;
  ddsrt_ifaddrs_t *ifa_root;
  const int afs[] = { DDSRT_AF_TERM };

  ret = ddsrt_getifaddrs(&ifa_root, afs);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  CU_ASSERT_PTR_EQUAL(ifa_root, NULL);
  ddsrt_freeifaddrs(ifa_root);
}

CU_Test(ddsrt_getifaddrs, ipv6)
{
#ifdef DDSRT_HAVE_IPV6
  if (ipv6_enabled == 1) {
    dds_return_t ret;
    int have_ipv6 = 0;
    ddsrt_ifaddrs_t *ifa_root, *ifa;
    const int afs[] = { AF_INET6, DDSRT_AF_TERM };

    ret = ddsrt_getifaddrs(&ifa_root, afs);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    for (ifa = ifa_root; ifa; ifa = ifa->next) {
      CU_ASSERT_PTR_NOT_EQUAL_FATAL(ifa->addr, NULL);
      assert (ifa->addr != NULL); /* for the benefit of clang's static analyzer */
      CU_ASSERT_EQUAL(ifa->addr->sa_family, AF_INET6);
      if (ifa->addr->sa_family == AF_INET6) {
        have_ipv6 = 1;
        /* macOS assigns a link-local address to the loopback interface, so
           the loopback address must be assigned to the loopback interface,
           but the loopback interface can have addresses other than the
           loopback address assigned. */
        if (ddsrt_sockaddr_isloopback(ifa->addr)) {
          CU_ASSERT(ifa->flags & IFF_LOOPBACK);
        }
      }
    }

    CU_ASSERT_EQUAL(have_ipv6, 1);
    ddsrt_freeifaddrs(ifa_root);
    CU_PASS("IPv6 enabled in test environment");
  } else {
    CU_PASS("IPv6 disabled in test environment");
  }
#else
  CU_PASS("IPv6 is not supported");
#endif
}

/* Assume at least one IPv4 and one IPv6 interface are available when IPv6 is
   available on the platform. */
CU_Test(ddsrt_getifaddrs, ipv4_n_ipv6)
{
#if DDSRT_HAVE_IPV6
  if (ipv6_enabled == 1) {
    dds_return_t ret;
    int have_ipv4 = 0;
    int have_ipv6 = 0;
    ddsrt_ifaddrs_t *ifa_root, *ifa;
    const int afs[] = { AF_INET, AF_INET6, DDSRT_AF_TERM };

    ret = ddsrt_getifaddrs(&ifa_root, afs);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    for (ifa = ifa_root; ifa; ifa = ifa->next) {
      CU_ASSERT_PTR_NOT_EQUAL_FATAL(ifa->addr, NULL);
      assert (ifa->addr != NULL); /* for the benefit of clang's static analyzer */
      CU_ASSERT(ifa->addr->sa_family == AF_INET ||
                ifa->addr->sa_family == AF_INET6);
      if (ifa->addr->sa_family == AF_INET) {
        have_ipv4 = 1;
      } else if (ifa->addr->sa_family == AF_INET6) {
        have_ipv6 = 1;
      }
    }

    CU_ASSERT_EQUAL(have_ipv4, 1);
    CU_ASSERT_EQUAL(have_ipv6, 1);
    ddsrt_freeifaddrs(ifa_root);
    CU_PASS("IPv6 enabled in test environment");
  } else {
    CU_PASS("IPv6 disabled in test environment");
  }
#else
  CU_PASS("IPv6 is not supported");
#endif /* DDSRT_HAVE_IPV6 */
}

