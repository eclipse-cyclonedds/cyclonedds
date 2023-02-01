.. include:: ../external-links.part.rst

.. index:: 
  single: Shared memory
  single: iceoryx; Shared memory

.. _shared_memory:

Shared memory exchange
======================

.. toctree::
    :maxdepth: 1
    :hidden:

    shared_mem_config
    shared_mem_examples
    limitations
    loan_mechanism
    developer_hints

This section describes how to support shared memory exchange in |var-project|, 
which is based on |url::iceoryx_link|.

Prerequisites
-------------

|url::iceoryx_link| depends on several packages (*cmake*, *libacl1*, *libncurses5*, *pkgconfig* and *maven*).

.. note:: 
  The following steps were done on Ubuntu 20.04.

#. Install the prerequisite packages:

   .. code-block:: console

     sudo apt install cmake libacl1-dev libncurses5-dev pkg-config maven

#. Get and build iceoryx. The following assumes that the install is in your home directory:

   .. code-block:: console

     git clone https://github.com/eclipse-iceoryx/iceoryx.git -b release_2.0
     cd iceoryx
     cmake -Bbuild -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=install -DBUILD_SHARED_LIBS=ON -Hiceoryx_meta
     cmake --build build --config Release --target install

#. Get |var-project-short| and build it with shared memory support:

   .. code-block:: console

     git clone https://github.com/eclipse-cyclonedds/cyclonedds.git
     cd cyclonedds
     cmake -Bbuild -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=install -DBUILD_EXAMPLES=On -DCMAKE_PREFIX_PATH=~/iceoryx/install/
     cmake --build build --config Release --target install

When the compiler has finished, the files for both iceoryx and |var-project-short| can 
be found in the specified install directories.
