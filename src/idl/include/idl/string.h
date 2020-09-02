/*
 * Copyright(c) 2020 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#ifndef IDL_STRING_H
#define IDL_STRING_H

#include <stdarg.h>
#include <stddef.h>

#include "idl/export.h"

IDL_EXPORT int idl_strcasecmp(const char *s1, const char *s2);

IDL_EXPORT int idl_strncasecmp(const char *s1, const char *s2, size_t n);

IDL_EXPORT int idl_asprintf(char **strp, const char *fmt, ...);

IDL_EXPORT int idl_vasprintf(char **strp, const char *fmt, va_list ap);

#endif /* IDL_STRING_H */
