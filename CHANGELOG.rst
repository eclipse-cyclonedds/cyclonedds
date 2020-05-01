
Changelog for Eclipse Cyclone DDS
=================================

Versions are numbered "Major.Minor.Patch", where:

Major versions signify guiding milestones. The current major version "0" reflects that Cyclone DDS is in its `Incubation Phase <https://www.eclipse.org/projects/dev_process/#6_2_3_Incubation>`_. Once it is "mature", the major version will be "1" and a new milestone will be chosen for "2".

Minor versions signify compatibility. A change in the minor version will accompany changes in public function signatures or data structures.

Patch versions signify public releases. New features or fixes may be introduced so long as compatibility is maintained.

`Unreleased <https://github.com/eclipse-cyclonedds/cyclonedds/compare/0.5.1...master>`_
---------------------------------------------------------------------------------------

`0.5.1 (2020-03-11) <https://github.com/eclipse-cyclonedds/cyclonedds/compare/V0.5.0...0.5.1>`_
-----------------------------------------------------------------------------------------------

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
