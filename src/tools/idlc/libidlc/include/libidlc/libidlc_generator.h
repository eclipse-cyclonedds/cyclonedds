// Copyright(c) 2020 to 2023 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef LIBIDLC_LIBIDLC_GENERATOR_H
#define LIBIDLC_LIBIDLC_GENERATOR_H

#include <stdint.h>

#include "idl/tree.h"
#include "libidlc_export.h"
#include "idlc/generator.h"

#if defined (__cplusplus)
extern "C" {
#endif

#define IDLC_GENERATOR_OPTIONS generator_options
#define IDLC_GENERATOR_ANNOTATIONS generator_annotations
#define IDLC_GENERATE generate

LIBIDLC_EXPORT idl_retcode_t
generate(const idl_pstate_t *pstate, const idlc_generator_config_t *config);

LIBIDLC_EXPORT const idlc_option_t**
generator_options(void);

#if defined(__cplusplus)
}
#endif

#endif /* LIBIDLC_LIBIDLC_GENERATOR_H */
