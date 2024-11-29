ARG ARCH=x86_64

FROM ubuntu:24.04 AS build
# supports x86_64 or i686
ARG ARCH

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y -q\
  build-essential \
  cmake \
  mingw-w64

WORKDIR /project
COPY --link . src

# Host build, only for idlc (+ libidlc) but building individual targets doesn't really work...
RUN cmake -S src -B host-build
RUN cmake --build host-build --parallel
RUN cmake --install host-build --prefix /project/install

# Cross build with mingw-w64 toolchain
RUN cmake -S src -B build \
  -DBUILD_EXAMPLES=1 \
  -DCMAKE_TOOLCHAIN_FILE=/project/src/ports/mingw-w64/mingw-w64-${ARCH}.cmake \
  -DCMAKE_PREFIX_PATH=/project/install
RUN cmake --build build --parallel
RUN cmake --install build 

FROM ubuntu:24.04
ARG ARCH

RUN if [ "${ARCH}" = "i686" ]; then \
  dpkg --add-architecture i386; \
  apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y -q mingw-w64 wine wine32:i386; \
  echo "export WINEARCH=win32" >> /etc/bash.bashrc; \
 else \
  apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y -q mingw-w64 wine; \
 fi

COPY --from=build /usr/local/bin /usr/local/bin
COPY --from=build /usr/local/lib /usr/local/lib
COPY --from=build /usr/local/include /usr/local/include
COPY --from=build /project/build/bin /usr/local/bin
