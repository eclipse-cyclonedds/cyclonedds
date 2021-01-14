## _Hello World!_ Subscriber Source Code

The **Subscriber.c** mainly contains the statements to wait for a _Hello World!_ message and reads it when it receives it.

Not that, the Cyclone DDS read operation sematic keeps the data sample in the Data Reader cache.

Subscriber application basically implements the previous steps defined in section 3.2.

```
1 #include "ddsc/dds.h"
2 #include "HelloWorldData.h"
3 #include <stdio.h>
4 #include <string.h>
5 #include <stdlib.h>
6
7 /* An array of one message (aka sample in dds terms) will be used. */
8 #define MAX_SAMPLES 1
9
10 int main (int argc, char ** argv)
11 {
12	dds_entity_t participant;
13	dds_entity_t topic;
14 	dds_entity_t reader;
15 	HelloWorldData_Msg *msg;
16	void *samples[MAX_SAMPLES];
17	dds_sample_info_t infos[MAX_SAMPLES];
18	dds_return_t ret;
19	dds_qos_t *qos;
20	(void)argc;
21	(void)argv;
22
23	/* Create a Participant. */
24	participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
25	DDS_ERR_CHECK (participant, DDS_CHECK_REPORT | DDS_CHECK_EXIT);
26
27	/* Create a Topic. */
28	topic = dds_create_topic (participant, &HelloWorldData_Msg_desc,
29	"HelloWorldData_Msg", NULL, NULL);
30	DDS_ERR_CHECK (topic, DDS_CHECK_REPORT | DDS_CHECK_EXIT);
31
32	/* Create a reliable Reader. */
33	qos = dds_create_qos ();
34	dds_qset_reliability (qos, DDS_RELIABILITY_RELIABLE, DDS_SECS (10));
35	reader = dds_create_reader (participant, topic, qos, NULL);
36	DDS_ERR_CHECK (reader, DDS_CHECK_REPORT | DDS_CHECK_EXIT);
37	dds_delete_qos(qos);
38
39	printf ("\n=== [Subscriber] Waiting for a sample ...\n");
40
41	/* Initialize sample buffer, by pointing the void pointer within
42	* the buffer array to a valid sample memory location. */
43	samples[0] = HelloWorldData_Msg alloc ();
44
45	/* Poll until data has been read. */
46	while (true)
47	{
48	/* Do the actual read.
49	* The return value contains the number of read samples. */
50	ret = dds_read (reader, samples, infos, MAX_SAMPLES, MAX_SAMPLES);
51	DDS_ERR_CHECK (ret, DDS_CHECK_REPORT | DDS_CHECK_EXIT);
52
53	/* Check if we read some data and it is valid. */
54	if ((ret > 0) && (infos[0].valid_data))
55	{
56	/* Print Message. */
57	msg = (HelloWorldData_Msg*) samples[0];
58	printf ("=== [Subscriber] Received : ");
59	printf ("Message (%d, %s)\n", msg->userID, msg->message);
60	break;
61	}
62	else
63	{
64	/* Polling sleep. */
65	dds_sleepfor (DDS_MSECS (20));
66	}
67	}
68
69	/* Free the data location. */
70	HelloWorldData_Msg_free (samples[0], DDS_FREE_ALL);
71
72	/* Deleting the participant will delete all its children recursively as well. */
73	ret = dds_delete (participant);
74	DDS_ERR_CHECK (ret, DDS_CHECK_REPORT | DDS_CHECK_EXIT);
75
76 	return EXIT_SUCCESS;
77  }
```


Within the subscriber code, we will be mainly using the DDS API and the _`HelloWorldData_Msg`_ type. Therefore, the following header files need to be included:

- The **dds.h** file give access to the DDS APIs ,
- The **HelloWorldData.h** is specific to the data type defined in the IDL as explained earlier.

```
#include "ddsc/dds.h"
#include "HelloWorldData.h"
```


With Cyclone DDS, at least three dds entities are needed to build a minimalistic application, the domain participant, the topic, and the reader. A DDS Subscriber entity is implicitly created by Cyclone DDS. If needed this behavior can be overridden.

```
dds_entity_t participant; 
dds_entity_t topic; 
dds_entity_t reader;
```


To handle the data. some buffers are declared and created: 

```
HelloWorldData_Msg *msg; 
void *samples[MAX_SAMPLES];
dds_sample_info_t info[MAX_SAMPLES];
```

As the `read()` operation may return more than one data sample (in the event that several publishing applications are started simultaneously to write different message instances), an array of samples is therefore needed.

In Cyclone DDS data and metadata are propagated together. The `dds_sample_info` array needs, therefore, to be declared to handle the metadata.

The dds participant is always attached to a specific dds domain. In the _Hello World!_ example, it is part of the _`Default_Domain`_, the one specified in the xml deployment file (see section 1.6 for more details).

```
participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
```

The next step is to create the topic witha given name. Topics with the same data type description and with different names are considered different topics. This means that readers or writers created for a given topic will not interfere with readers or writers created with another topic even though have the same data type.

```
topic = dds_create_topic (participant, &HelloWorldData_Msg_desc, "HelloWorldData_Msg", NULL, NULL);
```


Once the topic is created, we can create a data reader and attach to it. 

```
dds_qos_t *qos = dds_create_qos ();
dds_qset_reliability (qos, DDS_RELIABILITY_RELIABLE, DDS_SECS (10)); 
reader = dds_create_reader (participant, topic, qos, NULL); 
dds_delete_qos(qos);
```

The read operation expects an array of pointers to a valid memory location. This means the samples array needs initialization by pointing the void pointer within the buffer array to a valid sample memory location.

As in our example, we have an array of one element (`#define MAX_SAMPLES 1`.) we only need to allocate memory for one `HelloWorldData_Msg`.

```
samples[0] = HelloWorldData_Msg_alloc ();
```

At this stage, we can attempt to read data by going into a polling loop that will regularly scrutinize and examine the arrival of a message.

```
ret = dds_read (reader, samples, info, MAX_SAMPLES, MAX_SAMPLES);
```


The `dds_read` operation returns the number of samples equal to the parameter `MAX_SAMPLE`. If that number exceeds 0 that means data arrived in the reader's cache.

The Sample_info (`info`) structure will tell whether the data we are reading is _Valid_ or _Invalid_. Valid data means that it contains the payload provided by the publishing application. Invalid data means, that we are rather reading the DDS state of data Instance. The state of a data instance can be for instance _DISPOSED_ by the writer or it is _NOT\_ALIVE_ anymore, which could happen for instance the publisher application terminates while the subscriber is still active. In this case, the sample will not be considered as Valid, and its sample `info[].Valid_data` field will be `False`.

```
if ((ret > 0) && (info[0].valid_data))
```


If data has been read, then we can cast the void pointer to the actual message data type and display the contents.

```
msg = (HelloWorldData_Msg*) samples[0]; 
printf ("=== [Subscriber] Received : ");
printf ("Message (%d, %s)\n", msg->userID, msg->message);
break;
```


When data is received and the polling loop is stopped, we release the allocated memory and delete the domain participant. 

```
HelloWorldData_Msg_free (samples[0], DDS_FREE_ALL); 
dds_delete (participant);
```

All the entities that are created under the participant, such as the Data Reader and Topic, are recursively deleted.