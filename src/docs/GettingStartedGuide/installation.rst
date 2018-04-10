..
   Copyright(c) 2006 to 2018 ADLINK Technology Limited and others

   This program and the accompanying materials are made available under the
   terms of the Eclipse Public License v. 2.0 which is available at
   http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
   v. 1.0 which is available at
   http://www.eclipse.org/org/documents/edl-v10.php.

   SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

.. _`Installation`:

.. raw:: latex

    \newpage

#################
Install CycloneDDS
#################

.. .. contents::


.. _`SystemRequirements`:

*******************
System requirements
*******************

Currently AdLink CycloneDDS is supported on the following platforms:

+-------------------+--------------+--------------------+
| Operating systems | Architecture | Compiler           |
+===================+==============+====================+
| Ubuntu 16.04 LTS  | 64-bit       | gcc 5.4 or later   |
+-------------------+--------------+--------------------+
| Windows 10        | 64 -bit      | VS2015             |
+-------------------+--------------+--------------------+



*****
Linux
*****

Ubuntu
======

On Ubuntu and other debian-derived platforms, the product can be installed using a native package.

::

    sudo dpkg -i cyclonedds_<version>_<architecture>.deb
    sudo dpkg -i cyclonedds-dev_<version>_<architecture>.deb


.. _`CopyLinuxExamplesToUserFriendlyLocation`:

Post install steps
~~~~~~~~~~~~~~~~~~

The installation package installs examples in system directories.
In order to have a better user experience when building the CycloneDDS
examples, it is advised to copy the examples to a user-defined location.
This is to be able to build the examples natively and experiment with
the example source code.

For this, the installation package provides the vdds_install_examples
script, located in /usr/bin.

Create an user writable directory where the examples should go. Navigate
to that directory and execute the script. Answer 'yes' to the questions
and the examples will be installed in the current location.

Type :code:`vdds_install_examples -h` for more information.


Red Hat
=======

Not supported yet (CHAM-326).


Tarball
=======

For more generic Linux installations, different tar-balls (with the same
content) are provided.

+----------------------------------+---------------------------------------+
| Tarball                          | Description                           |
+==================================+=======================================+
| CycloneDDS-<version>-Linux.tar.Z  | Tar Compress compression.             |
+----------------------------------+---------------------------------------+
| CycloneDDS-<version>-Linux.tar.gz | Tar GZip compression.                 |
+----------------------------------+---------------------------------------+
| CycloneDDS-<version>-Linux.tar.sh | Self extracting Tar GZip compression. |
+----------------------------------+---------------------------------------+

By extracting one of them at any preferred location, CycloneDDS can be used.

.. _`LinuxSetLibPath`:

Paths
=====

To be able to run CycloneDDS executables, the required libraries (like
libddsc.so) need to be available to the executables.
Normally, these are installed in system default locations and it works
out-of-the-box. However, if they are not installed in those locations,
it is possible that the library search path has to be changed.
This can be achieved by executing the command:
::

    export LD_LIBRARY_PATH=<install_dir>/lib:$LD_LIBRARY_PATH


*******
Windows
*******

.. _`WindowsInstallMSI`:

MSI
===

The default deployment method on Windows is to install the product using the MSI installer.

The installation process is self-explanatory. Three components are available:

1. a runtime component, containing the runtime libraries
2. a development component, containing the header files, the IDL compiler,
   a precompiled Hello Word! example and other examples.
3. an examples component, containing the source code of the CycloneDDS examples.

The runtime and development components are (by default) installed in "Program Files" while
the CycloneDDS example component will be installed in the User Profile directory.
The CycloneDDS example code in the User Profile directory can be changed by the user.


ZIP
===

The Windows installation is also provided as a ZIP file. By extracting it
at any preferred location, CycloneDDS can be used.

.. _`WindowsSetLibPath`:

Paths
~~~~~

To be able to run CycloneDDS executables, the required libraries (like
ddsc.dll) need to be available to the executables.
Normally, these are installed in system default locations and it works
out-of-the-box. However, if they are not installed on those locations,
it is possible that the library search path has to be changed.
This can be achieved by executing the command:
::

    set PATH=<install_dir>/bin;%PATH%

.. note::
      The MSI installer will add this path to the PATH environment
      variable automatically.

.. _`TestYourInstallation`:

**********************
Test your installation
**********************

The installation provides a simple prebuilt :ref:`Hello World! <HelloWorld>` application which
can be run in order to test your installation. The *Hello World!* application consists of two
executables: a so called HelloworldPublisher and a HelloworldSubscriber, typically located in
:code:`/usr/share/CycloneDDS/examples/helloworld/bin` on Linux and in
:code:`C:\Program Files\ADLINK\Cyclone DDS\share\CycloneDDS\examples\helloworld\bin` on Windows.

To run the example application, please open two console windows and navigate to the appropriate
directory in both console windows. Run the HelloworldSubscriber in one of the console windows by the
typing following command:

  :Windows: :code:`HelloworldSubscriber.exe`
  :Linux: :code:`./HelloworldSubscriber`

and the HelloworldPublisher in the other console window by typing:

  :Windows: :code:`HelloworldPublisher.exe`
  :Linux: :code:`./HelloworldPublisher`


The output HelloworldPublisher should look like

.. image:: ../_static/pictures/HelloworldPublisherWindows.png

while the HelloworldSubscriber will be looking like this

.. image:: ../_static/pictures/HelloworldSubscriberWindows.png

For more information on how to build this application your own and the code which has
been used, please have a look at the :ref:`Hello World! <HelloWorld>` chapter.

*******
License
*******

TODO: CHAM-325

