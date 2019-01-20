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

#include "os/os.h"

extern const int *const os_supp_afs;

static int
copyaddr(os_ifaddrs_t **ifap, const struct ifaddrs *sys_ifa)
{
    int err = 0;
    os_ifaddrs_t *ifa;
    size_t sz;

    assert(ifap != NULL);
    assert(sys_ifa != NULL);

    sz = os_sockaddr_get_size(sys_ifa->ifa_addr);
    ifa = os_calloc_s(1, sizeof(*ifa));
    if (ifa == NULL) {
        err = errno;
    } else {
        ifa->index = if_nametoindex(sys_ifa->ifa_name);
        ifa->flags = sys_ifa->ifa_flags;
        if ((ifa->name = os_strdup(sys_ifa->ifa_name)) == NULL ||
            (ifa->addr = os_memdup(sys_ifa->ifa_addr, sz)) == NULL ||
              (sys_ifa->ifa_netmask != NULL &&
            (ifa->netmask = os_memdup(sys_ifa->ifa_netmask, sz)) == NULL) ||
              (sys_ifa->ifa_broadaddr != NULL &&
              (sys_ifa->ifa_flags & IFF_BROADCAST) &&
            (ifa->broadaddr = os_memdup(sys_ifa->ifa_broadaddr, sz)) == NULL))
        {
            err = errno;
        }
        /* Seen on macOS using OpenVPN: netmask without an address family,
           in which case copy it from the interface address */
        if (ifa->addr && ifa->netmask && ifa->netmask->sa_family == 0) {
            ifa->netmask->sa_family = ifa->addr->sa_family;
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
    _In_opt_ const int *afs)
{
    int err = 0;
    int use;
    os_ifaddrs_t *ifa, *ifa_root, *ifa_next;
    struct ifaddrs *sys_ifa, *sys_ifa_root;
    struct sockaddr *sa;

    assert(ifap != NULL);

    if (afs == NULL) {
        afs = os_supp_afs;
    }

    if (getifaddrs(&sys_ifa_root) == -1) {
        err = errno;
    } else {
        ifa = ifa_root = NULL;

        for (sys_ifa = sys_ifa_root;
             sys_ifa != NULL && err == 0;
             sys_ifa = sys_ifa->ifa_next)
        {
            sa = sys_ifa->ifa_addr;
            if (sa != NULL) {
                use = 0;
                for (int i = 0; !use && afs[i] != OS_AF_NULL; i++) {
                    use = (sa->sa_family == afs[i]);
                }

                if (use) {
                    err = copyaddr(&ifa_next, sys_ifa);
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
