Benchmarking Tools
==================

Introduction
------------

|var-project-short| provides a tool that primarily measures data *throughput*
and *latency* of the DDS applications within the network or
within the same board, namely ``ddsperf``. This tool also helps 
sanity checks to ensure your configuration is correctly set up and
running. This chapter describes how to use the ``ddsperf`` tool and 
read and interpret its outputs and results. Using the |var-project-short| Python
package, you can also run ``ddsperf`` as a graphical application, by running
``cyclonedds performance``.

As well as ``ddsperf``, you can find dedicated examples in the
product distribution that measure the DDS system throughput and the
latency with their associated codebase. You can start from the provided
code and customize it to fit your scenario and exact data types. Both the
``ddsperf`` tool and the provided examples perform the benchmarking using
sequences of octets with different parameterized sizes.

.. important::

    The ``throughput`` and ``latency`` examples are not suitable as general
    DDS benchmarking tools since they sacrifice performance for being as simple
    as possible. You cannot compare test results from these tools with results from other tools.

Testing your network configuration
----------------------------------

Once your |var-project-short| installation is successfully completed, you may
want to test if your network environment is correctly set up. This can
be done by running the **Hello World!** example or using the
``ddsperf`` tool. The **Hello World!** example sends a single message.
In contrast, the ``ddsperf`` tool can send a continuous stream of data at a
variable frequency for sanity checks and bypass sporadic network issues.

The ``ddsperf`` tool is pre-installed within ``<installation-dir>/bin``.

Complete the sanity checks of your DDS based system using the ``ddsperf``
tool as follows:

.. code-block:: bash

    ddsperf sanity

The sanity option sends only one data sample each second (1Hz).

In another terminal, start the ``ddsperf`` with the **Pong** mode to echo
the data to the first instance of the ``ddsperf`` started with the
*Sanity* option.

.. code-block:: bash

    ddsperf pong

.. image:: /_static/gettingstarted-figures/4.2-1.png
   :align: center

If the data is not exchanged on the network between the two ddsperf
instances, it is likely that |var-project| has not selected the
the appropriate network card on both machines or a firewall in-between is
preventing communication.

|var-project| automatically selects the most available network interface.
This behavior can be overridden by changing the configuration file. (see
section :ref:`test your installation <test_your_installation>` for more details) .

When running the previous scenario on a local machine, this test ensures
the loop-back option is enabled.

Measuring latency
-----------------

To measure latency between two different applications, you need to run
two instances of the ``ddsperf`` tool and instruct one of them to endorse
the role of a *sender* that sends a given amount of data (a sequence of
octets) at a given rate, and the other instance takes the role of
*receiver* that sends back the same amount of data to the sender in a
Ping-Pong scenario. The sending action is triggered by the **Ping**
option. The receiving behavior is triggered by the **Pong** action. The
sender measures the roundtrip time and computes the latency as half of
the roundtrip time.

The Ping-Pong scenario avoids clock desynchronization issues that might
occur between two machines that do not accurately share the same
perception of the time in the network.

.. image:: /_static/gettingstarted-figures/4.3-1.png
   :align: center

To differentiate  the two operational modes, the ``ddsperf`` tool can
operate either in a **Ping mode** or in a **Pong mode**.

To run this scenario, open two terminals (e.g., on Linux-like OSs) and run
the following commands in either of the terminals. The graphical
*python-based* alternative is also noted.

.. code-block:: bash

    ddsperf ping
    cyclonedds performance ping

Input this command in another terminal:

.. code-block:: bash

    ddsperf pong
    cyclonedds performance pong

This basic scenario performs a simple latency test with all the default
values. You may customize your test scenario by changing the following
options.

* In **Ping mode** you can specify:

  * The **Rate** and frequency at which data is written. This is
    specified through the [R[Hz]] option. The default rate is "as fast as
    possible". In **ping** mode, it always sends a new ping as soon as it
    gets a pong

  * The **Size** of the data that is exchanged. This is specified
    through the [Size S] option. Using the built-in default topic, 12 bytes 
    (an integer key, an integer sequence number, and an empty sequence of bytes) 
    are sent every time. The size is "as small as possible" by default, 
    depending on the default topic size.

  * The **Listening** mode, which can either be ``waitset`` based or
    ``Listener`` Callbacks modes. In the waitset mode the ``ddsperf``
    application creates a dedicated thread to wait for the data to return
    back from the receiving instance of ``ddsperf`` tool (i.e the instance
    started with the Pong mode). In the Listener Callback mode, the
    thread is created by the |var-project| middleware. The Listener mode is
    the default.

* In **Pong mode**,you can only specify one option:

  * The **Listening** mode (with two possible values, ``waitset`` or
    ``Listener``)


For instance, if you want to measure local latency between to processes
exchanging 2KB at the frequency of 50Hz, you can run the following
commands in 2 different terminals:

.. code-block:: bash

    ddsperf ping 50Hz 2048 waitset
    cyclonedds performance ping --rate 50Hz --size 2048 --triggering-mode waitset

.. code-block:: bash

    ddsperf pong waitset
    cyclonedds performance pong --triggering-mode waitset

The output of the ``ddsperf`` tool is as shown below:

1. The output for the **Ping** application indicates mainly:

-  The **size of the data** involved in the test (e.g. 12 bytes)
-  The **minimum latency** (e.g. 78.89 us)
-  The **maximum latency** (e.g. 544,85 us)
-  The **mean latency** (e.g. 118.434 us)
-  As well as the latency at 50%, 90%, or 99% of the time.

.. image:: /_static/gettingstarted-figures/4.3-2.png
   :align: center

2. The output for the **Pong** application:

-  **RSS** is the Resident Set Size; it indicates the amount of memory
   used by the process (e.g. 3.5MB used by the process id 2680);
-  **VCSW** is the number of voluntary switches, it indicates the
   times when the process waits for input or an event (e.g. 2097 times);
-  **IVCSW** is the number of involuntary switches, it indicates the
   times when the process is pre-empted or blocked by a mutex (e.g. 6
   times);
-  The percentage of time spent executing user code and the percentage
   of time spent executing the kernel code in a specific thread (e.g. spent
   almost 0% of the time executing the user code and 5% executing kernel
   code in thread "ping").

.. image:: /_static/gettingstarted-figures/4.3-3.png
   :align: center

Measuring throughput
--------------------

To measure throughput between two different applications, you need to
run at least two instances of the ``ddsperf``\ tool and instruct one of
them to endorse the role of a Publisher that sends a given amount of
data (a sequence of octets) at a given rate. The other instances take
the role of Subscriber applications. 

**Note:** When your scenario involves only one subscriber, the UDP unicast mode is used. 
If several subscriber instances are running, the multicast is automatically used.

.. image:: /_static/gettingstarted-figures/4.4-1.png
   :align: center

Two additional modes are therefore supported:

The **Pub** mode and the **Sub** mode.

In the Sub mode, the subscriber operates either:

-  Using the **Listener** notification mechanism,
-  The **WaitSet** notification mechanism, or
-  The **Pooling** mode. The pooling mode allows the subscriber to
   cyclically fetch the data from its local cache instead of being
   notified each time a new set of data is added to the subscriber's
   cache as is the case with the other modes.

You can publish data in two ways by publishing each data sample
individually or by sending them in a *Burst* mode.

-  The **Rate** and frequency at which data is written. This is
   specified through the [R[Hz]] option. The default rate is "as fast as
   possible". This means, in **pub** mode, instead of trying to reach a
   certain rate, it just pushes data as hard as it can.

-  The **Size** of the data that is exchanged. This is specified through
   the [Size S] option. The size is "as small as possible" by default,
   depending on the size of the topic it defaults to.
-  The **Burst Size** , defines the number of data samples issued together as a batch. 
   This parameter is defined by the [Burst N] option. The default size for burst is 1. 
   It doesn't make much difference when going "as fast as possible", and it only applies
   to the **pub** mode.
-  The triggering mode by default is *listener* for the **ping** ,
   **pong** and **sub** mode.

To run a simple throughput test, you can simply run a **pub** mode and a
**sub** mode in 2 different terminals without specifying any other
options or you can customize it as shown below:

Open two terminals, navigate to the directory where ``ddsperf`` is located
and write the following command:

.. code-block:: bash

    ddsperf pub size 1k
    cyclonedds performance publish --size 1k

And in the other terminal, type in:

.. code-block:: bash

    ddsperf -Qrss:1 sub
    cyclonedds performance -Qrss:1 subscribe

This measures the throughput of data samples with 1Kbytes written as
fast as possible.

The ``-Qrss:1`` option in **sub** mode sets the maximum allowed an increase
in RSS as 1MB. When running the test, if the memory occupied by the
process increases by less than 1MB, the test can successfully run.
Otherwise, an error message is printed out at the end of the test.

As the ``pub`` in this example only has a size of 1k, the sub does not
print out an RSS error message at the end of the test.

The output of the ``ddsperf`` tool when measuring throughput is as shown
below:

1. The output for the **Pub** application indicates mainly:

-  **RSS** is the Resident Set Size; it indicates the amount of memory
   is used by the process (e.g. 6.3MB used by the process id "4026");
-  **VCSW** is the number of voluntary switches, it indicates the
   times when the process waits for input or an event (e.g. 1054 times);
-  **IVCSW** is the number of involuntary switches, it indicates the
   times when the process is pre-empted or blocked by a mutex (e.g. 24
   times);
-  The percentage of time spent executing user code and the percentage
   of time spent executing kernel code in a specific thread (e.g. spent
   34% of the time executing the user code and 11% executing kernel code
   in thread "pub").

.. image:: /_static/gettingstarted-figures/4.4-2.png
   :align: center

2. The output for the **Sub** application indicates mainly:

-  The **size of the data** involved in this test (e.g. 1024 bytes,
   which is the "size 1k" defined in the pub command)
-  The **total packets received** (e.g. 614598);
-  The **total packets lost** t (e.g. 0);
-  The **packets received in a 1 second reporting period** (e.g.
   212648);
-  The **packets lost in a 1 second report period** (e.g. 0);
-  The **number of samples processed by the Sub application** in 1s
   (e.g. 21260 KS/s, with the unit KS/s is 1000 samples per second).

.. image:: /_static/gettingstarted-figures/4.4-3.png
   :align: center


Measuring Throughput and Latency in a mixed scenario
----------------------------------------------------

In some scenarios, it might be helpful to measure the throughput and
latency at the same time.

The ``ddsperf`` tool allows you to mix these two scenarios. 
The Ping mode can be combined with the Pub mode to address such cases.

The [Ping x%] switch combined with the Pub mode allows you to send a
fraction of samples x% as if they were used in the Ping mode.

The different modes of the ``ddsperf`` tool are summarized in the figure
below.

.. image:: /_static/gettingstarted-figures/4.5-1.png
   :align: center

You can get more information for the ``ddsperf`` tool by using the [help]
option:

.. code-block:: bash

    ddsperf help
    cyclonedds performance --help

Additional options
------------------

As well as selecting the ``mode``, you can also select the ``options``
to specify how to send and receive the data (such as modifying the
reliable QoS from Reliable to Best-Effort with the ``-u`` option), or
how to evaluate or view the data in the ``ddsperf``\ tool.

The ``options`` you can select are listed in the ``ddsperf`` ``help``
menu, as shown below.

.. image:: /_static/gettingstarted-figures/4.6-1.png
   :align: center
