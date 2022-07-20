This document is a declaration of software quality for the [Eclipse Cyclone DDS](https://github.com/eclipse-cyclonedds/cyclonedds) (hereafter simply "Cyclone DDS") project, based on the guidelines in [REP-2004](https://www.ros.org/reps/rep-2004.html). This quality declaration is therefore specific to use in [ROS 2](https://docs.ros.org/en/rolling/)-based systems.

# Cyclone DDS Quality Declaration

Cyclone DDS meets all the requirements of **Quality Level 2** category.
The requirements for the various quality level categories are defined by [REP-2004](https://www.ros.org/reps/rep-2004.html).
The rationale, notes and caveats for this claim are provided by the remainder of this document.

## Version Policy [1]

### Version Scheme [1.i]

Cyclone DDS version numbers are organised as MAJOR.MINOR.PATCH where all three components are non-negative decimal numbers.
Version number policy follows the following rules:

* MAJOR version is incremented when an incompatible API change is made;
* MINOR version when functionality is added in a backwards compatible manner. MINOR is source compatible, the project strives to also maintain binary compatibility;
* PATCH version when backwards compatible bug fixes are made. PATCH is binary compatible.

On incrementing a number, all numbers to the right of it are reset to 0.

Major version 0 is considered stable.

Additional labels for pre-release and build metadata are available as extensions to the MAJOR.MINOR.PATCH format and start at the first character following the PATCH that is not a decimal digit.

See also the [Releases section](https://www.eclipse.org/projects/handbook/#release) of the Eclipse project handbook which discusses _Major_, _Minor_, and _Service_ release criteria (where _Service_ corresponds to PATCH in the above description.

### Version Stability [1.ii]

Cyclone DDS is at a stable version.
The current version is the highest versioned Git tag that consists of three decimal numbers separated by periods.
For each minor release, there exists a releases/MAJOR.MINOR.x branch that is used for PATCH releases.

The change history can be found in the [CHANGELOG](https://github.com/eclipse-cyclonedds/cyclonedds/blob/master/CHANGELOG.rst).

### Public API Declaration [1.iii]

Symbols starting with `dds_` or `DDS_` and that are accessible after including only the top-level `dds.h` file, unless explicitly stated otherwise in a documentation comment, constitute the Public API.

Definitions available in installed include files that are outside this definitions are either internal or part of an Evolving API that is expected to eventually become part of the Public API. Internal definitions are deliberately included in the install: the project chooses not to artifically limit users of Cyclone DDS to the Public or Evolving APIs.

In the source repository, these header files reside with the modules that make up Cyclone DDS.

### API Stability Policy [1.iv]

For the Public API:
* Cyclone DDS provides Public API stability for PATCH and MINOR releases.
* Cyclone DDS strives to provide Public API stability for MAJOR releases.
* Cyclone DDS does not guarantee Public API stability for MAJOR releases.

For the Evolving API:
* Cyclone DDS provides Evolving API stability for PATCH releases.
* Cyclone DDS strives to provide Evolving API stability for MINOR releases.
* Cyclone DDS does not guarantee Evolving API stability for MINOR and MAJOR releases.

The RMW interface is what ROS 2 deals with, and for a variety of reasons it may be decided to rely on unstable interfaces (or even implementation details), to better support ROS 2's design decisions that do not fit so well with DDS.
Given ROS 2's importance to Cyclone DDS the Public API is expected to eventually cover all of ROS 2's needs, but without a defined time-line for that to happen.

The Cyclone DDS projects takes these uses into account, but it may at times even require source changes to the RMW layer.
As the Cyclone DDS project is actively involved in maintaining the RMW layer, such cases are handled by scheduling updates and performing additional compatibility checks against the ROS 2 test suite to ensure they do not cause any disruption to ROS 2.

Those changes are all hidden in the interface between the RMW layer and Cyclone DDS itself and are of no concern to ROS 2 applications.
It just means that, in the general case, one should always use a matched pair of `rmw_cyclonedds_cpp` and `cyclonedds`.

### ABI Stability Policy [1.v]

Cyclone DDS provides ABI stability for PATCH releases and strives to provide ABI stability for MINOR releases, for both Public and Evolving APIs.
Cyclone DDS does not guarantee ABI stability for MINOR or MAJOR releases.

### ABI and ABI Stability Within a Released ROS Distribution [1.vi]

ROS 2 users do not interact directly with Cyclone DDS and the mapping of the RMW interface to the Cyclone DDS APIs provided by the `rmw_cyclonedds` package hides any API or ABI changes from ROS 2 users.

Based on the ABI Stability Policy, Cyclone DDS PATCH releases can be accepted as updates within a Released ROS Distribution. MINOR releases can likely be accepted as updates after assessing the binary compatibility.
Cyclone DDS MINOR and MAJOR releases can, at least in principle, be accepted as updates within a Released ROS Distribution if the update is accompanied by an update of the `rmw_cyclonedds` package.

## Change Control [2]

Cyclone DDS follows the recommended guidelines of the [Eclipse Development Process](https://www.eclipse.org/projects/dev_process/).

The Eclipse Foundation manages write access to project repositories, allowing only designated [Committers](https://www.eclipse.org/projects/dev_process/#4_1_Committers), who have been voted for in elections overseen by the Eclipse Foundation, to commit changes.

API and ABI stability is part of the review process. The Cyclone DDS project runs CI and tests.
The Cyclone DDS project runs ROS CI for changes that are likely to affect ROS.

### Change Requests [2.i]

All changes occur through a pull request.

### Contributor Origin [2.ii]

This package uses DCO as its confirmation of contributor origin policy.
More information can be found in [Eclipse Foundation's DCO policy](https://www.eclipse.org/legal/DCO.php).
Eclipse projects furthermore require that an [Eclipse Contributor Agreement](https://www.eclipse.org/legal/ECA.php) is on file with the Eclipse Foundation.

### Peer Review Policy [2.iii]

All pull requests must pass peer-review except when no-one is able to provide a review for a PR introduced by a Committer. The exception exists solely as an escape hatch if no review can be obtained within a reasonable time frame while there is an urgent need for the change introduced by the PR.
Check [Eclipse Developer Process](https://www.eclipse.org/projects/dev_process/) for additional information.

### Continuous Integration [2.iv]

Pull requests are required to pass all tests in the CI system prior to merging, unless Committers consider there is sufficient evidence that a failure is the result of a mishap unrelated to the change.
Cyclone DDS CI results are [public](https://dev.azure.com/eclipse-cyclonedds/cyclonedds/_build) and cover x64 and x86 platforms running Linux, macOS and Windows:

- Ubuntu 20.04 with gcc 10
- Ubuntu 18.04 with gcc 7
- Ubuntu 20.04 with clang 10
- macOS 10.15 with clang 12
- macOS 10.15 with clang 12 while targeting macOS 10.12
- Windows Server 2019 with Visual Studio 2019

These are run with a mixture of debug, debug-with-address sanitizer and release builds.

ROS 2 CI also runs the Cyclone DDS tests, which provides further coverage, including the exact platforms used for ROS 2 releases.

### Documentation Policy [2.v]

All pull requests must resolve related documentation changes before merging.

## Documentation [3]

### Feature Documentation [3.i]

The project documentation includes a getting started that introduces the high-level concepts, but it generally prefers to instead refer to the [OMG DDS specification](https://www.omg.org/spec/DDS/1.4/PDF) for an accessible and full description of the concepts and high-level features.

Some components can be enabled/disabled at compile-time through compilation options from the CMake files (e.g., DDS Security support). These are done with CMake options.

Some other features are included/excluded at build time based on the features provided by the target platform (e.g., source-specific multicasting, DNS support to allow hostnames to be used in the configuration). All features are available on the tier 1 platforms, the ability to exclude them is needed for embedded platforms such as FreeRTOS.

### Public API Documentation [3.ii]

Reference information for the Public API is provided in the form of Doxygen comments. Generated documentation is provided [online](https://cyclonedds.io).
Configuration settings are documented in the source, with auto-generated [XSD files](https://github.com/eclipse-cyclonedds/cyclonedds/blob/master/etc/cyclonedds.xsd) and [markdown files](https://github.com/eclipse-cyclonedds/cyclonedds/blob/master/docs/manual/options.md) committed in the repository and linked from the front page.
[Background information](https://github.com/eclipse-cyclonedds/cyclonedds/blob/master/docs/manual/config.rst) on configuration settings is also provided.

### License [3.iii]

The license for Cyclone DDS is the Eclipse Public License 2.0 and the Eclipse Distribution License 1.0, and all of the code includes a header stating so.
The project includes a [`NOTICE`](https://github.com/eclipse-cyclonedds/cyclonedds/blob/master/NOTICE.md#declared-project-licenses) with links to more information about these licenses.
The Cyclone DDS repository also includes a [`LICENSE`](https://github.com/eclipse-cyclonedds/cyclonedds/blob/master/LICENSE) file with the full terms.

There is some third-party content included with Cyclone DDS which is licensed as Zlib, New BSD, and Public Domain.
Details can also be found in the included [`NOTICE`](https://github.com/eclipse-cyclonedds/cyclonedds/blob/master/NOTICE.md#third-party-content) document.

### Copyright Statement [3.iv]

The Cyclone DDS documentation includes a [policy](https://github.com/eclipse-cyclonedds/cyclonedds/blob/master/NOTICE.md#copyright) regarding content copyright, each of the source files containing code include a copyright statement with the license information in the file's header.

### Lists and Peer Review [3.v.c]

This section is not applicable to a non-ROS project.

## Testing [4]

Cyclone DDS source tree includes subdirectories for test code.
In all, the test code comprises approximately 25% of the codebase.

### Feature Testing [4.i]

Each feature in Cyclone DDS has corresponding tests which simulate typical usage, and they are located in separate directories next to the sources.
New features are required to have tests before being added.

A substantial amount of the tests found throughout the source tree verify functionality of various features of Cyclone DDS.

### Public API Testing [4.ii]

Each part of the public API has tests, and new additions or changes to the public API require tests before being added.
The tests aim to cover both typical usage and corner cases.
There are some tests throughout the Cyclone DDS source tree which specifically target the public API.

There is no automated tracking of correlation between tests and API functionality. On the basis of a manual assessment conducted by the project, the overall number of tests and the number of reported issues, it is fair to conclude that the Public API is well-covered.

In continuous integration, address sanitizer is enabled for some of the test matrix. Address sanitizer errors result in CI failure.

### Coverage [4.iii]

Cyclone DDS publishes [test coverage](https://codecov.io/github/eclipse-cyclonedds/cyclonedds?branch=master) using [Codecov](https://codecov.io).
New changes are required to include tests coverage. Line coverage is approximately 75% (as of 2020-09-04).

### Performance [4.iv]

While there are no public automated tests or results, there is evidence in PRs that performance does get taken into account (see, e.g., https://github.com/eclipse-cyclonedds/cyclonedds#558).
`ddsperf` is used to check for performance regressions regularly and before releases.
Performance-sensitive PRs are tested for regressions using ddsperf before changes are accepted.
[ddsperf](https://github.com/eclipse-cyclonedds/cyclonedds/tree/master/src/tools/ddsperf) is the tool to use for assessing Cyclone DDS performance.

There is automated performance testing run nightly as part interoperability testing on internal servers.

Open Robotics runs ros2 [nightly CI performance tests](http://build.ros2.org/job/Fci__nightly-performance_ubuntu_focal_amd64/) exist but is not yet reliable infrastructure. 
We suggest and would like to assist Open Robotics to move all performance testing to dedicated hardware.

### Linters and Static Analysis [4.v]

`rmw_cyclonedds` uses and passes all the ROS2 standard linters and static analysis tools for a C++ package as described in the [ROS 2 Developer Guide](https://docs.ros.org/en/rolling/Contributing/Developer-Guide.html#linters-and-static-analysis).
Passing implies there are no linter/static errors when testing against CI of supported platforms.

Cyclone DDS has automated [Synopsys Coverity static code analysis](https://www.synopsys.com/software-integrity/security-testing/static-analysis-sast.html) with public results that can be seen [here](https://scan.coverity.com/projects/eclipse-cyclonedds-cyclonedds).
Cyclone DDS has no outstanding defects as of 20-09-2021 on 240k lines analyzed. For comparison the average defect density of open source software projects of similar size is 0.5 per 1,000 lines of code.

Coding style is considered and enforced in the review of all Cyclone DDS pull requests.
Contributors are regularly asked to rewrite or reformat their code contributions before pull requests are accepted.
This is a matter of public record in the Cyclone DDS pull request history.

## Dependencies [5]

### Direct Runtime ROS Dependencies [5.i]

As an external dependency, there are no ROS dependencies in Cyclone DDS.

### Optional Direct Runtime ROS Dependencies [5.ii]

As an external dependency, there are no ROS dependencies in Cyclone DDS.

### Direct Runtime non-ROS Dependency [5.iii]

The only runtime dependency of Cyclone DDS is [OpenSSL](https://openssl.org), a widely-used secure communications suite.
If Cyclone DDS is built without security enabled, the product has no runtime dependencies.

## Platform Support [6]

Cyclone DDS supports all of the tier 1 platforms as described in REP-2000.
Cyclone DDS CI test coverage does not exactly match the tier 1 platforms of ROS 2.
The ROS 2 CI includes the Cyclone DDS CI tests, and so there is automated validation that all tier 1 platforms are fully supported.

Regarding minimum versions they are basically not known exactly because Cyclone DDS builds and runs on everything that was tested including ancient versions of Linux and macOS.
For evidence, the fact that Cyclone DDS builds and runs on [Solaris 2.6 on SPARCv8](https://github.com/eclipse-cyclonedds/cyclonedds/tree/master/ports/solaris2.6) (given pre-generated header files and IDL output) is a fair indication of its broad support of platforms and old versions.

The README specifies mininum versions that are new enough.

## Security [7]

### Vulnerability Disclosure Policy [7.i]

This package conforms to the Vulnerability Disclosure Policy in REP-2006.
The Eclipse Project Handbook states the project's [vulnerability disclosure policy](https://www.eclipse.org/projects/handbook/#vulnerability-disclosure) in detail.
