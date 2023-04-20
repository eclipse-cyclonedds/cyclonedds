// Copyright(c) 2021 Apex.AI Inc. All rights reserved.
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDS__SHM_MONITOR_H
#define DDS__SHM_MONITOR_H

#include "iceoryx_binding_c/subscriber.h"
#include "iceoryx_binding_c/listener.h"

#include "dds/ddsi/ddsi_shm_transport.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/threads.h"

#if defined (__cplusplus)
extern "C" {
#endif

// ICEORYX_TODO: the iceoryx listener has a maximum number of subscribers that can be registered but this can only be //  queried at runtime
// currently it is hardcoded to be 128 events in the iceoryx C binding
// and we need one registration slot for the wake up trigger
#define SHM_MAX_NUMBER_OF_READERS 127

struct dds_reader;
struct shm_monitor;

enum shm_monitor_states {
    SHM_MONITOR_NOT_RUNNING = 0,
    SHM_MONITOR_RUNNING = 1
};

/// @brief abstraction for monitoring the shared memory communication with an internal
///        thread responsible for reacting on received data via shared memory
struct shm_monitor {
    ddsrt_mutex_t m_lock;
    iox_listener_t m_listener;

    //use this if we wait but want to wake up for some reason e.g. terminate
    iox_user_trigger_t m_wakeup_trigger;

    uint32_t m_number_of_attached_readers;
    uint32_t m_state;
};

typedef struct shm_monitor shm_monitor_t;

/**
 * @brief Initialize the shm_monitor
 * @component shm_monitor
 *
 * @param monitor self
 */
void dds_shm_monitor_init(shm_monitor_t* monitor);

/**
 * @brief Delete the shm_monitor
 * @component shm_monitor
 *
 * @param monitor self
 */
void dds_shm_monitor_destroy(shm_monitor_t* monitor);

/**
 * @brief Wake up the internal listener and disable execution of listener callbacks due to received data
 * @component shm_monitor
 *
 * @param monitor self
 * @return dds_return_t
 */
dds_return_t dds_shm_monitor_wake_and_disable(shm_monitor_t* monitor);

/**
 * @brief Wake up the internal listener and enable execution of listener callbacks due to received data
 * @component shm_monitor
 *
 * @param monitor self
 * @return dds_return_t
 */
dds_return_t dds_shm_monitor_wake_and_enable(shm_monitor_t* monitor);

/**
 * @brief Attach a new reader
 * @component shm_monitor
 *
 * @param monitor self
 * @param reader reader to attach
 * @return dds_return_t
 */
dds_return_t dds_shm_monitor_attach_reader(shm_monitor_t* monitor, struct dds_reader* reader);

/**
 * @brief Detach a reader
 * @component shm_monitor
 *
 * @param monitor self
 * @param reader reader to detach
 * @return dds_return_t
 */
dds_return_t dds_shm_monitor_detach_reader(shm_monitor_t* monitor, struct dds_reader* reader);

// ICEORYX_TODO: clarify lifetime of readers, it should be ok since they are detached in the dds_reader_delete call

#if defined (__cplusplus)
}
#endif

#endif /* DDS__SHM_MONITOR_H */
