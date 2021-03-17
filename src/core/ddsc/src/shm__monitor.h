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
#ifndef _SHM_monitor_H_
#define _SHM_monitor_H_

#include "iceoryx_binding_c/wait_set.h"

#include "dds/ddsrt/threads.h"
#include "dds/ddsrt/sync.h"

#if defined (__cplusplus)
extern "C" {
#endif

// TODO: the iceoryx waitset has a maximum number of events that can be registered but this can only be queried
// at runtime
// currently it is hardcoded to be 128 events in the iceoryx C binding
// and we need one event for the wake up trigger
#define SHM_MAX_NUMBER_OF_READERS 127

//forward declaration to avoid circular dependencies with dds__types.h
struct dds_reader;

enum shm_monitor_run_states {
    SHM_monitor_STOP = 0,
    SHM_monitor_RUN = 1,
    SHM_monitor_NOT_RUNNING = 2
};

struct shm_monitor {
    ddsrt_mutex_t m_lock;

    iox_ws_storage_t m_waitset_storage;
    iox_ws_t m_waitset;

    //note: a little inefficient with arrays and brute force but it is an intermediate solution
    //      and will be replaced with a monitor from iceoryx
    //TODO: the iceoryx monitor is currently not usable here, since it lacks callback arguments
    uint32_t m_number_of_modifications_pending;
    uint32_t m_number_of_attached_readers;
    struct dds_reader* m_readers_to_attach[SHM_MAX_NUMBER_OF_READERS];
    struct dds_reader* m_readers_to_detach[SHM_MAX_NUMBER_OF_READERS];
 
    //use this if we wait but want to wake up for some reason
    //e.g. terminate, update the waitset etc.
    iox_user_trigger_storage_t m_wakeup_trigger_storage;
    iox_user_trigger_t m_wakeup_trigger;
    uint32_t m_run_state; //TODO: should be atomic

    ddsrt_thread_t m_thread;
};

//TODO: document functions once final

typedef struct shm_monitor shm_monitor_t;

void shm_monitor_init(shm_monitor_t* monitor);

void shm_monitor_destroy(shm_monitor_t* monitor);

dds_return_t shm_monitor_wake(shm_monitor_t* monitor);

dds_return_t shm_monitor_attach_reader(shm_monitor_t* monitor, struct dds_reader* reader);

dds_return_t shm_monitor_detach_reader(shm_monitor_t* monitor, struct dds_reader* reader);

dds_return_t shm_monitor_deferred_attach_reader(shm_monitor_t* monitor, struct dds_reader* reader);

dds_return_t shm_monitor_deferred_detach_reader(shm_monitor_t* monitor, struct dds_reader* reader);

dds_return_t shm_monitor_perform_deferred_modifications(shm_monitor_t* monitor);

#if defined (__cplusplus)
}
#endif
#endif
