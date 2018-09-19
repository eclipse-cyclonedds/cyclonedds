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

_Ret_z_
_Check_return_
char *
os_strdup(
    _In_z_ const char *s1)
{
    size_t len;
    char *dup;

    len = strlen(s1) + 1;
    dup = os_malloc(len);
    memcpy(dup, s1, len);

    return dup;
}
