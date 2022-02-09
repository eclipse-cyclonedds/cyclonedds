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
#include "iceoryx_binding_c/chunk.h"

#if defined (__cplusplus)
extern "C" {
#endif

static void shm_subscriber_callback(iox_sub_t subscriber, void * context_data);

void shm_monitor_init(shm_monitor_t* monitor) 
{
    ddsrt_mutex_init(&monitor->m_lock);
    
    // storage is ignored internally now but we cannot pass a nullptr    
    monitor->m_listener = iox_listener_init(&(iox_listener_storage_t){0});
    monitor->m_wakeup_trigger = iox_user_trigger_init(&(iox_user_trigger_storage_t){0});

    monitor->m_state = SHM_MONITOR_RUNNING;
}

void shm_monitor_destroy(shm_monitor_t* monitor) 
{
    shm_monitor_wake_and_disable(monitor);
    // waiting for the readers to be detached is not necessary, 
    // they will be detached when the listener is destroyed (deinit)
    // the deinit will wait for the internal listener thread to join,
    // any remaining callbacks will be executed

    iox_listener_deinit(monitor->m_listener);
    iox_user_trigger_deinit(monitor->m_wakeup_trigger);
    ddsrt_mutex_destroy(&monitor->m_lock);
}

dds_return_t shm_monitor_wake_and_disable(shm_monitor_t* monitor) 
{
    monitor->m_state = SHM_MONITOR_NOT_RUNNING;
    iox_user_trigger_trigger(monitor->m_wakeup_trigger);
    return DDS_RETCODE_OK;
}

dds_return_t shm_monitor_wake_and_enable(shm_monitor_t* monitor) 
{
    monitor->m_state = SHM_MONITOR_RUNNING;
    iox_user_trigger_trigger(monitor->m_wakeup_trigger);
    return DDS_RETCODE_OK;
}

dds_return_t shm_monitor_attach_reader(shm_monitor_t* monitor, struct dds_reader* reader) 
{
  enum iox_ListenerResult attach_result =
      iox_listener_attach_subscriber_event_with_context_data(monitor->m_listener,
                                                             reader->m_iox_sub,
                                                             SubscriberEvent_DATA_RECEIVED,
                                                             shm_subscriber_callback,
                                                             &reader->m_iox_sub_context);
  if(ListenerResult_SUCCESS != attach_result) {
    switch (attach_result) {
      case ListenerResult_EVENT_ALREADY_ATTACHED:{
        break;
      }
      case ListenerResult_LISTENER_FULL:
      case ListenerResult_EMPTY_EVENT_CALLBACK:
      case ListenerResult_EMPTY_INVALIDATION_CALLBACK:
      case ListenerResult_UNDEFINED_ERROR:
      default: {
        DDS_CLOG(DDS_LC_SHM, &reader->m_rd->e.gv->logconfig, "error attaching reader\n");
        return DDS_RETCODE_OUT_OF_RESOURCES;
      }
    }
  }

  // TODO(Sumanth), do we even use this at all?
  ++monitor->m_number_of_attached_readers;
  reader->m_iox_sub_context.monitor = &reader->m_entity.m_domain->m_shm_monitor;
  return DDS_RETCODE_OK;
}

dds_return_t shm_monitor_detach_reader(shm_monitor_t* monitor, struct dds_reader* reader) 
{
    ddsrt_mutex_lock(&monitor->m_lock);
    // if the reader is attached
    if (reader->m_iox_sub_context.monitor != NULL && reader->m_iox_sub_context.parent_reader != NULL) {
        iox_listener_detach_subscriber_event(monitor->m_listener, reader->m_iox_sub, SubscriberEvent_DATA_RECEIVED);
        // are we really tracking the number of attached readers?
        --monitor->m_number_of_attached_readers;
        reader->m_iox_sub_context.monitor = NULL;
        reader->m_iox_sub_context.parent_reader = NULL;
    }
    ddsrt_mutex_unlock(&monitor->m_lock);
    return DDS_RETCODE_OK;
}

static void receive_data_wakeup_handler(struct dds_reader* rd)
{
  dds_transfer_samples_from_iox_to_rhc(rd);
}

static void shm_subscriber_callback(iox_sub_t subscriber, void * context_data)
{
    (void)subscriber;
    // we know it is actually in extended storage since we created it like this
    iox_sub_context_t *context = (iox_sub_context_t*) context_data;
    if((context->monitor) && (context->monitor->m_state == SHM_MONITOR_RUNNING)) {
        receive_data_wakeup_handler(context->parent_reader);
    }
}

#if defined (__cplusplus)
}
#endif
