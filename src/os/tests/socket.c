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
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "CUnit/Theory.h"

#include "os/os.h"

OS_WARNING_MSVC_OFF(4305)
#if OS_ENDIANNESS == OS_BIG_ENDIAN
static const struct sockaddr_in ipv4_loopback =
    { .sin_family = AF_INET, .sin_addr = { .s_addr = 0x7f000001 } };
#else
static const struct sockaddr_in ipv4_loopback =
    { .sin_family = AF_INET, .sin_addr = { .s_addr = 0x0100007f } };
#endif /* OS_ENDIANNESS */
OS_WARNING_MSVC_ON(4305)

#if OS_SOCKET_HAS_IPV6
static const struct sockaddr_in6 ipv6_loopback =
    { .sin6_family = AF_INET6, .sin6_addr = IN6ADDR_LOOPBACK_INIT };
#endif

static void setup(void)
{
    os_osInit();
}

static void teardown(void)
{
    os_osExit();
}

CU_Test(os_sockaddrfromstr, bad_family)
{
    int err;
    os_sockaddr_storage sa;
    err = os_sockaddrfromstr(AF_UNSPEC, "127.0.0.1", &sa);
    CU_ASSERT_EQUAL(err, EAFNOSUPPORT);
}

static void sockaddrfromstr_test(char *str, int af, int ret)
{
    int err;
    os_sockaddr_storage ss;
    err = os_sockaddrfromstr(af, str, &ss);
    CU_ASSERT_EQUAL(err, ret);
    if (err == 0) {
        CU_ASSERT_EQUAL(ss.ss_family, af);
    }
}

CU_TheoryDataPoints(os_sockaddrfromstr, ipv4) = {
    CU_DataPoints(char *, "127.0.0.1", "0.0.0.0", "nip"),
    CU_DataPoints(int,    AF_INET,     AF_INET,   AF_INET),
    CU_DataPoints(int,    0,           0,         EINVAL)
};

CU_Theory((char *str, int af, int ret), os_sockaddrfromstr, ipv4, .init=setup, .fini=teardown)
{
    sockaddrfromstr_test(str, af, ret);
}

#if OS_SOCKET_HAS_IPV6
CU_TheoryDataPoints(os_sockaddrfromstr, ipv6) = {
    CU_DataPoints(char *, "127.0.0.1", "::1",    "::1",   "::",     "nip"),
    CU_DataPoints(int,    AF_INET6,    AF_INET6, AF_INET, AF_INET6, AF_INET6),
    CU_DataPoints(int,    EINVAL,      0,        EINVAL,  0,        EINVAL)
};

CU_Theory((char *str, int af, int ret), os_sockaddrfromstr, ipv6, .init=setup, .fini=teardown)
{
    sockaddrfromstr_test(str, af, ret);
}
#endif /* OS_SOCKET_HAS_IPV6 */

CU_Test(os_sockaddrtostr, bad_sockaddr, .init=setup, .fini=teardown)
{
    int err;
    char buf[128] = { 0 };
    os_sockaddr_in sa;
    memcpy(&sa, &ipv4_loopback, sizeof(ipv4_loopback));
    sa.sin_family = AF_UNSPEC;
    err = os_sockaddrtostr(&sa, buf, sizeof(buf));
    CU_ASSERT_EQUAL(err, EAFNOSUPPORT);
}

CU_Test(os_sockaddrtostr, no_space, .init=setup, .fini=teardown)
{
    int err;
    char buf[1] = { 0 };
    err = os_sockaddrtostr(&ipv4_loopback, buf, sizeof(buf));
    CU_ASSERT_EQUAL(err, ENOSPC);
}

CU_Test(os_sockaddrtostr, ipv4)
{
    int err;
    char buf[128] = { 0 };
    err = os_sockaddrtostr(&ipv4_loopback, buf, sizeof(buf));
    CU_ASSERT_EQUAL(err, 0);
    CU_ASSERT_STRING_EQUAL(buf, "127.0.0.1");
}

CU_Test(os_sockaddrtostr, ipv6)
{
    int err;
    char buf[128] = { 0 };
    err = os_sockaddrtostr(&ipv6_loopback, buf, sizeof(buf));
    CU_ASSERT_EQUAL(err, 0);
    CU_ASSERT_STRING_EQUAL(buf, "::1");
}

#if OS_SOCKET_HAS_DNS
static void gethostbyname_test(char *name, int af, int ret)
{
    int err;
    os_hostent_t *hent = NULL;
    err = os_gethostbyname(name, af, &hent);
    CU_ASSERT_EQUAL(err, ret);
    if (err == 0) {
        CU_ASSERT_FATAL(hent->naddrs > 0);
        if (af != AF_UNSPEC) {
            CU_ASSERT_EQUAL(hent->addrs[0].ss_family, af);
        }
    }
    os_free(hent);
}

CU_TheoryDataPoints(os_gethostbyname, ipv4) = {
    CU_DataPoints(char *, "",                "127.0.0.1", "127.0.0.1"),
    CU_DataPoints(int,    AF_UNSPEC,         AF_INET,     AF_UNSPEC),
    CU_DataPoints(int,    OS_HOST_NOT_FOUND, 0,           0)
};

CU_Theory((char *name, int af, int ret), os_gethostbyname, ipv4, .init=setup, .fini=teardown)
{
    gethostbyname_test(name, af, ret);
}

#if OS_SOCKET_HAS_IPV6
/* Lookup of IPv4 address and specifying AF_INET6 is not invalid as it may
   return an IPV4-mapped IPv6 address. */
CU_TheoryDataPoints(os_gethostbyname, ipv6) = {
    CU_DataPoints(char *, "::1",             "::1",     "::1"),
    CU_DataPoints(int,    AF_INET,           AF_INET6,  AF_UNSPEC),
    CU_DataPoints(int,    OS_HOST_NOT_FOUND, 0,         0)
};

CU_Theory((char *name, int af, int ret), os_gethostbyname, ipv6, .init=setup, .fini=teardown)
{
    gethostbyname_test(name, af, ret);
}
#endif /* OS_SOCKET_HAS_IPV6 */
#endif /* OS_SOCKET_HAS_DNS */
