..
   Copyright(c) 2006 to 2018 ADLINK Technology Limited and others

   This program and the accompanying materials are made available under the
   terms of the Eclipse Public License v. 2.0 which is available at
   http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
   v. 1.0 which is available at
   http://www.eclipse.org/org/documents/edl-v10.php.

   SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

.. _`Uninstall`:

.. raw:: latex

    \newpage

######################
Uninstalling CycloneDDS
######################

*****
Linux
*****

Uninstalling CycloneDDS on Linux can be established by invoking
the following two commands (of which the first is optional):
::

    sudo dpkg --remove cyclonedds-dev
    sudo dpkg --remove cyclonedds

.. note::
    Mind the order in which these commands are run. The development
    package (:code:`cyclonedds-dev`) need to be removed first since
    it depends on the library version (:code:`cyclonedds`).

*******
Windows
*******

There are two ways to uninstall CycloneDDS from Windows

1. By using the original CycloneDDS :ref:`MSI <WindowsInstallMSI>` file
2. By using Windows "Apps & features"

Original MSI
============

Locate the original CycloneDDS MSI file on your system and start it.
After clicking :code:`Next`, an overview of options appears, amongst which
is the remove option. By clicking :code:`Remove`, all files and folders are
removed, except the CycloneDDS examples (if installed).

Apps & features
===============

Go to :code:`Windows Settings` by clicking the :code:`Settings`-icon ( |settings_icon| )
in the Windows Start Menu. Choose :code:`Apps` in the
:code:`Windows Settings` screen. A list of all installed apps
and programs pops up. Select :code:`CycloneDDS` and choose :code:`Uninstall`.
All installed files and folders will be removed, except the
CycloneDDS examples (if installed).

.. |settings_icon| image:: ../_static/pictures/settings-icon.png
  :height: 9
  :width: 9
