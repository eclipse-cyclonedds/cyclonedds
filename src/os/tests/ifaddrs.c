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
#include "CUnit/Runner.h"
#include "os/os.h"

/* FIXME: It's not possible to predict what network interfaces are available
          on a given host. To properly test all combinations the abstracted
          operating system functions must be mocked. */

CUnit_Suite_Initialize(os_getifaddrs)
{
    os_osInit();
    return 0;
}

CUnit_Suite_Cleanup(os_getifaddrs)
{
    os_osExit();
    return 0;
}

/* Assume every test machine has at least one IPv4 enabled interface. This
   simple test verifies an interface can at least be found and that the
   IFF_LOOPBACK flags are properly set. */
CUnit_Test(os_getifaddrs, ipv4)
{
    int err;
    int seen = 0;
    os_ifaddrs_t *ifa_root, *ifa;
    const int afs[] = { AF_INET, 0 };

    err = os_getifaddrs(&ifa_root, afs);
    CU_ASSERT_EQUAL_FATAL(err, 0);
    for (ifa = ifa_root; ifa; ifa = ifa->next) {
        CU_ASSERT_EQUAL(ifa->addr->sa_family, AF_INET);
        if (ifa->addr->sa_family == AF_INET) {
            if (ifa->flags & IFF_LOOPBACK) {
                CU_ASSERT(os_sockaddrIsLoopback(ifa->addr));
            } else {
                CU_ASSERT(!os_sockaddrIsLoopback(ifa->addr));
            }
            seen = 1;
        }
    }

    CU_ASSERT_EQUAL(seen, 1);

    os_freeifaddrs(ifa_root);
}

CUnit_Test(os_getifaddrs, null_filter)
{
    int err;
    int cnt = 0;
    os_ifaddrs_t *ifa_root, *ifa;

    err = os_getifaddrs(&ifa_root, NULL);
    CU_ASSERT_EQUAL_FATAL(err, 0);
    for (ifa = ifa_root; ifa; ifa = ifa->next) {
        cnt++;
    }

    CU_ASSERT(cnt > 0);

    os_freeifaddrs(ifa_root);
}

CUnit_Test(os_getifaddrs, empty_filter)
{
    int err;
    os_ifaddrs_t *ifa_root;
    const int afs[] = { 0 };

    err = os_getifaddrs(&ifa_root, afs);
    CU_ASSERT_EQUAL_FATAL(err, 0);
    CU_ASSERT_PTR_EQUAL(ifa_root, NULL);

    os_freeifaddrs(ifa_root);
}

#ifdef OS_SOCKET_HAS_IPV6
CUnit_Test(os_getifaddrs, ipv6)
{
    int err;
    os_ifaddrs_t *ifa_root, *ifa;
    const int afs[] = { AF_INET6, 0 };

    err = os_getifaddrs(&ifa_root, afs);
    CU_ASSERT_EQUAL_FATAL(err, 0);
    for (ifa = ifa_root; ifa; ifa = ifa->next) {
        CU_ASSERT_EQUAL(ifa->addr->sa_family, AF_INET6);
        if (ifa->addr->sa_family == AF_INET6) {
            if (ifa->flags & IFF_LOOPBACK) {
                CU_ASSERT(os_sockaddrIsLoopback(ifa->addr));
            } else {
                CU_ASSERT(!os_sockaddrIsLoopback(ifa->addr));
            }
        }
    }

    os_freeifaddrs(ifa_root);
}

/* Assume at least one IPv4 and one IPv6 interface are available when IPv6 is
   available on the platform. */
CUnit_Test(os_getifaddrs, ipv4_n_ipv6)
{
    int err;
    int have_ipv4 = 0;
    int have_ipv6 = 0;
    os_ifaddrs_t *ifa_root, *ifa;
    const int afs[] = { AF_INET, AF_INET6, 0 };

    err = os_getifaddrs(&ifa_root, afs);
    CU_ASSERT_EQUAL_FATAL(err, 0);
    for (ifa = ifa_root; ifa; ifa = ifa->next) {
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

    os_freeifaddrs(ifa_root);
}

#endif /* OS_SOCKET_HAS_IPV6 */
