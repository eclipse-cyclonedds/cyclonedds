..
   Copyright(c) 2022 ZettaScale Technology and others

   This program and the accompanying materials are made available under the
   terms of the Eclipse Public License v. 2.0 which is available at
   http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
   v. 1.0 which is available at
   http://www.eclipse.org/org/documents/edl-v10.php.

   SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
   
.. index:: 
   single: Examples; dynsub
   single: dynsub example

.. _dynsub_bm:

dynsub
======

The dynsub example is a :term:`PoC` for a C-based JSON printer of arbitrary data. It assumes
that topic discovery is enabled, but doesn't require it.

Running the example
*******************

Pass the name of a topic to dynsub and it waits for a writer of that topic to show up. When it 
finds one in the DCPSPublication topic, it tries to subscribe and print the received samples as JSON.

For example: Start the ``HelloworldPublisher`` in one shell:

.. code-block:: c

    # bin/HelloworldPublisher
    === [Publisher]  Waiting for a reader to be discovered ...

In another shell start ``dynsub``:

.. code-block:: c

    # bin/dynsub HelloWorldData_Msg
    {"userID":1,"message":"Hello World"}
    {"userID":1}

The second line is the "invalid sample" generated because of the termination of the
publisher. In |var-project-short|, only the key fields are valid, and therefore printed.

Instead of the HelloWorldData_Msg, the small publisher program "variouspub" can publish 
a number of different types. Pass it the name of the type to publish. For example:

.. code-block:: c

    # bin/variouspub B

This publishes samples at 1Hz until killed.
