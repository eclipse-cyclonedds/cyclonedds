# Cyclone DDS Roadmap

## Introduction

The Eclipse Cyclone DDS project aims at implementing the OMG DDS core standards, namely:

- DDS OMG specification,
- DDSI OMG,
- DDS Security,
- DDS-Xtypes.

We ultimately aim to comply with the latest version of above OMG standards. Cyclone DDS supports currently the following languages binding for the DCPS APIs:

- C language,
- C++11 language (ISO/IEC C++ 2003 Language PSM for DDS).

The community is welcome to add new language bindings and contribute to the implementation of other OMG specifications.

The roadmap presented in this page is indicative and subject to change.

## Short-term milestones

**Cyclone 0.8, Dec 2020**

- C++11 APIs (GA),
- New APIs to access to serialized CDR data,
- New APIs for configuring the deployment of Cyclone DDS based application,
- Multi-Network interface cards support (Consolidation),
- Content filtering support for C++,
- IDL compiler front-end & back end support of the Xtypes annotations,
- Topic and data type discovery,

**Cyclone 0.9, March 2021**

- Integration of Durability Service for Transient and Persistent data,
- Shared memory transport support,
- Python APIs (GA),
- Internet-scale deployment support,
- Xtypes APIs.

## Midterm milestones

**Cyclone 1.0 June 2021**

- Static discovery,
- Static memory allocation,
- Writer side filtering support,
- Content Querying APIs.

## Long-term milestones

**Future versions**

- Network Mobility support
- C# language binding support ( DDS C# API)
- Java language binding support (Java 5 Language PSM for DDS)
- Time-sensitive Networking support DDS-TSN
- Rust language binding support
- Certifiable DDS
- Face 3 support
- Network Scheduling and Federated architecture support