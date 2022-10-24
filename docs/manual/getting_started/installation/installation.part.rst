Installing |var-project|
========================

This publication provides detailed information about how install |var-project-short|. The chapters will cover:

 - The installation and build process of |var-project-short| core, including the C-APIs.
 - How to install the C++ support packages. Short C/C++, and Python tutorials are detailed to show examples of how the DDS technology is used to share data.


System prerequisites
--------------------

Before building the Cyclone DDS implementation, ensure you meet all the system prerequisites.
Failure to meet the prerequisites will cause the build to fail.


Supported platforms
^^^^^^^^^^^^^^^^^^^

At the time of writing this document, |var-project-short| supports Linux, macOS, and Windows and is known to work on FreeRTOS, QNX and the
Solaris-like Openindiana OS.


Software requirements
^^^^^^^^^^^^^^^^^^^^^

Make sure you have the following software installed on your machine:

 - A C compiler (most commonly GCC or clang on Linux, Visual Studio on Windows, XCode on macOS);
 - `Git <https://git-scm.com/>`__ version control system;
 - `CMake <https://cmake.org/download/>`__, version 3.10 or later;
 - Optionally, `OpenSSL <https://www.openssl.org/>`__, preferably version 1.1 later to use TLS over TCP.

You can obtain the dependencies to build |var-project-short| by following the platform-specific instructions:

.. tabs::

    .. group-tab:: Linux

        Install the dependencies with a package manager of your choice:

        .. code-block:: bash

            yum install git cmake gcc
            apt-get install git cmake gcc
            aptitude install git cmake gcc
            # or others

    .. group-tab:: macOS

        Installing XCode from the App Store should be sufficient.

    .. group-tab:: Windows

        First install Visual Studio Code for the C compiler. Then it is easiest to install the `chocolatey package manager <https://chocolatey.org/>`__.

        .. code-block:: bash

            choco install cmake
            choco install git

        Alternatively you might use `scoop <https://scoop.sh/>` to aquire the dependencies.


.. note::

    When developing for |var-project| you might need additional tools and dependencies. See also the :ref:`contributing` section.

Obtaining |var-project-short|
-----------------------------

You can obtain |var-project-short| via Git from the github-hosted repository

.. code-block:: bash

    git clone https://github.com/eclipse-cyclonedds/cyclonedds.git
    cd cyclonedds


Building |var-project-short|
----------------------------

You can build and install the required libraries needed to develop
your applications using |var-project-short| in a few simple steps, as shown below:

.. tabs::

    .. group-tab:: Linux

        .. code-block:: bash

            cd build
            cmake -DCMAKE_INSTALL_PREFIX=<install-location> -DBUILD_EXAMPLES=ON ..
            cmake --build . --parallel

    .. group-tab:: macOS

        .. code-block:: bash

            cd build
            cmake -DCMAKE_INSTALL_PREFIX=<install-location> -DBUILD_EXAMPLES=ON ..
            cmake --build . --parallel

    .. group-tab:: Windows

        .. code-block:: bash

            cd build
            cmake -G <generator-name> -DCMAKE_INSTALL_PREFIX=<install-location> -DBUILD_EXAMPLES=ON ..
            cmake --build . --parallel

        On Windows you can build |var-project-short| with one of several generators. Usually if you omit the
        ``-G <generator-name>`` it will pick a sensible default, but if it doesn't work or picks something
        unexpected you can go to the `CMake generators documentation <https://cmake.org/cmake/help/latest/manual/cmake-generators.7.html>`__.
        For example, "Visual Studio 15 2017 Win64" targets a 64-bit build using Visual Studio 2017.


If you need to reduce the footprint, or have issues with the `FindOpenSSL.cmake` script, you can explicitly disable this by passing `-DENABLE\_SSL=NO` to the CMake invocation. If you do not plan
on running the examples you may omit the ``-DBUILD_EXAMPLES=ON``.

To install it after a successful build:

.. code-block:: bash

    cmake --build . --target install

Depending on the installation location, you may need administrator privileges. The install step copies everything to:

 -  ``<install-location>/lib``
 -  ``<install-location>/bin``
 -  ``<install-location>/include/ddsc``
 -  ``<install-location>/share/CycloneDDS``

At this point, you are ready to use Cyclone DDS in your projects.

.. note:: Build types

    The default build type is a release build that includes debugging information (``RelWithDebInfo``).
    This build is suitable for applications because of its high-performance and debugging capabilities.
    If you prefer a Debug or pure Release build, add ``-DCMAKE_BUILD_TYPE=<build-type>`` to your CMake invocation.

Installing |var-project-short| C++ API
======================================

The |var-project| C++ API is an implementation of the DDS ISO/IEC C++ PSM API,
or simply put, a C++ binding for |var-project-short|. It is made of an
IDL compiler backend that uses an IDL data model to generate their C++
representation and artifacts, a software layer that maps some DDS APIs
on the |var-project| C API and direct access to the core APIs
when managing data to lower overhead.

Before starting, make sure you have installed the core |var-project-short| libraries as
described above.


Obtaining |var-project-short| C++ API
-------------------------------------

You can obtain |var-project-short| via Git from the github-hosted repository

.. code-block:: bash

    git clone https://github.com/eclipse-cyclonedds/cyclonedds-cxx.git
    cd cyclonedds


Building |var-project-short| C++ API
------------------------------------

You can build and install the required libraries needed to develop
your applications using |var-project-short| C++ in a few simple steps, as shown below:

.. tabs::

    .. group-tab:: Linux

        .. code-block:: bash

            cd build
            cmake -DCMAKE_PREFIX_PATH=<core-install-location> -DCMAKE_INSTALL_PREFIX=<install-location> -DBUILD_EXAMPLES=ON ..
            cmake --build . --parallel

    .. group-tab:: macOS

        .. code-block:: bash

            cd build
            cmake -DCMAKE_PREFIX_PATH=<core-install-location> -DCMAKE_INSTALL_PREFIX=<install-location> -DBUILD_EXAMPLES=ON ..
            cmake --build . --parallel

    .. group-tab:: Windows

        .. code-block:: bash

            cd build
            cmake -G <generator-name> -DCMAKE_PREFIX_PATH=<core-install-location> -DCMAKE_INSTALL_PREFIX=<install-location> -DBUILD_EXAMPLES=ON ..
            cmake --build . --parallel

        On Windows you can build |var-project-short| C++ with one of several generators. Usually if you omit the
        ``-G <generator-name>`` it will pick a sensible default, but if it doesn't work or picks something
        unexpected you can go to the `CMake generators documentation <https://cmake.org/cmake/help/latest/manual/cmake-generators.7.html>`__.
        For example, "Visual Studio 15 2017 Win64" targets a 64-bit build using Visual Studio 2017.


If you do not plan on running the examples you may omit the ``-DBUILD_EXAMPLES=ON``.

To install it after a successful build:

.. code-block:: bash

    cmake --build . --target install

Depending on the installation location, you may need administrator privileges. The install step copies everything to:

 -  ``<install-location>/lib``
 -  ``<install-location>/bin``
 -  ``<install-location>/include``
 -  ``<install-location>/share/CycloneDDSCXX``

At this point, you are ready to use Cyclone DDS in your projects.

.. note:: Build types

    The default build type is a release build that includes debugging information (``RelWithDebInfo``).
    This build is suitable for applications because of its high-performance and debugging capabilities.
    If you prefer a Debug or pure Release build, add ``-DCMAKE_BUILD_TYPE=<build-type>`` to your CMake invocation.

Installing |var-project-short| Python
=====================================

Binaries or from source
-----------------------

The |var-project-short| Python API requires Python version 3.7 or higher, with 3.11 support provisional.
The wheels (binary archives) on PyPi contain a pre-built binary of the CycloneDDS C library and IDL compiler.
These have a couple of caveats. The pre-built package:

 * Does not provide support for DDS Security,
 * Does not provide support for shared memory via Iceoryx,
 * Comes with generic binaries that are not optimized per platform.

If you need these features, or cannot use the binaries for other reasons, you can install the |var-project-short| Python API from the source.
You will need to set the environment variable ``CYCLONEDDS_HOME`` to allow the installer to locate the |var-project-short| C library if it is not on the ``PATH``.
At runtime, we leverage several mechanisms to locate the appropriate library for the platform. If you get an exception about non-locatable libraries,
or wish to manage multiple |var-project-short| installations, you can use the environment variable ``CYCLONEDDS_HOME`` to override the load location.

Installing from PyPi
--------------------

Install with pip directly from PyPi.

.. code-block:: shell

    pip install cyclonedds

Installing from source
----------------------

You can install it from the GitHub link directly:

.. code-block:: shell

    CYCLONEDDS_HOME="<cyclonedds-install-location>" pip install git+https://github.com/eclipse-cyclonedds/cyclonedds-python

