.. include:: ../external-links.part.rst

.. index:: Software prerequisites, Prerequisites

.. _software_prerequisites:

Prerequisites
=============

Install the following software on your machine:

 - A C compiler (For example, GCC or Clang on Linux, Visual Studio on Windows (MSVC), 
   Clang on macOS).
   
 - |url::git_link| version control system.
 - |url::cmake_link|, version 3.10 or later, see :ref:`cmake_config`.
 - Optionally, |url::openssl_link|, preferably version 1.1 later to use TLS over TCP.

To obtain the dependencies for |var-project-short|, follow the platform-specific instructions:

.. tabs::

    .. group-tab:: Linux

      .. code-block:: console

        To install the dependencies, use a package manager. For example:

            yum install git cmake gcc
            apt-get install git cmake gcc
            aptitude install git cmake gcc
            # or others

    .. group-tab:: macOS

      .. code-block:: console

        Install XCode from the App Store.

    .. group-tab:: Windows

      Install Visual Studio Code for the C compiler, then install the |url::chocolatey_link|.

      .. code-block:: console

            choco install cmake
            choco install git

     Alternatively, to install the dependencies, use |url::scoop_link|.

.. _tools:

Additional tools
----------------

While developing for |var-project|, additional tools and dependencies may be required. The following is a list of the suggested tools:

 - Shared memory
    |url::iceoryx_link|
 - Unit testing / Development
    |url::cunit_link|
 - Documentation
    |url::sphinx_link|
 - Security 
    |url::openssl_link|
