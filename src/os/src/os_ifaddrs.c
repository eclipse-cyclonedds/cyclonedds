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
#include "os/os.h"

void
os_freeifaddrs(os_ifaddrs_t *ifa)
{
    os_ifaddrs_t *next;

    while (ifa != NULL) {
        next = ifa->next;
        os_free(ifa->name);
        os_free(ifa->addr);
        os_free(ifa->netmask);
        os_free(ifa->broadaddr);
        os_free(ifa);
        ifa = next;
    }
}

