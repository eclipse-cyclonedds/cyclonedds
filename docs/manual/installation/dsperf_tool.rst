.. _dsperf_tool:

The ``ddsperf`` tool
--------------------

The ``ddsperf`` tool is pre-installed within ``<installation-dir>/bin``.

.. note:: 
   The Python tooling uses ``ddsperf`` to provide the 
   cyclonedds ``performance`` subcommand and acts as a front-end for ``ddsperf``.

The following test ensures that the the loopback option is enabled.

To complete the sanity checks of your DDS-based system:

1. Open two terminals. 
2. In the first terminal, run the following command:

 .. code-block:: console

    ddsperf sanity

 The ``sanity`` option sends one data sample each second (1Hz).

3. In the second terminal, start ``ddsperf`` in **Pong** mode to echo
the data to the first instance of the ``ddsperf`` (started with the
*Sanity* option).

 .. code-block:: console

    ddsperf pong

.. image:: /_static/gettingstarted-figures/4.2-1.png
   :align: center

If the data is not exchanged on the network between the two ddsperf
instances, it is probable that |var-project| has not selected the
the appropriate network card on both machines, or a firewall in-between is
preventing communication.

|var-project| automatically selects the most available network interface.
This behavior can be overridden by changing the configuration file.