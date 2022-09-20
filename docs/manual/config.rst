..
   Copyright(c) 2006 to 2022 ZettaScale Technology and others

   This program and the accompanying materials are made available under the
   terms of the Eclipse Public License v. 2.0 which is available at
   http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
   v. 1.0 which is available at
   http://www.eclipse.org/org/documents/edl-v10.php.

.. SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#########################################
Eclipse Cyclone DDS Configuration Options
#########################################

This document attempts to provide background information that will help in adjusting the
configuration of Eclipse Cyclone DDS when the default settings do not give the desired behavior.
A full listing of all settings is out of scope for this document, but can be extracted
from the sources.

The configuration guide is broken down into two parts: A high level description of the :ref:`DDSI Concepts` where the user can learn about the generic DDSI concepts, and a separate
section that goes through the Eclipse Cyclone specifics.

Users that are already familiar with the concepts of DDS can go directly to the :ref:`Eclipse Cyclone DDS Specifics`, to learn how to configure Eclipse Cyclone DDS.

.. toctree::
   :maxdepth: 2

   config/ddsi_concepts
   config/cyclonedds_specifics
   
DDSI Concepts
=============
The DDSI standard is intimately related to the DDS 1.2 and 1.4 standards, with a clear correspondence between the entities in DDSI and those in DCPS. However, this correspondence is not one-to-one.

In this section we give a high-level description of the concepts of the DDSI specification, with hardly any reference to the specifics of the Eclipse Cyclone DDS implementation (addressed in Eclipse Cyclone DDS Specifics). This division aids readers interested in interoperability in understanding where the specification ends and the Eclipse Cyclone DDS implementation begins.

Mapping of DCPS Domains to DDSI Domains
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Mapping of DCPS Entities to DDSI Entities
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Reliable Communication
~~~~~~~~~~~~~~~~~~~~~~

DDSI-Specific Transient-Local Behaviour
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Discovery of Participants & Endpoints
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~





Eclipse Cyclone DDS Specifics
=============================

Discovery Behaviour
~~~~~~~~~~~~~~~~~~~

Proxy Participants and Endpoints
'''''''''''''''''''''''''''''''''


Network and Discovery Configuration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Combining Multiple Participants
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Data Path Configuration
~~~~~~~~~~~~~~~~~~~~~~~

Network Partition Configuration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Thread Configuration
~~~~~~~~~~~~~~~~~~~~

Reporting and Tracing
~~~~~~~~~~~~~~~~~~~~~

Compatibility and Conformance
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~








