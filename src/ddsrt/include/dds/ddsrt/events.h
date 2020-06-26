/*
 * Copyright(c) 2006 to 2020 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#ifndef DDSRT_EVENTS_H
#define DDSRT_EVENTS_H

#include "dds/ddsrt/retcode.h"
#include "dds/ddsrt/sockets.h"
#include "dds/ddsrt/atomics.h"

#if __APPLE__
#include "dds/ddsrt/events/darwin.h"
#else
#include "dds/ddsrt/events/posix.h"
#endif

#if defined (__cplusplus)
extern "C" {
#endif

  /**
  * @name Definitions for event trigger flags.
  * These flags describe the different type of trigger events which can be watched for.
  */
///@{
  /**< the flag is not set */
#define DDSRT_EVENT_FLAG_UNSET 0u
  /**< resource has data available for reading */
#define DDSRT_EVENT_FLAG_READ (1u<<0) 
  /**< resource can be written to */
//#define DDSRT_EVENT_FLAG_WRITE (1u<<1) 
  /**< resource has been opened */
//#define DDSRT_EVENT_FLAG_OPEN (1u<<2) 
  /**< resource has been closed */
//#define DDSRT_EVENT_FLAG_CLOSE (1u<<3) 
  /**< timeout has occurred on the resource */
//#define DDSRT_EVENT_FLAG_TIMEOUT (1u<<4)
  /**< notification of ip address change on the resource*/
//#define DDSRT_EVENT_FLAG_IP_CHANGE (1u<<5)  
///@}

  /**
  * @brief Describes the type of object being monitored for events.
  */
  enum ddsrt_event_type {
    ddsrt_event_type_unset, /**< unitialized state*/
    ddsrt_event_type_socket /**< indicating a socket type connection*/
    /*,
    ddsrt_event_type_file,
    ddsrt_event_type_ifaddr*/
  };
  typedef enum ddsrt_event_type ddsrt_event_type_t;

  /**
  * @brief Structure containing the information of which object is being watched.
  *
  * Is used by ddsrt_event_queue to keep track of objects being watched.
  */
  struct ddsrt_event {
    uint32_t flags; /**< Type of event being watched for, composited from DDSRT_EVENT_FLAG_$EVENT_TYPE$.*/
    ddsrt_event_type_t type; /**< Type of object being watched.*/
    ddsrt_atomic_uint32_t triggered; /**< Trigger status after a call to ddsrt_event_queue_wait.*/
    union {
      struct { ddsrt_socket_t sock; } socket;
      /*struct ddsrt_event_file file;  this is for future expansion to be able to register events on files*/
      /*struct ddsrt_event_ifaddr ifaddr; this is for future expansion to be able to register IP address changes*/
    } data; /**< Container for the object being watched.*/
  };

  typedef struct ddsrt_event ddsrt_event_t;

  typedef struct ddsrt_event_queue ddsrt_event_queue_t;
  
  /**
  * @brief Initializes an existing event to indicate a socket.
  *
  * @param ev Pointer to the event to initialize.
  * @param sock Socket to initialize the event with.
  * @param flags Flags to set for the event.
  *
  * @returns DDS_RETCODE_OK if everything went OK.
  */
  dds_return_t ddsrt_event_socket_init(ddsrt_event_t* ev, ddsrt_socket_t sock, uint32_t flags);

  dds_return_t ddsrt_event_fini(ddsrt_event_t* ev);

  /**
  * @brief Event queue creation function.
  *
  * @returns Pointer to the created queue.
  */
  ddsrt_event_queue_t* ddsrt_event_queue_create(void);

  /**
  * @brief Cleans up an event queue.
  *
  * Will finish the event queue first, then free the memory of the queue.
  *
  * @param queue The queue to clean up.
  *
  * @returns DDS_RETCODE_OK if everything went OK.
  */
  dds_return_t ddsrt_event_queue_destroy(ddsrt_event_queue_t* queue);

  /**
  * @brief Getter for the number of stored events.
  *
  * @param queue The queue to get the number of events of.
  *
  * @returns The number of events stored by the queue.
  */
  size_t ddsrt_event_queue_nevents(ddsrt_event_queue_t* queue);

  /**
  * @brief Triggers a wait for events for the queue.
  *
  * @param queue The queue to trigger.
  * @param reltime The maximum amount of time to wait.
  *
  * @returns DDS_RETCODE_OK if everything went OK.
  */
  dds_return_t ddsrt_event_queue_wait(ddsrt_event_queue_t* queue, dds_duration_t reltime);

  /**
  * @brief Interrupts a triggered wait of a queue.
  *
  * @param queue The queue to interrupt.
  *
  * @returns DDS_RETCODE_OK if everything went OK.
  */
  dds_return_t ddsrt_event_queue_signal(ddsrt_event_queue_t* queue);

  /**
  * @brief Adds an event to the queue.
  * 
  * This event will be stored by the queue, any additional actions such as registration to other services will also be done.
  *
  * @param queue The queue to add the event to.
  * @param evt Pointer to the event to add.
  *
  * @returns DDS_RETCODE_OK if everything went OK.
  */
  dds_return_t ddsrt_event_queue_add(ddsrt_event_queue_t* queue, ddsrt_event_t* evt);

  /**
  * @brief Removes an event from the queue.
  *
  * This event will be removed from the queue, any additional actions such as deregistration from other services will also be done.
  *
  * @param queue The queue to remove the event from.
  * @param evt Pointer to the event to remove.
  *
  * @returns DDS_RETCODE_OK if everything went OK.
  */
  dds_return_t ddsrt_event_queue_remove(ddsrt_event_queue_t* queue, ddsrt_event_t* evt);

  /**
  * @brief Gets the next triggered events from the queue.
  *
  * Will go over the stored events until it reaches one where any trigger flag is set.
  * Successive calls to this function will start from the previously used point.
  * Will be reset after a call to ddsrt_event_queue_wait.
  *
  * @param queue The queue to retrieve.
  *
  * @returns Pointer to the event which has a trigger flag set, NULL if none has this flag set.
  */
  ddsrt_event_t* ddsrt_event_queue_next(ddsrt_event_queue_t* queue);

#if defined (__cplusplus)
}
#endif
#endif /* DDSRT_EVENTS_H */
