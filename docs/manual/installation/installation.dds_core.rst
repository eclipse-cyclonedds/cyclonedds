.. _installing_dds_core:

Obtain |var-project-short| via Git from the repository hosted on GitHub:

.. code-block:: console

    git clone https://github.com/eclipse-cyclonedds/cyclonedds.git
    cd cyclonedds


Building |var-project-short|

To build and install the required libraries for your applications, use the following:

.. tabs::

    .. group-tab:: Linux

        .. code-block:: console

            cd build
            cmake -DCMAKE_INSTALL_PREFIX=<install-location> -DBUILD_EXAMPLES=ON ..
            cmake --build . --parallel

    .. group-tab:: macOS

        .. code-block:: console

            cd build
            cmake -DCMAKE_INSTALL_PREFIX=<install-location> -DBUILD_EXAMPLES=ON ..
            cmake --build . --parallel

    .. group-tab:: Windows

        .. code-block:: console

            cd build
            cmake -G <generator-name> -DCMAKE_INSTALL_PREFIX=<install-location> -DBUILD_EXAMPLES=ON ..
            cmake --build . --parallel

        You can build |var-project-short| with one of several generators. Usually, if you omit the
        ``-G <generator-name>``, it selects a sensible default. If it does not work, or selects something
        unexpected, refer to the `CMake generators documentation <https://cmake.org/cmake/help/latest/manual/cmake-generators.7.html>`__.
        For example, "Visual Studio 15 2017 Win64" targets a 64-bit build using Visual Studio 2017.

If you need to reduce the footprint, or have issues with the `FindOpenSSL.cmake` script, you can explicitly disable it by setting ``-DENABLE\_SSL=NO`` to the CMake invocation. For further information, refer to `FindOpenSSL <https://cmake.org/cmake/help/latest/module/FindOpenSSL.html>`_.

.. include:: core_cpp_common_build.rst