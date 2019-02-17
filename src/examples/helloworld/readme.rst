..
   Copyright(c) 2006 to 2018 ADLINK Technology Limited and others

   This program and the accompanying materials are made available under the
   terms of the Eclipse Public License v. 2.0 which is available at
   http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
   v. 1.0 which is available at
   http://www.eclipse.org/org/documents/edl-v10.php.

   SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

HelloWorld
==========

Description
***********

The basic HelloWorld example is used to illustrate the necessary steps to setup DCPS entities.
Note it is also used in the Getting Started Guide to explain the usage of Eclipse Cyclone DDS.

Design
******

It consists of 2 units:

- HelloworldPublisher: implements the publisher's main
- HelloworldSubscriber: implements the subscriber's main

Scenario
********

The publisher sends a single HelloWorld sample. The sample contains two fields:

- a userID field (long type)
- a message field (string type)

When it receives the sample, the subscriber displays the userID and the message field.

Running the example
*******************

It is recommended that you run the subscriber and publisher in separate terminals to avoid mixing the output.

- Open 2 terminals.
- In the first terminal start the subscriber by running HelloWorldSubscriber
- In the second terminal start the publisher by running HelloWorldPublisher
