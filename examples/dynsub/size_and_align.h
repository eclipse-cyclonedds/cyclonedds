/*
 * Copyright(c) 2026 ZettaScale Technology and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */

#ifndef SIZE_AND_ALIGN_H
#define SIZE_AND_ALIGN_H

#include "dyntypelib.h"

void *dtl_align (unsigned char *base, size_t *off, size_t align, size_t size);
bool dtl_simple_alignof_sizeof (const uint8_t disc, size_t *align, size_t *size);
size_t dtl_simple_size (const uint8_t disc);
size_t dtl_simple_align (const uint8_t disc);
bool dtl_is_unbounded_string_ti (const DDS_XTypes_TypeIdentifier *typeid);
bool dtl_is_bounded_string_ti (const DDS_XTypes_TypeIdentifier *typeid);
bool dtl_is_unbounded_string_to (const DDS_XTypes_CompleteTypeObject *typeobj);
size_t dtl_bounded_string_bound_ti (const DDS_XTypes_TypeIdentifier *typeid);
void *dtl_advance_string_ti (unsigned char *base, size_t *off, const DDS_XTypes_TypeIdentifier *typeid);
void *dtl_advance_simple (unsigned char *base, size_t *off, const uint8_t disc);
void *dtl_advance_ti (struct dyntypelib *dtl, unsigned char *base, size_t *off, const DDS_XTypes_TypeIdentifier *typeid, bool is_opt_or_ext);
void *dtl_advance_to (struct dyntypelib *dtl, unsigned char *base, size_t *off, const DDS_XTypes_CompleteTypeObject *typeobj, bool is_opt_or_ext);
size_t dtl_get_typeid_size (struct dyntypelib *dtl, DDS_XTypes_TypeIdentifier const * const typeid);
size_t dtl_get_typeid_align (struct dyntypelib *dtl, DDS_XTypes_TypeIdentifier const * const typeid);
size_t dtl_get_typeobj_size (struct dyntypelib *dtl, DDS_XTypes_CompleteTypeObject const * const typeobj);

#endif
