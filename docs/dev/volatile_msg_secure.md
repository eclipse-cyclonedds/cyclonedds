# ParticipantVolatileMessageSecure Handling

## Short Introduction

It is expected to have some knowledge of DDSI builtin (security) endpoints.

```cpp
#define DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_WRITER 0xff0202c3
#define DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_READER 0xff0202c4
```
These builtin endpoints have caused about the biggest code change in ddsi, regarding security.

Chapters 7.4.4.3 and 7.4.4.4 in the DDS Security specification indicates the main issue why these builtin endpoints are different from all the others and somewhat more complex.

> 7.4.4.3 Contents of the ParticipantVolatileMessageSecure
> The ParticipantVolatileMessageSecure is intended as a holder of secure information that
> is sent point-to-point from a DomainParticipant to another.
>
> [...]
>
> 7.4.4.4 Destination of the ParticipantVolatileMessageSecure
>
> If the destination_participant_guid member is not set to GUID_UNKNOWN, the message written is
> intended only for the BuiltinParticipantVolatileMessageSecureReader belonging to the
> DomainParticipant with a matching Participant Key.
>
> This is equivalent to saying that the BuiltinParticipantVolatileMessageSecureReader has an implied
> content filter with the logical expression:
>
> “destination_participant_guid == GUID_UNKNOWN
> || destination_participant_guid==BuiltinParticipantVolatileMessageSecureReader.participant.guid”
>
> Implementations of the specification can use this content filter or some other mechanism as long as the
> resulting behavior is equivalent to having this filter.
>
> [...]

The "point-to-point" and "content filter" remarks makes everything more elaborate.


## Complexity

It would be nice to be able to use the ```dds_set_topic_filter()``` functionality for these endpoints. However, that only works on the reader history cache (rhc), which is only available for ddsc entities and not for ddsi builtin entities. And it's the builtin entities that are being used.

The ```dds_set_topic_filter()``` basically simulates that the sample was inserted into the rhc (but didn't insert it), which causes the rest of ddsi (regarding heartbeat, acknacks, gaps, etc) to work as normal while the sample just isn't provided to the reader.

Unfortunately, the builtin volatile endpoints can not use that same simple sequence (just handle the sample but ignore it right at the end). Problem is, the sample is encoded. It can only decode samples that are intended for that reader. This would mean that it is best for the reader to only receive 'owned' samples that it can actually decode.

This has all kinds of affects regarding the heartbeat, acknacks, gaps, etc. Basically, every writer/reader combination should have information regarding gaps and sequence numbers between them, while normally such information about proxies are combined.


## Implementation Overview

This only depicts an overview. Some details will have been omitted.


### Writing

The function ```write_crypto_exchange_message()``` takes care of generating the right sample information and pass it on to ```ddsi_write_sample_p2p_wrlock_held()```.

A proxy reader can now have a filter callback function (```proxy_reader::filter```). This indicates (on the writer side) if a sample will be accepted by the actual reader or not. This could be made more generic for proper 'writer side' content filter implementation. However, now it'll only be used by ParticipantVolatileMessageSecure and the filter is hardcoded to ```ddsi_volatile_secure_data_filter()```.

So, if ```ddsi_write_sample_p2p_wrlock_held()``` is called with a proxy reader with a filter, it will get 'send/acked sequences' information between the writer and proxy reader. This is used to determine if gap information has to be send alongside the sample.

Then, ```ddsi_write_sample_p2p_wrlock_held()``` will enqueue the sample.

Just before the submessage is added to the rtps message and send, it is encoded (todo).


### Reading

First things first, the submessage is decoded when the rtps message is received (todo).

It is received on a builtin reader, so the builtin queue is used and ```ddsi_builtins_dqueue_handler()``` is called. That will forward the sample to the token exchange functionality, ignoring every sample that isn't related to the related participant (todo).


### Gaps on reader side

The reader remembers the last_seq it knows from every connected proxy writer (```pwr_rd_match::last_seq```).
This is updated when handling heartbeats, gaps and regular messages and used to check if there are gaps.
Normally, the ```last_seq``` of a specific writer is used here. But when the reader knows that the writer uses a 'writer side content filter' (```proxy_writer::uses_filter```), it'll use the the ```last_seq``` that is related to the actual reader/writer match.
It is used to generate the AckNack (which contains gap information) response to the writer.


### Gaps on writer side

The writer remembers which sample sequence it send the last to a specific reader through ```wr_prd_match::lst_seq```.
This is used to determine if a reader has received all relevant samples (through handling of acknack).
It is also used to determine the gap information that is added to samples to a specific reader when necessary.


### Heartbeats

A writer is triggered to send heartbeats once in a while. Normally, that is broadcasted. But, for the volatile secure writer, it has to be send to each reader specifically. The heartbeat submessage that is send to each reader individually is encoded with a reader specific key. This key is generated from the shared secret which was determined during the authentication phase.

When a writer should send heartbeats, ```handle_xevk_heartbeat()``` is called. For the volatile secure writer, the control is immediately submitted to ```send_heartbeat_to_all_readers()```. This will add heartbeat submessages to an rtps message for every reader it deems necessary.


### Reorder

Normally received samples are placed in the reorder administration of the proxy_writer. However in this case the writer applies a content filter which is specific for each destinated reader. In that case the common reorder administration in the proxy_writer can not be used and the reader specific reorder administration must be used to handle the gap's which will be reader specific.

</br>
</br>
</br>
=================================================</br>
Notes</br>
=================================================</br>

### Trying to put the security participant volatile endpoint implementation into context.

The following elements are added to the data structures:

* struct ddsi_wr_prd_match::lst_seq     : Highest seq send to this reader used when filter is applied
* struct ddsi_pwr_rd_match::last_seq    : Reader specific last received sequence number from the writer.
* struct ddsi_proxy_writer::uses_filter : Indicates that a content-filter is active
* struct ddsi_proxy_reader::filter      : The filter to apply for this specific reader

Functions added:

* writer_hbcontrol_p2p : This function creates a heartbeat destined for a specific reader. The volatile secure writer will use an submessage encoding which uses a distinct key for each reader. Therefor a reader specific heartbeat is needed.
* ddsi_defrag_prune : When a volatile secure reader is deleted then the defragmentation administration could still contain messages destined for this reader. This function removes these messages from the defragmentation administration.
* ddsi_volatile_secure_data_filter : The filter applied to the secure volatile messages which filters on the destination participant guid.
* ddsi_write_sample_p2p_wrlock_held : This function writes a message to a particular reader.

The use of the content-filter for the volatile secure writer implies that for each destination reader which message from the writer history cache is valid and had to be sent.
For messages that do not match this filter a GAP message should be sent to the reader. Each time a message is sent to a specific reader a possible gap message is added.
For the volatile secure writer the sequence number of the last message send to a particular reader is maintained in ```wr_prd_match::lst_seq'''. It is used to determine if a
HEARTBEAT has to send to this particular reader or that the reader has acknowledged all messages. At the reader side the sequence number of the last received message is
maintained in ```pwr_rd_match::last_seq'''. It is used to determine the contents of the ACKNACK message as response to a received HEARTBEAT.
When an ACKNACK (handle_AckNack) is received it is determined which samples should be resent related to the applied filter and for which sequence numbers a GAP message should be sent.
