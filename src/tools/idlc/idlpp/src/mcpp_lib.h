// Copyright(c) 2019 Jeroen Koekkoek <jeroen@koekkoek.nl>
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef MCPP_LIB_H
#define MCPP_LIB_H

#include "mcpp_out.h"
#include "mcpp_export.h"

MCPP_EXPORT int mcpp_lib_main(int argc, char **argv);

MCPP_EXPORT void mcpp_reset_def_out_func(void);

MCPP_EXPORT void mcpp_set_out_func(
  int (*func_fputc)(int chr, MCPP_OUTDEST od),
  int (*func_fputs)(const char *str, MCPP_OUTDEST od),
  int (*func_fprintf)(MCPP_OUTDEST od, const char *fmt, ...));

MCPP_EXPORT void mcpp_use_mem_buffers(int tf);

MCPP_EXPORT char *mcpp_get_mem_buffer(MCPP_OUTDEST od);

#endif /* MCPP_LIB_H */
