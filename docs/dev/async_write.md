
# asynchronous write

`Asynchronous Write` is a feature where the actual sending of the data packets occurs on a different thread, than the thread where the `dds_write()` function is invoked. By default when application writes data using a DDS datawriter, the entire operation write operation involving the steps from serialization to writing the actual data packets on to the socket happen synchronously in the same thread, this is called `Synchronous Write`. Applications can choose to enable `Asynchronous Write` mode where the `dds_write()` call will only copy the data to the writer history cache and the actual transmission of the data happens asynchronously in a separate thread.

### Current State

Currently, CycloneDDS has an option to configure `Asynchronous Write` using the `SendAsync` configuration option. The below diagram shows the current API call sequence:

<img src="pictures/async_write_cyclone_dds.png" alt="Asynchronous write API sequence">

\note: This doesn't seem to work out of the box

### Improvements/Follow-up

1. Threading model, One thread for each publisher or one thread for each DataWriter?
     1. If there is one thread for each publisher, then this should be shared across all the datawriters of this publisher.
2. How to configure the synchronous/asynchronous write mode?
     1. Does this happen automatically based on some higher level configuration like latency budget? If yes how do we ensure that the latency budget is guaranteed?
     2. Should this be a configuration option for the publisher/datawriter itself? If yes, should this be a QoS option?
     2. The current configuration option `SendAsync`, doesn't have enough fine grain control, since it configures the write mode for the entire system
3. Thread execution policy?
     1. Do we need to provide options for the user to choose threading execution policy? Or do we use a fixed strategy like round-robin
     1. Do we need to provide an option for user to provide a custom trigger on when the write can happen? (rather relying completely on the thread execution policy)
