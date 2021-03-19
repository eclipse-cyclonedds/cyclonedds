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

#include "iceoryx_binding_c/subscriber.h"
#include "iceoryx_binding_c/listener.h"

#include "dds/ddsrt/threads.h"
#include "dds/ddsrt/sync.h"

#if defined (__cplusplus)
extern "C" {
#endif

// TODO: the iceoryx waitset (listener) has a maximum number of events that can be registered but this can only be queried
// at runtime
// currently it is hardcoded to be 128 events in the iceoryx C binding
// and we need one event for the wake up trigger
#define SHM_MAX_NUMBER_OF_READERS 127

struct dds_reader;
struct shm_monitor ;

typedef struct {
    iox_sub_storage_t storage;
    struct shm_monitor* monitor;
    struct dds_reader* parent_reader;
} iox_sub_storage_extension_t;

typedef struct {
    iox_user_trigger_storage_t storage;
    struct shm_monitor* monitor;
    void* data;
} iox_user_trigger_storage_extension_t;

enum shm_monitor_states {
    SHM_MONITOR_RUNNING = 1,
    SHM_MONITOR_NOT_RUNNING = 2
};

struct shm_monitor {
    ddsrt_mutex_t m_lock;

    iox_listener_storage_t m_listener_storage;
    iox_listener_t m_listener;
 
    //use this if we wait but want to wake up for some reason e.g. terminate
    iox_user_trigger_storage_extension_t m_wakeup_trigger_storage;
    iox_user_trigger_t m_wakeup_trigger;

    uint32_t m_number_of_attached_readers;
    uint32_t m_state;
};

typedef struct shm_monitor shm_monitor_t;

void shm_monitor_init(shm_monitor_t* monitor);

void shm_monitor_destroy(shm_monitor_t* monitor);

dds_return_t shm_monitor_wake(shm_monitor_t* monitor);

dds_return_t shm_monitor_attach_reader(shm_monitor_t* monitor, struct dds_reader* reader);

dds_return_t shm_monitor_detach_reader(shm_monitor_t* monitor, struct dds_reader* reader);

#if defined (__cplusplus)
}
#endif
#endif
