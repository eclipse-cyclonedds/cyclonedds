..
   Copyright(c) 2021 ZettaScale Technology and others

   This program and the accompanying materials are made available under the
   terms of the Eclipse Public License v. 2.0 which is available at
   http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
   v. 1.0 which is available at
   http://www.eclipse.org/org/documents/edl-v10.php.

   SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
   
.. index:: 
   single: Examples; listtopics
   single: listtopics example
  
.. _listtopics_bm:

listtopics
==========

The listtopics example shows how to monitor topics that have been defined in the system.

Running the example
*******************

Discovering remote topics is only possible when topic discovery is enabled. To configure 
topic discovery set:

.. code-block:: xml

    <Discovery>
      <EnableTopicDiscoveryEndpoints>true</EnableTopicDiscoveryEndpoints>
    </Discovery>

Most applications create topics, which are always visible. The listtopics example creates 
no topics and therefore, if there is no discovery of remote topics, shows no output. To
mitigate this, the listtopics example also monitors the discovery of other participants. 
If any show up but no topics are discovered, it prints a warning.

.. note::
   Running two copies of the listtopics example (and nothing else) always triggers a warning.
