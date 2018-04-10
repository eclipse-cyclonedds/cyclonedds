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

Cyclone DDS has various configuration parameters and comes with a default built-in configuration.
To run an example, or any application that uses Cyclone DDS for its data exchange, this default
configuration is usually fine and no further action is required.

Configuration parameters for CycloneDDS are expressed in XML and grouped together in a single XML file.
To use a custom XML configuration in an application, the ``CYCLONEDDS_URI`` environment variable needs
to be set prior to starting the application and pointed to the location of the configuration file to
be used.

| *Example*
| **Windows:** ``set CYCLONEDDS_URI=file://%USERPROFILE%/CycloneDDS/my-config.xml``
| **Linux:** ``export CYCLONEDDS_URI="file://$HOME/CycloneDDS/my-config.xml"``

The CycloneDDS installation comes with a set of standard configuration files for common use cases.
You update existing configuration files or create your own by using the CycloneDDS Configurator tool,
which provides context-sensitive help on available configuration parameters and their applicability.

You can start the CycloneDDS Configuration tool through the CycloneDDS Launcher, or from your command-prompt
by entering the tools directory and running ``java -jar cycloneddsconf.jar``. The default location of the tools
directory is ``/usr/share/CycloneDDS/tools`` on Linux or ``C:\Program Files\ADLINK\Vortex DDS\share\CycloneDDS\tools``
on Windows.
