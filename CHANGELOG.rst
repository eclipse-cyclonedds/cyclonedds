
Changelog for Eclipse Cyclone DDS
=================================

`Unreleased <https://github.com/eclipse-cyclonedds/cyclonedds/compare/0.6.0...master>`_
---------------------------------------------------------------------------------------

`V0.6.0 (2020-05-21) <https://github.com/eclipse-cyclonedds/cyclonedds/compare/V0.5.0...0.6.0>`_
-----------------------------------------------------------------------------------------------

* Support for mixed-language programming by supporting multiple (de)serializers for a single topic in a single process. This way, a program that consists of, say, C and C++ can use a different representation of the data in C than in C++. Before, all readers/writers in the process would be forced to use the same representation (or perform an additional copy). Currently C is still the only natively supported language, but one can use an evolving-but-reasonable-stable interface to implement different mappings.
* Improved QoS support: full support for deadline, lifespan and liveliness. The first is for generating notifications when a configured instance update rate is not achieved, the second for automatically removing stale samples, the third for different modes of monitoring the liveliness of peers.
* Improved scalability in matching readers and writers. Before it used to try matching a new local or remote reader or writer with all known local & remote entities, now only with the right group with the correct topic name.
* Improved tracing: discovery data is now printed properly and user samples have more type information allowing floating-point and signed numbers to be traced in a more readable form.
* Extension of platform support
  * Known to work on FreeBSD, CheriBSD
  * Known to work with the musl C library
* Windows-specific changes
  * Fixes multicasts to addresses also used by non-Cyclone processes (caused by accidentally linking with an old sockets library)
  * Correct handling of non-English network interface names

`0.5.1 (2020-03-11) <https://github.com/eclipse-cyclonedds/cyclonedds/compare/V0.5.0...0.5.1>`_
-----------------------------------------------------------------------------------------------

An interim tag for the benefit of ROS2

* Enable QOS features: liveliness, lifespan, deadline
* Fix issues on Windows where multicast data was not received

`V0.5.0 (2019-11-21) <https://github.com/eclipse-cyclonedds/cyclonedds/compare/V0.1.0...V0.5.0>`_
-------------------------------------------------------------------------------------------------

This is the fist release candidate for the long overdue second release of Eclipse Cyclone DDS.
We are updating the version number from 0.1 to 0.5 to make up for the lack of more timely releases and to reflect that progress towards a 1.0 release (a minimum profile DDS implementation + DDS security) has been more significant than a version of "0.2" would suggest.

Updates since the first release have been legion, pretty much without impact on application code or interoperatbility on the network.
Some of the highlights:

* Support for ROS2 Dashing and Eloquent (via an adaption layer).
* Support for an arbitrary number of concurrent DDS domains (fully independent instantiations of DDS) in a single process.
* Abstracting the notion of samples, types and reader history caches, allowing overriding the default implementations of these to get behaviours more suited to the applications.
  This is particularly relevant to language bindings and embedding Cyclone DDS in other frameworks, such as ROS2.
* Platform support is extended beyond the usual Linux/Windows/macOS: FreeRTOS is now known to work, as is Solaris 2.6 on sun4m machines.
* Acceptance of some malformed messages from certain implementations improved interoperability on the wire.

.......................................
Limitations on backwards compatibility:
.......................................

* A change in how implicitly created "publisher" and "subscriber" entities are handled: they now never lose their "implicitness", and in consequence, are now deleted when their last child disappears rather than deleted when their one child disappears.
* The set of entities that can be attached to a waitset is now restricted to those owned by the parent of the waitset, before one was allowed to attach entities from different participants to the same waitset, which is tantamount to a bug.
* A participant entity now has a parent. The "get_parent" operation no longer returns 0 for a participant because of the addition of two additional levels to the entity hierarchy: a domain, possibly containing multiple participants; and one that represents the entire library.
* The data from a reader for a built-in topic has been extended, breaking binary compatibility.


`V0.1.0 (2019-03-06) <https://github.com/eclipse-cyclonedds/cyclonedds/compare/7b5cc4fa59ba57a3b796a48bc80bb1e8527fc7f3...V0.1.0>`_
-------------------------------------------------------------------------------------------------------------------------------------

Eclipse Cyclone DDS’ first release!
