..
   Copyright(c) 2006 to 2022 ZettaScale Technology and others

   This program and the accompanying materials are made available under the
   terms of the Eclipse Public License v. 2.0 which is available at
   http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
   v. 1.0 which is available at
   http://www.eclipse.org/org/documents/edl-v10.php.

.. SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

.. _config-docs:

###################
Configuration Guide
###################

|var-project| has various configuration parameters and comes with a default built-in
configuration.  To run an example, or any application that uses |var-project| for its data
exchange, this default configuration is usually fine and no further action is required.

This document attempts to provide background information that will help in adjusting the
configuration when the default settings do not give the desired behavior. A full listing of
all settings is included as generated document directly from the source code in the
:ref:`configuration_reference`. These also list the default value for each parameter.

Configuration parameters for |var-project-short| are expressed in XML and grouped together in an
XML file. To use a custom XML configuration in an application, the ``CYCLONEDDS_URI``
environment variable needs to be set prior to starting the application and pointed to the location
of the configuration file to be used.

| *Example*
| **Windows:** ``set CYCLONEDDS_URI=file://%USERPROFILE%/CycloneDDS/my-config.xml``
| **Linux:** ``export CYCLONEDDS_URI="file://$HOME/CycloneDDS/my-config.xml"``

If you run into any issues the first place to start are the "tracing" settings: these allow
you to trace very detailed information, and this includes the actual
configuration settings in use, including all those that are set to the default. When editing
configuration files by hand, this overview can be very useful. Increasing the Verbosity from
"warning" to, e.g., "config" already suffices for getting this information written to the log.

This configuration guide is broken down into three parts:

 - A high level description of the DDSI Concepts,
 - A section that goes through the |var-project| specifics,
 - A description of all supported configuration parameters, generated from the source code.

Users that are already familiar with the concepts of DDS can go directly to the :ref:`cyclonedds_specifics`.

.. toctree::
   :maxdepth: 2

   ddsi_concepts
   cyclonedds_specifics
   config_file_reference
