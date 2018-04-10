/*
 * Copyright(c) 2006 to 2018 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#include "os/os.h"
#include "ddsc/dds.h"
#include "dds_builtinTopics.h"


#define DURATION_INFINITE_SEC 0x7fffffff
#define DURATION_INFINITE_NSEC 0x7fffffff
#define zero(pp,sz) _zero((void**)pp,sz)

// FIXME Temporary workaround for lack of wait_for_historical implementation. Remove this on completion of CHAM-268.
#define dds_reader_wait_for_historical_data(a,b) DDS_SUCCESS; dds_sleepfor(DDS_MSECS(200));

/* Enable DEBUG for printing debug statements*/
//#define DEBUG

#ifdef DEBUG
#define PRINTD printf
#else
#define PRINTD(...)
#endif

#define DCPSTOPIC_FLAG 1
#define DCPSPARTICIPANT_FLAG (1<<1)
#define DCPSSUBSCRIPTION_FLAG (1<<2)
#define DCPSPUBLICATION_FLAG (1<<3)
#define CMPARTICIPANT_FLAG (1<<4)
#define CMPUBLISHER_FLAG (1<<5)
#define CMSUBSCRIBER_FLAG (1<<6)
#define CMDATAREADER_FLAG (1<<7)
#define CMDATAWRITER_FLAG (1<<8)

static struct topictab{
    const char *name;
    const int flag;
} topictab[] = {
        {"dcpstopic", DCPSTOPIC_FLAG},
        {"dcpsparticipant", DCPSPARTICIPANT_FLAG},
        {"dcpssubscription", DCPSSUBSCRIPTION_FLAG},
        {"dcpspublication", DCPSPUBLICATION_FLAG},
        {"cmparticipant", CMPARTICIPANT_FLAG},
        {"cmpublisher", CMPUBLISHER_FLAG},
        {"cmsubscriber", CMSUBSCRIBER_FLAG},
        {"cmdatareader", CMDATAREADER_FLAG},
        {"cmdatawriter", CMDATAWRITER_FLAG},

};
#define TOPICTAB_SIZE (sizeof(topictab)/sizeof(struct topictab))

const unsigned int MAX_SAMPLES = 10;
unsigned int states = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
int status = 0;
int reader_wait = 0;
dds_entity_t participant;
dds_entity_t subscriber;
dds_domainid_t did = DDS_DOMAIN_DEFAULT;
dds_sample_info_t info[10];
dds_qos_t* tqos;
dds_qos_t* sqos;

void _zero(void ** samples, int size) {
    int i;
    for(i = 0; i < size;i++) {
        samples[i] = NULL;
    }
}

int duration_is_infinite(const DDS_Duration_t *d){
    return d->sec == DURATION_INFINITE_SEC && d->nanosec == DURATION_INFINITE_NSEC;
}

void qp_durability (const DDS_DurabilityQosPolicy *q, FILE *fp)
{
    char buf[40];
    char *k;
    switch (q->kind)
    {
    case DDS_VOLATILE_DURABILITY_QOS: k = "volatile"; break;
    case DDS_TRANSIENT_LOCAL_DURABILITY_QOS: k = "transient-local"; break;
    case DDS_TRANSIENT_DURABILITY_QOS: k = "transient"; break;
    case DDS_PERSISTENT_DURABILITY_QOS: k = "persistent"; break;
    default: (void) snprintf (buf, sizeof (buf), "invalid (%d)", (int) q->kind); k = buf; break;
    }
    fprintf (fp, "  durability: kind = %s\n", k);
}

void qp_deadline (const DDS_DeadlineQosPolicy *q, FILE *fp)
{
    fprintf (fp,"  deadline: period = ");
    if (duration_is_infinite(&q->period))
        fprintf (fp, "infinite\n");
    else
        fprintf (fp,"%d.%09u\n", q->period.sec, q->period.nanosec);
}

void qp_latency_budget (const DDS_LatencyBudgetQosPolicy *q, FILE *fp)
{
    fprintf (fp,"  latency_budget: duration = ");
    if (duration_is_infinite (&q->duration))
        fprintf (fp,"infinite\n");
    else
        fprintf (fp,"%d.%09u\n", q->duration.sec, q->duration.nanosec);
}

void qp_liveliness (const DDS_LivelinessQosPolicy *q, FILE *fp)
{
    char buf[40];
    char *k;
    switch (q->kind)
    {
    case DDS_AUTOMATIC_LIVELINESS_QOS: k = "automatic"; break;
    case DDS_MANUAL_BY_PARTICIPANT_LIVELINESS_QOS: k = "manual-by-participant"; break;
    case DDS_MANUAL_BY_TOPIC_LIVELINESS_QOS: k = "manual-by-topic"; break;
    default: (void) snprintf (buf, sizeof (buf), "invalid (%d)", (int) q->kind); k = buf; break;
    }
    fprintf (fp,"  liveliness: kind = %s, lease_duration = ", k);
    if (duration_is_infinite(&q->lease_duration))
        fprintf (fp,"infinite\n");
    else
        fprintf (fp,"%d.%09u\n", q->lease_duration.sec, q->lease_duration.nanosec);
}

void qp_reliability (const DDS_ReliabilityQosPolicy *q, FILE *fp)
{
    char buf[40];
    char *k;
    switch (q->kind)
    {
    case DDS_BEST_EFFORT_RELIABILITY_QOS: k = "best-effort"; break;
    case DDS_RELIABLE_RELIABILITY_QOS: k = "reliable"; break;
    default: (void) snprintf (buf, sizeof (buf), "invalid (%d)", (int) q->kind); k = buf; break;
    }
    fprintf (fp,"  reliability: kind = %s, max_blocking_time = ", k);
    if (duration_is_infinite(&q->max_blocking_time))
        fprintf (fp,"infinite");
    else
        fprintf (fp,"%d.%09u", q->max_blocking_time.sec, q->max_blocking_time.nanosec);
    fprintf (fp,", synchronous = %s\n", q->synchronous ? "true" : "false");
}

void qp_transport_priority (const DDS_TransportPriorityQosPolicy *q, FILE *fp)
{
    fprintf (fp,"  transport_priority: priority = %d\n", q->value);
}

void qp_lifespan (const DDS_LifespanQosPolicy *q, FILE *fp)
{
    fprintf (fp,"  lifespan: duration = ");
    if (duration_is_infinite(&q->duration))
        fprintf (fp, "infinite\n");
    else
        fprintf (fp, "%d.%09u\n", q->duration.sec, q->duration.nanosec);
}

void qp_destination_order (const DDS_DestinationOrderQosPolicy *q, FILE *fp)
{
    char buf[40];
    char *k;
    switch (q->kind)
    {
    case DDS_BY_RECEPTION_TIMESTAMP_DESTINATIONORDER_QOS: k = "by-reception-timestamp"; break;
    case DDS_BY_SOURCE_TIMESTAMP_DESTINATIONORDER_QOS: k = "by-source-timestamp"; break;
    default: (void) snprintf (buf, sizeof (buf), "invalid (%d)", (int) q->kind); k = buf; break;
    }
    fprintf (fp,"  destination_order: kind = %s\n", k);
}

void qp_history_kind_1 (DDS_HistoryQosPolicyKind kind, int indent, FILE *fp)
{
    char buf[40];
    char *k;
    switch (kind)
    {
    case DDS_KEEP_LAST_HISTORY_QOS: k = "keep-last"; break;
    case DDS_KEEP_ALL_HISTORY_QOS: k = "keep-all"; break;
    default: (void) snprintf (buf, sizeof (buf), "invalid (%d)", (int) kind); k = buf; break;
    }
    fprintf (fp,"%*.*shistory: kind = %s", indent, indent, "", k);
}

void qp_history (const DDS_HistoryQosPolicy *q, FILE *fp)
{
    qp_history_kind_1 (q->kind, 2,fp);
    if (q->kind == DDS_KEEP_LAST_HISTORY_QOS)
        fprintf (fp,", depth = %d\n", q->depth);
    else
        fprintf (fp,", (depth = %d)\n", q->depth);
}

void qp_resource_limits_1(int max_samples, int max_instances, int max_samples_per_instance, int indent, FILE *fp)
{
    fprintf (fp,"%*.*sresource_limits: max_samples = ", indent, indent, "");
    if (max_samples == DDS_LENGTH_UNLIMITED)
        fprintf (fp,"unlimited");
    else
        fprintf (fp, "%d", max_samples);
    fprintf (fp, ", max_instances = ");
    if (max_instances == DDS_LENGTH_UNLIMITED)
        fprintf (fp, "unlimited");
    else
        fprintf (fp, "%d", max_instances);
    fprintf (fp,", max_samples_per_instance = ");
    if (max_samples_per_instance == DDS_LENGTH_UNLIMITED)
        fprintf (fp,"unlimited\n");
    else
        fprintf (fp,"%d\n", max_samples_per_instance);
}

void qp_resource_limits (const DDS_ResourceLimitsQosPolicy *q, FILE *fp)
{
    qp_resource_limits_1 (q->max_samples, q->max_instances, q->max_samples_per_instance, 2,fp);
}

void qp_ownership (const DDS_OwnershipQosPolicy *q, FILE *fp)
{
    char buf[40];
    char *k;
    switch (q->kind)
    {
    case DDS_SHARED_OWNERSHIP_QOS: k = "shared"; break;
    case DDS_EXCLUSIVE_OWNERSHIP_QOS: k = "exclusive"; break;
    default: (void) snprintf (buf, sizeof (buf), "invalid (%d)", (int) q->kind); k = buf; break;
    }
    fprintf (fp,"  ownership: kind = %s\n", k);
}

unsigned printable_seq_length(const unsigned char *as, unsigned n)
{
    unsigned i;
    for (i = 0; i < n; i++) {
        if (as[i] < 32 || as[i] >= 127)
            break;
    }
    return i;
}

void print_octetseq (const DDS_octSeq *v, FILE *fp)
{

    unsigned i, n;
    const char *sep = "";
    fprintf (fp, "%u<", v->_length);
    i = 0;
    while (i < v->_length)
    {
        if ((n = printable_seq_length (v->_buffer + i, v->_length - i)) < 4)
        {
            while (n--)
                fprintf (fp,"%s%u", sep, v->_buffer[i++]);
        }
        else
        {
            fprintf (fp, "\"%*.*s\"", n, n, v->_buffer + i);
            i += n;
        }
        sep = ",";
    }
    fprintf (fp,">");
}

void qp_topic_data (const DDS_TopicDataQosPolicy *q, FILE *fp)
{
    fprintf (fp, "  topic_data: value = ");
    print_octetseq (&q->value,fp);
    fprintf (fp, "\n");
}

void qp_user_data (const DDS_UserDataQosPolicy *q, FILE *fp)
{
    fprintf (fp, "  user_data: value = ");
    print_octetseq (&q->value,fp);
    fprintf (fp, "\n");
}

void qp_time_based_filter (const DDS_TimeBasedFilterQosPolicy *q, FILE *fp)
{
    fprintf (fp,"  time_based_filter: minimum_separation = ");
    if (duration_is_infinite (&q->minimum_separation))
        fprintf (fp,"infinite\n");
    else
        fprintf (fp,"%d.%09u\n", q->minimum_separation.sec, q->minimum_separation.nanosec);
}

void qp_presentation (const DDS_PresentationQosPolicy *q, FILE *fp)
{
    char buf[40];
    char *k;
    switch (q->access_scope)
    {
    case DDS_INSTANCE_PRESENTATION_QOS: k = "instance"; break;
    case DDS_TOPIC_PRESENTATION_QOS: k = "topic"; break;
    case DDS_GROUP_PRESENTATION_QOS: k = "group"; break;
    default: (void) snprintf (buf, sizeof (buf), "invalid (%d)", (int) q->access_scope); k = buf; break;
    }
    fprintf (fp, "  presentation: scope = %s, coherent_access = %s, ordered_access = %s\n", k,
            q->coherent_access ? "true" : "false",
                    q->ordered_access ? "true" : "false");
}

void qp_partition (const DDS_PartitionQosPolicy *q, FILE *fp)
{
    const DDS_StringSeq *s = &q->name;
    fprintf (fp, "  partition: name = ");
    if (s->_length == 0)
        fprintf (fp, "(default)");
    else if (s->_length == 1)
        fprintf (fp, "%s", s->_buffer[0]);
    else
    {
        unsigned i;
        fprintf (fp,"{");
        for (i = 0; i < s->_length; i++)
            fprintf (fp, "%s%s", (i > 0) ? "," : "", s->_buffer[i]);
        fprintf (fp,"}");
    }
    fprintf (fp,"\n");
}

void qp_group_data (const DDS_GroupDataQosPolicy *q, FILE *fp)
{
    fprintf (fp,"  group_data: value = ");
    print_octetseq (&q->value,fp);
    fprintf (fp,"\n");
}

void qp_ownership_strength (const DDS_OwnershipStrengthQosPolicy *q, FILE *fp)
{
    fprintf (fp,"  ownership_strength: value = %d\n", q->value);
}

void qp_product_data (const DDS_ProductDataQosPolicy *q, FILE *fp)
{
    fprintf (fp,"  product_data: value = %s\n", q->value);
}

void qp_entity_factory (const DDS_EntityFactoryQosPolicy *q, FILE *fp)
{
    fprintf (fp,"  entity_factory: autoenable_created_entities = %s\n",
            q->autoenable_created_entities ? "true" : "false");
}

void qp_share (const DDS_ShareQosPolicy *q, FILE *fp)
{
    fprintf (fp,"  share: enable = %s", q->enable ? "true" : "false");
    if (q->enable)
        fprintf (fp,", name = %s", q->name);
    fprintf (fp,"\n");
}

void qp_writer_data_lifecycle (const DDS_WriterDataLifecycleQosPolicy *q, FILE *fp)
{
    fprintf (fp,"  writer_data_lifecycle: autodispose_unregistered_instances = %s, autopurge_suspended_samples_delay = ",
            q->autodispose_unregistered_instances ? "true" : "false");
    if (duration_is_infinite (&q->autopurge_suspended_samples_delay))
        fprintf (fp,"infinite");
    else
        fprintf (fp,"%d.%09u", q->autopurge_suspended_samples_delay.sec, q->autopurge_suspended_samples_delay.nanosec);
    fprintf (fp,", autounregister_instance_delay = ");
    if (duration_is_infinite (&q->autounregister_instance_delay))
        fprintf (fp,"infinite\n");
    else
        fprintf (fp,"%d.%09u\n", q->autounregister_instance_delay.sec, q->autounregister_instance_delay.nanosec);
}

void qp_reader_data_lifecycle (const DDS_ReaderDataLifecycleQosPolicy *q, FILE *fp)
{
    fprintf (fp,"  reader_data_lifecycle: autopurge_nowriter_samples_delay = ");
    if (duration_is_infinite (&q->autopurge_nowriter_samples_delay))
        fprintf (fp,"infinite");
    else
        fprintf (fp,"%d.%09u", q->autopurge_nowriter_samples_delay.sec, q->autopurge_nowriter_samples_delay.nanosec);
    fprintf (fp,", autopurge_disposed_samples_delay = ");
    if (duration_is_infinite (&q->autopurge_disposed_samples_delay))
        fprintf (fp,"infinite");
    else
        fprintf (fp,"%d.%09u", q->autopurge_disposed_samples_delay.sec, q->autopurge_disposed_samples_delay.nanosec);
    fprintf (fp,", enable_invalid_samples = %s\n", q->enable_invalid_samples ? "true" : "false");
}

void qp_subscription_keys (const DDS_UserKeyQosPolicy *q, FILE *fp)
{
    fprintf (fp,"  subscription_keys: enable = %s expression = %s\n", q->enable ? "true" : "false", q->expression);
}

void qp_reader_lifespan (const DDS_ReaderLifespanQosPolicy *q, FILE *fp)
{
    fprintf (fp,"  reader_lifespan: use_lifespan = %s, ", q->use_lifespan ? "true" : "false");
    if (!q->use_lifespan)
        fprintf (fp,"(");
    fprintf (fp,"duration = ");
    if (duration_is_infinite (&q->duration))
        fprintf (fp,"infinite");
    else
        fprintf (fp,"%d.%09u", q->duration.sec, q->duration.nanosec);
    if (!q->use_lifespan)
        fprintf (fp,")");
    fprintf (fp,"\n");
}

void print_dcps_topic(FILE *fp){
    DDS_TopicBuiltinTopicData * dcps_topic_samples[10];
    dds_entity_t dcps_topic_reader;
    int i = 0;
    dcps_topic_reader = dds_create_reader(participant, DDS_BUILTIN_TOPIC_DCPSTOPIC, NULL, NULL);
    PRINTD("DCPSTopic Reader Create: %s\n", dds_err_str(dcps_topic_reader));
    reader_wait = dds_reader_wait_for_historical_data(dcps_topic_reader, DDS_SECS(5));
    PRINTD("reader wait status: %d, %s \n", reader_wait, dds_err_str(reader_wait));
    while(true){
        zero(dcps_topic_samples, MAX_SAMPLES);
        status = dds_take_mask(dcps_topic_reader, (void**)dcps_topic_samples, info, MAX_SAMPLES, MAX_SAMPLES, states);
        PRINTD("DDS reading samples returns %d \n", status);
        for(i = 0; i < status; i++) {
            DDS_TopicBuiltinTopicData * data = dcps_topic_samples[i];
            fprintf(fp,"TOPIC:\n");
            fprintf(fp," key = %u:%u:%u \n", (unsigned) data->key[0], (unsigned) data->key[1], (unsigned) data->key[2]);
            fprintf(fp," name = %s\n", data->name);
            fprintf(fp," type_name = %s\n", data->type_name);
            qp_durability (&data->durability,fp);
            qp_deadline (&data->deadline,fp);
            qp_latency_budget (&data->latency_budget,fp);
            qp_liveliness (&data->liveliness,fp);
            qp_reliability (&data->reliability,fp);
            qp_transport_priority (&data->transport_priority,fp);
            qp_lifespan (&data->lifespan,fp);
            qp_destination_order (&data->destination_order,fp);
            qp_history (&data->history,fp);
            qp_resource_limits (&data->resource_limits,fp);
            qp_ownership (&data->ownership,fp);
            qp_topic_data (&data->topic_data,fp);
        }
        if(status > 0) {
            status = dds_return_loan(dcps_topic_reader, (void**)dcps_topic_samples, status);
        }
        if(status <= 0){
            break;
        }
    }
}

void print_dcps_participant(FILE *fp){

    DDS_ParticipantBuiltinTopicData * dcps_participant_samples[10];
    dds_entity_t dcps_participant_reader;
    int i = 0;
    dcps_participant_reader = dds_create_reader(participant, DDS_BUILTIN_TOPIC_DCPSPARTICIPANT, NULL, NULL);
    PRINTD("DCPSSubscription Reader Create: %s\n", dds_err_str(dcps_participant_reader));
    reader_wait = dds_reader_wait_for_historical_data(dcps_participant_reader, DDS_SECS(5));
    PRINTD("reader wait status: %d, %s \n", reader_wait, dds_err_str(reader_wait));
    while(true){
        zero(dcps_participant_samples, MAX_SAMPLES);
        status = dds_take_mask(dcps_participant_reader, (void**)dcps_participant_samples, info, MAX_SAMPLES, MAX_SAMPLES, states);
        PRINTD("DDS reading samples returns %d \n", status);
        for(i = 0; i < status; i++) {
            DDS_ParticipantBuiltinTopicData *data = dcps_participant_samples[i];
            fprintf(fp,"PARTICIPANT:\n");
            fprintf(fp," key = %u:%u:%u \n", (unsigned) data->key[0], (unsigned) data->key[1], (unsigned) data->key[2]);
            qp_user_data(&data->user_data,fp);
        }
        if(status > 0) {
            status = dds_return_loan(dcps_participant_reader, (void**)dcps_participant_samples, status);
        }
        if(status <= 0){
            break;
        }
    }
}

void print_dcps_subscription(FILE *fp){
    DDS_SubscriptionBuiltinTopicData * dcps_subscription_samples[10];
    dds_entity_t dcps_subscription_reader;
    int i = 0;
    dcps_subscription_reader = dds_create_reader(participant, DDS_BUILTIN_TOPIC_DCPSSUBSCRIPTION, NULL, NULL);
    PRINTD("DCPSParticipant Reader Create: %s\n", dds_err_str(dcps_subscription_reader));
    reader_wait = dds_reader_wait_for_historical_data(dcps_subscription_reader, DDS_SECS(5));
    PRINTD("reader wait status: %d, %s \n", reader_wait, dds_err_str(reader_wait));
    while(true){
        zero(dcps_subscription_samples, MAX_SAMPLES);
        status = dds_take_mask(dcps_subscription_reader, (void**)dcps_subscription_samples, info, MAX_SAMPLES, MAX_SAMPLES, states);
        PRINTD("DDS reading samples returns %d \n", status);
        for(i = 0; i < status; i++) {
            DDS_SubscriptionBuiltinTopicData *data = dcps_subscription_samples[i];
            fprintf(fp,"SUBSCRIPTION:\n");
            fprintf(fp," key = %u:%u:%u\n", (uint32_t) data->key[0], (uint32_t) data->key[1], (uint32_t) data->key[2]);
            fprintf(fp," participant_key = %u:%u:%u\n", (uint32_t) data->participant_key[0], (uint32_t) data->participant_key[1], (uint32_t) data->participant_key[2]);
            fprintf(fp," topic_name = %s\n", data->topic_name);
            fprintf(fp," type_name = %s\n", data->type_name);
            qp_durability(&data->durability,fp);
            qp_deadline(&data->deadline,fp);
            qp_latency_budget(&data->latency_budget,fp);
            qp_liveliness(&data->liveliness,fp);
            qp_reliability(&data->reliability,fp);
            qp_ownership(&data->ownership,fp);
            qp_destination_order(&data->destination_order,fp);
            qp_user_data(&data->user_data,fp);
            qp_time_based_filter(&data->time_based_filter,fp);
            qp_presentation(&data->presentation,fp);
            qp_partition(&data->partition,fp);
            qp_topic_data(&data->topic_data,fp);
            qp_group_data(&data->group_data,fp);
        }
        if(status > 0) {
            status = dds_return_loan(dcps_subscription_reader, (void**)dcps_subscription_samples, status);
        }
        if(status <= 0){
            break;
        }
    }
}

void print_dcps_publication(FILE *fp){
    DDS_PublicationBuiltinTopicData * dcps_publication_samples[10];
    dds_entity_t dcps_publication_reader;
    int i = 0;
    dcps_publication_reader = dds_create_reader(participant, DDS_BUILTIN_TOPIC_DCPSPUBLICATION, NULL, NULL);
    PRINTD("DCPSPublication Reader Create: %s\n", dds_err_str(dcps_publication_reader));
    reader_wait = dds_reader_wait_for_historical_data(dcps_publication_reader, DDS_SECS(5));
    PRINTD("reader wait status: %d, %s \n", reader_wait, dds_err_str(reader_wait));
    while(true){
        zero(dcps_publication_samples, MAX_SAMPLES);
        status = dds_take_mask(dcps_publication_reader, (void**)dcps_publication_samples, info, MAX_SAMPLES, MAX_SAMPLES, states);
        PRINTD("DDS reading samples returns %d \n", status);
        for(i = 0; i < status; i++) {
            DDS_PublicationBuiltinTopicData *data = dcps_publication_samples[i];
            fprintf(fp,"PUBLICATION:\n");
            fprintf(fp," key = %u:%u:%u\n", (uint32_t) data->key[0], (uint32_t) data->key[1], (uint32_t) data->key[2]);
            fprintf(fp," participant_key = %u:%u:%u\n", (uint32_t) data->participant_key[0], (uint32_t) data->participant_key[1], (uint32_t) data->participant_key[2]);
            fprintf(fp," topic_name = %s\n", data->topic_name);
            fprintf(fp," type_name = %s\n", data->type_name);
            qp_durability(&data->durability,fp);
            qp_deadline(&data->deadline,fp);
            qp_latency_budget(&data->latency_budget,fp);
            qp_liveliness(&data->liveliness,fp);
            qp_reliability(&data->reliability,fp);
            qp_lifespan (&data->lifespan,fp);
            qp_destination_order (&data->destination_order,fp);
            qp_user_data (&data->user_data,fp);
            qp_ownership (&data->ownership,fp);
            qp_ownership_strength (&data->ownership_strength,fp);
            qp_presentation (&data->presentation,fp);
            qp_partition (&data->partition,fp);
            qp_topic_data (&data->topic_data,fp);
            qp_group_data (&data->group_data,fp);
        }
        if(status > 0) {
            status = dds_return_loan(dcps_publication_reader, (void**)dcps_publication_samples, status);
        }
        if(status <= 0){
            break;
        }
    }
}

void print_cm_participant(FILE *fp){
    DDS_CMParticipantBuiltinTopicData * cm_participant_samples[10];
    dds_entity_t cm_participant_reader;
    int i = 0;
    cm_participant_reader = dds_create_reader(participant, DDS_BUILTIN_TOPIC_CMPARTICIPANT, NULL, NULL);
    PRINTD("CMParticipant Reader Create: %s\n", dds_err_str(cm_participant_reader));
    reader_wait = dds_reader_wait_for_historical_data(cm_participant_reader, DDS_SECS(5));
    PRINTD("reader wait status: %d, %s \n", reader_wait, dds_err_str(reader_wait));
    while(true){
        zero(cm_participant_samples, MAX_SAMPLES);
        status = dds_take_mask(cm_participant_reader, (void**)cm_participant_samples, info, MAX_SAMPLES, MAX_SAMPLES, states);
        PRINTD("DDS reading samples returns %d \n", status);
        for(i = 0; i < status; i++) {
            DDS_CMParticipantBuiltinTopicData *data = cm_participant_samples[i];
            fprintf(fp,"CMPARTICIPANT:\n");
            fprintf(fp," key = %u:%u:%u \n", (unsigned) data->key[0], (unsigned) data->key[1], (unsigned) data->key[2]);
            qp_product_data(&data->product,fp);
        }
        if(status > 0) {
            status = dds_return_loan(cm_participant_reader, (void**)cm_participant_samples, status);
        }
        if(status <= 0){
            break;
        }
    }
}

void print_cm_publisher(FILE *fp){
    DDS_CMPublisherBuiltinTopicData * cm_publisher_samples[10];
    dds_entity_t cm_publisher_reader;
    int i = 0;
    cm_publisher_reader = dds_create_reader(participant, DDS_BUILTIN_TOPIC_CMPUBLISHER, NULL, NULL);
    PRINTD("CMPublisher Reader Create: %s\n", dds_err_str(cm_publisher_reader));
    reader_wait = dds_reader_wait_for_historical_data(cm_publisher_reader, DDS_SECS(5));
    PRINTD("reader wait status: %d, %s \n", reader_wait, dds_err_str(reader_wait));
    while(true){
        zero(cm_publisher_samples, MAX_SAMPLES);
        status = dds_take_mask(cm_publisher_reader, (void**)cm_publisher_samples, info, MAX_SAMPLES, MAX_SAMPLES, states);
        PRINTD("DDS reading samples returns %d \n", status);
        for(i = 0; i < status; i++) {
            DDS_CMPublisherBuiltinTopicData *data = cm_publisher_samples[i];
            fprintf(fp,"CMPUBLISHER:\n");
            fprintf(fp," key = %u:%u:%u \n", (unsigned) data->key[0], (unsigned) data->key[1], (unsigned) data->key[2]);
            fprintf(fp," participant_key = %u:%u:%u\n", (unsigned) data->key[0], (unsigned) data->key[1], (unsigned) data->key[2]);
            fprintf(fp," name = %s\n", data->name);
            qp_entity_factory(&data->entity_factory,fp);
            qp_partition(&data->partition,fp);
            qp_product_data(&data->product,fp);
        }
        if(status > 0) {
            status = dds_return_loan(cm_publisher_reader, (void**)cm_publisher_samples, status);
        }
        if(status <= 0){
            break;
        }
    }
}

void print_cm_subscriber(FILE *fp){
    DDS_CMSubscriberBuiltinTopicData * cm_subscriber_samples[10];
    dds_entity_t cm_subscriber_reader;
    int i = 0;
    cm_subscriber_reader = dds_create_reader(participant, DDS_BUILTIN_TOPIC_CMSUBSCRIBER, NULL, NULL);
    PRINTD("CMSubscriber Reader Create: %s\n", dds_err_str(cm_subscriber_reader));
    reader_wait = dds_reader_wait_for_historical_data(cm_subscriber_reader, DDS_SECS(5));
    PRINTD("reader wait status: %d, %s \n", reader_wait, dds_err_str(reader_wait));
    while(true){
        zero(cm_subscriber_samples, MAX_SAMPLES);
        status = dds_take_mask(cm_subscriber_reader, (void**)cm_subscriber_samples, info, MAX_SAMPLES, MAX_SAMPLES, states);
        PRINTD("DDS reading samples returns %d \n", status);
        for(i = 0; i < status; i++) {
            DDS_CMSubscriberBuiltinTopicData *data = cm_subscriber_samples[i];
            fprintf(fp,"CMSUBSCRIBER:\n");
            fprintf(fp," key = %u:%u:%u \n", (unsigned) data->key[0], (unsigned) data->key[1], (unsigned) data->key[2]);
            fprintf(fp," participant_key = %u:%u:%u\n", (unsigned) data->participant_key[0], (unsigned) data->participant_key[1], (unsigned) data->participant_key[2]);
            fprintf(fp," name = %s\n", data->name);
            qp_entity_factory(&data->entity_factory,fp);
            qp_partition(&data->partition,fp);
            qp_share(&data->share,fp);
            qp_product_data(&data->product,fp);
        }
        if(status > 0) {
            status = dds_return_loan(cm_subscriber_reader, (void**)cm_subscriber_samples, status);
        }
        if(status <= 0){
            break;
        }
    }
}

void print_cm_datawriter(FILE *fp){
    DDS_CMDataWriterBuiltinTopicData * cm_datawriter_samples[10];
    dds_entity_t cm_datawriter_reader;
    int i = 0;
    cm_datawriter_reader = dds_create_reader(participant, DDS_BUILTIN_TOPIC_CMDATAWRITER, NULL, NULL);
    PRINTD("CMDataWriter Reader Create: %s\n", dds_err_str(cm_datawriter_reader));
    reader_wait = dds_reader_wait_for_historical_data(cm_datawriter_reader, DDS_SECS(5));
    PRINTD("reader wait status: %d, %s \n", reader_wait, dds_err_str(reader_wait));
    while(true){
        zero(cm_datawriter_samples, MAX_SAMPLES);
        status = dds_take_mask(cm_datawriter_reader, (void**)cm_datawriter_samples, info, MAX_SAMPLES, MAX_SAMPLES, states);
        PRINTD("DDS reading samples returns %d \n", status);
        for(i = 0; i < status; i++) {
            DDS_CMDataWriterBuiltinTopicData *data = cm_datawriter_samples[i];
            fprintf(fp,"CMDATAWRITER:\n");
            fprintf(fp," key = %u:%u:%u \n", (unsigned) data->key[0], (unsigned) data->key[1], (unsigned) data->key[2]);
            fprintf(fp," publisher_key = %u:%u:%u\n", (unsigned) data->publisher_key[0], (unsigned) data->publisher_key[1], (unsigned) data->publisher_key[2]);
            fprintf(fp," name = %s\n", data->name);
            qp_history (&data->history,fp);
            qp_resource_limits (&data->resource_limits,fp);
            qp_writer_data_lifecycle (&data->writer_data_lifecycle,fp);
            qp_product_data (&data->product,fp);
        }
        if(status > 0) {
            status = dds_return_loan(cm_datawriter_reader, (void**)cm_datawriter_samples, status);
        }
        if(status <= 0){
            break;
        }
    }
}

void print_cm_datareader(FILE *fp){
    DDS_CMDataReaderBuiltinTopicData * cm_datareader_samples[10];
    dds_entity_t cm_datareader_reader;
    int i = 0;
    cm_datareader_reader = dds_create_reader(participant, DDS_BUILTIN_TOPIC_CMDATAREADER, NULL, NULL);
    PRINTD("CMDataReader Reader Create: %s\n", dds_err_str(cm_datareader_reader));
    reader_wait = dds_reader_wait_for_historical_data(cm_datareader_reader, DDS_SECS(5));
    PRINTD("reader wait status: %d, %s \n", reader_wait, dds_err_str(reader_wait));
    while(true){
        zero(cm_datareader_samples, MAX_SAMPLES);
        status = dds_take_mask(cm_datareader_reader, (void**)cm_datareader_samples, info, MAX_SAMPLES, MAX_SAMPLES, states);
        PRINTD("DDS reading samples returns %d \n", status);
        for(i = 0; i < status; i++) {
            DDS_CMDataReaderBuiltinTopicData *data = cm_datareader_samples[i];
            fprintf(fp,"CMDATAREADER:\n");
            fprintf(fp," key = %u:%u:%u \n", (unsigned) data->key[0], (unsigned) data->key[1], (unsigned) data->key[2]);
            fprintf(fp," subscriber_key = %u:%u:%u\n", (unsigned) data->subscriber_key[0], (unsigned) data->subscriber_key[1], (unsigned) data->subscriber_key[2]);
            fprintf(fp," name = %s\n", data->name);
            qp_history (&data->history,fp);
            qp_resource_limits (&data->resource_limits,fp);
            qp_reader_data_lifecycle (&data->reader_data_lifecycle,fp);
            qp_subscription_keys (&data->subscription_keys,fp);
            qp_reader_lifespan (&data->reader_lifespan,fp);
            qp_share (&data->share,fp);
            qp_product_data (&data->product,fp);
        }
        if(status > 0) {
            status = dds_return_loan(cm_datareader_reader, (void**)cm_datareader_samples, status);
        }
        if(status <= 0){
            break;
        }
    }
}

void usage(){
    /*describe the default options*/
    int tpindex;
    printf("\n OPTIONS:\n");
    printf("-f <filename> <topics>    -- write to file\n");
    printf("-a             -- all topics\n");
    printf("\nTOPICS\n");
    for(tpindex = 0; tpindex < TOPICTAB_SIZE; tpindex++){
        printf("%s\n", topictab[tpindex].name);
    }
}

int main(int argc, char **argv){
    FILE *fp = NULL;
    int flags = 0;
    int j;
    int index;
    char *fname = NULL;
    if(argc == 1){
        usage();
    }
    int choice=0;
    while((choice = os_getopt(argc,argv,"f:a")) != -1)
    {
        switch(choice)
        {
        case 'f':
            fname = os_get_optarg();
            if(fname != NULL){

                PRINTD("opening file %s \n", fname);
                fp = fopen(fname, "w");
                if(fp == NULL)
                {
                    printf("file does not exist\n");
                    exit(1);
                }
            }
            break;
        case 'a':
            for(index=0; index<TOPICTAB_SIZE; index++){
                        flags |= topictab[index].flag;
            }
            break;
        default:
            printf("%s Invalid option\n", argv[1]);
            usage();
            break;
        }
    }

    if(fp == NULL){
        fp = stdout;
    }

    for(j = os_get_optind(); j < argc; j++) {
        if(argv[j][0] == '-') {
            // it's an option, don't process it...
            continue;
        }
        int k;
        bool matched = false;
        for(k = 0; k < TOPICTAB_SIZE; k++) {
            if(os_strcasecmp(argv[j], topictab[k].name) == 0) {
                // match
                flags |= topictab[k].flag;
                matched = true;
                break;
            }
        }
        if(!matched) {
            printf("%s: topic unknown\n", argv[j]);
        }
    }

    /* Create the participant with the default domain */
    participant = dds_create_participant(did, NULL, NULL);
    PRINTD("DDS Participant Create: %s\n", dds_err_str(status));

    if(flags & DCPSTOPIC_FLAG) {
        print_dcps_topic(fp);
    }

    if(flags & DCPSPARTICIPANT_FLAG) {
        print_dcps_participant(fp);
    }

    if(flags & DCPSSUBSCRIPTION_FLAG){
        print_dcps_subscription(fp);
    }

    if(flags & DCPSPUBLICATION_FLAG){
        print_dcps_publication(fp);
    }

    if(flags & CMPARTICIPANT_FLAG){
        print_cm_participant(fp);
    }

    if(flags & CMPUBLISHER_FLAG){
        print_cm_publisher(fp);
    }

    if(flags & CMSUBSCRIBER_FLAG){
        print_cm_subscriber(fp);
    }

    if(flags & CMDATAWRITER_FLAG){
        print_cm_datawriter(fp);
    }

    if(flags & CMDATAREADER_FLAG){
        print_cm_datareader(fp);
    }

    dds_delete(participant);
    fclose(fp);
    return 0;
}
