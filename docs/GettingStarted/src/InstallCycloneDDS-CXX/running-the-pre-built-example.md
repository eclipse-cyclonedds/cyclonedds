## Running the pre-built example

A simple _Hello World_ application is included in the Eclipse Cyclone DDSCXX, it can be used to test the installation. The _Hello World_ application is located in:
- **Windows:** `<cyclonedds-cxx-directory>\build\bin\Debug` 
- **Linux:** `<cyclone-cxx-directory>/build/bin`

To run the example application, open two console windows, and navigate to the appropriate directory. Run the `ddscxxHelloworldPublisher` in one of the console windows by using the following command:

- **Windows:** `ddscxxHelloworldPublisher.exe`

- **Linux:** `./ddscxxHelloworldPublisher`

Run the `ddscxxHelloworldSubscriber` in the other console window using:

- **Windows:** `ddscxxHelloworldSubscriber.exe`

- **Linux:** `./ddscxxHelloworldSubscriber.exe`

The output for the `ddscxxHelloworldPublisher` is as follows:

<div align=center> <img src="figs/5.6.2-1.png"></div>

The output for the `ddscxxHelloworldSubscriber` is as follows:

<div align=center> <img src="figs/5.6.2-2.png"></div>

For more information on how to build this application and the code which has been used, refer to [_Hello World_.](Build-cxx-app/building-your-first-cyclonedds-cxx-example.html)