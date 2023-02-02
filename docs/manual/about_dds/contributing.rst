.. include:: ../external-links.part.rst

.. index:: Contributing

.. _contributing_to_dds:

Contributing to |var-project|
=============================

We welcome all contributions to the project, including questions,
examples, bug fixes, enhancements or improvements to the documentation,
etc.


.. tip::

    Contributing to |var-project| means donating your code to the Eclipse foundation. It requires that you
    sign the |url::eclipse_link| using |url::eclipse-form_link|. In summary, this means that your contribution is
    yours to give away, and that you allow others to use and distribute it. However, don't take legal advice
    from this getting started guide, read the terms linked above.

To contribute code, it may be helpful to know that build configurations for Azure DevOps Pipelines 
are present in the repositories.
There is a test suite using CTest and |url::cunit_link| that can be built locally.

.. tabs::

    .. group-tab:: Linux

        Set the CMake variable ``BUILD_TESTING`` to ``ON`` when configuring, e.g.:

        .. code-block:: bash

            cd build
            cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON ..
            cmake --build .
            ctest

        This build requires |url::cunit_link|. You can
        install this yourself, or you can choose to instead rely on the
        |url::conan_link| packaging system that the CI build
        infrastructure also uses. In that case, install Conan in the build
        directory before running CMake:

        .. code-block:: bash

            conan install .. --build missing

    .. group-tab:: macOS

        Set the CMake variable ``BUILD_TESTING`` to ``ON`` when configuring, e.g.:

        .. code-block:: bash

            cd build
            cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON ..
            cmake --build .
            ctest

        This build requires |url::cunit_link|. You can
        install this yourself, or you can choose to instead rely on the
        |url::conan_link| packaging system that the CI build
        infrastructure also uses. In that case, install Conan in the build
        directory before running CMake:

        .. code-block:: bash

            conan install .. --build missing

    .. group-tab:: Windows

        Set the CMake variable ``BUILD_TESTING`` to ``ON`` when configuring, e.g.:

        .. code-block:: bash

            cd build
            cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON ..
            cmake --build .
            ctest

        This build requires |url::cunit_link|. You can
        install this yourself, or you can choose to instead rely on the
        |url::conan_link| packaging system that the CI build
        infrastructure also uses. In that case, install Conan in the build
        directory before running CMake:

        .. code-block:: bash

            conan install .. --build missing

        This automatically downloads and builds CUnit (and currently OpenSSL for transport security).

        .. note::

            Depending on the generator, you may also need to add switches to select the architecture and build type, e.g.:

            .. code-block:: bash

                conan install -s arch=x86_64 -s build_type=Debug ..

