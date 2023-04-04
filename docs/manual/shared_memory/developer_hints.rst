.. include:: ../external-links.part.rst

.. index:: 
  single: Shared memory; Developer hints
  single: Developer hints
  single: Contributing

.. _dev_hints:

Developer hints
---------------

The initial implementation is from |url::adlink-ROS|. To integrate the latest 
|url::iceoryx_link| C-API and support zero copy data transfer, contributions 
were made by |url::Apex.AI|. Further contributions and feedback from the community 
are very welcome, see :ref:`contributing_to_dds`.

The following is a list of useful tips:

* Most of the shared memory modification is under the define :c:macro:`DDS_HAS_SHM`. 
  `DDS_HAS_SHM` is a flag set through ``cmake`` when compiling, which enables 
  shared memory support.

* To learn about the internal happenings of the |url::iceoryx_link| service, there 
  is a useful tool from iceoryx called |url::iceoryx_introspection|:

  .. code-block:: console

    ~/iceoryx/build/iox-introspection-client --all

* |var-project-short| can be configured to show logging information from shared memory.

  * Setting Tracing::Category to *shm* shows the |var-project-short| log related to shared memory, while SharedMemory::LogLevel decides which log level iceoryx shows:

  .. code-block:: xml
  
    <?xml version="1.0" encoding="UTF-8" ?>
    <CycloneDDS xmlns="https://cdds.io/config"
                xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
                xsi:schemaLocation="https://cdds.io/config https://raw.githubusercontent.com/eclipse-cyclonedds/cyclonedds/iceoryx/etc/cyclonedds.xsd">
        <Domain id="any">
            <Tracing>
                <Category>shm</Category>
                <OutputFile>stdout</OutputFile>
            </Tracing>
            <SharedMemory>
                <Enable>true</Enable>
                <LogLevel>info</LogLevel>
            </SharedMemory>
        </Domain>
    </CycloneDDS>
