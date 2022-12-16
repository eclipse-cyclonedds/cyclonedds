.. _`Thread configuration`:

********************
Thread Configuration
********************

|var-project| creates several threads, each with a number of
properties that can be controlled individually. These properties are:

+ stack size,
+ scheduling class, and
+ scheduling priority.

Each thread is uniquely named and using that name with the :ref:`Threads/Thread[@name] <//CycloneDDS/Domain/Threads/Thread[@name]>`
option can be used to set the properties for that thread. Any subset of threads can be given special properties;
anything not specified explicitly is left at the default value.

The following threads exist:

  .. list-table::
     :align: left

     * - ``gc``
       - Garbage collector, which sleeps until garbage collection is requested for an entity, at which point it starts monitoring the state of |var-project|, pushing the entity through whatever state transitions are needed once it is safe to do so, ending with the freeing of the memory.
     * - ``recv``
       - Accepts incoming network packets from all sockets/ports, performs all protocol processing, queues (nearly) all protocol messages sent in response for handling by the timed-event thread, queues for delivery or, in special cases, delivers it directly to the data Readers.
     * - ``dq.builtins``
       - Processes all discovery data coming in from the network.
     * - ``lease``
       - Performs internal liveliness monitoring of |var-project|.
     * - ``tev``
       - Timed-event handling, used for all kinds of things, such as periodic transmission of participant discovery and liveliness messages, transmission of control messages for reliable Writers and Readers (except those that have their own timed-event thread), retransmitting of reliable data on request (except those that have their own timed-event thread), and handling of start-up mode to normal mode transition.

and, for each defined channel:

  .. list-table::
     :align: left


     * - ``dq.channel-name``
       - Deserialisation and asynchronous delivery of all user data.
     * - ``tev.channel-name``
       - Channel-specific "timed-event" handling transmission of control messages for reliable Writers and Readers and retransmission of data on request. Channel-specific threads exist only if the configuration includes an element for it or if an auxiliary bandwidth limit is set for the channel.

When no channels are explicitly defined, there is one channel named *user*.
