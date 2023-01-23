..
   Copyright(c) 2006 to 2019 ZettaScale Technology and others

   This program and the accompanying materials are made available under the
   terms of the Eclipse Public License v. 2.0 which is available at
   http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
   v. 1.0 which is available at
   http://www.eclipse.org/org/documents/edl-v10.php.

   SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

.. index:: 
   single: Examples; HelloWorld
   single: HelloWorld example
   single: HelloWorld

.. _helloworld_bm:

HelloWorld
==========

The basic HelloWorld example describes the steps necessary for setting up DCPS entities 
(see also :ref:`helloworld_main`).

.. note:: 
   The HelloWorld example is referenced throughout the :ref:`Getting_Started` Guide.

Design
******

The HelloWorld example consists of two units:

- **HelloworldPublisher**: implements the publisher's main
- **HelloworldSubscriber**: implements the subscriber's main

Scenario
********

The publisher sends a single HelloWorld sample. The sample contains two fields:

- a ``userID`` field (long type)
- a ``message`` field (string type)

When it receives the sample, the subscriber displays the ``userID`` and the ``message`` field.

Running the example
*******************

To avoid mixing the output, run the subscriber and publisher in separate terminals.

.. include:: ../../../docs/manual/getting_started/helloworld/helloworld_run.part.rst
