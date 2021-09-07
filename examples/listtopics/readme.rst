..
   Copyright(c) 2021 ADLINK Technology Limited and others

   This program and the accompanying materials are made available under the
   terms of the Eclipse Public License v. 2.0 which is available at
   http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
   v. 1.0 which is available at
   http://www.eclipse.org/org/documents/edl-v10.php.

   SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

listtopics
==========

Description
***********

The "listtopics" example shows how to monitor which topics have been defined in the
system.

Running the example
*******************

Discovering remote topics is only possible when topic discovery is enabled in the
configuration using:

    <Discovery>
      <EnableTopicDiscoveryEndpoints>true</EnableTopicDiscoveryEndpoints>
    </Discovery>

most applications create some topics and those are always visible, but this example
creates none and so yields no output if there is no discovery of remote topics.

It does make an effort to detect this case by also monitoring the discovery of other
participants.  If any show up but no topics are discovered, it will print a warning.
Running two copies of this program (and nothing) else will always trigger this warning.
