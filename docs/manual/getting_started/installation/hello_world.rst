
.. index:: HelloWorld

.. _hello_world:

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

To avoid mixing the output, run the subscriber and publisher in separate terminals.

#. Open two terminals.
#. In the first terminal, start the subscriber by running ``HelloWorldSubscriber``.
#. In the second terminal, start the publisher by running ``HelloWorldPublisher``.

.. tabs::

    .. group-tab:: Linux

        .. code-block:: bash

            ./HelloworldSubscriber

    .. group-tab:: macOS

        .. code-block:: bash

            ./HelloworldSubscriber

    .. group-tab:: Windows

        .. code-block:: PowerShell

            HelloworldSubscriber.exe

Run ``HelloworldPublisher`` in the other console window using:

.. tabs::

    .. group-tab:: Linux

        .. code-block:: bash

            ./HelloworldPublisher

    .. group-tab:: macOS

        .. code-block:: bash

            ./HelloworldPublisher

    .. group-tab:: Windows

        .. code-block:: PowerShell

            HelloworldPublisher.exe


``HelloworldPublisher`` appears as follows:

.. image:: /_static/gettingstarted-figures/helloworld_publisher.png
   :align: center


``HelloworldSubscriber`` appears as follows:

.. image:: /_static/gettingstarted-figures/helloworld_subscriber.png
   :align: center

.. note::
    There are some common issues with multiple network interface cards on machine configurations:

    The default behavior automatically detects the first available network interface card on your machine for exchanging the ``hello world`` message. To ensure that your publisher and subscriber applications are on the same network, you must select the correct interface card. 

    To override the default behavior, create or edit a deployment file (for example, ``cyclonedds.xml``) and update the property :ref:`//CycloneDDS/Domain/General/Interfaces/NetworkInterface[@address]` to point to it through the ``CYCLONEDDS\_URI`` OS environment variable.

    For further information, refer to :ref:`config-docs` and the :ref:`configuration_reference`.