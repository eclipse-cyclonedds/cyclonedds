// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include "dds/ddsi/ddsi_config.h" // for transport_selector
#include "ddsi__eth.h"

int ddsi_eth_enumerate_interfaces (struct ddsi_tran_factory * fact, enum ddsi_transport_selector transport_selector, ddsrt_ifaddrs_t **ifs)
{
    int afs[] = { AF_INET, DDSRT_AF_TERM };

    (void)fact;
    (void)transport_selector;

#if DDSRT_HAVE_IPV6
    if (transport_selector == DDSI_TRANS_TCP6 ||
        transport_selector == DDSI_TRANS_UDP6)
    {
      afs[0] = AF_INET6;
    }
#endif /* DDSRT_HAVE_IPV6 */

    return ddsrt_getifaddrs(ifs, afs);
}
