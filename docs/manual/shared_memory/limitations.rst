.. include:: ../external-links.part.rst

.. index:: Shared memory; Limitations

.. _shared_mem_limits:

Limitations of shared memory
============================

Due to the manner in which data is exchanged over shared memory, there are some 
limitations to the types of data that may be exchanged, and delivery are required to ensure their correct 
functioning.

Fixed size data types
---------------------

The data types to be exchanged need to have a fixed size. This precludes the 
use of strings and sequences at any level in the data type. This does not prevent 
the use of arrays, as their size is fixed at compile time. If any of these types 
of member variables are encountered in the IDL code generating the data types, 
shared memory exchange is disabled.

A possible workaround for this limitation is to use fixed size arrays of chars 
instead of strings (and arrays of other types in stead of sequences), and accept 
the overhead.

.. index:: iceoryx; Data exchange

iceoryx data exchange
---------------------

The manner in which the |url::iceoryx_link| memory pool keeps track of exchanged data puts a 
number of limitations on the QoS settings. For Writers, the following QoS settings 
are prerequisites for shared memory exchange:

.. list-table::
    :align: left
    :widths: 20 80

    * - Liveliness
      - :c:macro:`DDS_LIVELINESS_AUTOMATIC`

    * - Deadline
      - :c:macro:`DDS_INFINITY`

        ``DDS_INFINITY`` is a define in |var-project-short| that indicates an amount 
        of time in excess of the maximum duration that can be represented. It is used 
        to tell the interface that this duration is infinite. If timeouts depend on 
        this duration, they will never occur in the program's lifetime. Due to this, 
        some code paths can differ, and be more efficient.

    * - Reliability
      - :c:macro:`DDS_RELIABILITY_RELIABLE`

    * - Durability
      - :c:macro:`DDS_DURABILITY_VOLATILE`

    * - History
      - :c:macro:`DDS_HISTORY_KEEP_LAST`
        
        The depth is no larger than the publisher history capacity as set in the configuration file

For Readers, the following QoS settings are prerequisites for shared memory exchange:

.. list-table::
    :align: left
    :widths: 20 80

    * - Liveliness
      - :c:macro:`DDS_LIVELINESS_AUTOMATIC`

    * - Deadline
      - :c:macro:`DDS_INFINITY`

    * - Reliability
      - :c:macro:`DDS_RELIABILITY_RELIABLE`

    * - Durability
      - :c:macro:`DDS_DURABILITY_VOLATILE`

    * - History
      - :c:macro:`DDS_HISTORY_KEEP_LAST`
      
        The depth is no larger than the subscriber history request as set in the configuration file

If any of these prerequisites are not satisfied, shared memory exchange is disabled 
and data transfer falls back to the network interface.

A further limitation is that the maximum number of subscriptions per process for the 
iceoryx service is 127.

Operating system limitations
----------------------------

The limit on the operating system. iceoryx has no functioning implementation for the 
Windows operating system. Refer to |url::iceoryx_issues| for further information.
