/*
 * Copyright(c) 2021 Apex.AI Inc. All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */

#ifndef SHM_INIT_H
#define SHM_INIT_H

#include "dds/ddsi/ddsi_config.h"

// ICEORYX_TODO: move shared memory initialization here (runtime name etc.)
// this should be done after some consolidation to avoid conflicts.
// Note that it is not as eay as moving the initialization logic in q_init.c, 
// since error handling must be considered.
// 
// dds_return_t shm_init();

// For now we just add some of the logging logic here. Major restructuring will follow 
// once the shared memory functionality is complete. 

void shm_set_loglevel(enum ddsi_shm_loglevel);

#endif

