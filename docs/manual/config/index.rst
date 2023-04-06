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
Configuration guide
###################

Configuration parameters for |var-project-short| are expressed in XML and grouped together in an
XML file. To use a custom XML configuration in an application, you must set the ``CYCLONEDDS_URI`` 
environment variable to the location of the configuration file. For example:

.. tabs::

    .. group-tab:: Windows

       .. code-block:: console
         
          set CYCLONEDDS_URI=file://%USERPROFILE%/CycloneDDS/my-config.xml

    .. group-tab:: Linux

       .. code-block:: console
         
          export CYCLONEDDS_URI="file://$HOME/CycloneDDS/my-config.xml"

The following shows an example XML configuration:

.. code-block:: xml
   :linenos:
   :caption: ``/path/to/dds/configuration.xml``

   <?xml version="1.0" encoding="utf-8"?>
   <CycloneDDS
     xmlns="https://cdds.io/config"
     xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
     xsi:schemaLocation="https://cdds.io/config https://raw.githubusercontent.com/eclipse-cyclonedds/cyclonedds/master/etc/cyclonedds.xsd"
   >
     <Domain Id="any">
       <General>
         <Interfaces>
           <NetworkInterface autodetermine="true" priority="default"
           multicast="default" />
         </Interfaces>
         <AllowMulticast>default</AllowMulticast>
         <MaxMessageSize>65500B</MaxMessageSize>
       </General>
       <Tracing>
         <Verbosity>config</Verbosity>
         <OutputFile>
           ${HOME}/dds/log/cdds.log.${CYCLONEDDS_PID}
         </OutputFile>
       </Tracing>
     </Domain>
   </CycloneDDS>

For a full listing of the configuration settings (and default value for each parameter) refer
to the :ref:`configuration_reference`, which is generated directly from the source code.

The configuration does not depend exclusively on the xml file. The content of the xml can be
set directly into the envrionment variable ``CYCLONEDDS_URI``. In the following block a
example is given for windows and linux. On windows it is important to set the quotation mark
directly after the ``set`` command, otherwise ``<`` and ``>`` has to be escaped with ``^``.

.. tabs::

    .. group-tab:: Windows

       .. code-block:: console

          set "CYCLONEDDS_URI=<CycloneDDS><Domain><General><NetworkInterfaceAddress>127.0.0.1</NetworkInterfaceAddress></General></Domain></CycloneDDS>"
          set CYCLONEDDS_URI=^<CycloneDDS^>^<Domain^>^<General^>^<NetworkInterfaceAddress^>127.0.0.1^</NetworkInterfaceAddress^>^</General^>^</Domain^>^</CycloneDDS^>

    .. group-tab:: Linux

       .. code-block:: console
         
          export CYCLONEDDS_URI="<CycloneDDS><Domain><General><NetworkInterfaceAddress>127.0.0.1</NetworkInterfaceAddress></General></Domain></CycloneDDS>"

The example configuration above is helpfull if you are developing on a machine with activated firewall.
Otherwise it would not be possible to send and receive messages between apps on the local machine.
The ip ``127.0.0.1`` expresses that the communication shall be restricted to your pc only (localhost).

Configuration log files
=======================

When editing configuration files, the ``cdds.log`` can be very useful for providing information about the build. 
To determine the information included in the log file, change the :ref:`Tracing/Verbosity <//CycloneDDS/Domain/Tracing/Verbosity>` settings.

.. toctree::
   :maxdepth: 1
   :hidden:
   
   cmake_config
   discovery-behavior
   discovery-config
   network-config
   combining-participants
   data-path-config
   network_interfaces
   thread-config
   reporting-tracing
   conformance
   config_file_reference
   benchmarking
