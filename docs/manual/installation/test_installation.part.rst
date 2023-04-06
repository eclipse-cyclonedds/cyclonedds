.. index:: 
   single: Testing your installation
   single: Installation; Testing
   single: HelloWorld

.. _test__install:

Test your installation
======================

To test if your installation and configuration are working correctly, either:

- Use the |var-project-short| :ref:`dsperf_tool`
   The ``ddsperf`` tool sends a continuous stream of data at a variable frequency. This is useful for sanity checks and to bypass other sporadic network issues.
- Use the :ref:`helloworld_test` example.
   The **Hello World!** example sends a single message.

.. include:: dsperf_tool.rst

.. index:: HelloWorld

.. _helloworld_test:

HelloWorld
----------

To test your installation, |var-project| includes a simple **HelloWorld!** application 
(see also the :ref:`helloworld_bm` example). **HelloWorld!** consists of two executables:

 -  ``HelloworldPublisher``
 -  ``HelloworldSubscriber``

The **HelloWorld!** executables are located in:

- ``<cyclonedds-directory>\build\bin\Debug`` on Windows
- ``<cyclonedds-directory>/build/bin`` on Linux/macOS. 

.. note::
   Requires CMake with ``-DBUILD_EXAMPLES=ON``.

.. include:: ../getting_started/helloworld/helloworld_run.part.rst

.. note::
   There are some common issues with multiple network interface cards on machine 
   configurations. The default behavior automatically detects the first available 
   network interface card on your machine for exchanging the ``hello world`` message. 
   To ensure that your publisher and subscriber applications are on the same network, 
   you must select the correct interface card. To override the default behavior, 
   create or edit a deployment file (for example, ``cyclonedds.xml``) and update the 
   property :ref:`//CycloneDDS/Domain/General/Interfaces/NetworkInterface[@address]` 
   to point to it through the ``CYCLONEDDS\_URI`` OS environment variable. For further 
   information, refer to :ref:`config-docs` and the :ref:`configuration_reference`.
