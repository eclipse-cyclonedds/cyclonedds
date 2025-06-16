
#include <assert.h>
#include <string.h>

#include "dds/ddsrt/log.h"
#include "dds/ddsrt/misc.h"
#include "sockets_priv.h"

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/io.h"
#include "dds/ddsrt/ifaddrs.h"
#include "dds/ddsrt/retcode.h"
#include "dds/ddsrt/string.h"

extern const int *const os_supp_afs;


static dds_return_t copyaddr(ddsrt_ifaddrs_t **ifap)
{
    dds_return_t rc = DDS_RETCODE_OK;
    ddsrt_ifaddrs_t *ifa;
    struct sockaddr sa = {0U};

    assert(ifap != NULL);

    /* note: local IP shoule be got after STACK up! */
    u32 lip, netmask, bcip;
    bcip = ipBROADCAST_IP_ADDRESS;
    FreeRTOS_GetAddressConfiguration(&lip, &netmask, NULL, NULL);

    ifa = ddsrt_calloc(1, sizeof(*ifa));
    if(ifa == NULL)
    {
        rc = DDS_RETCODE_OUT_OF_RESOURCES;
        goto __exit;
    }

    ifa->addr = ddsrt_calloc(1, sizeof(struct sockaddr));
    ifa->netmask = ddsrt_calloc(1, sizeof(struct sockaddr));
    ifa->broadaddr = ddsrt_calloc(1, sizeof(struct sockaddr));
    if ((ifa->addr == NULL)
        || (ifa->netmask == NULL)
        || (ifa->broadaddr == NULL))
    {
        rc = DDS_RETCODE_OUT_OF_RESOURCES;
        goto __exit;
    }

    sa.sa_len = sizeof(struct sockaddr);
    sa.sa_family = AF_INET;
    sa.sin_addr = (lip);        /* storage IP, no need. FreeRTOS_htonl */
    memcpy((void*)ifa->addr, &sa, sizeof(struct sockaddr));

    sa.sin_addr = (netmask);
    memcpy((void*)ifa->netmask, &sa, sizeof(struct sockaddr));

    sa.sin_addr = (bcip);
    memcpy((void*)ifa->broadaddr, &sa, sizeof(struct sockaddr));

    ifa->next = NULL;
    ifa->type = DDSRT_IFTYPE_WIRED;
    ifa->name = "eqos0";
    ifa->index = 0;
    ifa->flags = IFF_UP | IFF_BROADCAST;    // | IFF_MULTICAST;

__exit:
    if (rc == DDS_RETCODE_OK)
    {
        *ifap = ifa;
    }
    else
    {
        ddsrt_freeifaddrs(ifa);
        *ifap = NULL;
    }

    return rc;
}

dds_return_t ddsrt_getifaddrs(ddsrt_ifaddrs_t **ifap, const int *afs)
{
    dds_return_t rc = DDS_RETCODE_OK;
    int use_ip4 = 0;
    int use_ip6 = 0;

    assert(ifap != NULL);

    if (afs == NULL)
    {
        afs = os_supp_afs;
    }

    for (int i = 0; afs[i] != DDSRT_AF_TERM; i++)
    {
        if (afs[i] == AF_INET)
        {
            use_ip4 = 1;
        }
#ifdef DDSRT_HAVE_IPV6
        else if (afs[i] == AF_INET6)
        {
            use_ip6 = 1;
        }
#endif
    }

    rc = copyaddr(ifap);

    return rc;
}
