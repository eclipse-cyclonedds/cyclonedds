.. include:: ../external-links.part.rst

.. index::
    single: Shared memory; Configuration

.. _shared_mem_config:

Shared memory configuration
===========================

There are two levels of configuration for |var-project-short| with shared memory 
support, the shared memory service (|url::iceoryx_link|) level, and the 
|var-project-short| level.

The performance of the shared memory service is strongly dependent on how well 
its configuration matches up with the use-cases it is asked to support. By configuring 
shared memory correctly, large gains in performance can be made.

The memory of iceoryx is layed out as numbers of fixed-size segments, which are 
iceoryx's memory pool. When a subscriber requests a block of memory from iceoryx, the 
smallest block that fits the requested size (and is available) is provided to the 
subscriber from the pool. If no blocks can be found that satisfy these requirements, 
the publisher requesting the block gives an error and aborts the process.

.. note::
  For testing, the default memory configuration usually suffices. 

The default configuration has blocks in varying numbers and sizes up to 4MiB. iceoryx 
falls back to this configuration when it is not supplied with a suitable configuration 
file, or cannot find one at the default location.

To ensure the best performance with the smallest footprint, configure iceoryx so that 
the memory pool only consists of blocks that are useful to the exchanges to be done. 
Due to header information being sent along with the sample, the block size required 
from the pool is 64 bytes larger than the data type being exchanged. iceoryx requires 
that the blocks are aligned to 4 bytes.

The following is an example of an iceoryx configuration file that has a memory pool of 2^15 
blocks, which can store data types of 16384 bytes (+ 64 byte header = 16448 byte block):

.. code-block:: toml

  [general]
  version = 1

  [[segment]]

  [[segment.mempool]]
  size = 16448
  count = 32768

The configuration file is supplied to iceoryx using the ``-c`` parameter. 

.. note::
  This file is used in the :ref:`shared_mem_example`. Save this file as 
  *iox_config.toml* in your home directory.

|var-project-short| also needs to be configured correctly, to allow it to use shared memory exchange.

The location where |var-project-short| looks for the config file is set through the environment variable *CYCLONEDDS_URI*:

.. code-block:: console

  export CYCLONEDDS_URI=file://cyclonedds.xml

The following optional configuration parameters in SharedMemory govern how |var-project-short| treats shared memory:

* Enable

  * When set to *true* enables |var-project-short| to use shared memory for local data exchange

  * Defaults to *false*

* LogLevel

  * Controls the output of the iceoryx runtime and can be set to, in order of decreasing output:

    * *verbose*

    * *debug*

    * *info* (default)

    * *warn*

    * *error*

    * *fatal*

    * *off*

The following is an example of a |var-project-short| configuration file supporting shared memory exchange:

.. code-block:: xml

  <?xml version="1.0" encoding="UTF-8" ?>
  <CycloneDDS xmlns="https://cdds.io/config"
              xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
              xsi:schemaLocation="https://cdds.io/config https://raw.githubusercontent.com/eclipse-cyclonedds/cyclonedds/iceoryx/etc/cyclonedds.xsd">
      <Domain id="any">
          <SharedMemory>
              <Enable>true</Enable>
              <LogLevel>info</LogLevel>
          </SharedMemory>
      </Domain>
  </CycloneDDS>

.. note::
  This file is used in the :ref:`shared_mem_example`. Save this file as 
  *cyclonedds.xml* in your home directory.