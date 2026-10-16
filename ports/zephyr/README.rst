.. _cyclonedds_zephyr:

Cyclone DDS on Zephyr RTOS
##########################

Overview
********

Eclipse Cyclone DDS is a `Zephyr module`_: add this repository to the west
manifest of your application (or pass it in ``ZEPHYR_EXTRA_MODULES``) and set
``CONFIG_CYCLONEDDS=y`` to build the ``ddsc`` library into the Zephyr image.

.. code-block:: yaml

   # west.yml of your application
   manifest:
     projects:
       - name: cyclonedds
         url: https://github.com/eclipse-cyclonedds/cyclonedds
         revision: master

This directory is a Zephyr sample application. Without any of its ``BUILD_*``
options it builds a self-test that creates a domain and a participant and reads
the participant back from the ``DCPSParticipant`` built-in topic. The self-test
needs neither a peer nor IDL-generated code, which makes it the integration test
run by CI on ``native_sim`` (see ``sample.yaml`` and
``.github/workflows/zephyr.yml``). With one of the ``BUILD_*`` options the
Cyclone DDS examples or ``ddsperf`` are built instead.

Requirements
************

* Zephyr 4.2 or newer. CI builds against 4.4.2 (see ``west.yml`` in the
  repository root); the port is also tested with 4.5.0.
* ``xxd``, used to convert ``config.xml`` for the examples.
* For the examples: the Cyclone DDS IDL compiler ``idlc`` in ``PATH``. It is a
  host tool, so build Cyclone DDS natively first and run
  ``copy_examples.sh`` to copy the example sources and generate their types.
* A board with networking, or ``native_sim`` (loopback only) or ``qemu_x86``
  with a TAP device (see `networking with QEMU`_).

Configuration
*************

``CONFIG_CYCLONEDDS`` selects the POSIX API, a full C library, ``socketpair``
and thread-local storage. ``prj.conf`` shows the thread, mutex, poll and
network buffer limits a DDS application typically needs. Points to keep in
mind for your own application:

* Threads run on statically allocated stacks sized by
  ``CONFIG_CYCLONEDDS_THREAD_COUNT`` and ``CONFIG_CYCLONEDDS_THREAD_STACK_SIZE``.
  One domain with a single receive thread needs at least 6 threads.
* With thread-local storage every thread stack, including Zephyr's own idle and
  work queue threads, carries a copy of the TLS block of over 2 KiB; size the
  stacks accordingly. The module raises the idle stack default to 4 KiB.
* The malloc arena (``CONFIG_COMMON_LIBC_MALLOC_ARENA_SIZE``) defaults to only
  16 KiB on MMU systems and on ``native_sim``; a single domain with one
  participant needs a few hundred KiB.
* Configure the library with ``dds_create_domain_with_rawconfig()`` (see
  ``src/main.c``). XML configuration through ``CYCLONEDDS_URI`` requires an
  ``environ``; the examples construct one from ``config.xml`` at build time.
* A static IPv4 address is set in the board and overlay configuration files.
  To run two Zephyr nodes build each with its own address (``-DCONF_FILE=...``)
  or enable DHCP.

Building and Running
********************

Build and run the self-test on ``native_sim``:

.. code-block:: console

   west build -b native_sim/native/64 ports/zephyr -t run

Build the examples or ``ddsperf`` for a board, for example:

.. code-block:: console

   west build -b s32z2xxdc2/s32z270/rtu0 ports/zephyr -- -DBUILD_ROUNDTRIP_PING=1
   west build -b qemu_x86 ports/zephyr -- -DEXTRA_CONF_FILE=overlay-e1000.conf -DBUILD_HELLOWORLD_SUB=1

Run the self-test with twister, as CI does:

.. code-block:: console

   west twister -T ports/zephyr -p native_sim/native/64 -v --inline-logs

Sample Output
*************

.. code-block:: console

   *** Booting Zephyr OS build v4.5.0 ***
   cyclonedds: self-test on native_sim/native/64
   cyclonedds: self-test passed

Module layout
*************

* ``zephyr/module.yml``, ``zephyr/Kconfig`` and
  ``zephyr/CMakeLists.txt`` in the repository root integrate Cyclone DDS
  into the Zephyr build system.
* ``src/ddsrt`` contains the Zephyr-specific parts of the runtime.
* Cyclone DDS is licensed under the Eclipse Public License 2.0 or the Eclipse
  Distribution License 1.0 (``EPL-2.0 OR BSD-3-Clause``), see ``LICENSE``.

.. _Zephyr module: https://docs.zephyrproject.org/latest/develop/modules.html
.. _networking with QEMU: https://docs.zephyrproject.org/latest/connectivity/networking/qemu_setup.html
