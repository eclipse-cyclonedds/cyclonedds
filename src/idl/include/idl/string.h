// Copyright(c) 2021 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef IDL_STRING_H
#define IDL_STRING_H

#include <stdarg.h>
#include <stddef.h>
#include <string.h>

#include "idl/export.h"
#include "idl/attributes.h"

IDL_EXPORT int idl_isalnum(int c);
IDL_EXPORT int idl_isalpha(int c);
IDL_EXPORT int idl_isblank(int c);
IDL_EXPORT int idl_iscntrl(int c);
IDL_EXPORT int idl_isgraph(int c);
IDL_EXPORT int idl_islower(int c);
IDL_EXPORT int idl_isprint(int c);
IDL_EXPORT int idl_ispunct(int c);
IDL_EXPORT int idl_isspace(int c);
IDL_EXPORT int idl_isupper(int c);
IDL_EXPORT int idl_isdigit(int c, int base);
IDL_EXPORT int idl_toupper(int c);
IDL_EXPORT int idl_tolower(int c);

IDL_EXPORT int idl_strcasecmp(const char *s1, const char *s2);

IDL_EXPORT int idl_strncasecmp(const char *s1, const char *s2, size_t n);

IDL_EXPORT char *idl_strdup(const char *str);

IDL_EXPORT char *idl_strndup(const char *str, size_t len);

IDL_EXPORT size_t idl_strlcpy(char * __restrict dest, const char * __restrict src, size_t size);

IDL_EXPORT int idl_snprintf(char *str, size_t size, const char *fmt, ...)
idl_attribute_format_printf(3, 4);

IDL_EXPORT int idl_vsnprintf(char *str, size_t size, const char *fmt, va_list ap);

IDL_EXPORT int idl_asprintf(char **strp, const char *fmt, ...)
idl_attribute_format_printf(2, 3);

IDL_EXPORT int idl_vasprintf(char **strp, const char *fmt, va_list ap);

IDL_EXPORT unsigned long long idl_strtoull(const char *str, char **endptr, int base);

IDL_EXPORT long double idl_strtold(const char *str, char **endptr);

IDL_EXPORT char *idl_strtok_r(char *str, const char *delim, char **saveptr);

#endif /* IDL_STRING_H */
