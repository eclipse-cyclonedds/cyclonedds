/*
 * Copyright(c) 2006 to 2018 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#define OS_HAVE_INLINE 0 /* override automatic determination of inlining */
#define VDDS_INLINE        /* no "inline" in function defs (not really needed) */
#define OS_ATOMICS_OMIT_FUNCTIONS 0 /* force inclusion of functions defs */

#include "os/os.h"
