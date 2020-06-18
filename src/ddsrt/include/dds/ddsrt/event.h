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
#ifndef EVENT_H
#define EVENT_H

#if defined (__cplusplus)
extern "C" {
#endif

/*currently supported monitorable quantities*/
enum MONITORABLE_TYPE {
    MONITORABLE_UNSET,
    MONITORABLE_FILE,
    MONITORABLE_SOCKET,
    MONITORABLE_PIPE,
    MONITORABLE_VOLATILE
};

/*currently supported types of events*/
enum EVENT_TYPE {
    EVENT_UNSET = 0x00000000,
    EVENT_DATA_IN = 0x00000001,
    EVENT_DATA_OUT = 0x00000002,
    EVENT_CONNECT = 0x0000004,
    EVENT_DISCONNECT = 0x00000008,
    EVENT_TIMEOUT = 0x00000010,
    EVENT_ERROR = 0x80000000
};

/**
* ddsrt_event struct definition
* 
* mon_type: type of monitorable for events
* mon_ptr: pointer to stored value of monitorable
* mon_sz: number of bytes stored by mon_ptr
* evt_type: type of event, composited from EVENT_TYPE
*/
struct ddsrt_event {
    enum MONITORABLE_TYPE   mon_type;
    void*                   mon_ptr;
    unsigned int            mon_sz;
    int                     evt_type;
};

/**
* ddsrt_event creation function
* 
* Will set the appropriate fields and create storage of mon_sz bytes, which are copied from ptr_to_mon
*
* mon_type: type of monitorable to set
* ptr_to_mon: pointer to monitorable to set
* mon_sz: number of bytes of monitorable
* evt_type: type of event
*/
struct ddsrt_event* ddsrt_event_create(enum MONITORABLE_TYPE mon_type, void* ptr_to_mon, unsigned int mon_sz, int evt_type);

/*shorthand, you can just supply a type, and it copies the contents of the correct size*/
#define ddsrt_event_create_val(mtp,val,etp) ddsrt_event_create(mtp,&val,sizeof(val),etp)

/**
* ddsrt_event copy function
*
* copies the contents of src to dst, will reassign memory for mon_ptr if necessary
* 
* dst: destination to copy to
* src: source to copy from
*/
void ddsrt_event_copy(struct ddsrt_event* dst, struct ddsrt_event* src);

/**
* ddsrt_event cleanup function
*
* Will deassign all memory associated with the event.
*
* evt: the event to clean
*/
void ddsrt_event_destroy(struct ddsrt_event *evt);

/* Forward declaration of the ddsrt_monitor structure, implementation will happen for each supported architecture.*/
typedef struct ddsrt_monitor ddsrt_monitor;

/**
* ddsrt_monitor creation function
* 
* Will create containers for events and triggers.
* Will create method for early triggering.
* 
* cap: initial capacity of the monitor
* fixedsize: whether the capacity of the monitor can be expanded
* 
* returns:
*      NULL: something went wrong, i.e.:
*              capacity <= 0
*      pointer to the instance otherwise
*/
struct ddsrt_monitor *ddsrt_monitor_create(int cap, int fixedfize);

/**
* ddsrt_monitor destruction function
* 
* Will clean up all associated structures and invalidate all stored events.
* 
* mon: the monitor to clean up
*/
void ddsrt_monitor_destroy(struct ddsrt_monitor *mon);

/**
* Event trigger registration function.
* 
* mon: monitor to register the trigger to
* evt: event to register to the monitor
*/
int ddsrt_monitor_register_trigger(struct ddsrt_monitor *mon, struct ddsrt_event *evt);

/**
* Event trigger deregistration function.
* 
* Will remove all triggers for the monitorable type and value, 
* by erasing the event types and the event itself if there are no events to trigger for.
* 
* mon: the monitor to remove from
* evt: the event to remove
* 
* returns:
*      -1: something went wrong
*      number of stored event triggers otherwise
*/
int ddsrt_monitor_deregister_trigger(struct ddsrt_monitor* mon, struct ddsrt_event* evt);

/**
* Wait trigger function for monitor.
* 
* Starts waiting until an event matching the event types registered is received on any of the monitored fds.
* Will wait a maximum of milliseconds before returning, or until ddsrt_monitor_interrupt_wait is called.
* 
* mon: the monitor to trigger
* milliseconds: maximum number of milliseconds to wait
* 
* returns: the number of triggered events
*/
int ddsrt_monitor_start_wait(struct ddsrt_monitor *mon, int milliseconds);

/**
* Wait interruption function for monitor.
* 
* Will preempt a monitor which is waiting fro monitor_start_wait to return.
* 
* mon: monitor to preempt
* 
* returns:
*      0: in case of success
*      -1: om case of error
*/
int ddsrt_monitor_interrupt_wait(struct ddsrt_monitor *mon);

/**
* Monitor event retrieval function.
* 
* Retrieves a stored event from the event buffer, returns NULL if there are no more events to retrieve.
* This pointer will no longer be safe/valid after calling ddsrt_monitor_start_wait again.
* 
* mon: the monitor to retrieve the event from
* 
* returns:
*      NULL: there are no events to return
*      pointer to the event otherwise
*/
struct ddsrt_event *ddsrt_monitor_pop_event(struct ddsrt_monitor *mon);

#if defined (__cplusplus)
}
#endif
#endif /* EVENT_H */