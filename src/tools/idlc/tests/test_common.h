// Copyright(c) 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef IDLC_TEST_COMMON_H
#define IDLC_TEST_COMMON_H

#include <stdlib.h>
#include "descriptor.h"

idl_retcode_t generate_test_descriptor (idl_pstate_t *pstate, const char *idl, struct descriptor *descriptor);

#endif /* IDLC_TEST_COMMON_H */
