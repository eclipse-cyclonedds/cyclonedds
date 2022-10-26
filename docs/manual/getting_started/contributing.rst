.. _contributing:

Contributing to |var-project|
=============================

We welcome all contributions to the project, including questions,
examples, bug fixes, enhancements or improvements to the documentation,
etc.


.. tip::

    Contributing to |var-project| means donating your code to the Eclipse foundation. This means you need to
    sign the `Eclipse Contributor Agreement <https://www.eclipse.org/legal/ECA.php>`__ using
    `this form <https://accounts.eclipse.org/user/eca>`__. It effectively means that what you are contributing is
    yours to give away and you are fine with everyone using and distributing it. However, don't take legal advice
    from this getting started guide, read the terms linked above.

If you want to contribute code, it is helpful to know that build
configurations for Azure DevOps Pipelines are present in the repositories.
There is a test suite using CTest and CUnit that can be built
locally.

The following sections explain how to do this for the different
operating systems.

Linux and macOS
---------------

Set the CMake variable ``BUILD_TESTING`` to ``ON`` when configuring, e.g.:

.. code-block:: bash

    cd build
    cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON ..
    cmake --build .
    ctest

This build requires `CUnit <http://cunit.sourceforge.net/>`__. You can
install this yourself, or you can choose to instead rely on the
`Conan <https://conan.io/>`__ packaging system that the CI build
infrastructure also uses. In that case, install Conan in the build
directory before running CMake:

.. code-block:: bash

    conan install .. --build missing


Windows
-------

Set the CMake variable ``BUILD_TESTING`` to ``ON`` when configuring, e.g.:

.. code-block:: bash

    cd build
    cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON ..
    cmake --build .
    ctest

This build requires `CUnit <http://cunit.sourceforge.net/>`__. You can
install this yourself, or you can choose to instead rely on the
`Conan <https://conan.io/>`__ packaging system that the CI build
infrastructure also uses. In that case, install Conan in the build
directory before running CMake:

.. code-block:: bash

    conan install .. --build missing

This automatically downloads and builds CUnit (and currently OpenSSL for transport security).

.. note::

    Depending on the generator, you may also need to add switches to select the architecture and build type, e.g.:

    .. code-block:: bash

        conan install -s arch=x86_64 -s build_type=Debug ..
