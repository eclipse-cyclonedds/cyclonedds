.. index:: Runtime configuration
    
.. _runtime_config:

#####################
Runtime configuration
#####################

The out-of-the-box configuration should usually be fine, but there are a great many options that can be tweaked by creating an XML file with the desired settings and defining the `CYCLONEDDS_URI` to point to it.
E.g. (on Linux):

    $ cat cyclonedds.xml
    <?xml version="1.0" encoding="UTF-8" ?>
    <CycloneDDS xmlns="https://cdds.io/config" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="https://cdds.io/config https://raw.githubusercontent.com/eclipse-cyclonedds/cyclonedds/master/etc/cyclonedds.xsd">
        <Domain Id="any">
            <General>
                <Interfaces>
                    <NetworkInterface autodetermine="true" priority="default" multicast="default" />
                </Interfaces>
                <AllowMulticast>default</AllowMulticast>
                <MaxMessageSize>65500B</MaxMessageSize>
            </General>
            <Discovery>
                <EnableTopicDiscoveryEndpoints>true</EnableTopicDiscoveryEndpoints>
            </Discovery>
            <Internal>
                <Watermarks>
                    <WhcHigh>500kB</WhcHigh>
                </Watermarks>
            </Internal>
            <Tracing>
                <Verbosity>config</Verbosity>
                <OutputFile>cdds.log.${CYCLONEDDS_PID}</OutputFile>
            </Tracing>
        </Domain>
    </CycloneDDS>
    $ export CYCLONEDDS_URI=file://$PWD/cyclonedds.xml

(on Windows, one would have to use `set CYCLONEDDS_URI=file://...` instead.)

This example shows a few things:

* `Interfaces` can be used to override the interfaces selected by default.
  Members are
  * `NetworkInterface[@autodetermine]` tells Cyclone DDS to autoselect the interface it deems best.
  * `NetworkInterface[@name]` specifies the name of an interface to select (not shown above, alternative for autodetermine).
  * `NetworkInterface[@address]` specifies the IPv4/IPv6 address of an interface to select (not shown above, alternative for autodetermine).
  * `NetworkInterface[@multicast]` specifies whether multicast should be used on this interface.
    The default value 'default' means Cyclone DDS will check the OS reported flags of the interface and enable multicast if it is supported.
    Use 'true' to ignore what the OS reports and enable it anyway and 'false' to always disable multicast on this interface.
  * `NetworkInterface[@priority]` specifies the priority of an interface.
    The default value (`default`) means priority `0` for normal interfaces and `2` for loopback interfaces.
* `AllowMulticast` configures the circumstances under which multicast will be used.
  If the selected interface doesn't support it, it obviously won't be used (`false`); but if it does support it, the type of the network adapter determines the default value.
  For a wired network, it will use multicast for initial discovery as well as for data when there are multiple peers that the data needs to go to (`true`).
  On a WiFi network it will use it only for initial discovery (`spdp`), because multicast on WiFi is very unreliable.
* `EnableTopicDiscoveryEndpoints` turns on topic discovery (assuming it is enabled at compile time), it is disabled by default because it isn't used in many system and comes with a significant amount of overhead in discovery traffic.
* `Verbosity` allows control over the tracing, "config" dumps the configuration to the trace output (which defaults to "cyclonedds.log", but here the process id is appended).
  Which interface is used, what multicast settings are used, etc., is all in the trace.
  Setting the verbosity to "finest" gives way more output on the inner workings, and there are various other levels as well.
* `MaxMessageSize` controls the maximum size of the RTPS messages (basically the size of the UDP payload).
  Large values such as these typically improve performance over the (current) default values on a loopback interface.
* `WhcHigh` determines when the sender will wait for acknowledgements from the readers because it has buffered too much unacknowledged data.
  There is some auto-tuning, the (current) default value is a bit small to get really high throughput.

Background information on configuring Cyclone DDS can be found [here](docs/manual/config.rst) and a list of settings is [available](docs/manual/options.md).