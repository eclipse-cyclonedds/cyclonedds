..
   Copyright(c) 2006 to 2018 ADLINK Technology Limited and others

   This program and the accompanying materials are made available under the
   terms of the Eclipse Public License v. 2.0 which is available at
   http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
   v. 1.0 which is available at
   http://www.eclipse.org/org/documents/edl-v10.php.

   SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

Throughput
==========

Description
***********

The Throughput example allows the measurement of data throughput when receiving samples from a publisher.


Design
******

It consists of 2 units:

- Publisher: sends samples at a specified size and rate.
- Subscriber: Receives samples and outputs statistics about throughput

Scenario
********

The **publisher** sends samples and allows you to specify a payload size in bytes as well as allowing you to specify whether
to send data in bursts. The **publisher** will continue to send data forever unless a time-out is specified.

Configurable:

- payloadSize: the size of the payload in bytes
- burstInterval: the time interval between each burst in ms
- burstSize: the number of samples to send each burst
- timeOut: the number of seconds the publisher should run for (0=infinite)
- partitionName: the name of the partition

The **subscriber** will receive data and output the total amount received and the data-rate in bytes-per-second. It will
also indicate if any samples were received out-of-order. A maximum number of cycles can be specified and once this has
been reached the subscriber will terminate and output totals and averages.

The **subscriber** executable measures:

- transferred: the total amount of data transferred in bytes.
- outOfOrder: the number of samples that were received out of order.
- transfer rate: the data transfer rate in bytes per second.
- subscriber also calculates statistics on these values over a configurable number of cycles.

Configurable:

- maxCycles: the number of times to output statistics before terminating
- pollingDelay
- partitionName: the name of the partition


Running the example
*******************

It is recommended that you run ping and pong in separate terminals to avoid mixing the output.

- Open 2 terminals.
- In the first terminal start Publisher by running publisher

  publisher usage (parameters must be supplied in order):
    ``./publisher [payloadSize (bytes)] [burstInterval (ms)] [burstSize (samples)] [timeOut (seconds)] [partitionName]``
  defaults:
    ``./publisher 8192 0 1 0 "Throughput example"``

- In the second terminal start Ping by running subscriber

  subscriber usage (parameters must be supplied in order):
    ``./subscriber [maxCycles (0=infinite)] [pollingDelay (ms, 0 = event based)] [partitionName]``
  defaults:
    ``./subscriber 0 0 "Throughput example"``

- To achieve optimal performance it is recommended to set the CPU affinity so that ping and pong run on separate CPU cores,
  and use real-time scheduling. In a Linux environment this can be achieved as follows:

  publisher usage:
    ``taskset -c 0 chrt -f 80 ./publisher [payloadSize (bytes)] [burstInterval (ms)] [burstSize (samples)] [timeOut (seconds)] [partitionName]``
  subscriber usage:
    ``taskset -c 1 chrt -f 80 ./subscriber [maxCycles (0 = infinite)] [pollingDelay (ms, 0 = event based)] [partitionName]``

  On Windows the CPU affinity and prioritized scheduling class can be set as follows:

  publisher usage:
    ``START /affinity 1 /high cmd /k "publisher.exe" [payloadSize (bytes)] [burstInterval (ms)] [burstSize (samples)] [timeOut (seconds)] [partitionName]``
  subscriber usage:
    ``START /affinity 2 /high cmd /k "subscriber.exe" [maxCycles (0 = infinite)] [pollingDelay (ms, 0 = event based)] [partitionName]``





