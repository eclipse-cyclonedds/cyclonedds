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
#include <string.h>

#include "os/os.h"

os_result
os_gethostname(
    char *hostname,
    size_t buffersize)
{
    os_result result;
    char hostnamebuf[MAXHOSTNAMELEN];

    if (gethostname (hostnamebuf, MAXHOSTNAMELEN) == 0) {
        if ((strlen(hostnamebuf)+1) > buffersize) {
            result = os_resultFail;
        } else {
            os_strlcpy (hostname, hostnamebuf, buffersize);
            result = os_resultSuccess;
        }
    } else {
        result = os_resultFail;
    }

    return result;
}
