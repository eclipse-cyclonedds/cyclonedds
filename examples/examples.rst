..
   Copyright(c) 2006 to 2018 ADLINK Technology Limited and others

   This program and the accompanying materials are made available under the
   terms of the Eclipse Public License v. 2.0 which is available at
   http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
   v. 1.0 which is available at
   http://www.eclipse.org/org/documents/edl-v10.php.

   SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

Examples
========

.. toctree::
   :maxdepth: 1
   :caption: Contents:

   helloworld/readme
   roundtrip/readme
   throughput/readme

Configuration
*************

Eclipse Cyclone DDS has various configuration parameters and comes with a default built-in
configuration.  To run an example, or any application that uses Eclipse Cyclone DDS for its data
exchange, this default configuration is usually fine and no further action is required.

Configuration parameters for Eclipse CycloneDDS are expressed in XML and grouped together in a
single XML file.  To use a custom XML configuration in an application, the ``CYCLONEDDS_URI``
environment variable needs to be set prior to starting the application and pointed to the location
of the configuration file to be used.

| *Example*
| **Windows:** ``set CYCLONEDDS_URI=file://%USERPROFILE%/CycloneDDS/my-config.xml``
| **Linux:** ``export CYCLONEDDS_URI="file://$HOME/CycloneDDS/my-config.xml"``

The Eclipse CycloneDDS installation comes with a configuration file that corresponds to the default
behaviour.  You can modify it or add your using any text or XML editor, or using by using the
Eclipse CycloneDDS Configurator tool, which provides context-sensitive help on available
configuration parameters and their applicability.

One very important part of the configuration settings are the "tracing" settings: these allow
letting Eclipse Cyclone DDS trace very detailed information to a file, and this includes the actual
configuration settings in use, including all those that are set at the default.  When editing
configuration files by hand, this overview can be very useful.  Increasing the Verbosity from
"warning" to, e.g., "config" already suffices for getting this information written to the log.
