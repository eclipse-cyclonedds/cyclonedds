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
#ifndef DDSRT_EVENT_H
#define DDSRT_EVENT_H

#if defined (__cplusplus)
extern "C" {
#endif

/**
* Currently supported monitorable quantities.
*/
enum ddsrt_monitorable {
  ddsrt_monitorable_unset,
  ddsrt_monitorable_socket,
  ddsrt_monitorable_file,
  ddsrt_monitorable_pipe/*,
  ddsrt_monitorable_volatile*/
};

/**
* Currently supported types of events.
*/
enum ddsrt_monitorable_event {
  ddsrt_monitorable_event_unset = 0x00000000,
  ddsrt_monitorable_event_data_in = 0x00000001,
  ddsrt_monitorable_event_data_out = 0x00000002,
  ddsrt_monitorable_event_connect = 0x0000004,
  ddsrt_monitorable_event_disconnect = 0x00000008,
  ddsrt_monitorable_event_timeout = 0x00000010,
  ddsrt_monitorable_event_error = 0x80000000
};

/**
* @brief ddsrt_event struct definition
*
* mon_type: type of monitorable for events
* mon_ptr: pointer to stored value of monitorable
* mon_sz: number of bytes stored by mon_ptr
* evt_type: type of event, composited from ddsrt_monitorable_event
*/
struct ddsrt_event {
  enum ddsrt_monitorable   mon_type;
  void* mon_ptr;
  unsigned int            mon_sz;
  int                     evt_type;
};

typedef struct ddsrt_event ddsrt_event_t;

/**
* @brief ddsrt_event creation function
*
* Will set the appropriate fields and create storage of mon_sz bytes, which are copied from ptr_to_mon
*
* @param mon_type type of monitorable to set
* @param ptr_to_mon pointer to monitorable to set
* @param mon_sz number of bytes of monitorable
* @param evt_type type of event
*
* @returns pointer to the event if created succesfully, NULL otherwise
*/
ddsrt_event_t* ddsrt_event_create(enum ddsrt_monitorable mon_type, void* ptr_to_mon, unsigned int mon_sz, int evt_type);

/**
* @brief shorthand version of ddsrt_event_create
* 
* In this case you can just supply a type, and it copies the contents of the correct size
*/
#define ddsrt_event_create_val(mtp,val,etp) ddsrt_event_create(mtp,&val,sizeof(val),etp)

/**
* @brief ddsrt_event copy function
*
* Copies the contents of src to dst, will reassign memory for mon_ptr if necessary.
*
* @param dst destination to copy to
* @param src source to copy from
*/
void ddsrt_event_copy(ddsrt_event_t* dst, ddsrt_event_t* src);

/**
* @brief ddsrt_event cleanup function
*
* Will deassign all memory associated with the event.
*
* @param evt the event to clean
*/
void ddsrt_event_destroy(ddsrt_event_t* evt);

/** 
* @brief Forward declaration of the ddsrt_monitor structure, implementation will happen for each supported architecture.
*/
typedef struct ddsrt_monitor ddsrt_monitor_t;

/**
* @brief ddsrt_monitor creation function
*
* Will create containers for events and triggers.
* Will create method for early triggering.
*
* @param cap initial capacity of the monitor
* @param fixedsize whether the capacity of the monitor can be expanded
*
* @returns NULL: something went wrong, i.e.: capacity <= 0, pointer to the instance otherwise
*/
ddsrt_monitor_t* ddsrt_monitor_create(int cap, int fixedfize);

/**
* @brief ddsrt_monitor destruction function
*
* Will clean up all associated structures and invalidate all stored events.
*
* @param mon the monitor to clean up
*/
void ddsrt_monitor_destroy(ddsrt_monitor_t* mon);

/**
* @brief Event trigger registration function.
*
* Will check for the existence of a monitorable type in the triggers, will add evt if it does not exist,
* will OR the event_type with the stored trigger with that of evt otherwise.
*
* @param mon monitor to register the trigger to
* @param evt event to register to the monitor
*
* @returns the number of triggers stored if succesful, -1 otherwise
*/
int ddsrt_monitor_register_trigger(ddsrt_monitor_t* mon, ddsrt_event_t* evt);

/**
* @brief Event trigger deregistration function.
*
* Will remove all triggers for the monitorable type and value,
* by erasing the event types and the event itself if there are no events to trigger for.
*
* @param mon the monitor to remove from
* @param evt the event to remove
*
* @returns the number of triggers stored if succesful, -1 otherwise
*/
int ddsrt_monitor_deregister_trigger(ddsrt_monitor_t* mon, ddsrt_event_t* evt);

/**
* @brief Wait trigger function for monitor.
*
* Starts waiting until an event matching the event types registered is received on any of the monitored fds.
* Will wait a maximum of milliseconds before returning, or until ddsrt_monitor_interrupt_wait is called.
*
* @param mon the monitor to trigger
* @param milliseconds maximum number of milliseconds to wait
*
* @returns the number of triggered events
*/
int ddsrt_monitor_start_wait(ddsrt_monitor_t* mon, int milliseconds);

/**
* @brief Wait interruption function for monitor.
*
* Will preempt a monitor which is waiting fro monitor_start_wait to return.
*
* @param mon monitor to preempt
*
* @returns 0: in case of success, -1: om case of error
*/
int ddsrt_monitor_interrupt_wait(ddsrt_monitor_t* mon);

/**
* @brief Monitor event retrieval function.
*
* Retrieves a stored event from the event buffer, returns NULL if there are no more events to retrieve.
* This pointer will no longer be safe/valid after calling ddsrt_monitor_start_wait again.
*
* @param mon the monitor to retrieve the event from
*
* @returns NULL: there are no events to return, pointer to the event otherwise
*/
ddsrt_event_t* ddsrt_monitor_pop_event(ddsrt_monitor_t* mon);

#if defined (__cplusplus)
}
#endif
#endif /* DDSRT_EVENT_H */