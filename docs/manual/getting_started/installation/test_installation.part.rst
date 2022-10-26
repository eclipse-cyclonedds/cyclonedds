.. _`test_your_installation`:

Test your installation
======================

To test if your installation and configuration are working correctly, you can use the |var-project-short| ``ddsperf`` tool or you
can use the **Hello World!** example. To use ``ddsperf`` you skip ahead to `testing your network configuration. <#testing-your-network-configuration>`__.

Running the **Hello World!** example
------------------------------------

|var-project| includes a simple **Hello World!** application that can be executed to test your installation.
The **Hello World!** application consists of two executables:

 -  ``HelloworldPublisher``
 -  ``HelloworldSubscriber``

The **Hello World!** executables are located in ``<cyclonedds-directory>\build\bin\Debug`` on Windows, and
``<cyclonedds-directory>/build/bin`` on Linux/macOS. Please note that you did not run CMake with ``-DBUILD_EXAMPLES=ON`` earlier the executables will be missing.

To run the example application, open two console windows and navigate to the appropriate directory in both console windows. Run ``HelloworldSubscriber`` 
in one of the console windows:

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

    The default behavior is automatically detecting the first network interface card available on your machine and using it to
    exchange the hello world message. Therefore, selecting the correct interface card is essential to ensure that your publisher
    and subscriber applications are on the same network. This is a common issue with multiple network interface cards on machine configurations.

    You can override this default behavior by updating the property :ref:`//CycloneDDS/Domain/General/Interfaces/NetworkInterface[@address]`
    in a deployment file (e.g. ``cyclonedds.xml``) that you created to point to it through an OS environment variable named CYCLONEDDS\_URI.

    See also the :ref:`config-docs` and the :ref:`configuration_reference`.
