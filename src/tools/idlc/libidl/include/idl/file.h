// Copyright(c) 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef IDL_FILE_H
#define IDL_FILE_H

#include "idl/export.h"
#include "idl/retcode.h"

#define IDL_GEN_OUTFILE(path, output_dir, base_dir, out_ext, out_ptr) \
  idl_generate_out_file(path, output_dir, base_dir, out_est, out_ptr, false)

IDL_EXPORT unsigned int
idl_isseparator(int chr);

IDL_EXPORT unsigned int
idl_isabsolute(const char *path);

IDL_EXPORT idl_retcode_t
idl_current_path(char **abspathp);

IDL_EXPORT idl_retcode_t
idl_normalize_path(const char *path, char **abspathp);

IDL_EXPORT idl_retcode_t
idl_relative_path(const char *base, const char *path, char **relpathp);

IDL_EXPORT idl_retcode_t
idl_mkpath(const char *path);

IDL_EXPORT idl_retcode_t
idl_generate_out_file(const char *path, const char *output_dir, const char *base_dir, const char *out_ext, char ** out_ptr, int skip_mkpath);

#endif /* IDL_FILE_H */
