.. _prerequisites:

.. links to external sites that open in a separate tab:

.. |url::git_link| raw:: html

   <a href="https://git-scm.com/" target="_blank">Git</a>

.. |url::cmake_link| raw:: html

   <a href="https://cmake.org/download/" target="_blank">CMake</a>

.. |url::openssl_link| raw:: html

   <a href="https://www.openssl.org/" target="_blank">OpenSSL</a>

.. |url::iceoryx_link| raw:: html

   <a href="https://projects.eclipse.org/proposals/eclipse-iceoryx/" target="_blank">Eclipse iceoryx</a>

.. |url::cunit_link| raw:: html

   <a href="https://cunit.sourceforge.net/" target="_blank">CUnit</a>

.. |url::sphinx_link| raw:: html

   <a href="https://www.sphinx-doc.org/en/master/" target="_blank">Sphinx</a>
 

Prerequisites
=============

Install the following software on your machine:

 - A C compiler (For example, GCC or clang on Linux, Visual Studio on Windows, XCode on macOS)
 - |url::git_link| version control system;
 - |url::cmake_link|, version 3.10 or later;
 - Optionally, |url::openssl_link|, preferably version 1.1 later to use TLS over TCP.

To obtain the dependencies for |var-project-short|, follow the platform-specific instructions:

.. tabs::

    .. group-tab:: Linux

        To install the dependencies, use a package manager. For example:

        .. code-block:: bash

            yum install git cmake gcc
            apt-get install git cmake gcc
            aptitude install git cmake gcc
            # or others

    .. group-tab:: macOS

        Install XCode from the App Store.

    .. group-tab:: Windows

        Install Visual Studio Code for the C compiler, then install the `chocolatey package manager <https://chocolatey.org/>`_.

        .. code-block:: bash

            choco install cmake
            choco install git

        Alternatively, to install the dependencies, use `scoop <https://scoop.sh/>`_.

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
