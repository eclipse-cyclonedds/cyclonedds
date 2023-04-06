.. include:: ../external-links.part.rst

.. index:: Python

.. _installing_python:

**Binaries or from source**

The |var-project-short| Python API requires Python version 3.7 or higher (with 3.11 support provisional).

At runtime, there are several mechanisms to locate the appropriate library for the platform. 
If you get an exception about non-locatable libraries, or need to manage multiple |var-project-short| installations, override the load location by setting the ``CYCLONEDDS_HOME`` environment variable.

**Installing from PyPi**

The wheels (binary archives) on PyPi contain a pre-built binary of the CycloneDDS C library and IDL compiler. However, the pre-built package:

 * does not provide support for DDS Security,
 * does not provide support for shared memory via |url::iceoryx_link|,
 * comes with generic binaries that are not optimized per platform.

If you need these features, or cannot use the binaries for other reasons, install the |var-project-short| Python API from source (see below).
If the |var-project-short| C library is not on the ``PATH``, set the environment variable ``CYCLONEDDS_HOME``.

Install |var-project-short| using pip directly from PyPi.

.. code-block:: shell

    pip install cyclonedds

**Installing from source**

Install |var-project-short| directly from the GitHub link:

.. code-block:: shell

    CYCLONEDDS_HOME="<cyclonedds-install-location>" pip install git+https://github.com/eclipse-cyclonedds/cyclonedds-python