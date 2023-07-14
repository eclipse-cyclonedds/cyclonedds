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
There is a test suite using CTest that can be built locally.

.. tabs::

    .. group-tab:: Linux

        Set the CMake variable ``BUILD_TESTING`` to ``ON`` when configuring, e.g.:

        .. code-block:: bash

            cd build
            cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON ..
            cmake --build .
            ctest

    .. group-tab:: macOS

        Set the CMake variable ``BUILD_TESTING`` to ``ON`` when configuring, e.g.:

        .. code-block:: bash

            cd build
            cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON ..
            cmake --build .
            ctest

    .. group-tab:: Windows

        Set the CMake variable ``BUILD_TESTING`` to ``ON`` when configuring, e.g.:

        .. code-block:: bash

            cd build
            cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON ..
            cmake --build .
            ctest
