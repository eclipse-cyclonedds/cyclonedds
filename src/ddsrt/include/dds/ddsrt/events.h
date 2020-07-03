/*
 * Copyright(c) 2020 ADLINK Technology Limited and others
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
#include "dds/ddsrt/attributes.h"
#include "dds/export.h"

#if defined(__APPLE__) || defined(__FreeBSD__)
#include "dds/ddsrt/events/kqueue.h"
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
 typedef enum ddsrt_event_type {
    DDSRT_EVENT_TYPE_UNSET, /**< uninitialized state*/
    DDSRT_EVENT_TYPE_SOCKET /**< indicating a socket type connection*/
    /*,
    DDSRT_EVENT_TYPE_FILE,
    DDSRT_EVENT_TYPE_IFADDR*/
  } ddsrt_event_type_t;

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
  * This function will set the type to be DDSRT_EVENT_TYPE_SOCKET, the triggered status to DDSRT_EVENT_FLAG_UNSET and
  * the flags and socket fields to the values supplied.
  *
  * @param[in,out] ev Pointer to the event to initialize.
  * @param[in] sock Socket to initialize the event with.
  * @param[in] flags Flags to set for the event.
  */
  DDS_EXPORT void ddsrt_event_socket_init(ddsrt_event_t* ev, ddsrt_socket_t sock, uint32_t flags) ddsrt_nonnull((1));

  /**
  * @brief Event queue creation function.
  *
  * This function will attempt to reserve memory for the event queue and open any necessary additional resources, i.e.:
  * - a pipe for interrupting its own wait state
  * - an instance to a kernel event monitor (in the case of BSD operating systems)
  *
  * @returns Pointer to the event queue that was created, NULL if a failure occurred.
  */
  DDS_EXPORT ddsrt_event_queue_t* ddsrt_event_queue_create(void);

  /**
  * @brief Cleans up an event queue.
  *
  * Will close/free any resources managed by the event_queue (if any), then free the memory of the queue.
  *
  * @param[in,out] queue The queue to clean up.
  */
  DDS_EXPORT void ddsrt_event_queue_delete(ddsrt_event_queue_t* queue) ddsrt_nonnull_all;

  /**
  * @brief Getter for the number of stored events.
  *
  * The number of stored events does not have to equal the current capacity of the instance.
  *
  * @param[in,out] queue The queue to get the number of events of.
  *
  * @returns The number of events stored by the queue.
  */
  DDS_EXPORT size_t ddsrt_event_queue_nevents(ddsrt_event_queue_t* queue) ddsrt_nonnull_all;

  /**
  * @brief Triggers a wait for events for the queue.
  *
  * After calling this function the queue will go over the events it has to monitor and unset all triggered statuses.
  * The event queue will wait for a maximum of reltime forany of the monitored quantities to change state.
  * For the events which have changed state, the triggered status will be set.
  *
  * @param[in,out] queue The queue to trigger.
  * @param[in] reltime The maximum amount of time to wait.
  *
  * @retval DDS_RETCODE_OK
  *             The wait was concluded succesfully: either the timeout was reached, or data was received on one of the monitored sockets or the interrupt was signalled.
  * @retval DDS_RETCODE_ERROR
  *             An error occurred: the interrupt socket/pipe could not be read succesfully, or the select/kevent function did not complete succesfully.
  */
  DDS_EXPORT dds_return_t ddsrt_event_queue_wait(ddsrt_event_queue_t* queue, dds_duration_t reltime) ddsrt_nonnull_all;

  /**
  * @brief Interrupts a triggered wait of a queue.
  *
  * This function is used to stop a ddsrt_event_queue_wait before it would have returned on its own accord.
  *
  * @param[in,out] queue The queue to interrupt.
  *
  * @retval DDS_RETCODE_OK
  *             The signal was given succesfully.
  * @retval DDS_RETCODE_ERROR
  *             The signal could not be written to the pipe/socket correctly.
  */
  DDS_EXPORT dds_return_t ddsrt_event_queue_signal(ddsrt_event_queue_t* queue) ddsrt_nonnull_all;

  /**
  * @brief Adds an event to the queue.
  * 
  * This event will be stored by the queue, any additional actions such as registration to other services will also be done.
  *
  * @param[in,out] queue The queue to add the event to.
  * @param[in,out] evt Pointer to the event to add.
  */
  DDS_EXPORT void ddsrt_event_queue_add(ddsrt_event_queue_t* queue, ddsrt_event_t* evt) ddsrt_nonnull_all;

  /**
  * @brief Removes an event from the queue.
  *
  * This event will be removed from the queue, any additional actions such as deregistration from other services will also be done.
  *
  * @param[in,out] queue The queue to remove the event from.
  * @param[in,out] evt Pointer to the event to remove.
  *
  * @retval DDS_RETCODE_OK
  *             The event was succesfully removed from the queue.
  * @retval DDS_RETCODE_ALREADY_DELETED
  *             No event matching the supplied pointer could be found, and was therefore not removed.
  */
  DDS_EXPORT dds_return_t ddsrt_event_queue_remove(ddsrt_event_queue_t* queue, ddsrt_event_t* evt) ddsrt_nonnull_all;

  /**
  * @brief Gets the next triggered events from the queue.
  *
  * Will go over the stored events until it reaches one where any trigger flag is set.
  * Successive calls to this function will start from the previously used point.
  * Will be reset after a call to ddsrt_event_queue_wait.
  *
  * @param[in,out] queue The queue to retrieve.
  *
  * @returns Pointer to the event which has a trigger flag set, NULL if none of the stored events has this flag set.
  */
  DDS_EXPORT ddsrt_event_t* ddsrt_event_queue_next(ddsrt_event_queue_t* queue) ddsrt_nonnull_all;

#if defined (__cplusplus)
}
#endif
#endif /* DDSRT_EVENTS_H */
