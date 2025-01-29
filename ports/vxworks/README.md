# VxWorks port

Wind River provides the VxWorks SDK for `QEMU (x86-64)` which includes a ready-to-use VxWorks kernel.

## Installing the VxWorks SDK

[1]: https://labs.windriver.com/vxworks-sdk/#read
[2]: https://forums.windriver.com/t/vxworks-software-development-kit-sdk/43

 * Download the [VxWorks SDK][2] and extract the archive.

```bash
$ wget https://d13321s3lxgewa.cloudfront.net/wrsdk-vxworks7-up2-1.10.tar.bz2
$ tar xvfj wrsdk-vxworks7-up2-1.10.tar.bz2
```

## Building `idlc`

Build `idlc` on your PC and install it to `/usr/local`. It will be used later to build the `examples`.

```bash
$ cd cyclonedds
$ mkdir build
$ cd build
$ cmake ..
$ make install
```

## Building Eclipse Cyclone DDS

[3]: https://d13321s3lxgewa.cloudfront.net/downloads/wrsdk-vxworks7-docs/2403/README_qemu.html

 * Run the following commands to set up the *VxWorks Development Environment* and build the project.

```bash
$ source <path/to/VxWorks/SDK>/sdkenv.sh
$ export WIND_CC_SYSROOT=$WIND_SDK_CC_SYSROOT
```

```bash
$ cd cyclonedds
$ mkdir build.vxworks
$ cd build.vxworks
$ cmake .. -DCMAKE_TOOLCHAIN_FILE=$WIND_SDK_CC_SYSROOT/mk/toolchain.cmake \
  -DBUILD_EXAMPLES=ON -DCMAKE_PREFIX_PATH=/usr/local
$ cmake --build .
```

## Deploying `examples`

 * Configure the `tap0` interface.

```bash
$ sudo apt-get install uml-utilities
$ sudo tunctl -u $USER -t tap0
$ sudo ifconfig tap0 192.168.200.254 up
```

 * Install `QEMU`.

```bash
$ sudo apt-get install qemu-system
$ qemu-system-x86_64 --version
QEMU emulator version 6.2.0 (Debian 1:6.2+dfsg-2ubuntu6.2)
Copyright (c) 2003-2021 Fabrice Bellard and the QEMU Project developers
```

 * Create `cyclonedds.xml`.

```bash
$ cd build.vxworks
$ cat <<EOF > cyclonedds.xml
<CycloneDDS>
  <Domain id="any">
    <General>
      <Interfaces>
        <NetworkInterface address="127.0.0.1"></NetworkInterface>
      </Interfaces>
    </General>
  </Domain>
</CycloneDDS>
EOF
``` 
 
 
 * Launch `QEMU` with USB storage attached.
 
```bash
$ cd build.vxworks
qemu-system-x86_64 -m 512M -kernel $WIND_SDK_HOME/vxsdk/bsps/*/vxWorks \
  -net nic -net tap,ifname=tap0,script=no,downscript=no -display none -serial mon:stdio \
  -append "bootline:fs(0,0)host:vxWorks h=192.168.200.254 e=192.168.200.1 g=192.168.200.254 u=target pw=vxTarget o=gei0" \
  -usb -device usb-ehci,id=ehci -device usb-storage,drive=fat32 -drive file=fat:rw:`pwd`,id=fat32,format=raw,if=none
```

 * Run `HelloworldPublisher`.
 
```
-> cmd
[vxWorks *]# cd /bd0a/bin/
[vxWorks *]# set env LD_LIBRARY_PATH="/bd0a/lib"
[vxWorks *]# set env CYCLONEDDS_URI="/bd0a/cyclonedds.xml"
[vxWorks *]# ./HelloworldPublisher
=== [Publisher]  Waiting for a reader to be discovered ...
=== [Publisher]  Writing : Message (1, Hello World)
```

 * Use `telnet` to connect to `QEMU` and run `HelloworldSubscriber`.
 
```
$ telnet 192.168.200.1

-> cmd
[vxWorks *]# cd /bd0a/bin/
[vxWorks *]# set env LD_LIBRARY_PATH="/bd0a/lib"
[vxWorks *]# set env CYCLONEDDS_URI="/bd0a/cyclonedds.xml"
[vxWorks *]# ./HelloworldSubscriber
=== [Subscriber] Waiting for a sample ...
=== [Subscriber] Received : Message (1, Hello World)
```

 * To exit `QEMU` type `Ctrl-A X`.

