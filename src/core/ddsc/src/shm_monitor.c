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
static void shm_subscriber_callback(iox_sub_t sub);

void shm_monitor_init(shm_monitor_t* monitor) {
    printf("***create shm_monitor\n");
    ddsrt_mutex_init(&monitor->m_lock);

    monitor->m_wakeup_trigger = iox_user_trigger_init(&monitor->m_wakeup_trigger_storage);

    monitor->m_listener = iox_listener_init(&monitor->m_listener_storage);
    iox_listener_attach_user_trigger_event(monitor->m_listener, monitor->m_wakeup_trigger, shm_wakeup_trigger_callback);

    monitor->m_run_state = SHM_MONITOR_RUNNING;
}

void shm_monitor_destroy(shm_monitor_t* monitor) {    
    //note: we must ensure no readers are actively using the monitor anymore,
    //the monitor and thus the waitset is to be destroyed after all readers are destroyed
    iox_listener_deinit(monitor->m_listener);

    ddsrt_mutex_destroy(&monitor->m_lock);
    printf("***destroyed shm_monitor\n");
}

dds_return_t shm_monitor_wake(shm_monitor_t* monitor) {
    iox_user_trigger_trigger(monitor->m_wakeup_trigger);
    return DDS_RETCODE_OK;
}

dds_return_t shm_monitor_attach_reader(shm_monitor_t* monitor, struct dds_reader* reader) {
    //TODO: using the pointer could be the fastest way and could be safe without deferred attach/detach (?)
    //TODO: what to use to get the handle ? Or should we pass the handle? The entity? - What is idiomatic here?
    dds_entity_t handle = reader->m_entity.m_hdllink.hdl;
    uint64_t reader_id = (uint64_t) handle;

    if(iox_listener_attach_subscriber_event(monitor->m_listener, reader->m_iox_sub.subscriber, SubscriberEvent_HAS_DATA, shm_subscriber_callback) != ListenerResult_SUCCESS) {  
        printf("error attaching reader %p\n", reader);      
        return DDS_RETCODE_OUT_OF_RESOURCES;
    }
    ++monitor->m_number_of_attached_readers;

    printf("attached reader %p with id %ld\n", reader, reader_id);
    return DDS_RETCODE_OK;
}

dds_return_t shm_monitor_detach_reader(shm_monitor_t* monitor, struct dds_reader* reader) {
    iox_listener_detach_subscriber_event(monitor->m_listener, reader->m_iox_sub.subscriber, SubscriberEvent_HAS_DATA); 
    --monitor->m_number_of_attached_readers;

    printf("detached reader %p\n", reader);
    return DDS_RETCODE_OK;
}

static void receive_data_wakeup_handler(struct dds_reader* rd)
{
  void* chunk = NULL;
  thread_state_awake(lookup_thread_state(), rd->m_rd->e.gv);
  while (ChunkReceiveResult_SUCCESS == iox_sub_take_chunk(rd->m_iox_sub.subscriber, (const void** const)&chunk))
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
    struct ddsi_serdata* d = ddsi_serdata_from_iox(rd->m_topic->m_stype, ice_hdr->data_kind, &rd->m_iox_sub.subscriber, chunk);
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
#if 0
static uint32_t shm_monitor_thread_main(void* arg) {
    shm_monitor_t* monitor = arg; 
    uint64_t number_of_missed_events = 0;
    uint64_t number_of_events = 0;
    iox_event_info_t events[SHM_MAX_NUMBER_OF_READERS];

    while(monitor->m_run_state == SHM_monitor_RUN) {

        number_of_events = iox_ws_wait(monitor->m_waitset, events, SHM_MAX_NUMBER_OF_READERS,
                                       &number_of_missed_events);

        //should not happen as the waitset is designed is configured here
        assert(number_of_missed_events == 0);

        //we woke up either due to termination request, modification request or
        //because some reader got data
        //check all the events and handle them accordingly

        for (uint64_t i = 0; i < number_of_events; ++i) {
            iox_event_info_t event = events[i];
            if (iox_event_info_does_originate_from_user_trigger(event, monitor->m_wakeup_trigger))
            {
                //note: having an additional trigger for termination only
                //will cause issues with the reader_ids beyond our control: 
                //what would its id be (and not collide with a reader id/handle)?
                
                //TODO: I sense a subtle race here in the waitset usage (by design)
                //      which may cause aus to miss wake ups
                iox_user_trigger_reset_trigger(monitor->m_wakeup_trigger);
                shm_monitor_perform_deferred_modifications(monitor);
                printf("shm monitor woke up\n");
            } else {
                //some reader got data, identify the reader
                uint64_t reader_id = iox_event_info_get_event_id(event);                             
                dds_entity_t handle = (dds_entity_t) reader_id;
                dds_entity* entity;             

                //TODO: this is potentially costly, we may be able to use the pointer directly
                //      when it is used differently (monitor vs. waitset, concurrent execution of
                //      chunk receive handling)
                dds_entity_pin(handle, &entity);

                if(!entity) {
                    printf("pinning reader %ld failed\n", reader_id);
                    continue; //pinning failed
                }

                dds_reader* reader = (dds_reader*) entity;

                printf("reader %p with id %ld received data\n", reader, reader_id);
                receive_data_wakeup_handler(reader);

                dds_entity_unpin(entity);
            }
        }
        //now that we woke up and performed the required actions
        //we will check for termination request and if there is none wait again
    }

    printf("***shm monitor thread stopped\n");
    return 0;
}
#endif
static void shm_wakeup_trigger_callback(iox_user_trigger_t trigger) {    
    printf("trigger callback %p\n", trigger);
    //monitor->m_run_state != SHM_MONITOR_STOP);
}

static void shm_subscriber_callback(iox_sub_t sub) {
    //we need to take the data here but also guarantee in a way that the reader is pinned
    //however, we have no access to the reader by only knowing the subscriber
    printf("subscriber callback %p\n", sub);
}


#if defined (__cplusplus)
}
#endif
