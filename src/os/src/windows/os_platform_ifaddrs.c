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

#include "os/os.h"

extern const int *const os_supp_afs;

static int
getifaces(PIP_ADAPTER_ADDRESSES *ptr)
{
    int err = ERANGE;
    PIP_ADAPTER_ADDRESSES buf = NULL;
    ULONG bufsz = 0; /* Size is determined on first iteration. */
    ULONG ret;
    size_t i;

    static const size_t max = 2;
    static const ULONG filter = GAA_FLAG_INCLUDE_PREFIX |
                                GAA_FLAG_SKIP_ANYCAST |
                                GAA_FLAG_SKIP_MULTICAST |
                                GAA_FLAG_SKIP_DNS_SERVER;

    assert(ptr != NULL);

    for (i = 0; err == ERANGE && i < max; i++) {
        ret = GetAdaptersAddresses(AF_UNSPEC, filter, NULL, buf, &bufsz);
        assert(ret != ERROR_INVALID_PARAMETER);
        switch (ret) {
            case ERROR_BUFFER_OVERFLOW:
                err = ERANGE;
                os_free(buf);
                if ((buf = (IP_ADAPTER_ADDRESSES *)os_malloc(bufsz)) == NULL) {
                    err = ENOMEM;
                }
                break;
            case ERROR_NOT_ENOUGH_MEMORY:
                err = ENOMEM;
                break;
            case ERROR_SUCCESS:
            case ERROR_ADDRESS_NOT_ASSOCIATED: /* No address associated yet. */
            case ERROR_NO_DATA: /* No adapters that match the filter. */
            default:
                err = 0;
                break;
        }
    }

    if (err == 0) {
        *ptr = buf;
    }

    return err;
}

static int
getaddrtable(PMIB_IPADDRTABLE *ptr)
{
    int err = ERANGE;
    PMIB_IPADDRTABLE buf = NULL;
    ULONG bufsz = 0;
    DWORD ret;
    size_t i;

    static const size_t max = 2;

    assert(ptr != NULL);

    for (i = 0; err == ERANGE && i < max; i++) {
        ret = GetIpAddrTable(buf, &bufsz, 0);
        assert(ret != ERROR_INVALID_PARAMETER &&
               ret != ERROR_NOT_SUPPORTED);
        switch (ret) {
            case ERROR_INSUFFICIENT_BUFFER:
                err = ERANGE;
                os_free(buf);
                if ((buf = (PMIB_IPADDRTABLE)os_malloc(bufsz)) == NULL) {
                    err = ENOMEM;
                }
                break;
            case ERROR_SUCCESS:
            default:
                err = GetLastError();
                break;
        }
    }

    if (err == 0) {
        *ptr = buf;
    } else {
        os_free(buf);
    }

    return err;
}

static uint32_t
getflags(const PIP_ADAPTER_ADDRESSES iface)
{
    uint32_t flags = 0;

    if (iface->OperStatus == IfOperStatusUp) {
        flags |= IFF_UP;
    }
    if (!(iface->Flags & IP_ADAPTER_NO_MULTICAST) && iface->IfType != IF_TYPE_SOFTWARE_LOOPBACK) {
        /* multicast over loopback doesn't seem to work despite the NO_MULTICAST flag being clear
           assuming an interface is multicast-capable when in fact it isn't is disastrous, so it
           makes more sense to err by assuming it won't work as there is always the
           AssumeMulticastCapable setting to overrule it */
        flags |= IFF_MULTICAST;
    }

    switch (iface->IfType) {
        case IF_TYPE_SOFTWARE_LOOPBACK:
            flags |= IFF_LOOPBACK;
            break;
        case IF_TYPE_ETHERNET_CSMACD:
        case IF_TYPE_IEEE80211:
        case IF_TYPE_IEEE1394:
        case IF_TYPE_ISO88025_TOKENRING:
            flags |= IFF_BROADCAST;
            break;
        default:
            flags |= IFF_POINTTOPOINT;
            break;
    }

    return flags;
}

static int
copyaddr(
    os_ifaddrs_t **ifap,
    const PIP_ADAPTER_ADDRESSES iface,
    const PMIB_IPADDRTABLE addrtable,
    const PIP_ADAPTER_UNICAST_ADDRESS addr)
{
    int err = 0;
    int eq = 0;
    os_ifaddrs_t *ifa;
    DWORD i;
    struct sockaddr *sa = (struct sockaddr *)addr->Address.lpSockaddr;
    size_t size;

    assert(iface != NULL);
    assert(addrtable != NULL);
    assert(addr != NULL);

    if ((ifa = os_calloc_s(1, sizeof(*ifa))) == NULL) {
        err = ENOMEM;
    } else {
        ifa->flags = getflags(iface);
        (void)os_asprintf(&ifa->name, "%wS", iface->FriendlyName);

        if (ifa->name == NULL) {
            err = ENOMEM;
        } else {
            ifa->addr = os_memdup(sa, addr->Address.iSockaddrLength);
            if (ifa->addr == NULL) {
                err = ENOMEM;
            } else if (ifa->addr->sa_family == AF_INET) {
                size = sizeof(struct sockaddr_in);
                struct sockaddr_in netmask, broadaddr;

                memset(&netmask, 0, size);
                memset(&broadaddr, 0, size);
                netmask.sin_family = broadaddr.sin_family = AF_INET;

                for (i = 0; !eq && i < addrtable->dwNumEntries;  i++) {
                    eq = (((struct sockaddr_in *)sa)->sin_addr.s_addr ==
                        addrtable->table[i].dwAddr);
                    if (eq) {
                        ifa->index = addrtable->table[i].dwIndex;
                        netmask.sin_addr.s_addr = addrtable->table[i].dwMask;
                        broadaddr.sin_addr.s_addr =
                            ((struct sockaddr_in *)sa)->sin_addr.s_addr | ~(netmask.sin_addr.s_addr);
                    }
                }

                assert(eq != 0);

                if ((ifa->netmask = os_memdup(&netmask, size)) == NULL ||
                    (ifa->broadaddr = os_memdup(&broadaddr, size)) == NULL)
                {
                    err = ENOMEM;
                }
            } else {
                ifa->index = iface->Ipv6IfIndex;
            }
        }
    }

    if (err == 0) {
        *ifap = ifa;
    } else {
        os_free(ifa);
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
    PIP_ADAPTER_ADDRESSES ifaces = NULL, iface;
    PIP_ADAPTER_UNICAST_ADDRESS addr = NULL;
    PMIB_IPADDRTABLE addrtable = NULL;
    os_ifaddrs_t *ifa, *ifa_root, *ifa_next;
    struct sockaddr *sa;

    assert(ifap != NULL);

    if (afs == NULL) {
        afs = os_supp_afs;
    }

    ifa = ifa_root = ifa_next = NULL;

    if ((err = getifaces(&ifaces)) == 0 &&
        (err = getaddrtable(&addrtable)) == 0)
    {
        for (iface = ifaces; !err && iface != NULL; iface = iface->Next) {
            for (addr = iface->FirstUnicastAddress;
                 addr != NULL;
                 addr = addr->Next)
            {
                sa = (struct sockaddr *)addr->Address.lpSockaddr;
                use = 0;
                for (int i = 0; !use && afs[i] != OS_AF_NULL; i++) {
                    use = (afs[i] == sa->sa_family);
                }

                if (use) {
                    err = copyaddr(&ifa_next, iface, addrtable, addr);
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
    }

    os_free(ifaces);
    os_free(addrtable);

    if (err == 0) {
        *ifap = ifa_root;
    } else {
        os_freeifaddrs(ifa_root);
    }

    return err;
}
