// Copyright(c) 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef ANNOTATION_H
#define ANNOTATION_H

#include "idl/processor.h"

extern const idl_builtin_annotation_t *builtin_annotations;

idl_retcode_t
idl_annotate(
  idl_pstate_t *pstate,
  void *node,
  idl_annotation_appl_t *annotations);

#endif /* ANNOTATION_H */
