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
#define _GNU_SOURCE

#include <assert.h>
#include <string.h>

#include "os/os.h"

int
os_gethostbyname(
    const char *name,
    int af,
    os_hostent_t **hentp)
{
    int err = 0;
    int gai_err = 0;
    struct addrinfo hints, *res = NULL;
    os_hostent_t *hent = NULL;

    assert(name != NULL);
    assert(hentp != NULL);

    switch (af) {
#if OS_SOCKET_HAS_IPV6
        case AF_INET6:
#endif
        case AF_INET:
        case AF_UNSPEC:
            break;
        default:
            return EAFNOSUPPORT;
    }

    /* Windows returns all registered addresses on the local computer if the
       "nodename" parameter is an empty string. *NIX return HOST_NOT_FOUND.
       Deny empty hostnames to keep behavior across platforms consistent. */
    if (strlen(name) == 0) {
        return OS_HOST_NOT_FOUND;
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = af;

    gai_err = getaddrinfo(name, NULL, &hints, &res);
    /* gai_strerror cannot be used because Windows does not offer a thread-safe
       implementation and lwIP (there maybe others as well) does not offer an
       implementation at all.

       NOTE: Error codes returned by getaddrinfo map directly onto Windows
             Socket error codes and WSAGetLastError can be used instead. */
    DDS_TRACE("getaddrinfo for %s returned %d\n", name, gai_err);
    switch (gai_err) {
#if defined(EAI_AGAIN)
        case EAI_AGAIN:
            /* Name server returned a temporary failure indication. */
            err = OS_TRY_AGAIN;
            break;
#endif
        case EAI_FAIL:
            /* Name server returned a permanent failure indication. */
            err = OS_NO_RECOVERY;
            break;
/* Windows defines EAI_NODATA to EAI_NONAME. */
#if defined(EAI_NODATA) && (EAI_NODATA != EAI_NONAME)
        case EAI_NODATA:
            /* Host exists, but does not have any network addresses defined. */
            err = OS_NO_DATA;
            break;
#endif
#if defined(EAI_ADDRFAMILY)
        case EAI_ADDRFAMILY: /* Host has no addresses in requested family. */
#endif
#if defined(EAI_NOSECURENAME) /* Windows */
        case EAI_NOSECURENAME:
#endif
        case EAI_NONAME:
            /* Host does not exist. */
            err = OS_HOST_NOT_FOUND;
            break;
        case EAI_MEMORY:
            /* Out of memory. */
            err = ENOMEM;
            break;
#if defined(EAI_SYSTEM)
        case EAI_SYSTEM:
            /* Other system error. */
            err = errno;
            break;
#endif
        case EAI_BADFLAGS: /* Invalid flags in hints.ai_flags. */
        case EAI_FAMILY: /* Address family not supported. */
        case EAI_SERVICE: /* Service not available for socket type. */
        case EAI_SOCKTYPE: /* Socket type not supported. */
        case 0:
        {
            struct addrinfo *ai;
            size_t addrno, naddrs, size;

            assert(gai_err == 0);
            assert(res != NULL);

            naddrs = 0;
            for (ai = res; ai != NULL; ai = ai->ai_next) {
                naddrs++;
            }

            size = sizeof(*hent) + (naddrs * sizeof(hent->addrs[0]));
            if ((hent = os_malloc_0_s(size)) != NULL) {
                hent->naddrs = naddrs;
                for (addrno = 0, ai = res;
                     addrno < naddrs && ai != NULL;
                     addrno++, ai = ai->ai_next)
                {
                    memcpy(&hent->addrs[addrno], res->ai_addr, res->ai_addrlen);
                }
            } else {
                err = ENOMEM;
            }
        }
            break;
        default:
            DDS_FATAL("getaddrinfo returned unkown error %d\n", gai_err);
    }

    if (res != NULL) {
        freeaddrinfo(res);
    }

    if (err == 0) {
        *hentp = hent;
    } else {
        os_free(hent);
    }

    return err;
}
