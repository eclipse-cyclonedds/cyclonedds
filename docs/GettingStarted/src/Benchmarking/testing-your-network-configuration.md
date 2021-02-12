## Testing your network configuration

Once your Cyclone DDS installation is successfully completed, you may want to test if your network environment is correctly set up. This can be done either by running the _HelloWorld_ example or by using the _ddsperf_ tool. The Helloworld example sends a message in one shot, whereas the ddsperf tool can send a continuous stream of data at a low frequency rate for sanity checks and can therefore bypass sporadic network issues.

If you have installed Cyclone DDS using the product installer, the _ddsperf_ tool is pre-installed within `<cyclonedds_dir>/bin`. If you have installed Cyclone DDS through the native installation process, (from GitHub), you can locate the tool within `<cyclonedds_dir>/build/bin`.

Complete the sanity checks of your DDS based system using the _ddsperf_ tool as follows:

```
./ddsperf sanity
```

With the sanity option, only one data sample is sent each second (1Hz).

In another terminal, start the _ddsperf_ with the **Pong** mode to echo the data to the first instance of the _ddsperf_ started with the _Sanity_ option.

```
./ddsperf pong
```

<div align=center> <img src="figs/4.2-1.png"></div>

If the data is not exchanged on the network between the two ddsperf instances, it is likely that Cyclone DDS has not selected the appropriate network card on both machines or a firewall in-between is preventing the communication.

Cyclone DDS automatically selects the most available network interface. This behavior can be overridden by changing the configuration file. (see section 1.6 for more details) .

When running the previous scenario on a local machine, this test ensures the loop-back option is enabled.