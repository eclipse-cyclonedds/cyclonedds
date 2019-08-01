# Solaris 2.6 / sun4m port

Building for Solaris 2.6 / sun4m, e.g., a Sun Microsystems SPARCStation 20 running Solaris 2.6,
firstly involves getting a sufficiently modern gcc onto the machine (gcc-4.3.x with GNU binutils
certainly works, but it is very well possible that older versions and/or using the Sun assembler and
linker work fine, too) and a sufficiently new gmake (3.81 should do).

Secondly, because the port relies on a custom makefile rather than "cmake", and that makefile
doesn't build the Java-based IDL preprocessor to avoid pulling in tons of dependencies, you will
need to do a build on a "normal" platform first.  The makefile assumes that the required parts of
that build process are available in a "build" directory underneath the project root.  Note that only
the CMake generate export.h and the ddsperf-related IDL preprocessor output is required (if other
applications are to be be built, they may require additional files).

The results are stored in a directory named "gen".  After a successful build, there will be
libddsc.so and ddsperf in that directory.  No attempts are made at tracking header file
dependencies.  It seems unlikely that anyone would want to use such a machine as a development
machine.

The makefile expects to be run from the project root directory.

E.g., on a regular supported platform:
```
# mkdir build && cd build
# cmake ../src
# make
# cd ..
# git archive -o cdds.zip HEAD
# find build -name '*.[ch]' | xargs zip -9r cdds.zip
```

copy cdds.zip to the Solaris box, log in and:
```
# mkdir cdds && cd cdds
# unzip .../cdds.zip
# make -f ports/solaris2.6/makefile -j4
# gen/ddsperf -D20 sub & gen/ddsperf -D20 pub &
```

It takes about 10 minutes to do the build on a quad 100MHz HyperSPARC.
