## Running the pre-built example

Eclipse Cyclone DDS includes a simple _Hello World!_ application that can be executed to test your installation. The _Hello World!_ application consists of two executables:

- the **HelloworldPublisher** and,
- the **HelloworldSubscriber**.

The _Hello World!_ application is located in the `<cyclonedds-directory>\build\bin\Debug` directory in Windows, and in `<cyclonedds-directory>/build/bin` in Linux.

To run the example application, you can open two console windows and navigate to the appropriate directory in both console windows. Run the HelloworldSubscriber in one of the console windows by typing the following command:

&nbsp; **Windows:** HelloworldSubscriber.exe

&nbsp; **Linux:** ./HelloworldSubscriber

and the HelloworldPublisher in the other console window by typing:

&nbsp; **Windows:** HelloworldPublisher.exe

&nbsp; **Linux:** ./HelloworldPublisher

The output HelloworldPublisher is captured in the screenshot below

<div align=center> <img src="figs/1.6.2-1.png"></div>

while the HelloworldSubscriber will be looking like this:

<div align=center> <img src="figs/1.6.2-2.png"></div>

Note that Cyclone's default behavior is to pick up automatically the first network interface card available on your machine and will use it to exchange the hello world message. Assure that your publisher and subscriber applications are on the same network, selecting the right interface card. This is one of the most common issues on machine configurations with multiple network interface cards.

This default behavior can be overridden by updating the property `//CycloneDDS/Domain/General/`

`NetworkInterfaceAddress` in a deployment file (e.g `cyclonedds.xml`) that you have typically had to create and point to it through an OS environment variable named CYCLONEDDS\_URI. More information on this topic can be found in the [github repository](https://github.com/eclipse-cyclonedds/cyclonedds/blob/master/docs/manual/options.md) and the configuration section on https://github.com/eclipse-cyclonedds/cyclonedds.