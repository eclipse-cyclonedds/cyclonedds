# Android port

Building for Android 9 (codename Pie), or API level 28. Earlier versions of
Android may be supported at some point, but that would require implementing
replacements for `pthread_attr_setinheritsched` (introduced in level 28) and
`getifaddrs` (introduced in level 24), which is outside the scope of this
document.

> Linux, specifically Fedora 31, was used to create this guide. The steps to
> build Eclipse Cyclone DDS should be more-or-less the same on other
> platforms, the steps to setup the emulator are probably not.


## Installing the Android SDK

[1]: https://developer.android.com/studio
[2]: https://stackoverflow.com/questions/43923996/adb-root-is-not-working-on-emulator-cannot-run-as-root-in-production-builds

 * Download [Android Studio][1] and extract the archive.

 * Run `android-studio/bin/studio.sh` to launch the *Android Studio Setup
   Wizard* and install the *Android SDK*. Select the *Android Virtual Device*
   option to install the emulator.

 * Click *Configure* from the *Android Studio* welcome screen and select
   *SDK Manager*. Switch to the *SDK Tools* tab, select *NDK (Side by side)*,
   click *Ok* and wait for the **Android Native Development Kit (NDK)** to
   finish downloading.

> *Configure* is available from the *Welcome to Android Studio* window, but
> maybe hidden because it is not appropriatly sized. Resize the window so
> *Configure* and *Get Help* are shown.

 * Click *Configure* from the *Android Studio* welcome screen and select
   *AVD Manager*. Click *Create Virtual Device*, select the *Phone* category
   and choose the *Pixel XL* device definition. Click *Next* and hit
   *Download* next to *Q*. *Accept* the *License Agreement* and click *Next*
   to download the emulator image. When the download is finished, select *Q*,
   click *Next*, then *Finish* to create the *Android Virtual Device (AVD)*.

> As can be read from [this StackOverflow post][2], it is important to NOT
> select a *Google Play* image, as these images do not allow you to gain
> root privileges.


## Building Eclipse Cyclone DDS

[3]: https://developer.android.com/ndk/guides/cmake
[4]: https://developer.android.com/ndk/guides/cmake#variables

The [Android NDK supports CMake][3] via a toolchain file. Build parameters
such as ABI can specified on the command line. For the complete list of
supported variables, consult the [Toolchain Arguments][4] section.

```
$ cd cyclonedds
$ mkdir build.android
$ cd build.android
$ cmake -DCMAKE_TOOLCHAIN_FILE=<path/to/Android/Sdk>/ndk/<version>/build/cmake/android.toolchain.cmake \
        -DANDROID_ABI=x86 -DANDROID_NATIVE_API_LEVEL=28 -DBUILD_SHARED_LIBS=Off ..
$ cmake --build .
```

> Android supports shared libraries. `BUILD_SHARED_LIBS` is disabled solely
> for demonstration purposes.


## Deploying ddsperf
 * Launch the emulator.
```
$ <path/to/Android/Sdk>/emulator/emulator -avd Pixel_XL_API_29 -netdelay none -netspeed full
```

 * Push the `ddsperf` binary to the emulator.
```
$ <path/to/Android/Sdk>/platform-tools/adb push <path/to/cyclonedds>/build.android/bin/ddsperf /data/local/tmp
```

> The binary must be copied to the local filesystem, not the sd-card, as that
> is mounted with the `noexec` flag and will not allow you to execute
> binaries.

 * Open a shell on the emulator.
```
$ <path/to/Android/Sdk>/platform-tools/adb shell
```

 * Change to `/data/local/tmp` and make the binary executable.
```
$ cd /data/local/tmp
$ chmod 755 ddsperf
```


## Running ddsperf over the loopback interface
 * Ensure the emulator is running.
 * Open a shell on the emulator.
 * Change to `/data/local/tmp`.
 * Execute `./ddsperf -D10 pub & ./ddsperf -D10 sub`.


## Running ddsperf over a network interface
Each emulator instance runs behind a virtual router/firewall service that
isolates it from the host. The Android version running in the emulator can
communicate with the host over the loopback interface using the specialized
alias network address *10.0.2.2*, but IGMP and multicast are not supported.
To communicate with binaries not running in the QEMU guest an additional
network interface is required.

 * Create a network bridge and a *tap* device for the emulator to use.
```
$ ip link add name br0 type bridge
$ ip link set br0 up
$ ip addr add 192.168.178.1/24 dev br0
$ ip tuntap add dev tap0 mode tap user $USER
$ ip link set tap0 master br0
$ ip link set dev tap0 up promisc on
```

 * Launch the emulator with the *tap* device attached.
```
$ <path/to/Android/Sdk>/emulator/emulator -avd Pixel_XL_API_29 -netdelay none -netspeed full -qemu -device virtio-net-pci,netdev=eth1 -netdev tap,id=eth1,ifname=tap0,script=no
```

 * Open a shell on the emulator.
 * Execute `su` from the shell to become *root*.
 * Bring up the interface and assign an IP address.
```
# ip link set dev eth1 up
# ip addr add 192.168.178.2/24 dev eth1
```

 * Make normal routing take precedence over anything that google/qemu configured.
```
$ ip rule add from all lookup main pref 99
```

 * You should now be able to ping the host from the emulator and vice versa.

> The [KDE Community Wiki][5] is an excellent source of information on the
> Android emulator. The second-to-last instruction stems from there and was
> vital to establish communication between the emulator and the host.

[5]: https://community.kde.org/KDEConnect/Android_Emulator

 * Force the guest to use the `eth1` interface.
```
$ export CYCLONEDDS_URI='<CycloneDDS><Domain><General><Interfaces><NetworkInterface name="eth1" /></Interfaces></General></Domain></CycloneDDS>'
```

 * Change to `/data/local/tmp`.
 * Execute `./ddsperf -D10 pub`.
 * Switch to the host.
 * Force the host to use the `br0` interface.
```
$ export CYCLONEDDS_URI='<CycloneDDS><Domain><General><Interfaces><NetworkInterface name="br0" /></Interfaces></General></Domain></CycloneDDS>'
```
 * Execute `./ddsperf -D10 sub` (native build).
