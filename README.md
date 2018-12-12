# Eclipse Cyclone DDS

Eclipse Cyclone DDS is by far the most performant and robust DDS implementation available on the market. 

Beside, Cyclone DDS is developed completely in the open and is undergoing the acceptance process to become part of Eclipse IoT (see  [eclipse-cyclone-dds](https://projects.eclipse.org/proposals/eclipse-cyclone-dds)).


# Getting Started
## Building Cyclone DDS

In order to build cyclone DDS you need to have installed on your host [cmake](https://cmake.org/download/) **v3.6.0** or higher, the [Java 8 JDK](http://www.oracle.com/technetwork/java/javase/downloads/jdk8-downloads-2133151.html) or simply the [Java 8 RE](http://www.oracle.com/technetwork/java/javase/downloads/server-jre8-downloads-2133154.html), and [Apache Maven 3.5.x or higher](http://maven.apache.org/download.cgi).

Assuming that **git** is also available on your machine then, simply do:

    $ git clone https://github.com/eclipse/cyclonedds.git 
    $ cd cyclonedds
    $ mkdir build
    $ cd build
    $ cmake ../src
    $ make
    $ make install

At this point you are ready to use **cyclonedds** for your next DDS project!
 

## Examples
Now that you have built and installed **cyclonecdds** it is time to experiment with some examples.

### Building and Running the Roundtrip Example
The first example we will show you how to build and run, measures cyclonedds latency and will allow you to see with your eyes how fast it is!

Do as follows:

    $ cd cyclonedds/src/examples/roundtrip
    $ mkdir build
    $ cd build
    $ cmake ..
    $ make
    
Now that you've build the roundtrip example it is time to run it. 

On one terminal start the applications that will be responding to **cyclonedds** pings.
    $ ./RoundtripPong

On another terminal, start the application that will be sending the ping.
    
    $ ./RoundtripPing 0 0 0 
    # payloadSize: 0 | numSamples: 0 | timeOut: 0
    # Waiting for startup jitter to stabilise
    # Warm up complete.
    # Round trip measurements (in us)
    #             Round trip time [us]                           Write-access time [us]       Read-access time [us]
    # Seconds     Count   median      min      99%      max      Count   median      min      Count   median      min
        1     28065       17       16       23       87      28065        8        6      28065        1        0
        2     28115       17       16       23       46      28115        8        6      28115        1        0
        3     28381       17       16       22       46      28381        8        6      28381        1        0
        4     27928       17       16       24      127      27928        8        6      27928        1        0
        5     28427       17       16       20       47      28427        8        6      28427        1        0
        6     27685       17       16       26       51      27685        8        6      27685        1        0
        7     28391       17       16       23       47      28391        8        6      28391        1        0
        8     27938       17       16       24       63      27938        8        6      27938        1        0
        9     28242       17       16       24      132      28242        8        6      28242        1        0
       10     28075       17       16       23       46      28075        8        6      28075        1        0

           
The number above were measure on Mac running a 4,2 GHz Intel Core i7 on December 12th 2018. From these number you can see how the roundtrip is incredibly stable and the minimal latency is now down to  17 micro-seconds (used to be 25 micro-seconds) on this HW.

## Documentation
The Cyclone DDS documentation is available [here](http://cdds.io/docs).
