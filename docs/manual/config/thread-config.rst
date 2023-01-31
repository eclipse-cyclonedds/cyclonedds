.. index:: Thread configuration

.. _thread_config:

********************
Thread configuration
********************

|var-project| creates several threads, each with a number of
properties that can be controlled individually. These properties are:

- Stack size
- Scheduling class
- Scheduling priority

Each thread is uniquely named. To set the properties for that thread, use the unique thread 
name with the :ref:`Threads/Thread[@name] <//CycloneDDS/Domain/Threads/Thread[@name]>`
option. 

Any subset of threads can be given special properties. Any paramaters that are not explicitly 
specified use the default values.

The following threads exist:

.. list-table::
    :align: left
    :widths: 20 80

    * - ``gc``
      - Garbage collector, which sleeps until garbage collection is requested for an entity. 
        When requested, it starts monitoring the state of |var-project|, and when safe to do so, 
        pushes the entity through the necessary state transitions. The process ends with the 
        freeing of the memory.
    * - ``recv``
      -  - Accepts incoming network packets from all sockets/ports.
         - Performs all protocol processing.
         - Queues (nearly) all protocol messages sent in response for handling by the timed-event 
           thread.
         - Queues for delivery (in special cases, delivers it directly to the data readers).
    * - ``dq.builtins``
      - Processes all discovery data coming in from the network.
    * - ``lease``
      - Performs internal liveliness monitoring of |var-project|.
    * - ``tev``
      - Timed-event handling. Used for: 
         - Periodic transmission of participant discovery and liveliness messages.
         - Transmission of control messages for reliable writers and readers (except those that have 
           their own timed-event thread)
         - Retransmitting of reliable data on request (except those that have their own timed-event 
           thread)
         - Handling of start-up mode to normal mode transition.

For each defined channel:

.. list-table::
    :align: left
    :widths: 20 80

    * - ``dq.channel-name``
      - Deserialisation and asynchronous delivery of all user data.
    * - ``tev.channel-name``
      - Channel-specific "timed-event" handling transmission of control messages for reliable writers 
        and Readers and re-transmission of data on request. 
        
        Channel-specific threads exist only if the configuration includes an element for it, or if an 
        auxiliary bandwidth limit is set for the channel.

When no channels are explicitly defined, there is one channel named *user*.
