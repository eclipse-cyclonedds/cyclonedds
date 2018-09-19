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
#include <string.h>

#if defined(__linux)
#include <linux/if_packet.h> /* sockaddr_ll */
#endif /* __linux */

#include "os/os.h"

static int
copy_ifaddrs(os_ifaddrs_t **ifap, const struct ifaddrs *sys_ifa)
{
    int err = 0;
    os_ifaddrs_t *ifa;
    size_t size;

    assert(ifap != NULL);
    assert(sys_ifa != NULL);
    assert(sys_ifa->ifa_addr->sa_family == AF_INET ||
           sys_ifa->ifa_addr->sa_family == AF_INET6 ||
           sys_ifa->ifa_addr->sa_family == AF_PACKET);

    ifa = os_malloc(sizeof(*ifa));
    if (ifa == NULL) {
        err = errno;
    } else {
        (void)memset(ifa, 0, sizeof(*ifa));

        ifa->index = if_nametoindex(sys_ifa->ifa_name);
        ifa->flags = sys_ifa->ifa_flags;
        if ((ifa->name = os_strdup(sys_ifa->ifa_name)) == NULL) {
            err = errno;
        } else if (sys_ifa->ifa_addr->sa_family == AF_INET6) {
          size = sizeof(struct sockaddr_in6);
          if (!(ifa->addr = os_memdup(sys_ifa->ifa_addr, size))) {
              err = errno;
          }
        } else {
            if (sys_ifa->ifa_addr->sa_family == AF_INET) {
                size = sizeof(struct sockaddr_in);
            } else {
                assert(sys_ifa->ifa_addr->sa_family == AF_PACKET);
                size = sizeof(struct sockaddr_ll);
            }

            if (!(ifa->addr = os_memdup(sys_ifa->ifa_addr, size)) ||
                (sys_ifa->ifa_netmask &&
                  !(ifa->netmask = os_memdup(sys_ifa->ifa_netmask, size))) ||
                (sys_ifa->ifa_broadaddr &&
                  !(ifa->broadaddr = os_memdup(sys_ifa->ifa_broadaddr, size))))
            {
                err = errno;
            }
        }
    }

    if (err == 0) {
        *ifap = ifa;
    } else {
        os_freeifaddrs(ifa);
    }

    return err;
}

_Success_(return == 0) int
os_getifaddrs(
    _Inout_ os_ifaddrs_t **ifap,
    _In_opt_ const os_ifaddr_filter_t *ifltr)
{
    int err = 0;
    os_ifaddrs_t *ifa, *ifa_root, *ifa_next;
    struct ifaddrs *sys_ifa, *sys_ifa_root;
    struct sockaddr *addr;

    assert(ifap != NULL);
    assert(ifltr != NULL);

    if (getifaddrs(&sys_ifa_root) == -1) {
        err = errno;
    } else {
        ifa = ifa_root = NULL;

        for (sys_ifa = sys_ifa_root;
             sys_ifa != NULL && err == 0;
             sys_ifa = sys_ifa->ifa_next)
        {
            addr = sys_ifa->ifa_addr;
            if ((addr->sa_family == AF_PACKET && ifltr->af_packet)
             || (addr->sa_family == AF_INET && ifltr->af_inet)
             || (addr->sa_family == AF_INET6 && ifltr->af_inet6 &&
                 !IN6_IS_ADDR_UNSPECIFIED(
                     &((struct sockaddr_in6 *)addr)->sin6_addr)))
            {
                err = copy_ifaddrs(&ifa_next, sys_ifa);
                if (err == 0) {
                    if (ifa == NULL) {
                        ifa = ifa_root = ifa_next;
                    } else {
                        ifa->next = ifa_next;
                        ifa = ifa_next;
                    }
                }
            }
        }

        freeifaddrs(sys_ifa_root);

        if (err == 0) {
            *ifap = ifa_root;
        } else {
            os_freeifaddrs(ifa_root);
        }
    }

    return err;
}
