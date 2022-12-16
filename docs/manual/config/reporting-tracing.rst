.. _`Reporting and tracing`:

*********************
Reporting and Tracing
*********************

|var-project| can produce highly detailed traces of all traffic and internal activities.
It enables individual categories of information and a simple verbosity
level that enables fixed sets of categories.

The categorisation of tracing output is incomplete, hence most of the verbosity
levels and categories are useless in the current release. This is an ongoing
process and here we describe the target rather than the current situation.

All *fatal* and *error* messages are written both to the trace and to the
``cyclonedds-error.log`` file; similarly all "warning" messages are written to the trace
and the ``cyclonedds-info.log`` file.

The Tracing element has the following sub elements:

+ *Verbosity*:
  selects a tracing level by enabling a pre-defined set of categories. The
  list below gives the known tracing levels, and the categories they enable:

  .. list-table::
     :align: left

     * - ``none``
       -
     * - ``severe``
       - ``error``, ``fatal``
     * - ``warning``, ``info``
       - ``severe``, ``warning``
     * - ``config``
       - ``info``, ``config``
     * - ``fine``
       - ``config``, ``discovery``
     * - ``finer``
       - ``fine``, ``traffic``, ``timing``, ``info``
     * - ``finest``
       - ``fine``, ``trace``

+ *EnableCategory*:
  a comma-separated list of keywords, each keyword enabling
  individual categories. The following keywords are recognised:

  .. list-table::
     :align: left

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

In addition, the keyword *trace* enables everything from *fatal* to *throttle*. The
*topic* and *plist* ones are useful only for particular classes of discovery failures;
and *radmin* and *whc* only help in analysing the detailed behaviour of those two
components and produce significant amounts of output.

 * :ref:`OutputFile <//CycloneDDS/Domain/Tracing/OutputFile>`: the file to write the trace to
 * :ref:`AppendToFile <//CycloneDDS/Domain/Tracing/AppendToFile>`: boolean, set to ``true`` to append to the trace instead of replacing the
   file.

Currently, the useful verbosity settings are *config*, *fine*, and *finest*.

*Config* writes the complete configuration to the trace file and any warnings or
errors, which can be an effective method to verify that everything is configured
and behaving as expected.

*Fine* adds the complete discovery information in the trace, but nothing
related to application data or protocol activities. If a system has a stable topology,
this will therefore typically result in a moderate size trace.

*Finest* provides a detailed trace of everything that occurs and is an
indispensable source of information when analysing problems; however,
it also requires a significant amount of time and results in huge log files.

Whether these logging levels are set using the verbosity level or by enabling the
corresponding categories is immaterial.
