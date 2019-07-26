..
   Copyright(c) 2006 to 2018 ADLINK Technology Limited and others

   This program and the accompanying materials are made available under the
   terms of the Eclipse Public License v. 2.0 which is available at
   http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
   v. 1.0 which is available at
   http://www.eclipse.org/org/documents/edl-v10.php.

   SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

Roundtrip
==========

Description
***********

The Roundtrip example allows the measurement of roundtrip duration when sending and receiving back a single message.

Design
******

It consists of 2 units:

- Pong: waits for messages from ping and sends the same message back.
- Ping: Sends a message to pong and waits for its return.

Scenario
********

A message is sent by the **ping** executable on the "PING" partition, which the **pong** executable is waiting for.
The **pong** executable sends the same message back on the "PONG" partition, which the **ping** executable is waiting for.
This sequence is repeated a configurable number of times.

The **ping** executable measures:

- writeAccess time: time the write() method took.
- readAccess time: time the take() method took.
- roundTrip time: time between the call to the write() method and the return of the take() method.
- **ping** also calculates min/max/average statistics on these values over a configurable number of samples and/or time out period.

Configurable:

- payloadSize: the size of the payload in bytes.
- numSamples: the number of samples to send.
- timeOut: the number of seconds ping should run for.


Running the example
*******************

It is recommended that you run ping and pong in separate terminals to avoid mixing the output.

- Open 2 terminals.
- In the first terminal start Pong by running pong.

  pong usage:
    ``./pong``

- In the second terminal start Ping by running ping.

  ping usage (parameters must be supplied in order):
    ``./ping [payloadSize (bytes, 0 - 655536)] [numSamples (0 = infinite)] [timeOut (seconds, 0 = infinite)]``

    ``./ping quit - ping sends a quit signal to pong.``
  defaults:
    ``./ping 0 0 0``

- To achieve optimal performance it is recommended to set the CPU affinity so that ping and pong run on separate CPU cores,
  and use real-time scheduling. In a Linux environment this can be achieved as follows:

  pong usage:
    ``taskset -c 0 chrt -f 80 ./pong``
  ping usage:
    ``taskset -c 1 chrt -f 80 ./ping [payloadSize (bytes, 0 - 655536)] [numSamples (0 = infinite)] [timeOut (seconds, 0 = infinite)]``

  On Windows the CPU affinity and scheduling class can be set as follows:

  pong usage:
    ``START /affinity 1 /high cmd /k "pong.exe"``
  ping usage:
    ``START /affinity 2 /high cmd /k "ping.exe" [payloadSize (bytes, 0 - 655536)] [numSamples (0 = infinite)] [timeOut (seconds, 0 = infinite)]``

