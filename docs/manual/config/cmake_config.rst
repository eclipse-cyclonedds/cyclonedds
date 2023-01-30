.. include:: ../external-links.part.rst

.. index:: CMake; Build configuration

.. _cmake_config:

#########################
CMake build configuration
#########################

The following table lists some configuration options specified using CMake defines (in 
addition to the standard options such as ``CMAKE_BUILD_TYPE``):

.. list-table::
    :align: left
    :widths: 20 80

    * - ``-DBUILD_EXAMPLES=ON``
      - Build the included examples
    * - ``-DBUILD_TESTING=ON``
      - Build the test suite (this requires |url::cunit_link|), see :ref:`contributing_to_dds`.
    * - ``-DBUILD_IDLC=NO``
      - Disable building the IDL compiler (affects building examples, tests and ``ddsperf``)
    * - ``-DBUILD_DDSPERF=NO``
      - Disable building the :ref:`dsperf_tool` (|url::ddsperf_github|) tool for performance measurement.
    * - ``-DENABLE_SSL=NO``
      - Do not look for OpenSSL, remove TLS/TCP support and avoid building the plugins that 
        implement authentication and encryption (default is ``AUTO`` to enable them if OpenSSL is found)
    * - ``-DENABLE_SHM=NO``
      - Do not look for |url::iceoryx_link| and disable :ref:`shared_memory` (default 
        is ``AUTO`` to enable it if iceoryx is found)
    * - ``-DENABLE_SECURITY=NO``
      - Do not build the security interfaces and hooks in the core code, nor the plugins 
        (you can enable security without OpenSSL present, you'll just have to find 
        plugins elsewhere in that case)
    * - ``-DENABLE_LIFESPAN=NO``
      - Exclude support for finite lifespans QoS
    * - ``-DENABLE_DEADLINE_MISSED=NO``
      - Exclude support for finite deadline QoS settings
    * - ``-DENABLE_TYPE_DISCOVERY=NO``
      - Exclude support for type discovery and checking type compatibility 
        (effectively most of XTypes), requires also disabling topic discovery using 
        ``-DENABLE_TOPIC_DISCOVERY=NO``
    * - ``-DENABLE_TOPIC_DISCOVERY=NO`` 
      - Exclude support for topic discovery
    * - ``-DENABLE_SOURCE_SPECIFIC_MULTICAST=NO``
      - Disable support for source-specific multicast (disabling this and ``-DENABLE_IPV6=NO`` 
        may be needed for QNX builds)
    * - ``-DENABLE_IPV6=NO``: 
      - Disable ipv6 support (disabling this and ``-DENABLE_SOURCE_SPECIFIC_MULTICAST=NO`` 
        may be needed for QNX builds) 
    * - ``-DBUILD_IDLC_XTESTS=NO``
      - Include a set of tests for the IDL compiler that use the C back-end to compile 
        an IDL file at (test) runtime, and use the C compiler to build a test 
        application for the generated types, that is executed to do the actual testing 
        (not supported on Windows) 
