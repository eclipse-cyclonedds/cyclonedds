## Running the pre-built example

Eclipse Cyclone DDS includes a simple _Hello World!_ application that can be executed to test your installation. The _Hello World!_ application consists of two executables:

- **HelloworldPublisher**
- **HelloworldSubscriber**

The _Hello World!_ application is located in `<cyclonedds-directory>\build\bin\Debug` in Windows, and `<cyclonedds-directory>/build/bin` in Linux.

To run the example application, open two console windows and navigate to the appropriate directory in both console windows. Run `HelloworldSubscriber` in one of the console windows using:

&nbsp; **Windows** `HelloworldSubscriber.exe`

&nbsp; **Linux** `./HelloworldSubscriber`

Run `HelloworldPublisher` in the other console window using:

&nbsp; **Windows** `HelloworldPublisher.exe`

&nbsp; **Linux** `./HelloworldPublisher`

`HelloworldPublisher` appears as follows:

<div align=center> <img src="src/figs/1.6.2-1.png"></div>

`HelloworldSubscriber` appears as follows:

<div align=center> <img src="src/figs/1.6.2-2.png"></div>

**Note:** Cyclone's default behavior is to automatically detect the first network interface card available on your machine and uses it to exchange the hello world message. Ensure that your publisher and subscriber applications are on the same network, selecting the right interface card. This is one of the most common issues on machine configurations with multiple network interface cards.

This default behavior can be overridden by updating the property `//CycloneDDS/Domain/General/`

`NetworkInterfaceAddress` in a deployment file (e.g. `cyclonedds.xml`) that you created to point to it through an OS environment variable named CYCLONEDDS\_URI. More information on this topic can be found in the [github repository](https://github.com/eclipse-cyclonedds/cyclonedds/blob/master/docs/manual/options.md) and the configuration section on https://github.com/eclipse-cyclonedds/cyclonedds.