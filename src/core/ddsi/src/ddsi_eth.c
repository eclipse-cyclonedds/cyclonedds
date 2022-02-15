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
#include "dds/ddsi/q_protocol.h" // for NN_LOCATOR_KIND_...
#include "dds/ddsi/ddsi_config_impl.h" // for transport_selector

int ddsi_eth_enumerate_interfaces (ddsi_tran_factory_t fact, enum ddsi_transport_selector transport_selector, ddsrt_ifaddrs_t **ifs)
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
