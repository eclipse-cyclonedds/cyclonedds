## Testing your network configuration

Once your Cyclone DDS installation is done successfully you may want to test if your network environment is properly set. This can be done either by running the _HelloWorld_ example or by using the _ddsperf_ tool. The Helloworld example sends in one shot a message, whereas the ddsperf tool can send continuously a stream of data at a low frequency rate for sanity checks and can therefore bypass sporadic network issues.

Users that installed Cyclone DDS using the package installation procedure will find the _ddsperf_ tool pre-installed under the directory of `<cyclonedds_dir>/bin`. Users that installed Cyclone DDS through the native installation process, (from GitHub), can locate the tool in `<cyclonedds_dir>/build/bin`.

The sanity checks of your dds based system using the _ddsperf_ tool is done as follows
```
./ddsperf sanity
```

With the sanity option, one and only one data sample is sent each second (1Hz).

In another terminal, you need to start the _ddspef_ with the **Pong** mode to echo the data to the first instance of the _ddsperf_ started with the _Sanity_ option.
```
./ddsperf pong
```

<div align=center> <img src="figs/4.2-1.png"></div>

If the data is not exchanged on the network between the two ddsperf instances, it is likely that cyclone dds did not select the appropriate network card on both machines or a firewall in-between is preventing communication to happen.

Cyclone DDS selects automatically the most available network interface. This can behavior can be overridden by changing the configuration file. (see section 1.6 for more details) .

When running the previous scenario on a local machine, this test assures the loop-back option is enabled.