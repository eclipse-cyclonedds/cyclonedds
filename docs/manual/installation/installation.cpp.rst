.. include:: ../external-links.part.rst

.. _installing_cpp:

The |var-project| C++ API is an implementation of the DDS |url::omg.org|, that is, a C++ binding for |var-project-short|. 

The |var-project| C++ API consists of the following:

- An IDL compiler backend that uses an IDL data model to generate their C++ representation and artifacts.
- A software layer that maps some DDS APIs onto the |var-project| C API, and to lower the overhead when managing data, direct access to the core APIs.

**Obtaining C++ API**

Obtain |var-project-short| via Git from the repository hosted on GitHub:

.. code-block:: console

    git clone https://github.com/eclipse-cyclonedds/cyclonedds-cxx.git
    cd cyclonedds


**Building C++ API**

To build and install the required libraries for your applications, use the following:

.. tabs::

    .. group-tab:: Linux

        .. code-block:: console

            cd build
            cmake -DCMAKE_PREFIX_PATH=<core-install-location> -DCMAKE_INSTALL_PREFIX=<install-location> -DBUILD_EXAMPLES=ON ..
            cmake --build . --parallel

    .. group-tab:: macOS

        .. code-block:: console

            cd build
            cmake -DCMAKE_PREFIX_PATH=<core-install-location> -DCMAKE_INSTALL_PREFIX=<install-location> -DBUILD_EXAMPLES=ON ..
            cmake --build . --parallel

    .. group-tab:: Windows

        .. code-block:: console

            cd build
            cmake -G <generator-name> -DCMAKE_PREFIX_PATH=<core-install-location> -DCMAKE_INSTALL_PREFIX=<install-location> -DBUILD_EXAMPLES=ON ..
            cmake --build . --parallel

        On Windows you can build |var-project-short| C++ with one of several generators. Usually, if you omit the
        ``-G <generator-name>`` it picks a sensible default. However, if the project does not work, or does something
        unexpected, refer to the `CMake generators documentation <https://cmake.org/cmake/help/latest/manual/cmake-generators.7.html>`__.
        For example, "Visual Studio 15 2017 Win64" targets a 64-bit build using Visual Studio 2017.

.. include:: core_cpp_common_build.rst