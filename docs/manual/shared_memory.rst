.. _`Shared Memory`:

#############
Shared Memory
#############

The documentation is to describe the Proof of Concepts detail of supporting shared memory in Cyclone DDS.
The shared memory implementation is based on `Eclipse iceoryx <https://projects.eclipse.org/proposals/eclipse-iceoryx>`_.

*****
Build
*****

The following step is under Ubuntu 18.04, but other Linux distribution should also work, too.

Before using iceoryx, there are some dependencies which should be installed.
These packages are used by iceoryx.

.. code-block:: bash

  sudo apt install cmake libacl1-dev libncurses5-dev pkg-config maven

We use colcon to build iceoryx and Cyclone DDS.

.. code-block:: bash

  mkdir -p ~/cyclone_iceoryx_ws/src
  cd ~/cyclone_iceoryx_ws/src
  git clone https://github.com/eclipse-cyclonedds/cyclonedds.git -b iceoryx
  git clone https://github.com/eclipse-iceoryx/iceoryx.git -b master
  cd ~/cyclone_iceoryx_ws
  colcon build --cmake-args -DCMAKE_BUILD_TYPE=Release

***
Run
***

Setup the environment before trying to run Cyclone DDS.

.. code-block:: bash

  cd ~/cyclone_iceoryx_ws
  source install/local_setup.bash

We need to enable shared memory configuration to run with iceoryx.
Save the following xml as cyclonedds.xml.

.. code-block:: xml

  <?xml version="1.0" encoding="UTF-8" ?>
  <CycloneDDS xmlns="https://cdds.io/config" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="https://cdds.io/config https://raw.githubusercontent.com/eclipse-cyclonedds/cyclonedds/iceoryx/etc/cyclonedds.xsd">
      <Domain id="any">
          <SharedMemory>
              <Enable>true</Enable>
              <SubQueueCapacity>256</SubQueueCapacity>
              <SubHistoryRequest>16</SubHistoryRequest>
              <PubHistoryCapacity>16</PubHistoryCapacity>
          </SharedMemory>
      </Domain>
  </CycloneDDS>

SubQueueCapacity, SubHistoryRequest and PubHistoryCapacity can be optionally set if Shared Memory is enabled.
SubQueueCapacity controls how many samples a reader using shared memory can hold before the least recent is discarded.
PubHistoryCapacity defines how many samples a shared memory writer will keep to send to late-joining subscribers.
SubHistoryRequest is the number of samples a late-joining reader will request from a writer (this can be at most 
as many as were send and at most PubHistoryCapacity).   

Now we start to run Cyclone DDS with shared memory.
In the 1st terminal we will start RouDi.

.. code-block:: bash

  source install/local_setup.bash
  iox-roudi

The 2nd terminal will run "ddsperf pub".

.. code-block:: bash

  source install/local_setup.bash
  export CYCLONEDDS_URI=file://$PWD/cyclonedds.xml
  ddsperf pub size 16k

The 3rd terminal will run "ddsperf sub".

.. code-block:: bash

  source install/local_setup.bash
  export CYCLONEDDS_URI=file://$PWD/cyclonedds.xml
  ddsperf sub

You can compare the result between native Cyclone DDS and Cyclone DDS with shared memory.

***********
Performance
***********

A performance improvement can be observed for sufficiently large sample sizes and data transfer on the same machine (otherwise the regular network transmission is used and their will be no performance gain).

Once the loan interface is fully implemented transmission speed will be independent of the sample size when shared memory is used, i.e. take constant time.

*************
To developers
*************

The initial implementation is from `ADLINK Advanced Robotics Platform Group <https://github.com/adlink-ROS/>`_.
Contributions were made by `Apex.AI <https://www.apex.ai/>`_ in order to integrate the latest iceoryx C-API to support zero copy  data transfer (still requires the cyclonedds loan API to be implemented).
Further contributions and feedback from the community are very welcome.

Here is some tips for you to get started.

- Most of the shared memory modification is under the define "DDS_HAS_SHM".
  You can search the define to have a quick scan.
- If you are curious about the detail of what is inside the iceoryx,
  There are a useful tool from iceoryx called iceoryx_introspection_client.

  .. code-block:: bash

    source install/local_setup.bash
    iox-introspection-client --all

- There are some configurations about showing log from shared memory.
  The Category "shm" under Tracing shows the Cyclone DDS log related to shared memory,
  while the LogLevel under SharedMemory decides which log level iceoryx shows.
  Please refer to the following XML.

  .. code-block:: xml
  
    <?xml version="1.0" encoding="UTF-8" ?>
    <CycloneDDS xmlns="https://cdds.io/config" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="https://cdds.io/config https://raw.githubusercontent.com/eclipse-cyclonedds/cyclonedds/iceoryx/etc/cyclonedds.xsd">
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

***********
Limitations
***********

Since the shared memory is still under POC stage, there are some limitations currently.

- Platform Support:
  Now the implementation can only run under the Linux environment.
  Since iceoryx also support MacOS and will have `Windows 10 support <https://github.com/eclipse/iceoryx/issues/33>`_ in the future,
  Support of MacOS and Windows are still work in progress.
- QoS Support:
  The current design doesn't consider the DDS QoS support.
  The suitable kind of data sent by shared memory only needs reliable and keep last, which are already supported by iceoryx.
  However, it would be nice if Cyclone DDS with shared memory also support QoS.
- True Zero copy:
  The current implementation is not zero copy, and still needs to copy data from user buffer into shared memory.
  To achieve zero copy, users must change the API they use and put the data into shared memory from the beginning.
  Although it needs some changes on user side, it'll improve the performance.

*********
TODO List
*********

- Support DDS QoS:
  Please refer to the `Limitations`_.
- Support true zero copy:
  Please refer to the `Limitations`_.
- Extend configuration options for Shared Memory
- Add data and measurements of performance improvements

