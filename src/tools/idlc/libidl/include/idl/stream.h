// Copyright(c) 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef IDL_STREAM_H
#define IDL_STREAM_H

#include <stdarg.h>
#include <stdio.h>

#include "idl/export.h"
#include "idl/attributes.h"

IDL_EXPORT FILE *idl_fopen(const char *pathname, const char *mode);
IDL_EXPORT int   idl_fclose(FILE * file);

IDL_EXPORT int idl_fprintf(FILE *fp, const char *fmt, ...)
idl_attribute_format_printf(2, 3);

IDL_EXPORT int idl_vfprintf(FILE *fp, const char *fmt, va_list ap);

#endif /* IDL_STREAM_H */
