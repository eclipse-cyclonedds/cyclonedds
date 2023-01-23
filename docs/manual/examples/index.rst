.. index:: Examples

.. _examples_bm:

========
Examples
========

.. toctree::
    :maxdepth: 1
    :hidden:

    helloworld
    roundtrip
    throughput
    listtopics
    dynsub

- :ref:`helloworld_bm`
  
  The basic HelloWorld example describes the steps necessary for setting up DCPS entities.

- :ref:`roundtrip_bm`
  
  The roundtrip example measures the roundtrip duration when sending and receiving a single message.

- :ref:`throughput_bm`

  The Throughput example measures data throughput when receiving samples from a publisher.

- :ref:`listtopics_bm`

  The listtopics example shows how to monitor topics that have been defined in the system.

- :ref:`dynsub_bm`

  The dynsub example is a :term:`PoC` for a C-based JSON printer for arbitrary data. It assumes 
  that topic discovery is enabled (but doesn't require it).

.. note::
    |var-project| has various configuration parameters and comes with a default built-in
    configuration. To run an example, or any application that uses |var-project| for its data
    exchange, this default configuration is usually sufficient and no further action is required. 
    If you need to change the configuration, refer to the :ref:`Configuration guide<config-docs>`.
