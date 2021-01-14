## Running the pre-built example

A simple _Hello World_ application is included in the Eclipse Cyclone DDSCXX, it can be used to test the installation. The _Hello World_ application is located in the `<cyclonedds-cxx-directory>\build\bin\Debug` in Windows, and in `<cyclone-cxx-directory>/build/bin` in Linux.

To run the example application, open two console windows, and navigate to the appropriate directory as mentioned above. Run the `ddscxxHelloworldPublisher` in one of the console windows by typing the following command:

&nbsp;&nbsp; **Windows:** `ddscxxHelloworldPublisher.exe`

&nbsp;&nbsp; **Linux:** `./ddscxxHelloworldPublisher`

And run the `ddscxxHelloworldSubscriber` in the other console window with:

&nbsp;&nbsp; **Windows:** `ddscxxHelloworldSubscriber.exe`

&nbsp;&nbsp; **Linux:** `./ddscxxHelloworldSubscriber.exe`

The output for the `ddscxxHelloworldPublihser` should look like:

<div align=center> <img src="figs/5.6.2-1.png"></div>

The output for the `ddscxxHelloworldSubscriber` should look like:

<div align=center> <img src="figs/5.6.2-2.png"></div>

For more information on how to build this application on your own and the code which has been used, refer to the [_Hello World_ Chapter.](Build-cxx-app/building-your-first-cyclonedds-cxx-example.html)