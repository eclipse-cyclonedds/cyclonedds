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


#include "shm__monitor.h"

#include "dds__types.h"
#include "dds__entity.h"
#include "dds__reader.h"

#include "dds/ddsi/q_entity.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/q_xmsg.h"
#include "dds/ddsi/ddsi_tkmap.h"
#include "dds/ddsi/ddsi_rhc.h"

#if defined (__cplusplus)
extern "C" {
#endif

static void shm_wakeup_trigger_callback(iox_user_trigger_t trigger);
static void shm_subscriber_callback(iox_sub_t subscriber);

void shm_monitor_init(shm_monitor_t* monitor) {
    printf("***create shm_monitor\n");
    ddsrt_mutex_init(&monitor->m_lock);

    monitor->m_listener = iox_listener_init(&monitor->m_listener_storage);
    monitor->m_wakeup_trigger = iox_user_trigger_init(&monitor->m_wakeup_trigger_storage.storage);
    monitor->m_wakeup_trigger_storage.monitor = monitor;
    iox_listener_attach_user_trigger_event(monitor->m_listener, monitor->m_wakeup_trigger, shm_wakeup_trigger_callback);

    monitor->m_state = SHM_MONITOR_RUNNING;
}

void shm_monitor_destroy(shm_monitor_t* monitor) {    
    //shm_monitor_wake_and_disable(monitor); //do we need this?

    //note: we must ensure no readers are actively using the monitor anymore,
    //the monitor and thus the waitset is to be destroyed after all readers are destroyed
    iox_listener_deinit(monitor->m_listener);

    ddsrt_mutex_destroy(&monitor->m_lock);
    printf("***destroyed shm_monitor\n");
}

dds_return_t shm_monitor_wake_and_invoke(shm_monitor_t* monitor, void (*function) (void*), void* arg) {
    iox_user_trigger_storage_extension_t* storage = (iox_user_trigger_storage_extension_t*) monitor->m_wakeup_trigger;
    storage->call = function;
    storage->arg = arg;
    iox_user_trigger_trigger(monitor->m_wakeup_trigger);
    return DDS_RETCODE_OK;
}

dds_return_t shm_monitor_wake_and_disable(shm_monitor_t* monitor) {
    monitor->m_state = SHM_MONITOR_NOT_RUNNING;
    iox_user_trigger_trigger(monitor->m_wakeup_trigger);
    return DDS_RETCODE_OK;
}

dds_return_t shm_monitor_wake_and_enable(shm_monitor_t* monitor) {
    monitor->m_state = SHM_MONITOR_RUNNING;
    iox_user_trigger_trigger(monitor->m_wakeup_trigger);
    return DDS_RETCODE_OK;
}

dds_return_t shm_monitor_attach_reader(shm_monitor_t* monitor, struct dds_reader* reader) {

    if(iox_listener_attach_subscriber_event(monitor->m_listener, reader->m_iox_sub, SubscriberEvent_HAS_DATA, shm_subscriber_callback) != ListenerResult_SUCCESS) {
        printf("error attaching reader %p\n", reader);      
        return DDS_RETCODE_OUT_OF_RESOURCES;
    }
    ++monitor->m_number_of_attached_readers;

    printf("attached reader %p with storage %p, subscriber %p\n", reader, &reader->m_iox_sub_stor, reader->m_iox_sub);
    return DDS_RETCODE_OK;
}

dds_return_t shm_monitor_detach_reader(shm_monitor_t* monitor, struct dds_reader* reader) {
    iox_listener_detach_subscriber_event(monitor->m_listener, reader->m_iox_sub, SubscriberEvent_HAS_DATA); 
    --monitor->m_number_of_attached_readers;

    printf("detached reader %p\n", reader);
    return DDS_RETCODE_OK;
}

static void receive_data_wakeup_handler(struct dds_reader* rd)
{
  printf("***received data via iceoryx\n");
  void* chunk = NULL;
  thread_state_awake(lookup_thread_state(), rd->m_rd->e.gv);
  while (ChunkReceiveResult_SUCCESS == iox_sub_take_chunk(rd->m_iox_sub, (const void** const)&chunk))
  {
    iceoryx_header_t* ice_hdr = (iceoryx_header_t*)chunk;
    // Get proxy writer
    struct proxy_writer* pwr = entidx_lookup_proxy_writer_guid(rd->m_rd->e.gv->entity_index, &ice_hdr->guid);
    if (pwr == NULL)
    {
      // We should ignore chunk which does not match the pwr in receiver side.
      // For example, intra-process has local pwr and does not need to use iceoryx, so we can ignore it.
      DDS_CLOG(DDS_LC_SHM, &rd->m_rd->e.gv->logconfig, "pwr is NULL and we'll ignore.\n");
      continue;
    }

    //ICEORYX_TODO: we do not create a copy during this call (?)
    //              afterwards we have the data pointer (chunk) and additional information 
    //              such as timestamp
    // Create struct ddsi_serdata
    struct ddsi_serdata* d = ddsi_serdata_from_iox(rd->m_topic->m_stype, ice_hdr->data_kind, &rd->m_iox_sub, chunk);
    //keyhash needs to be set here
    d->timestamp.v = ice_hdr->tstamp;

    // Get struct ddsi_tkmap_instance
    struct ddsi_tkmap_instance* tk;
    if ((tk = ddsi_tkmap_lookup_instance_ref(rd->m_rd->e.gv->m_tkmap, d)) == NULL)
    {
      DDS_CLOG(DDS_LC_SHM, &rd->m_rd->e.gv->logconfig, "ddsi_tkmap_lookup_instance_ref failed.\n");
      goto release;
    }

    // Generate writer_info
    struct ddsi_writer_info wrinfo;
    ddsi_make_writer_info(&wrinfo, &pwr->e, pwr->c.xqos, d->statusinfo);

    (void)ddsi_rhc_store(rd->m_rd->rhc, &wrinfo, d, tk);

release:
    if (tk)
      ddsi_tkmap_instance_unref(rd->m_rd->e.gv->m_tkmap, tk);
    if (d)
      ddsi_serdata_unref(d);
  }
  thread_state_asleep(lookup_thread_state());
}

static void shm_wakeup_trigger_callback(iox_user_trigger_t trigger) {    
    printf("trigger callback %p\n", trigger);
    // we know it is actually in extended storage since we created it like this
    iox_user_trigger_storage_extension_t* storage = (iox_user_trigger_storage_extension_t*) trigger;
    if(storage->monitor->m_state == SHM_MONITOR_RUNNING && storage->call) {
        storage->call(storage->arg);
    }
}

static void shm_subscriber_callback(iox_sub_t subscriber) {
    printf("subscriber callback %p \n", subscriber);
    // we know it is actually in extended storage since we created it like this
    iox_sub_storage_extension_t *storage = (iox_sub_storage_extension_t*) subscriber; 
    if(storage->monitor->m_state == SHM_MONITOR_RUNNING) {
        receive_data_wakeup_handler(storage->parent_reader);
    }
}

#if defined (__cplusplus)
}
#endif
