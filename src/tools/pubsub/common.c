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
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <stdarg.h>
#include <math.h>

#include "dds/ddsrt/string.h"

#include "testtype.h"
#include "common.h"

dds_entity_t dp = 0;
dds_entity_t qosprov = 0;
const dds_topic_descriptor_t *ts_KeyedSeq;
const dds_topic_descriptor_t *ts_Keyed32;
const dds_topic_descriptor_t *ts_Keyed64;
const dds_topic_descriptor_t *ts_Keyed128;
const dds_topic_descriptor_t *ts_Keyed256;
const dds_topic_descriptor_t *ts_OneULong;

const char *saved_argv0;

//void nowll_as_ddstime(DDS_Time_t *t) {
//    os_time ost = os_timeGet();
//    t->sec = ost.tv_sec;
//    t->nanosec = (DDS_unsigned_long) ost.tv_nsec;
//}
//
//void bindelta(unsigned long long *bins, unsigned long long d, unsigned repeat) {
//    int bin = 0;
//    while (d) {
//        bin++;
//        d >>= 1;
//    }
//    bins[bin] += repeat;
//}
//
//void binprint(unsigned long long *bins, unsigned long long telapsed) {
//    unsigned long long n;
//    unsigned i, minbin = BINS_LENGTH-1, maxbin = 0;
//    n = 0;
//    for (i = 0; i < BINS_LENGTH; i++) {
//        n += bins[i];
//        if (bins[i] && i < minbin)
//            minbin = i;
//        if (bins[i] && i > maxbin)
//            maxbin = i;
//    }
//    printf ("< 2**n | %llu in %.06fs avg %.1f/s\n", n, telapsed * 1e-9, n / (telapsed * 1e-9));
//    for (i = minbin; i <= maxbin; i++) {
//        static const char ats[] = "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@";
//        double pct = 100.0 * (double) bins[i] / n;
//        int nats = (int) ((pct / 100.0) * (sizeof(ats) - 1));
//        printf ("%2d: %6.2f%% %*.*s\n", i, pct, nats, nats, ats);
//    }
//}

struct hist {
    unsigned nbins;
    uint64_t binwidth;
    uint64_t bin0; /* bins are [bin0,bin0+binwidth),[bin0+binwidth,bin0+2*binwidth) */
    uint64_t binN; /* bin0 + nbins*binwidth */
    uint64_t min, max; /* min and max observed since last reset */
    uint64_t under, over; /* < bin0, >= binN */
    uint64_t bins[];
};

struct hist *hist_new(unsigned nbins, uint64_t binwidth, uint64_t bin0) {
    struct hist *h = dds_alloc(sizeof(*h) + nbins * sizeof(*h->bins));
    h->nbins = nbins;
    h->binwidth = binwidth;
    h->bin0 = bin0;
    h->binN = h->bin0 + h->nbins * h->binwidth;
    hist_reset(h);
    return h;
}

void hist_free(struct hist *h) {
    dds_free(h);
}

void hist_reset_minmax(struct hist *h) {
    h->min = UINT64_MAX;
    h->max = 0;
}

void hist_reset(struct hist *h) {
    hist_reset_minmax(h);
    h->under = 0;
    h->over = 0;
    memset(h->bins, 0, h->nbins * sizeof(*h->bins));
}

void hist_record(struct hist *h, uint64_t x, unsigned weight) {
    if (x < h->min)
        h->min = x;
    if (x > h->max)
        h->max = x;
    if (x < h->bin0)
        h->under += weight;
    else if (x >= h->binN)
        h->over += weight;
    else
        h->bins[(x - h->bin0) / h->binwidth] += weight;
}

static void xsnprintf(char *buf, size_t bufsz, size_t *p, const char *fmt, ...) ddsrt_attribute_format_printf(4, 5);

static void xsnprintf(char *buf, size_t bufsz, size_t *p, const char *fmt, ...) {
    if (*p < bufsz) {
        int n;
        va_list ap;
        va_start(ap, fmt);
        n = vsnprintf(buf + *p, bufsz - *p, fmt, ap);
        va_end(ap);
        *p += (size_t)n;
    }
}

void hist_print(struct hist *h, dds_time_t dt, int reset) {
    const size_t l_size = sizeof(char) * h->nbins + 200;
    const size_t hist_size = sizeof(char) * h->nbins + 1;
    char *l = (char *) dds_alloc(l_size);
    char *hist = (char *) dds_alloc(hist_size);
    double dt_s = (double)dt / 1e9, avg;
    uint64_t peak = 0, cnt = h->under + h->over;
    size_t p = 0;
    hist[h->nbins] = 0;
    for (unsigned i = 0; i < h->nbins; i++) {
        cnt += h->bins[i];
        if (h->bins[i] > peak)
            peak = h->bins[i];
    }

    const uint64_t p1 = peak / 100;
    const uint64_t p10 = peak / 10;
    const uint64_t p20 = 1 * peak / 5;
    const uint64_t p40 = 2 * peak / 5;
    const uint64_t p60 = 3 * peak / 5;
    const uint64_t p80 = 4 * peak / 5;
    for (unsigned i = 0; i < h->nbins; i++) {
        if (h->bins[i] == 0) hist[i] = ' ';
        else if (h->bins[i] <= p1) hist[i] = '.';
        else if (h->bins[i] <= p10) hist[i] = '_';
        else if (h->bins[i] <= p20) hist[i] = '-';
        else if (h->bins[i] <= p40) hist[i] = '=';
        else if (h->bins[i] <= p60) hist[i] = 'x';
        else if (h->bins[i] <= p80) hist[i] = 'X';
        else hist[i] = '@';
    }

    avg = (double)cnt / dt_s;
    if (avg < 999.5)
        xsnprintf(l, l_size, &p, "%5.3g", avg);
    else if (avg < 1e6)
        xsnprintf(l, l_size, &p, "%4.3gk", avg / 1e3);
    else
        xsnprintf(l, l_size, &p, "%4.3gM", avg / 1e6);
    xsnprintf(l, l_size, &p, "/s (");

    if (cnt < (uint64_t) 10e3)
        xsnprintf(l, l_size, &p, "%5"PRIu64" ", cnt);
    else if (cnt < (uint64_t) 1e6)
        xsnprintf(l, l_size, &p, "%5.1fk", (double)cnt / 1e3);
    else
        xsnprintf(l, l_size, &p, "%5.1fM", (double)cnt / 1e6);

    xsnprintf(l, l_size, &p, " in %.1fs) ", dt_s);

    if (h->min == UINT64_MAX)
        xsnprintf(l, l_size, &p, " inf ");
    else if (h->min < 1000)
        xsnprintf(l, l_size, &p, "%3"PRIu64"n ", h->min);
    else if (h->min + 500 < 1000000)
        xsnprintf(l, l_size, &p, "%3"PRIu64"u ", (h->min + 500) / 1000);
    else if (h->min + 500000 < 1000000000)
        xsnprintf(l, l_size, &p, "%3"PRIu64"m ", (h->min + 500000) / 1000000);
    else
        xsnprintf(l, l_size, &p, "%3"PRIu64"s ", (h->min + 500000000) / 1000000000);

    if (h->bin0 > 0) {
        int pct = (cnt == 0) ? 0 : 100 * (int) ((h->under + cnt/2) / cnt);
        xsnprintf(l, l_size, &p, "%3d%% ", pct);
    }

    {
        int pct = (cnt == 0) ? 0 : 100 * (int) ((h->over + cnt/2) / cnt);
        xsnprintf(l, l_size, &p, "|%s| %3d%%", hist, pct);
    }

    if (h->max < 1000)
        xsnprintf(l, l_size, &p, " %3"PRIu64"n", h->max);
    else if (h->max + 500 < 1000000)
        xsnprintf(l, l_size, &p, " %3"PRIu64"u", (h->max + 500) / 1000);
    else if (h->max + 500000 < 1000000000)
        xsnprintf(l, l_size, &p, " %3"PRIu64"m", (h->max + 500000) / 1000000);
    else
        xsnprintf(l, l_size, &p, " %3"PRIu64"s", (h->max + 500000000) / 1000000000);

    (void) p;
    puts(l);
    dds_free(l);
    dds_free(hist);
    if (reset)
        hist_reset(h);
}

void error(const char *fmt, ...) ddsrt_attribute_format_printf(1, 2);
void error(const char *fmt, ...) {
    va_list ap;
    fprintf (stderr, "%s: error: ", saved_argv0);
    va_start(ap, fmt);
    vfprintf (stderr, fmt, ap);
    va_end(ap);
    fprintf (stderr, "\n");
}

void save_argv0(const char *argv0) {
    saved_argv0 = argv0;
}

int common_init(const char *argv0) {
    save_argv0(argv0);
    dp = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    error_abort(dp, "dds_create_participant failed");

    ts_KeyedSeq = &KeyedSeq_desc;
    ts_Keyed32 = &Keyed32_desc;
    ts_Keyed64 = &Keyed64_desc;
    ts_Keyed128 = &Keyed128_desc;
    ts_Keyed256 = &Keyed256_desc;
    ts_OneULong = &OneULong_desc;
    return 0;
}

void common_fini(void) {
    dds_return_t rc;
    if (qosprov != 0) {
        rc = dds_delete(qosprov);
        error_report(rc, "dds_delete qosprov failed");
    }
    rc = dds_delete(dp);
    error_report(rc, "dds_delete participant failed");
}

int change_publisher_partitions(dds_entity_t pub, unsigned npartitions, const char *partitions[]) {
    dds_qos_t *qos;
    dds_return_t rc;

    qos = dds_create_qos();
    rc = dds_get_qos(pub, qos);
    if (rc == DDS_RETCODE_OK) {
        dds_qset_partition(qos, npartitions, partitions);
        rc = dds_set_qos(pub, qos);
    }
    dds_delete_qos(qos);
    return rc;
}

int change_subscriber_partitions(dds_entity_t sub, unsigned npartitions, const char *partitions[]) {
    dds_qos_t *qos;
    dds_return_t rc;

    qos = dds_create_qos();
    rc = dds_get_qos(sub, qos);
    if (rc == DDS_RETCODE_OK) {
        dds_qset_partition(qos, npartitions, partitions);
        rc = dds_set_qos(sub, qos);
    }
    dds_delete_qos(qos);
    return rc;
}

static dds_qos_t *get_topic_qos(dds_entity_t tp) {
    dds_qos_t *tQos = dds_create_qos();
    dds_return_t rc = dds_get_qos(tp, tQos);
    error_abort(rc, "dds_qos_get_topic_qos");
    return tQos;
}

dds_qos_t *new_tqos(void) {
    dds_qos_t *q = dds_create_qos();

    /* Not all defaults are those of DCPS: */
    dds_qset_reliability(q, DDS_RELIABILITY_RELIABLE, DDS_SECS(1));
    dds_qset_destination_order(q, DDS_DESTINATIONORDER_BY_SOURCE_TIMESTAMP);
    return q;
}

dds_qos_t *new_rdqos(dds_entity_t tp) {
    dds_qos_t *tQos = get_topic_qos(tp);
    dds_qos_t *qos = dds_create_qos();

    dds_return_t rc = dds_copy_qos(qos, tQos);
    error_abort(rc ,"new_rdqos: dds_copy_qos");
    dds_delete_qos(tQos);
    return qos;
}

dds_qos_t *new_wrqos(dds_entity_t tp) {
    dds_qos_t *tQos = get_topic_qos(tp);
    dds_qos_t *qos = dds_create_qos();

    dds_return_t rc = dds_copy_qos(qos, tQos);
    error_abort(rc ,"new_wrqos: dds_copy_qos");
    dds_delete_qos(tQos);

    /* Not all defaults are those of DCPS: */
    dds_qset_writer_data_lifecycle(qos, false);
    return qos;
}

dds_entity_t new_topic(const char *name, const dds_topic_descriptor_t *topicDesc, const dds_qos_t *q) {
    dds_entity_t tp = dds_create_topic(dp, topicDesc, name, q, NULL);
    error_abort(tp, "dds_create_topic failed");
    return tp;
}

dds_entity_t new_publisher(dds_qos_t *q, unsigned npartitions, const char **partitions) {
    dds_qos_t *pQos;
    if (q == NULL) {
        pQos = dds_create_qos();
    } else {
        pQos = q;
    }
    dds_qset_partition(pQos, npartitions, partitions);
    dds_entity_t pub = dds_create_publisher(dp, pQos, NULL);
    error_abort(pub, "new_publisher: dds_create_publisher");
    if (q == NULL)
        dds_delete_qos(pQos);
    return pub;
}

dds_entity_t new_subscriber(dds_qos_t *q, unsigned npartitions, const char **partitions) {
    dds_qos_t *sQos;
    if (q == NULL) {
        sQos = dds_create_qos();
    } else {
        sQos = q;
    }
    dds_qset_partition(sQos, npartitions, partitions);
    dds_entity_t sub = dds_create_subscriber(dp, sQos, NULL);
    error_abort(sub, "new_subscriber: dds_create_subscriber");
    if (q == NULL)
        dds_delete_qos(sQos);
    return sub;
}

dds_entity_t new_datawriter_listener(const dds_entity_t pub, const dds_entity_t tp, const dds_qos_t *q, const dds_listener_t *l) {
    dds_entity_t wr = dds_create_writer(pub, tp, q, l);
    error_abort(wr, "dds_create_writer failed");
    return wr;
}

dds_entity_t new_datawriter(const dds_entity_t pub, const dds_entity_t tp, const dds_qos_t *q) {
    return new_datawriter_listener(pub, tp, q, NULL);
}

dds_entity_t new_datareader_listener(const dds_entity_t sub, const dds_entity_t tp, const dds_qos_t *q, const dds_listener_t *l) {
    dds_entity_t rd = dds_create_reader(sub, tp, q, l);
    error_abort(rd, "dds_create_reader failed");
    return rd;
}

dds_entity_t new_datareader(const dds_entity_t sub, const dds_entity_t tp, const dds_qos_t *q) {
    return new_datareader_listener(sub, tp, q, NULL);
}

static void inapplicable_qos(dds_entity_kind_t qt, const char *n) {
    const char *en = "?";
    switch (qt) {
    case DDS_KIND_TOPIC: en = "topic"; break;
    case DDS_KIND_PUBLISHER: en = "publisher"; break;
    case DDS_KIND_SUBSCRIBER: en = "subscriber"; break;
    case DDS_KIND_WRITER: en = "writer"; break;
    case DDS_KIND_READER: en = "reader"; break;
    case DDS_KIND_DONTCARE: en = "dontcare"; break;
    case DDS_KIND_PARTICIPANT: en = "participant"; break;
    case DDS_KIND_COND_READ: en = "cond read"; break;
    case DDS_KIND_COND_QUERY: en = "cond query"; break;
    case DDS_KIND_WAITSET: en = "waitset"; break;
    default: en = "?"; break;
    }
    fprintf(stderr, "warning: %s entity ignoring inapplicable QoS \"%s\"\n", en, n);
}

#define   get_qos_T(qt, q, n) ((qt == DDS_KIND_TOPIC)                                                               ? q : (inapplicable_qos((qt), n), (dds_qos_t*)0))
#define   get_qos_R(qt, q, n) ((qt == DDS_KIND_READER)                                                              ? q : (inapplicable_qos((qt), n), (dds_qos_t*)0))
#define   get_qos_W(qt, q, n) ((qt == DDS_KIND_WRITER)                                                              ? q : (inapplicable_qos((qt), n), (dds_qos_t*)0))
#define  get_qos_TW(qt, q, n) ((qt == DDS_KIND_TOPIC)     || (qt == DDS_KIND_WRITER)                                ? q : (inapplicable_qos((qt), n), (dds_qos_t*)0))
#define  get_qos_RW(qt, q, n) ((qt == DDS_KIND_READER)    || (qt == DDS_KIND_WRITER)                                ? q : (inapplicable_qos((qt), n), (dds_qos_t*)0))
#define  get_qos_MRW(qt, q, n) ((qt == DDS_KIND_READER) || (qt == DDS_KIND_WRITER) || (qt == DDS_KIND_PARTICIPANT)  ? q : (inapplicable_qos((qt), n), (dds_qos_t*)0))
#define  get_qos_PS(qt, q, n) ((qt == DDS_KIND_PUBLISHER) || (qt == DDS_KIND_SUBSCRIBER)                            ? q : (inapplicable_qos((qt), n), (dds_qos_t*)0))
#define get_qos_TRW(qt, q, n) ((qt == DDS_KIND_TOPIC)     || (qt == DDS_KIND_READER)     || (qt == DDS_KIND_WRITER) ? q : (inapplicable_qos((qt), n), (dds_qos_t*)0))

void qos_durability(dds_entity_kind_t qt, dds_qos_t *q, const char *arg) {
    dds_qos_t *qp = get_qos_TRW(qt, q, "durability");
    if (qp == NULL)
        return;
    if (strcmp(arg, "v") == 0)
        dds_qset_durability(qp, DDS_DURABILITY_VOLATILE);
    else if (strcmp(arg, "tl") == 0)
        dds_qset_durability(qp, DDS_DURABILITY_TRANSIENT_LOCAL);
    else if (strcmp(arg, "t") == 0)
        dds_qset_durability(qp, DDS_DURABILITY_TRANSIENT);
    else if (strcmp(arg, "p") == 0)
        dds_qset_durability(qp, DDS_DURABILITY_PERSISTENT);
    else
        error_exit("durability qos: %s: invalid\n", arg);
}

void qos_history(dds_entity_kind_t qt, dds_qos_t *q, const char *arg) {
    dds_qos_t *qp = get_qos_TRW(qt, q, "history");
    int hist_depth, pos;
    if (qp == NULL)
        return;
    if (strcmp(arg, "all") == 0) {
        dds_qset_history(qp, DDS_HISTORY_KEEP_ALL, DDS_LENGTH_UNLIMITED);
    } else if (sscanf(arg, "%d%n", &hist_depth, &pos) == 1 && arg[pos] == 0 && hist_depth > 0) {
        dds_qset_history(qp, DDS_HISTORY_KEEP_LAST, hist_depth);
    } else {
        error_exit("history qos: %s: invalid\n", arg);
    }
}

void qos_destination_order(dds_entity_kind_t qt, dds_qos_t *q, const char *arg) {
    dds_qos_t *qp = get_qos_TRW(qt, q, "destination_order");
    if (qp == NULL)
        return;
    if (strcmp(arg, "r") == 0) {
        dds_qset_destination_order(qp, DDS_DESTINATIONORDER_BY_RECEPTION_TIMESTAMP);
    } else if (strcmp(arg, "s") == 0) {
        dds_qset_destination_order(qp, DDS_DESTINATIONORDER_BY_SOURCE_TIMESTAMP);
    } else {
        error_exit("destination order qos: %s: invalid\n", arg);
    }
}

void qos_ownership(dds_entity_kind_t qt, dds_qos_t *q, const char *arg) {
    dds_qos_t *qp = get_qos_TRW(qt, q, "ownership");
    int strength, pos;
    if (qp == NULL)
        return;
    if (strcmp(arg, "s") == 0) {
        dds_qset_ownership(qp, DDS_OWNERSHIP_SHARED);
    } else if (strcmp(arg, "x") == 0) {
        dds_qset_ownership(qp, DDS_OWNERSHIP_EXCLUSIVE);
    } else if (sscanf(arg, "x:%d%n", &strength, &pos) == 1 && arg[pos] == 0) {
        dds_qos_t *qps = get_qos_W(qt, q, "ownership_strength");
        dds_qset_ownership(qp, DDS_OWNERSHIP_EXCLUSIVE);
        if(qps) {
            dds_qset_ownership_strength(qps, strength);
        }
    } else {
        error_exit("ownership qos: %s invalid\n", arg);
    }
}

void qos_transport_priority(dds_entity_kind_t qt, dds_qos_t *q, const char *arg) {
    dds_qos_t *qp = get_qos_W(qt, q, "transport_priority");
    int pos;
    int value;
    if (qp == NULL)
        return;
    if (sscanf(arg, "%d%n", &value, &pos) != 1 || arg[pos] != 0)
        error_exit("transport_priority qos: %s invalid\n", arg);
    dds_qset_transport_priority(qp, value);
}

static unsigned char gethexchar(const char **str) {
    unsigned char v = 0;
    int empty = 1;
    while (**str) {
        switch (**str) {
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            v = (unsigned char) (16 * v + (unsigned char) **str - '0');
            (*str)++;
            break;
        case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
            v = (unsigned char) (16 * v + (unsigned char) **str - 'a' + 10);
            (*str)++;
            break;
        case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
            v = (unsigned char) (16 * v + (unsigned char) **str - 'A' + 10);
            (*str)++;
            break;
        default:
            if (empty)
                error_exit("empty \\x escape");
            goto done;
        }
        empty = 0;
    }
    done:
        return v;
}

static unsigned char getoctchar(const char **str) {
    unsigned char v = 0;
    int nseen = 0;
    while (**str && nseen < 3) {
        if (**str >= '0' && **str <= '7') {
            v = (unsigned char) (8 * v + (unsigned char) **str - '0');
            (*str)++;
            nseen++;
        } else {
            if (nseen == 0)
                error_exit("empty \\ooo escape");
            break;
        }
    }
    return v;
}

static void *unescape(const char *str, size_t *len) {
    /* results in a blob without explicit terminator, i.e., can't get
     * any longer than strlen(str) */
    unsigned char *x = dds_alloc(strlen(str)), *p = x;
    while (*str) {
        if (*str != '\\')
            *p++ = (unsigned char) *str++;
        else {
            str++;
            switch (*str) {
            case '\\': case ',': case '\'': case '"': case '?':
                *p++ = (unsigned char) *str;
                str++;
                break;
            case 'x':
                str++;
                *p++ = gethexchar(&str);
                break;
            case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7':
                *p++ = getoctchar(&str);
                break;
            case 'a': *p++ = '\a'; str++; break;
            case 'b': *p++ = '\b'; str++; break;
            case 'f': *p++ = '\f'; str++; break;
            case 'n': *p++ = '\n'; str++; break;
            case 'r': *p++ = '\r'; str++; break;
            case 't': *p++ = '\t'; str++; break;
            case 'v': *p++ = '\v'; str++; break;
            case 'e': *p++ = 0x1b; str++; break;
            default:
                error_exit("invalid escape string: %s\n", str);
                break;
            }
        }
    }
    *len = (size_t) (p - x);
    return x;
}

void qos_user_data(dds_entity_kind_t qt, dds_qos_t *q, const char *arg) {
    dds_qos_t *qp = get_qos_MRW(qt, q, "user_data");
    size_t len;
    if (qp == NULL)
        return;

    void *unesc = unescape(arg, &len);
    if(len==0) {
        dds_qset_userdata(qp, NULL, 0);
    } else {
        dds_qset_userdata(qp, unesc, len);
    }

    dds_free(unesc);
}

int double_to_dds_duration(dds_duration_t *dd, double d) {
    if (d < 0)
        return -1;
    double nanosec = d * 1e9;
    if(nanosec > (double)INT64_MAX) {
        *dd = DDS_INFINITY;
    } else {
        *dd = (int64_t) nanosec;
    }
    return 0;
}

void set_infinite_dds_duration(dds_duration_t *dd) {
    *dd = DDS_INFINITY;
}

void qos_reliability(dds_entity_kind_t qt, dds_qos_t *q, const char *arg) {
    dds_qos_t *qp = get_qos_TRW(qt, q, "reliability");
    const char *argp = arg;
    dds_duration_t max_block_t = DDS_MSECS(100);

    if (qp == NULL)
        return;

    switch (*argp++) {
    case 'b':
    case 'n':
        dds_qset_reliability(qp, DDS_RELIABILITY_BEST_EFFORT, max_block_t);
        break;
    case 'r':
    case 'y':
        if (*argp == ':') {
            double max_blocking_time;
            int pos;
            if (strcmp(argp, ":inf") == 0) {
                set_infinite_dds_duration(&max_block_t);
                argp += 4;
            } else if (sscanf(argp, ":%lf%n", &max_blocking_time, &pos) == 1 && argp[pos] == 0) {
                if (max_blocking_time <= 0 || double_to_dds_duration(&max_block_t, max_blocking_time) < 0)
                    error_exit("reliability qos: %s: max blocking time out of range\n", arg);
                argp += pos;
            } else {
                error_exit("reliability qos: %s: invalid max_blocking_time\n", arg);
            }
        }
        dds_qset_reliability(qp, DDS_RELIABILITY_RELIABLE, max_block_t);
        break;
    default:
        error_exit("reliability qos: %s: invalid\n", arg);
    }

    if (*argp != 0) {
        error_exit("reliability qos: %s: invalid\n", arg);
    }
}

void qos_liveliness(dds_entity_kind_t qt, dds_qos_t *q, const char *arg) {
    dds_duration_t dd = 0;
    dds_qos_t *qp = get_qos_TRW(qt, q, "liveliness");
    double lease_duration;
    int pos;

    if (qp == NULL)
        return;

    if (strcmp(arg, "a") == 0) {
        dds_qset_liveliness(qp, DDS_LIVELINESS_AUTOMATIC, DDS_INFINITY);
    } else if (sscanf(arg, "p:%lf%n", &lease_duration, &pos) == 1 && arg[pos] == 0) {
        if (lease_duration <= 0 || double_to_dds_duration(&dd, lease_duration) < 0)
            error_exit("liveliness qos: %s: lease duration out of range\n", arg);
        dds_qset_liveliness(qp, DDS_LIVELINESS_MANUAL_BY_PARTICIPANT, dd);
    } else if (sscanf(arg, "w:%lf%n", &lease_duration, &pos) == 1 && arg[pos] == 0) {
        if (lease_duration <= 0 || double_to_dds_duration(&dd, lease_duration) < 0)
            error_exit("liveliness qos: %s: lease duration out of range\n", arg);
        dds_qset_liveliness(qp, DDS_LIVELINESS_MANUAL_BY_TOPIC, dd);
    } else {
        error_exit("liveliness qos: %s: invalid\n", arg);
    }
}

static void qos_simple_duration(dds_duration_t *dd, const char *name, const char *arg) {
    double duration;
    int pos;
    if (strcmp(arg, "inf") == 0) {
        set_infinite_dds_duration(dd);
    } else if (sscanf(arg, "%lf%n", &duration, &pos) == 1 && arg[pos] == 0) {
        if (double_to_dds_duration(dd, duration) < 0)
            error_exit("%s qos: %s: duration invalid\n", name, arg);
    } else {
        error_exit("%s qos: %s: invalid\n", name, arg);
    }
}

void qos_latency_budget(dds_entity_kind_t qt, dds_qos_t *q, const char *arg) {
    dds_qos_t *qp = get_qos_TRW(qt, q, "latency_budget");
    dds_duration_t duration = 0;
    if (qp == NULL)
        return;
    qos_simple_duration(&duration, "latency_budget", arg);
    dds_qset_latency_budget(qp, duration);
}

void qos_deadline(dds_entity_kind_t qt, dds_qos_t *q, const char *arg) {
    dds_qos_t *qp = get_qos_TRW(qt, q, "deadline");
    dds_duration_t deadline = 0;
    if (qp == NULL)
        return;
    qos_simple_duration(&deadline, "deadline", arg);
    dds_qset_deadline(qp, deadline);
}

void qos_lifespan(dds_entity_kind_t qt, dds_qos_t *q, const char *arg) {
    dds_qos_t *qp = get_qos_TW(qt, q, "lifespan");
    dds_duration_t duration = 0;
    if (qp == NULL)
        return;
    qos_simple_duration(&duration, "lifespan", arg);
    dds_qset_lifespan(qp, duration);
}

static int one_resource_limit(int32_t *val, const char **arg) {
    int pos;
    if (strncmp(*arg, "inf", 3) == 0) {
        *val = DDS_LENGTH_UNLIMITED;
        (*arg) += 3;
        return 1;
    } else if (sscanf(*arg, "%"PRId32"%n", val, &pos) == 1) {
        (*arg) += pos;
        return 1;
    } else {
        return 0;
    }
}

void qos_resource_limits(dds_entity_kind_t qt, dds_qos_t *q, const char *arg) {
    dds_qos_t *qp = get_qos_TRW(qt, q, "resource_limits");
    const char *argp = arg;
    int32_t max_samples = 0;
    int32_t max_instances = 0;
    int32_t max_samples_per_instance = 0;
    if (qp == NULL)
        return;

    if (!one_resource_limit(&max_samples, &argp))
        goto err;
    if (*argp++ != '/')
        goto err;
    if (!one_resource_limit(&max_instances, &argp))
        goto err;
    if (*argp++ != '/')
        goto err;
    if (!one_resource_limit(&max_samples_per_instance, &argp))
        goto err;

    dds_qset_resource_limits(qp, max_samples, max_instances, max_samples_per_instance);

    if (*argp != 0)
        goto err;
    return;

    err:
        error_exit("resource limits qos: %s: invalid\n", arg);
}

void qos_durability_service(dds_entity_kind_t qt, dds_qos_t *q, const char *arg) {
    dds_qos_t *qp = get_qos_T(qt, q, "durability_service");
    const char *argp = arg;
    double service_cleanup_delay_t;
    int pos, hist_depth;
    dds_duration_t service_cleanup_delay = 0;
    dds_history_kind_t history_kind = DDS_HISTORY_KEEP_LAST;
    int32_t history_depth = 1;
    int32_t max_samples = DDS_LENGTH_UNLIMITED;
    int32_t max_instances = DDS_LENGTH_UNLIMITED;
    int32_t max_samples_per_instance = DDS_LENGTH_UNLIMITED;

    if (qp == NULL)
        return;

    argp = arg;
    if (strncmp(argp, "inf", 3) == 0) {
        set_infinite_dds_duration(&service_cleanup_delay);
        pos = 3;
    } else if (sscanf(argp, "%lf%n", &service_cleanup_delay_t, &pos) == 1) {
        if (service_cleanup_delay_t < 0 || double_to_dds_duration(&service_cleanup_delay, service_cleanup_delay_t) < 0)
            error_exit("durability service qos: %s: service cleanup delay out of range\n", arg);
    } else {
        goto err;
    }

    if (argp[pos] == 0) {
        dds_qset_durability_service(qp, service_cleanup_delay, history_kind, history_depth, max_samples, max_instances, max_samples_per_instance);
        return;
    } else if (argp[pos] != '/') goto err;
    argp += pos + 1;

    if (strncmp(argp, "all", 3) == 0) {
        history_kind = DDS_HISTORY_KEEP_ALL;
        pos = 3;
    } else if (sscanf(argp, "%d%n", &hist_depth, &pos) == 1 && hist_depth > 0) {
        history_depth = hist_depth;
    } else {
        goto err;
    }

    if (argp[pos] == 0) {
        dds_qset_durability_service(qp, service_cleanup_delay, history_kind, history_depth, max_samples, max_instances, max_samples_per_instance);
        return;
    } else if (argp[pos] != '/') goto err;
    argp += pos + 1;

    if (!one_resource_limit(&max_samples, &argp))
        goto err;
    if (*argp++ != '/')
        goto err;
    if (!one_resource_limit(&max_instances, &argp))
        goto err;
    if (*argp++ != '/')
        goto err;
    if (!one_resource_limit(&max_samples_per_instance, &argp))
        goto err;

    dds_qset_durability_service(qp, service_cleanup_delay, history_kind, history_depth, max_samples, max_instances, max_samples_per_instance);

    if (*argp != 0)
        goto err;
    return;

    err:
        error_exit("durability service qos: %s: invalid\n", arg);
}

void qos_presentation(dds_entity_kind_t qt, dds_qos_t *q, const char *arg) {
    dds_qos_t *qp = get_qos_PS(qt, q, "presentation");
    if (qp == NULL)
        return;
    if (strcmp(arg, "i") == 0) {
        dds_qset_presentation(qp, DDS_PRESENTATION_INSTANCE, 0, 0);
    } else if (strcmp(arg, "t") == 0) {
        dds_qset_presentation(qp, DDS_PRESENTATION_TOPIC, 1, 0);
    } else if (strcmp(arg, "g") == 0) {
        dds_qset_presentation(qp, DDS_PRESENTATION_GROUP, 1, 0);
    } else {
        error_exit("presentation qos: %s: invalid\n", arg);
    }
}

void qos_autodispose_unregistered_instances(dds_entity_kind_t qt, dds_qos_t *q, const char *arg) {
    dds_qos_t *qp = get_qos_W(qt, q, "writer_data_lifecycle");
    if (qp == NULL)
        return;
    if (strcmp(arg, "n") == 0)
        dds_qset_writer_data_lifecycle(qp, false);
    else if (strcmp(arg, "y") == 0)
        dds_qset_writer_data_lifecycle(qp, true);
    else
        error_exit("autodispose_unregistered_instances qos: %s: invalid\n", arg);
}

const char *qos_arg_usagestr = "\
QOS (not all are universally applicable):\n\
    A={a|p:S|w:S}     liveliness (automatic, participant or writer, S in seconds)\n\
    d={v|tl|t|p}      durability (default: v)\n\
    D=P               deadline P in seconds (default: inf)\n\
    k={all|N}         KEEP_ALL or KEEP_LAST N\n\
    l=D               latency budget in seconds (default: 0)\n\
    L=D               lifespan in seconds (default: inf)\n\
    o=[r|s]           order by reception or source timestamp (default: s)\n\
    O=[s|x[:S]]       ownership: shared or exclusive, strength S (default: s)\n\
    p=PRIO            transport priority (default: 0)\n\
    P={i|t|g}         instance, or {topic|group} + coherent updates\n\
    r={r[:T]|b}       reliability, T is max blocking time in seconds,\n\
                      (default: r:1)\n\
    R=S/I/SpI         resource limits (samples, insts, S/I; default: inf/inf/inf)\n\
    S=C[/H[/S/I/SpI]] durability_service (cleanup delay, history, resource limits)\n\
    u={y|n}           autodispose unregistered instances (default: n)\n\
    U=TEXT            set user_data to TEXT\n\
";

void set_qosprovider(const char *arg) {
    //Todo: There is no qosprovider_create in dds.h, yet
    (void)arg;
//    int result = DDS_RETCODE_OK;
//    const char *p = strchr(arg, ',');
//    const char *xs = strstr(arg, "://");
//    char *profile = NULL;
//    const char *uri;
//    if (p == NULL || xs == NULL || p >= xs)
//        uri = arg;
//    else {
//        uri = p+1;
//        profile = dds_string_dup(arg);
//        profile[p-arg] = 0;
//    }
//
//    if((result = dds_qosprovider_create(&qosprov, uri, profile)) != DDS_RETCODE_OK)
//        error("dds_qosprovider_create(%s,%s) failed\n", uri, profile ? profile : "(null)");
//    dds_free(profile);
}

void setqos_from_args(dds_entity_kind_t qt, dds_qos_t *q, int n, const char *args[]) {
    int i;
    for (i = 0; i < n; i++) {
        char *args_copy = dds_string_dup(args[i]), *cursor = args_copy;
        const char *arg;
        while ((arg = ddsrt_strsep(&cursor, ",")) != NULL) {
            if (arg[0] && arg[1] == '=') {
                const char *a = arg + 2;
                switch (arg[0]) {
                case 'A': qos_liveliness(qt, q, a); break;
                case 'd': qos_durability(qt, q, a); break;
                case 'D': qos_deadline(qt, q, a); break;
                case 'k': qos_history(qt, q, a); break;
                case 'l': qos_latency_budget(qt, q, a); break;
                case 'L': qos_lifespan(qt, q, a); break;
                case 'o': qos_destination_order(qt, q, a); break;
                case 'O': qos_ownership(qt, q, a); break;
                case 'p': qos_transport_priority(qt, q, a); break;
                case 'P': qos_presentation(qt, q, a); break;
                case 'r': qos_reliability(qt, q, a); break;
                case 'R': qos_resource_limits(qt, q, a); break;
                case 'S': qos_durability_service(qt, q, a); break;
                case 'u': qos_autodispose_unregistered_instances(qt, q, a); break;
                case 'U': qos_user_data(qt, q, a); break;
                default:
                    error_exit("%s: unknown QoS\n", arg);
                }
            } else if (!qosprov) {
                error_exit("QoS specification %s requires a QoS provider but none set\n", arg);
            } else {
                printf ("Qos provider not supported\n"); //Todo: Commentted qos provider. Could not find in dds.h. Fix required.
//                int result;
//                if (*arg == 0)
//                    arg = NULL;
//                switch (q->qt) {
//                case QT_TOPIC:
//                    if ((result = dds_qosprovider_get_topic_qos(qosprov, q->u.topic.q, arg)) != DDS_RETCODE_OK)
//                        error ("dds_qosprovider_get_topic_qos(%s): error %d (%s)\n", arg, (int) result, dds_strerror(result));
//                    break;
//                case QT_PUBLISHER:
//                    if ((result = dds_qosprovider_get_publisher_qos(qosprov, q->u.pub.q, arg)) != DDS_RETCODE_OK)
//                        error ("dds_qosprovider_get_publisher_qos(%s): error %d (%s)\n", arg, (int) result, dds_strerror(result));
//                    break;
//                case QT_SUBSCRIBER:
//                    if ((result = dds_qosprovider_get_subscriber_qos(qosprov, q->u.sub.q, arg)) != DDS_RETCODE_OK)
//                        error ("dds_qosprovider_subscriber_qos(%s): error %d (%s)\n", arg, (int) result, dds_strerror(result));
//                    break;
//                case QT_WRITER:
//                    if ((result = dds_qosprovider_get_writer_qos(qosprov, q->u.wr.q, arg)) != DDS_RETCODE_OK)
//                        error ("dds_qosprovider_get_writer_qos(%s): error %d (%s)\n", arg, (int) result, dds_strerror(result));
//                    break;
//                case QT_READER:
//                    if ((result = dds_qosprovider_get_reader_qos(qosprov, q->u.rd.q, arg)) != DDS_RETCODE_OK)
//                        error ("dds_qosprovider_get_reader_qos(%s): error %d (%s)\n", arg, (int) result, dds_strerror(result));
//                    break;
//                }
            }
        }
        dds_free((char *)arg);
        dds_free(args_copy);
    }
}

#define DDS_ERR_MSG_MAX 128

static void dds_fail (const char * msg, const char * where)
{
  fprintf (stderr, "Aborting Failure: %s %s\n", where, msg);
  abort ();
}

bool dds_err_check (dds_return_t err, unsigned flags, const char * where)
{
  if (err < 0)
  {
    if (flags & (DDS_CHECK_REPORT | DDS_CHECK_FAIL))
    {
      char msg[DDS_ERR_MSG_MAX];
      (void) snprintf (msg, DDS_ERR_MSG_MAX, "Error:%s", dds_err_str(err));
      if (flags & DDS_CHECK_REPORT)
      {
        printf ("%s: %s\n", where, msg);
      }
      if (flags & DDS_CHECK_FAIL)
      {
        dds_fail (msg, where);
      }
    }
    if (flags & DDS_CHECK_EXIT)
    {
      exit (-1);
    }
  }
  return (err >= 0);
}

