## Measuring Throughput and Latency in a mixed scenario

In some scenarios, it might be useful to measure the throughput and latency at the same time.

The _ddsperf_ tool allows you to mix these two scenarios. To address such cases the Ping mode can be combined with the Pub mode.

The [Ping x%] switch combined with the Pub mode allows you to send a fraction of samples x% as if they were used in the Ping mode.

The different modes of the _ddsperf_ tool are summarized in the figure below.

<div align=center> <img src="figs/4.5-1.png"></div>

You can get more information for the _ddsperf_ tool by using the [help] option:

```
./ddsperf help
```