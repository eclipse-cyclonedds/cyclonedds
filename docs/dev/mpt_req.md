# Eclipse Cyclone DDS Multi Process Testing Requirements

This document present some requirements and considerations regarding the
[Multi Process Test Framework](multi_process_testing.md).

## Requirements
1.1) To test certain Cyclone DDS features, multiple processes running Cyclone
     DDS are needed to force communication through the whole stack.

1.2) Should be buildable and runnable on platforms that support multiprocess
     and filesystems including the ones used in the continues integration
     context.

1.3) Results should be easily analyzable within the continues integration
     context and when running locally. This can be done by reporting the
     results in a standard format like xunit or cunit.

1.4) No processes must be left behind (f.i. deadlock in child process) when the
     test finished (or timed out).

1.5) When running tests parallel, they should not interfere with each other.

1.6) Processes of the same test should be able to communicate (for settings,
     syncing, etc).

1.7) It should be possible to analyze output/messages/tracing of the parent
     and child processes to be able to draw a proper test conclusion.


## Considerations
2.1)
The files that actually contain the tests, should be focused on those tests.
This means that the process management and setting up (and usage of) IPC
between test processes should be handled by a test framework so that the
test files can remain as clean as possible.

2.2)
If possible, there shouldn't be a need for writing log files to a file system
when running the tests normally. It could be helpful, however, that these log
files are written when debugging related tests.

2.3)
Preferably, the DDS communication between the processes should not leave
localhost.


## Intentions
There doesn't seem to be a 3rd party test framework that addresses our
requirements in a satisfactory manner.

After some discussions with a few people (different people in different
meetings), it was decided to create our own framework and to go in the
following direction:

- Process creation/destruction/etc is (re)introduced in the ddsrt.<br>
  [1.1/1.2]

- The files that contain the tests, should be easy to understand and focus on
  the tests themselves.<br>
  [2.1]

- Other files (generated or in the framework) should take care of the
  intricacies of starting/monitoring the proper processes with the proper
  settings.<br>
  [1.4/1.6/2.1]

- To do this, a similar approach of the current CUnit build will be used;
  CMake will scan the test files and create runners according to macros within
  the test files.<br>
  [2.1]

- The tests should be executed by CTest. For now this means that a proper
  runner exit code for pass/fail is enough. We would like to add CUnit like
  output in the future.<br>
  [1.2/1.3]

- The Cyclone DDS API contains the possibility to monitor generated log traces.
  This means we won't be needing to monitor actual log files. Just register a
  log callback and go from there.<br>
  [1.7/2.2]

- The framework should be able to generate unique domain ids and unique topic
  names when necessary. That way, tests won't interfere with each other when
  running in parallel.<br>
  [1.5]

