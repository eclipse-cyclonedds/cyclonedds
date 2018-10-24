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
#include "ddsi_eth.h"

int ddsi_eth_enumerate_interfaces(ddsi_tran_factory_t fact, os_ifaddrs_t **ifs)
{
    int afs[] = { AF_INET, OS_AF_NULL };

    (void)fact;

#if OS_SOCKET_HAVE_IPV6
    if (config.transport_selector == TRANS_TCP6 ||
        config.transport_selector == TRANS_UDP6)
    {
      afs[0] = AF_INET6;
    }
#endif /* OS_SOCKET_HAVE_IPV6 */

    return -os_getifaddrs(ifs, afs);
}
