# Cyclone DDS Roadmap

## Introduction

The Eclipse Cyclone DDS project aims at implementing the OMG DDS core standards, namely:

 * [Data Distribution Service][1] (DDS)
 * [DDS Interoperability Wire Protocol][2] (DDSI-RTPS)
 * [DDS Security][3] (DDS-SECURITY)
 * [Extensible and Dynamic Topic Types for DDS][1] (DDS-XTypes)

We ultimately aim to comply with the latest version of above OMG standards. Cyclone DDS supports currently the following language bindings for the DCPS APIs:

 * C language
 * C++ language ([ISO/IEC C++ 2003 Language PSM for DDS][5]) (DDS-PSM-Cxx)

The community is welcome to add new language bindings and contribute to the implementation of other OMG specifications.

The roadmap presented here is indicative and subject to change.

## Short-term milestones

**Cyclone 0.8**

 * C++11 APIs (GA)
 * New APIs to access to serialized CDR data
 * New APIs for configuring the deployment of Cyclone DDS based application
 * Multi-Network interface cards support (Consolidation)
 * Content filtering support for C++
 * IDL compiler front-end & back end support of the Xtypes annotations
 * Topic and data type discovery
 * ROS 2 Quality Level 2
   * Automated performance and regression testing
   * Formal feature list also showing test coverage
   * APIs marked stable (each API in formal feature list), evolving or experimental

**Cyclone 0.9**

 * Integration of Durability Service for Transient and Persistent data
 * Shared memory transport support
 * Python APIs (GA)
 * Internet-scale deployment support
 * DDS-Xtypes APIs
 * Asynchronous mode of operation
 * ROS 2 Quality Level 1

## Midterm milestones

**Cyclone 1.0**

 * Static discovery
 * Static memory allocation
 * Writer side filtering support
 * Content Querying APIs

## Long-term milestones

**Future versions**

 * Network Mobility support
 * C# language binding support (DDS C# API)
 * Java language binding support (Java 5 Language PSM for DDS)
 * Time-sensitive Networking support DDS-TSN
 * Rust language binding support
 * Certifiable DDS
 * Face 3 support
 * Network Scheduling and Federated architecture support
 * [TrustZone](https://developer.arm.com/ip-products/security-ip/trustzone) support for DDS Security

[1]: https://www.omg.org/spec/DDS/About-DDS/
[2]: https://www.omg.org/spec/DDSI-RTPS/About-DDSI-RTPS/
[3]: https://www.omg.org/spec/DDS-SECURITY/About-DDS-SECURITY/
[4]: https://www.omg.org/spec/DDS-XTypes/About-DDS-XTypes/
[5]: https://www.omg.org/spec/DDS-PSM-Cxx/About-DDS-PSM-Cxx/
