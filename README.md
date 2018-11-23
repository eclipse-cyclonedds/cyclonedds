# Eclipse Cyclone DDS

Eclipse Cyclone DDS is by far the most performant and robust DDS implementation available on the market. 

Beside, Cyclone DDS is developed completely in the open and is undergoing the acceptance process to become part of Eclipse IoT (see  [eclipse-cyclone-dds](https://projects.eclipse.org/proposals/eclipse-cyclone-dds)).


# Getting Started
## Building Cyclone DDS

In order to use Eclipse Cyclone DDS you need to have installed on your host [cmake](https://cmake.org/download/) **v3.6.0** or higher, [git](https://git-scm.com/),  the [Java 8 JDK](http://www.oracle.com/technetwork/java/javase/downloads/jdk8-downloads-2133151.html) or simply the [Java 8 RE](http://www.oracle.com/technetwork/java/javase/downloads/server-jre8-downloads-2133154.html), and [Apache Maven 3.5.x or higher](http://maven.apache.org/download.cgi).

To obtain Eclipse Cyclone DDS, do

    $ git clone https://github.com/eclipse-cyclonedds/cyclonedds.git 
    $ cd cyclonedds
    $ mkdir build

Depending on whether you want to develop applications using Eclipse Cyclone DDS or contribute to Eclipse Cyclone DDS you can follow different procedures

### For application developers

To build and install the required libraries needed to develop your own applications that use Eclipe Cyclone DDS

On Linux:

    $ cd build
    $ cmake -DCMAKE_BUILD_TYPE=<build-type> -DCMAKE_INSTALL_PREFIX=<install-location> ../src
    $ cmake --build . --target install

On Windows:

    $ cd build
    $ cmake -DCMAKE_BUILD_TYPE=<build-type> -G "<generator-name>" -DCMAKE_INSTALL_PREFIX=<install-location> ../src
    $ cmake --build . --target install

### For contributors

To contribute to the development of Eclipse Cyclone DDS you may need to provide test cases using [cunit](http://cunit.sourceforge.net/). Once cunit is installed do

On Linux:

    $ cd build
    $ cmake -DCMAKE_BUILD_TYPE=<build-type> -DBUILD_TESTING=on -DCMAKE_INSTALL_PREFIX=<install-location> ../src
    $ cmake --build . --target install

On Windows:

    $ cd build
    $ cmake -DCMAKE_BUILD_TYPE=<build-type> -G "<generator-name>" -DBUILD_TESTING=on -DCMAKE_INSTALL_PREFIX=<install-location> ../src
    $ cmake --build . --config Debug --target install


where build type is Debug or Release, and '<generator name>' specifies the type of files to use (see [here](https://cmake.org/cmake/help/v3.0/manual/cmake-generators.7.html)).

Depending on the location where to install you may need administrator privileges. To uninstall use

    $ cmake --build . --target uninstall

You are now ready to use **cyclonedds** for your next DDS project!


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
            1     17382       56       50       77      269      17382       10        9      17382        1        1
            2     17502       55       50       75      137      17502       10        9      17502        1        1
            3     17482       56       50       73      165      17482       10        9      17482        1        1
            4     17429       56       50       73      135      17429       10        9      17429        1        1
            5     17514       56       50       73      146      17514       10        9      17514        1        1
            6     17566       55       50       74      256      17566       10        9      17566        1        1
            7     17555       55       51       74      119      17555       10        9      17555        1        1
            8     17551       55       51       74      137      17551       10        9      17551        1        1
            9     17562       55       50       72      193      17562       10        9      17562        1        1
           10     17461       56       50       74      143      17461       10        9      17461        1        1
           
The number above were measure on Mac running a 4,2 GHz Intel Core i7. From these number you can see how the roundtrip is incredibly stable and the minimal latency is about 25 micro-seconds (on this HW).

## Documentation
The Cyclone DDS documentation is available [here](http://cdds.io/docs).
