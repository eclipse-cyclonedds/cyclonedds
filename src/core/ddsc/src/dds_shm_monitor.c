// Copyright(c) 2021 Apex.AI Inc. All rights reserved.
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include "dds__shm_monitor.h"

#include "dds__types.h"
#include "dds__entity.h"
#include "dds__reader.h"

#include "dds/ddsi/ddsi_entity.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/ddsi_xmsg.h"
#include "dds/ddsi/ddsi_tkmap.h"
#include "dds/ddsi/ddsi_rhc.h"
#include "dds/ddsi/ddsi_serdata.h"
#include "iceoryx_binding_c/chunk.h"

#if defined (__cplusplus)
extern "C" {
#endif

static void shm_subscriber_callback(iox_sub_t subscriber, void * context_data);

void dds_shm_monitor_init(shm_monitor_t* monitor)
{
    ddsrt_mutex_init(&monitor->m_lock);

    // storage is ignored internally now but we cannot pass a nullptr
    monitor->m_listener = iox_listener_init(&(iox_listener_storage_t){0});
    monitor->m_wakeup_trigger = iox_user_trigger_init(&(iox_user_trigger_storage_t){0});

    monitor->m_state = SHM_MONITOR_RUNNING;
}

void dds_shm_monitor_destroy(shm_monitor_t* monitor)
{
    dds_shm_monitor_wake_and_disable(monitor);
    // waiting for the readers to be detached is not necessary,
    // they will be detached when the listener is destroyed (deinit)
    // the deinit will wait for the internal listener thread to join,
    // any remaining callbacks will be executed

    iox_listener_deinit(monitor->m_listener);
    iox_user_trigger_deinit(monitor->m_wakeup_trigger);
    ddsrt_mutex_destroy(&monitor->m_lock);
}

dds_return_t dds_shm_monitor_wake_and_disable(shm_monitor_t* monitor)
{
    monitor->m_state = SHM_MONITOR_NOT_RUNNING;
    iox_user_trigger_trigger(monitor->m_wakeup_trigger);
    return DDS_RETCODE_OK;
}

dds_return_t dds_shm_monitor_wake_and_enable(shm_monitor_t* monitor)
{
    monitor->m_state = SHM_MONITOR_RUNNING;
    iox_user_trigger_trigger(monitor->m_wakeup_trigger);
    return DDS_RETCODE_OK;
}

dds_return_t dds_shm_monitor_attach_reader(shm_monitor_t* monitor, struct dds_reader* reader)
{

    if(iox_listener_attach_subscriber_event_with_context_data(monitor->m_listener,
                                                              reader->m_iox_sub,
                                                              SubscriberEvent_DATA_RECEIVED,
                                                              shm_subscriber_callback,
                                                              &reader->m_iox_sub_context) != ListenerResult_SUCCESS) {
        DDS_CLOG(DDS_LC_SHM, &reader->m_rd->e.gv->logconfig, "error attaching reader\n");
        return DDS_RETCODE_OUT_OF_RESOURCES;
    }
    ++monitor->m_number_of_attached_readers;

    return DDS_RETCODE_OK;
}

dds_return_t dds_shm_monitor_detach_reader(shm_monitor_t* monitor, struct dds_reader* reader)
{
    iox_listener_detach_subscriber_event(monitor->m_listener, reader->m_iox_sub, SubscriberEvent_DATA_RECEIVED);
    --monitor->m_number_of_attached_readers;
    return DDS_RETCODE_OK;
}

static void receive_data_wakeup_handler(struct dds_reader* rd)
{
  void* chunk = NULL;
  struct ddsi_domaingv* gv = rd->m_rd->e.gv;
  ddsi_thread_state_awake(ddsi_lookup_thread_state(), gv);

  while (true)
  {
    shm_lock_iox_sub(rd->m_iox_sub);
    enum iox_ChunkReceiveResult take_result = iox_sub_take_chunk(rd->m_iox_sub, (const void** const)&chunk);
    shm_unlock_iox_sub(rd->m_iox_sub);

    // NB: If we cannot take the chunk (sample) the user may lose data.
    // Since the subscriber queue can overflow and will evict the least recent sample.
    // This entirely depends on the producer and consumer frequency (and the queue size if they are close).
    // The consumer here is essentially the reader history cache.
    if (ChunkReceiveResult_SUCCESS != take_result)
    {
      switch(take_result)
      {
        case ChunkReceiveResult_TOO_MANY_CHUNKS_HELD_IN_PARALLEL :
        {
          // we hold to many chunks and cannot get more
          DDS_CLOG (DDS_LC_WARNING | DDS_LC_SHM, &rd->m_entity.m_domain->gv.logconfig,
              "DDS reader with topic %s : iceoryx subscriber - TOO_MANY_CHUNKS_HELD_IN_PARALLEL -"
              "could not take sample\n", rd->m_topic->m_name);
          break;
        }
        case ChunkReceiveResult_NO_CHUNK_AVAILABLE: {
          // no more chunks to take, ok
          break;
        }
        default : {
          // some unkown error occurred
          DDS_CLOG(DDS_LC_WARNING | DDS_LC_SHM, &rd->m_entity.m_domain->gv.logconfig,
              "DDS reader with topic %s : iceoryx subscriber - UNKNOWN ERROR -"
              "could not take sample\n", rd->m_topic->m_name);
        }
      }

      break;
    }

    const iceoryx_header_t* ice_hdr = iceoryx_header_from_chunk(chunk);

    // Get writer or proxy writer
    struct ddsi_entity_common * e = ddsi_entidx_lookup_guid_untyped (gv->entity_index, &ice_hdr->guid);
    if (e == NULL || (e->kind != DDSI_EK_PROXY_WRITER && e->kind != DDSI_EK_WRITER))
    {
      // Ignore that doesn't match a known writer or proxy writer
      DDS_CLOG (DDS_LC_SHM, &gv->logconfig, "unknown source entity, ignore.\n");
      shm_lock_iox_sub(rd->m_iox_sub);
      iox_sub_release_chunk(rd->m_iox_sub, chunk);
      chunk = NULL;
      shm_unlock_iox_sub(rd->m_iox_sub);
      continue;
    }

    // Create struct ddsi_serdata
    struct ddsi_serdata* d = ddsi_serdata_from_iox(rd->m_topic->m_stype, ice_hdr->data_kind, &rd->m_iox_sub, chunk);
    d->timestamp.v = ice_hdr->tstamp;
    d->statusinfo = ice_hdr->statusinfo;

    // Get struct ddsi_tkmap_instance
    struct ddsi_tkmap_instance* tk;
    if ((tk = ddsi_tkmap_lookup_instance_ref(gv->m_tkmap, d)) == NULL)
    {
      DDS_CLOG(DDS_LC_SHM, &gv->logconfig, "ddsi_tkmap_lookup_instance_ref failed.\n");
      goto release;
    }

    // Generate writer_info
    struct ddsi_writer_info wrinfo;
    struct dds_qos *xqos;
    if (e->kind == DDSI_EK_PROXY_WRITER)
      xqos = ((struct ddsi_proxy_writer *) e)->c.xqos;
    else
      xqos = ((struct ddsi_writer *) e)->xqos;
    ddsi_make_writer_info(&wrinfo, e, xqos, d->statusinfo);
    (void)ddsi_rhc_store(rd->m_rd->rhc, &wrinfo, d, tk);

release:
    if (tk)
      ddsi_tkmap_instance_unref(gv->m_tkmap, tk);
    if (d)
      ddsi_serdata_unref(d);
  }
  ddsi_thread_state_asleep(ddsi_lookup_thread_state());
}

static void shm_subscriber_callback(iox_sub_t subscriber, void * context_data)
{
    (void)subscriber;
    // we know it is actually in extended storage since we created it like this
    iox_sub_context_t *context = (iox_sub_context_t*) context_data;
    if(context->monitor->m_state == SHM_MONITOR_RUNNING) {
        receive_data_wakeup_handler(context->parent_reader);
    }
}

#if defined (__cplusplus)
}
#endif
