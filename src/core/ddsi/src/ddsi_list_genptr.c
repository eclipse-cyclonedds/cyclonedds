// Copyright(c) 2006 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include "dds/ddsrt/attributes.h"
#include "dds/ddsrt/heap.h"
#include "ddsi__list_tmpl.h"
#include "ddsi__list_genptr.h"

DDSI_LIST_CODE_TMPL(extern, generic_ptr_list, void *, NULL, ddsrt_malloc, ddsrt_free)
