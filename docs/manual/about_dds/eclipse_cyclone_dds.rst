.. include:: ../external-links.part.rst

.. image:: ../_static/gettingstarted-figures/Cyclone_DDS_logo.svg
   :width: 1px
   :height: 1px

|url::logoimage|

#############
|var-project|
#############

.. toctree::
   :maxdepth: 1
   :hidden:

   data_centric_architecture
   domainparticipants
   topics
   publishers
   subscribers
   datareaders
   datawriters
   qos
   listener
   waitset
   status
   performance
   idl
   ddsi_concepts
   contributing

|var-project| is a performant and robust OMG-compliant **Data Distribution Service** 
(DDS) implementation (see |url::dds_spec|). |var-project-short| is developed 
completely in the open as an Eclipse IoT project (see |url::cyclone_dds-link|) with a 
growing list of |url::cyclone_adopters|. It is a tier-1 middleware for the Robot 
Operating System |url::ros2|.

The core of |var-project-short| is implemented in C and provides C-APIs to applications. 
Through its C++ package, the |url::omg.org| 2003 language binding is also supported.

.. index:: About DDS

About DDS
=========

DDS is the best-kept secret in distributed systems, one that has been around for much 
longer than most publish-subscribe messaging systems and still outclasses so many of them.
DDS is used in a wide variety of systems, including air-traffic control, jet engine 
testing, railway control, medical systems, naval command-and-control, smart greenhouses 
and much more. In short, it is well-established in aerospace and defense but no longer 
limited to that. And yet it is easy to use!

Types are usually defined in IDL and preprocessed with the IDL compiler included in Cyclone, 
but our |url::python-link2| allows you to define data types on the fly:

.. code-block:: python

   from dataclasses import dataclass
   from cyclonedds.domain import DomainParticipant
   from cyclonedds.core import Qos, Policy
   from cyclonedds.pub import DataWriter
   from cyclonedds.sub import DataReader
   from cyclonedds.topic import Topic
   from cyclonedds.idl import IdlStruct
   from cyclonedds.idl.annotations import key
   from time import sleep
   import numpy as np
   try:
      from names import get_full_name
      name = get_full_name()
   except:
      import os
      name = f"{os.getpid()}"

   # C, C++ require using IDL, Python doesn't
   @dataclass
   class Chatter(IdlStruct, typename="Chatter"):
      name: str
      key("name")
      message: str
      count: int

   rng = np.random.default_rng()
   dp = DomainParticipant()
   tp = Topic(dp, "Hello", Chatter, qos=Qos(Policy.Reliability.Reliable(0)))
   dw = DataWriter(dp, tp)
   dr = DataReader(dp, tp)
   count = 0
   while True:
      sample = Chatter(name=name, message="Hello, World!", count=count)
      count = count + 1
      print("Writing ", sample)
      dw.write(sample)
      for sample in dr.take(10):
         print("Read ", sample)
      sleep(rng.exponential())

Today DDS is also popular in robotics and autonomous vehicles because those really 
depend on high-throughput, low-latency control systems without introducing a single 
point of failure by having a message broker in the middle. Indeed, it is by far the 
most used and the default middleware choice in ROS 2. It is used to transfer commands, 
sensor data and even video and point clouds between components.

The OMG DDS specifications cover everything one needs to build systems using 
publish-subscribe messaging. They define a structural type system that allows 
automatic endianness conversion and type checking between readers and writers. This 
type system also supports type evolution. The interoperable networking protocol and 
standard C++ API make it easy to build systems that integrate multiple DDS 
implementations. Zero-configuration discovery is also included in the standard and 
supported by all implementations.

DDS actually brings more: publish-subscribe messaging is a nice abstraction over 
"ordinary" networking, but plain publish-subscribe doesn't affect how one *thinks* 
about systems. A very powerful architecture that truly changes the perspective on 
distributed systems is that of the "shared data space", in itself an old idea, and 
really just a distributed database. Most shared data space designs have failed 
miserably in real-time control systems because they provided strong consistency 
guarantees and sacrificed too much performance and flexibility. The *eventually 
consistent* shared data space of DDS has been very successful in helping with building 
systems that need to satisfy many "ilities": dependability, maintainability, 
extensibility, upgradeability, ... Truth be told, that's why it was invented, and 
publish-subscribe messaging was simply an implementation technique.

|var-project| aims at full coverage of the specs and today already covers most of this.
With references to the individual OMG specifications, the following is available:

- |url::dds_spec| the base specification
  - zero configuration discovery (if multicast works)
  - publish/subscribe messaging
  - configurable storage of data in subscribers
  - many QoS settings - liveliness monitoring, deadlines, historical data, ...
  - coverage includes the Minimum, Ownership and (partially) Content profiles
- |url::omg.security| - providing authentication, access control and encryption
- |url::omg.org|
- |url::dds_xtypes| - the structural type system (some [caveats](docs/dev/xtypes_relnotes.md) here)
- |url::dds2.5| - the interoperable network protocol

The network stack in |var-project| has been around for over a decade in one form or 
another and has proven itself in many systems, including large, high-availability 
ones and systems where inter-operation with other implementations was needed.

.. include:: disclaimer.part.rst