..
   Copyright(c) 2022 ZettaScale Technology and others

   This program and the accompanying materials are made available under the
   terms of the Eclipse Public License v. 2.0 which is available at
   http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
   v. 1.0 which is available at
   http://www.eclipse.org/org/documents/edl-v10.php.

   SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

dynsub
======

Description
***********

The "dynsub" example is a PoC for a C-based JSON printer for arbitrary data. It assumes
that topic discovery is enabled, but doesn't require it.

Running the example
*******************

Pass it the name of a topic and it will wait for some time for a writer of that topic to
show up.  Once it finds on in the DCPSPublication topic, it then tries to subscribe and
print the received samples as JSON.

It is far from complete and definitely buggy.  It is just a PoC!

For example: start the HelloworldPublisher in one shell:

    # bin/HelloworldPublisher
    === [Publisher]  Waiting for a reader to be discovered ...

then in another shell start this program:

    # bin/dynsub HelloWorldData_Msg
    {"userID":1,"message":"Hello World"}
    {"userID":1}

The second line is the "invalid sample" generated because of the termination of the
publisher.  In Cyclone DDS that means only the key fields are valid and so that is what
gets printed.

There is a small publisher program "variouspub" that can publish a number of different
types to make things a bit more interesting than HelloWorldData_Msg.  Pass it the name of
the type to publish, like:

    # bin/variouspub B

This will publish samples at 1Hz until killed.
