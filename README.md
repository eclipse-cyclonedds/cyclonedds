# Cyclone DDS

Cyclone DDS is by far the most performant and robust DDS implementation available on the market. 

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
    #.          Round trip time [us]         Write-access time [us]      Read-access time [us]
    # Seconds Count     Median    Min      Count     Median    Min     Count     Median      Min
    
        1     13476       70       66      13476       14       12      13476        2        1
        2     11972       73       66      11972       14       13      11972        2        1
        3     13109       71       67      13109       14       12      13109        2        1
        4     12259       72       67      12259       14       12      12259        2        1
        5     12928       71       67      12928       14       12      12928        2        1

The number above were measure on a 13' MacBook pro running a 3,1 GHz Intel Core i5. From these number you can see how the roundtrip is incredibly stable and the minimal latency is slightly over 30 micro-seconds (on this HW).

## Documentation
The Cyclone DDS documentation is available [here](http://cdds.io/docs).
