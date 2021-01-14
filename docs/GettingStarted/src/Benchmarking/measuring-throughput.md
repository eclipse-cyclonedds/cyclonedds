## Measuring Throughput

To measure throughput between two different applications, you need to run at least two instances of the _ddsperf_tool and instruct one of them to endorse the role of a Publisher that sends a given amount of data (a sequence of octets) at a given rate, the other instances will take the role of Subscriber applications. Please note that when your scenario involves only one subscriber, the UDP unicast mode is used. If several subscriber instances are running, the multicast will be automatically used.

<div align=center> <img src="figs/4.4-1.png"></div>

Two additional modes are therefore supported:

The **Pub** mode and the **Sub** mode.

In the Sub mode the subscriber operates either:

- using the **Listener** notification mechanism,
- the **WaitSet** notification mechanism, or
- the **Pooling** mode. The pooling mode allows the subscriber to cyclically fetch the data from its local cache instead of being notified each time a new set of data is added to the subscriber's cache as it is the case with the other modes.

You can publish data in two ways by publishing each data sample individually or by sending them in a _Burst_ mode.

- The **Rate** and frequency at which data is written. This is specified through the [R[Hz]] option. The default rate is &quot;as fast as possible&quot;. Which means, in **pub** mode, instead of trying to reach a certain rate, it just pushes data as hard as it can.

- The **Size** of the data that will be exchanged. This is specified through the [Size S] option. The size is &quot;as small as possible&quot; by default, depending on the size of the topic it defaults to.
- The **Burst Size** , defines the number of data samples that will be issued together in as a batch. This parameter is defined by the [Burst N] option. The default size for burst is 1. It doesn't make much of a difference when going &quot;as fast as possible&quot;, and it only applies to the **pub** mode.
- The triggering mode by default is _listener_ for the **ping** , **pong** and **sub** mode.

To run a simple throughput test, you can simply run a **pub** mode and a **sub** mode in 2 different terminals without specifying any other options or you can customize it as shown below:

Open two terminals, navigate to the directory where _ddsperf_is located and write the following command:
```
./ddsperf pub size 1k
```

And in the other terminal, type in:
```
./ddsperf -Qrss:1 sub`
```


This measures the throughput of data samples with 1Kbytes written as fast as possible.

The `-Qrss:1` option in **sub** mode sets the maximum allowed increase in RSS as 1MB. When running the test, if the memory occupies by the process increases by less than 1MB, the test can be successfully run. Otherwise, an error message is printed out at the end of the test.

As the `pub` in this example only has a size of 1k, the sub will not print out an RSS error message at the end of the test.

The output of the _ddsperf_ tool when measuring throughput is as shown below:

1. The output for the **Pub** application indicates mainly:

- _**RSS**_ is the Resident Set Size; it indicates the amount of memory is used by the process (e.g. 6.3MB used by the process id &quot;4026&quot;);
- _**VCSW**_ is the number of voluntary switches, it indicates the times when the process waits for input or an event (e.g. 1054 times);
- _**IVCSW**_ is the number of involuntary switches, it indicates the times when the process is pre-empted or blocked by a mutex (e.g. 24 times);
- The percentage of time spent executing user code and the percentage of time spent executing kernel code in a specific thread (e.g. spent 34% of the time executing the user code and 11% executing kernel code in thread &quot;pub&quot;).

<div align=center> <img src="figs/4.4-2.png"></div>

1. The output for the **Sub** application indicates mainly:

- The **size of the data** involving in this test (e.g. 1024 bytes, which is the &quot;size 1k&quot; defined in the pub command)
- The **total packets received** (e.g. 614598);
- The **total packets los** t (e.g. 0);
- The **packets received in 1 second reporting period** (e.g. 212648);
- The **packets lost in 1 second report period** (e.g. 0);
- The **number of samples processed by the Sub application** in 1s (e.g. 21260 KS/s, with the unit KS/s is 1000 samples per second).

<div align=center> <img src="figs/4.4-3.png"></div>