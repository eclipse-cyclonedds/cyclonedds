.. The following text is included in installation.cpp.rst and core_cpp_common.rst (at the end of the files).

If you do not require the examples, use ``-DBUILD_EXAMPLES=OFF`` to omit them.

To install |var-project-short| after a successful build:

.. code-block:: console

    cmake --build . --target install

The install step copies everything to:

 -  ``<install-location>/lib``
 -  ``<install-location>/bin``
 -  ``<install-location>/include/ddsc``
 -  ``<install-location>/share/CycloneDDS``

.. note::
    Depending on the installation location, you may need administrator privileges. 

At this point, you are ready to use var-project-short| in your projects.

.. note:: Build types

    The default build type is a release build that includes debugging information (``RelWithDebInfo``).
    This build is suitable for applications because it allows the resulting application to be more easily debugged while still maintaining high performance.
    If you prefer a `Debug` or pure `Release` build, add ``-DCMAKE_BUILD_TYPE=<build-type>`` to your CMake invocation.