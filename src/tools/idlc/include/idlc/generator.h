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
#ifndef IDLC_GENERATOR_H
#define IDLC_GENERATOR_H

#include <stdint.h>

#include "idl/processor.h"
#include "idlc/options.h"

#if defined (__cplusplus)
extern "C" {
#endif

#define IDLC_GENERATOR_OPTIONS generator_options
#define IDLC_GENERATOR_ANNOTATIONS generator_annotations
#define IDLC_GENERATE generate

typedef const idlc_option_t **(*idlc_generator_options_t)(void);
typedef const idl_builtin_annotation_t **(*idlc_generator_annotations_t)(void);
typedef int(*idlc_generate_t)(const idl_pstate_t *);

#if defined(__cplusplus)
}
#endif

#endif /* IDLC_GENERATOR_H */
