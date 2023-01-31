.. index:: 
  single: Reporting
  single: Tracing
  single: Verbosity

.. _`Reporting and tracing`:

*********************
Reporting and tracing
*********************

|var-project| can produce highly detailed traces of all traffic and internal activities.
It enables individual categories of information and a simple verbosity level that 
enables fixed sets of categories.

All *fatal* and *error* messages are written both to the trace and to the
``cyclonedds-error.log`` file. 

All "warning" messages are written to the trace and the ``cyclonedds-info.log`` file.

The Tracing element has the following sub elements:

- Verbosity
- EnableCategory

Whether these logging levels are set in the verbosity level or by enabling the
corresponding categories is immaterial.

Verbosity
=========

Selects a tracing level by enabling a pre-defined set of categories. The following
table lists the known tracing levels, and the categories they enable:

.. list-table::
    :align: left
    :widths: 20 80

    * - ``none``
      -
    * - ``severe``
      - ``error``, ``fatal``
    * - ``warning``, ``info``
      - ``severe``, ``warning``
    * - ``config``
      - ``info``, ``config``
        
        Writes the complete configuration to the trace file and any warnings or
        errors, which can be an effective method to verify that everything is configured
        and behaving as expected.
    * - ``fine``
      - ``config``, ``discovery``
        
        Adds the complete discovery information in the trace (but nothing related to 
        application data or protocol activities). If a system has a stable topology,
        this typically results in a moderate size trace.
    * - ``finer``
      - ``fine``, ``traffic``, ``timing``, ``info``
    * - ``finest``
      - ``fine``, ``trace``
       
        Provides a detailed trace of everything that occurs and is an indispensable 
        source of information when analysing problems. However, it also requires a 
        significant amount of time and results in very large log files. 

EnableCategory
==============

This is a comma-separated list of keywords. Each keyword enables individual categories. 
The following keywords are recognised:

.. list-table::
    :align: left
    :widths: 20 80

    * - ``fatal``
      - All fatal errors, errors causing immediate termination.
    * - ``error``
      - Failures probably impacting correctness but not necessarily causing immediate termination.
    * - ``warning``
      - Abnormal situations that will likely not impact correctness.
    * - ``config``
      - Full dump of the configuration.
    * - ``info``
      - General informational notices.
    * - ``discovery``
      - All discovery activity.
    * - ``data``
      - Include data content of samples in traces.
    * - ``timing``
      - Periodic reporting of CPU loads per thread.
    * - ``traffic``
      - Periodic reporting of total outgoing data.
    * - ``tcp``
      - Connection and connection cache management for the TCP support.
    * - ``throttle``
      - Throttling events where the Writer stalls because its WHC hit the high-water mark.
    * - ``topic``
      - Detailed information on topic interpretation (in particular topic keys).
    * - ``plist``
      - Dumping of parameter lists encountered in discovery and inline QoS.
    * - ``radmin``
      - Receive buffer administration.
    * - ``whc``
      - Very detailed tracing of WHC content management.

The keyword *trace* enables all categories from *fatal* to *throttle*. 
  
The *topic* and *plist* categories are useful only for particular classes of discovery failures.

The *radmin* and *whc* categories only help in analysing the detailed behaviour of those two
components and produce significant amounts of output.

The file location is set in the configuration: :ref:`OutputFile <//CycloneDDS/Domain/Tracing/OutputFile>`

To append to the trace instead of replacing the file, set: 
:ref:`AppendToFile <//CycloneDDS/Domain/Tracing/AppendToFile>` to ``true``
