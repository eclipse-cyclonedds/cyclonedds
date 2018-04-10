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
#include <stdio.h>
#include <string.h>
#include "porting.h"

#if NEED_STRSEP
char *strsep(char **str, const char *sep) {
    char *ret;
    if (*str == NULL)
        return NULL;
    ret = *str;
    while (**str && strchr(sep, **str) == 0)
        (*str)++;
    if (**str == '\0') {
        *str = NULL;
    } else {
        **str = '\0';
        (*str)++;
    }
    return ret;
}
#endif
