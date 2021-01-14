## Measuring Latency

To measure latency between two different applications, you need to run two instances of the _ddsperf_ tool and instruct one of them to endorse the role of a _sender_ that sends a given amount of data (a sequence of octets) at a given rate and the other instance will take the role of _receiver_ that sends back the same amount of data to the sender in a Ping-Pong scenario. The sending action is triggered by the **Ping** option. The receiving behavior is triggered by the **Pong** action. The sender measures the roundtrip time and computes the latency as the half of the roundtrip time.

The Ping Pong scenario avoids clock desynchronization issue that might occur between two machines that do not share accurately the same perception of the time in the network.

<div align=center> <img src="figs/4.3-1.png"></div>

To differential the two operational modes, the _ddsperf_tool can operate either in a **Ping mode** or in a **Pong mode**.

To run this scenario, open 2 terminals (e.g on Linux like OSs), navigate to the directory where _ddsperf_ is located then write the following command in one of the terminals:
```
./ddsperf ping
```

Input this command in another terminal:
```
./ddsperf pong
```


This basic scenario performs a simple latency test with all the default values. You may customize your test scenario by changing the following options.

In **Ping mode** you can specify:

- The _**Rate**_ and frequency at which data is written. This is specified through the [R[Hz]] option. The default rate is &quot;as fast as possible&quot;. In **ping** mode, it always sends a new ping as soon as it gets a pong;
- The _**Size**_ of the data that will be exchanged. This is specified through the [Size S] option. Using the default built-in topic, 12 bytes (an integer key, an integer sequence number, and an empty sequence of bytes). are sent every time. The size is &quot;as small as possible&quot; by default, depending on the size of the topic it defaults to;

- The _Listening_ mode, which can either be `waitset` based or `Listener` Callbacks modes. In the waitset mode the _ddsperf_ application creates a dedicated thread to wait for the data to return back from the receiving instance of _ddsperf_ tool (i.e the instance started with the Pong mode). In the Listener Callback mode, the thread is created by the cyclone dds middleware. The Listener mode is the default.

The **Pong mode** have only one option:

- the Listening mode [with two possible values, `waitset` or `Listener`]

For instance, if you want to measure local latency between to processes exchanging 2KB at the frequency of 50Hz, you can run the following commands in 2 different terminals:
```
./ddsperf ping 50Hz 2048 waitset
```

```
./ddsperf pong waitset
```

The output of the _ddsperf_ tool is as shown below:

1. The output for the **Ping** application indicates mainly:

- The **size of the data** involved in the test (e.g. 12 bytes)
- The **minimum latency** (e.g. 78.89 us)
- The **maximum latency** (e.g. 544,85 us)
- The **mean latency** (e.g. 118.434 us)
- As well as the latency at 50%, 90% or 99% of the time.

<div align=center> <img src="figs/4.3-2.png"></div>


2. The output for the **Pong** application:

- _**RSS**_ is the Resident Set Size, it indicates the amount of memory used by the process (e.g. 3.5MB used by the process id 2680);
- _**VCSW**_ is the number of voluntary switches, it indicates the times when the process waits for input or an event (e.g. 2097 times);
- _**IVCSW**_ is the number of involuntary switches, it indicates the times when the process is pre-empted or blocked by a mutex (e.g. 6 times);
- The percentage of time spent executing user code and the percentage of time spent executing kernel code in a specific thread (e.g. spent almost 0% of the time executing the user code and 5% executing kernel code in thread &quot;ping&quot;).

<div align=center> <img src="figs/4.3-3.png"></div>