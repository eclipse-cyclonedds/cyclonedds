..
   Copyright(c) 2006 to 2019 ZettaScale Technology and others

   This program and the accompanying materials are made available under the
   terms of the Eclipse Public License v. 2.0 which is available at
   http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
   v. 1.0 which is available at
   http://www.eclipse.org/org/documents/edl-v10.php.

   SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
   
.. index:: 
   single: Examples; Throughput
   single: Throughput example
  
.. _throughput_bm:


Throughput
==========

The Throughput example allows the measurement of data throughput when receiving samples from a publisher.

Design
******

The Throughput example consists of two units:

- **Publisher**: Sends samples at a specified size and rate.
- **Subscriber**: Receives samples and outputs statistics about throughput

Scenario
********

The **publisher** sends the samples. You can specify a payload size in bytes, and whether
to send data in bursts. Unless a time-out is specified, the publisher sends data forever.

Configuration:

- ``payloadSize``: The size of the payload in bytes.
- ``burstInterval``: The time interval between each burst in ms.
- ``burstSize``: The number of samples to send each burst.
- ``timeOut``: The number of seconds the publisher should run for (0=infinite).
- ``partitionName``: The name of the partition.

The **subscriber** receives the data and outputs: 

- The total amount of data received.
- The data-rate in bytes-per-second.
- Whether any samples were received out-of-order. 

You can specify the maximum number of cycles. When the maximum is reached, the 
subscriber terminates and outputs the totals and averages.

The **subscriber** executable measures:

- ``transferred``: the total amount of data transferred in bytes.
- ``outOfOrder``: the number of samples that were received out of order.
- The transfer rate: the data transfer rate in bytes per second.
- The subscriber also calculates statistics on these values over a configurable number of cycles.

Configuration:

- ``maxCycles``: The number of times to output statistics before terminating.
- ``pollingDelay``
- ``partitionName``: The name of the partition.

Running the example
*******************

To avoid mixing the output, run the publisher and subscriber in separate terminals.

#. Open 2 terminals.

#. In the first terminal start publisher by running **publisher**.

   Publisher usage (parameters must be supplied in order):

   ``./publisher [payloadSize (bytes)] [burstInterval (ms)] [burstSize (samples)] [timeOut (seconds)] [partitionName]``

   Defaults:

   ``./publisher 8192 0 1 0 "Throughput example"``

#. In the second terminal start subscriber by running **subscriber**.

   Subscriber usage (parameters must be supplied in order):

   ``./subscriber [maxCycles (0=infinite)] [pollingDelay (ms, 0 = event based)] [partitionName]``

   Defaults:

   ``./subscriber 0 0 "Throughput example"``  

#. To achieve optimal performance, set the CPU affinity so that the publisher and subscriber 
   run on separate CPU cores and use real-time scheduling:

  .. tabs::

    .. group-tab:: Linux

      .. code-block:: console

        Publisher usage:
          ``taskset -c 0 chrt -f 80 ./publisher [payloadSize (bytes)] [burstInterval (ms)] [burstSize (samples)] [timeOut (seconds)] [partitionName]``
        Subscriber usage:
          ``taskset -c 1 chrt -f 80 ./subscriber [maxCycles (0 = infinite)] [pollingDelay (ms, 0 = event based)] [partitionName]``

    .. group-tab:: Windows

      .. code-block:: console

        Publisher usage:
          ``START /affinity 1 /high cmd /k "publisher.exe" [payloadSize (bytes)] [burstInterval (ms)] [burstSize (samples)] [timeOut (seconds)] [partitionName]``
        Subscriber usage:
          ``START /affinity 2 /high cmd /k "subscriber.exe" [maxCycles (0 = infinite)] [pollingDelay (ms, 0 = event based)] [partitionName]``
