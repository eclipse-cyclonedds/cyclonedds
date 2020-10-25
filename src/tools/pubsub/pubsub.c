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
#ifdef __APPLE__
#define USE_EDITLINE 0
#endif

#define _ISOC99_SOURCE
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <limits.h>
#include <ctype.h>
#include <sys/types.h>
#include <errno.h>
#include <getopt.h>

#if USE_EDITLINE
#include <histedit.h>
#endif

#include "common.h"
#include "testtype.h"
#include "tglib.h"
#include "porting.h"

#include "dds/ddsrt/environ.h"
#include "dds/ddsrt/process.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/strtol.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/threads.h"

//#define NUMSTR "0123456789"
//#define HOSTNAMESTR "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ-." NUMSTR

typedef dds_return_t (*write_oper_t) (dds_entity_t wr, const void *d, const dds_time_t ts);

enum topicsel { UNSPEC, KS, K32, K64, K128, K256, OU, ARB };
enum readermode { MODE_PRINT, MODE_CHECK, MODE_ZEROLOAD, MODE_DUMP, MODE_NONE };

#define PM_PID 1u
#define PM_TOPIC 2u
#define PM_TIME 4u
#define PM_IHANDLE 8u
#define PM_PHANDLE 16u
#define PM_STIME 32u
#define PM_DGEN 64u
#define PM_NWGEN 128u
#define PM_RANKS 256u
#define PM_STATE 512u

static volatile sig_atomic_t termflag = 0;
static int flushflag = 0;
static int pid;
static dds_entity_t termcond;
static unsigned nkeyvals = 1;
static int once_mode = 0;
static int wait_hist_data = 0;
static dds_duration_t wait_hist_data_timeout = 0;
static double dur = 0.0;
//static int sigpipe[2]; // TODO signal handling support
//static int termpipe[2];
//static int fdin = 0; // TODO ARB type support
static enum tgprint_mode printmode = TGPM_FIELDS;
static unsigned print_metadata = PM_STATE;
static int printtype = 0;

struct tstamp_t {
    int isabs;
    int64_t t;
};

struct readerspec {
    dds_entity_t rd;
    dds_entity_t sub;
    enum topicsel topicsel;
    struct tgtopic *tgtp;
    enum readermode mode;
    int use_take;
    dds_duration_t sleep_ns;
    int polling;
    uint32_t read_maxsamples;
    int print_match_pre_read;
    unsigned idx;
};

enum writermode {
    WRM_NONE,
    WRM_AUTO,
    WRM_INPUT
};

struct writerspec {
    dds_entity_t wr;
    dds_entity_t dupwr;
    dds_entity_t pub;
    enum topicsel topicsel;
    char *tpname;
    struct tgtopic *tgtp;
    double writerate;
    unsigned baggagesize;
    int register_instances;
    int duplicate_writer_flag;
    unsigned burstsize;
    enum writermode mode;
};

static const struct readerspec def_readerspec = {
    .rd = 0,
    .sub = 0,
    .topicsel = UNSPEC,
    .tgtp = NULL,
    .mode = MODE_PRINT,
    .use_take = 1,
    .sleep_ns = 0,
    .polling = 0,
    .read_maxsamples = INT16_MAX,
    .print_match_pre_read = 0
};

static const struct writerspec def_writerspec = {
    .wr = 0,
    .dupwr = 0,
    .pub = 0,
    .topicsel = UNSPEC,
    .tpname = NULL,
    .tgtp = NULL,
    .writerate = 0.0,
    .baggagesize = 0,
    .register_instances = 0,
    .duplicate_writer_flag = 0,
    .burstsize = 1,
    .mode = WRM_INPUT
};

struct wrspeclist {
    struct writerspec *spec;
    struct wrspeclist *prev, *next; /* circular */
};

static ddsrt_mutex_t output_mutex;

static void terminate(void) {
//    const char c = 0;
    termflag = 1;
//    os_write(termpipe[1], &c, 1); // TODO: signal handling support; for abstraction layer
    dds_waitset_set_trigger(termcond, true);
}

static void usage(const char *argv0) {
    fprintf (stderr, "\
usage: %s [OPTIONS] PARTITION...\n\
\n\
OPTIONS:\n\
    -T TOPIC        set topic name to TOPIC\n\
                    specifying a topic name when one has already been given\n\
                    introduces a new reader/writer pair\n\
    -K TYPE         select type (KS is default),\n\
                    (default topic name in parenthesis):\n\
                        KS                - key, seq, octet sequence (PubSub)\n\
                        K32,K64,K128,K256 - key, seq, octet array (PubSub<N>)\n\
                        OU                - one ulong, keyless (PubSubOU)\n\
                    specifying a type when one has already been given introduces\n\
                    a new reader/writer pair\n\
    -q FS:QOS       set QoS for entities indicated by FS, which must be one or\n\
                    more of: t (topic), p (publisher), s (subscriber),\n\
                    w (writer), r (reader), or a (all of them). For QoS syntax,\n\
                    see below. Inapplicable QoS's are ignored.\n\
    -m [0|p[p]|c[p][:N]|z|d[p]]  no reader, print values, check sequence numbers\n\
                    (expecting N keys), \"zero-load\" mode or \"dump\" mode (which\n\
                    is differs from \"print\" primarily because it uses a data-\n\
                    available trigger and reads all samples in read-mode(default:\n\
                    p; pp, cp, dp are polling modes); set per-reader\n\
    -D DUR          run for DUR seconds\n\
    -n N            limit take/read to N samples\n\
    -O              take/read once then exit 0 if samples present, or 1 if not\n\
    -P MODES        printing control (prefixing with \"no\" disables):\n\
                        meta           enable printing of all metadata\n\
                        trad           pid, time, phandle, stime, state\n\
                        pid            process id of pubsub\n\
                        topic          which topic (topic is def. for multi-topic)\n\
                        time           read time relative to program start\n\
                        phandle        publication handle\n\
                        ihandle        instance handle\n\
                        stime          source timestamp\n\
                        rtime          reception timestamp\n\
                        dgen           disposed generation count\n\
                        nwgen          no-writers generation count\n\
                        ranks          sample, generation, absolute generation ranks\n\
                        state          instance/sample/view states\n\
                    additionally, the following have effect for sample data values:\n\
                        dense          no additional white space, no field names\n\
                        fields         field names, some white space\n\
                        multiline      field names, one field per line\n\
                    default is \"nometa,state,fields\".\n\
                    For K* types, the .baggage field is omitted from the data output\n\
    -r              register instances (-wN mode only)\n\
    -R              use 'read' instead of 'take'\n\
    -s MS           sleep MS ms after each read/take (default: 0)\n\
    -w F            writer mode/input selection, F:\n\
                        -     stdin (default)\n\
                        N     cycle through N keys as fast as possible\n\
                        N:R*B cycle through N keys at R bursts/second, each burst\n\
                            consisting of B samples\n\
                        N:R   as above, B=1\n\
                    no writer is created if -w0 and no writer listener\n\
                    automatic specifications can be given per writer; final\n\
                    interactive specification determines input used for non-\n\
                    automatic ones\n\
    -S EVENTS       monitor status events (comma separated; default: none)\n\
                    reader (abbreviated and full form):\n\
                        pr   pre-read (virtual event)\n\
                        sl   sample-lost\n\
                        sr   sample-rejected\n\
                        lc   liveliness-changed\n\
                        sm   subscription-matched\n\
                        riq  requested-incompatible-qos\n\
                        rdm  requested-deadline-missed\n\
                    writer:\n\
                        ll   liveliness-lost\n\
                        pm   publication-matched\n\
                        oiq  offered-incompatible-qos\n\
                        odm  offered-deadline-missed\n\
    -z N            topic size (affects KeyedSeq only)\n\
    -@              echo everything on duplicate writer (only for interactive)\n\
    -* N            sleep for N seconds just before returning from main()\n\
\n\
%s\n\
Note: defaults above are overridden as follows:\n\
    r:k=all,R=10000/inf/inf\n\
    w:k=all,R=100/inf/inf\n\
\n\
Input format is a white-space separated sequence (K* and OU, newline\n\
separated for ARB) of:\n\
    N     write next sample, key value N\n\
    wN    synonym for plain N; w@T N same with timestamp of T\n\
          T is absolute of prefixed with \"=\", T currently in seconds\n\
    dN    dispose, key value N; d@T N as above\n\
    DN    write dispose, key value N; D@T N as above\n\
    uN    unregister, key value N; u@T N as above\n\
    rN    register, key value N; u@T N as above\n\
    sN    sleep for N seconds\n\
    zN    set topic size to N (affects KeyedSeq only)\n\
Note: for K*, OU types, in the above N is always a decimal\n\
integer (possibly negative); because the OneULong type has no key\n\
the actual key value is irrelevant. For ARB types, N must be a\n\
valid initializer. X must always be a list of names.\n\
\n\
PARTITION:\n\
If partition name contains spaces, then wrap it inside quotation marks.\n\
Use \"\" for default partition.\n",
            argv0, qos_arg_usagestr);
    exit (1);
}

static unsigned split_partitions(const char ***p_ps, char **p_bufcopy, const char *buf) {
    const char *b;
    const char **ps;
    char *bufcopy, *bc;
    unsigned i, nps;
    nps = 1;
    for (b = buf; *b; b++) {
        nps += (*b == ',');
    }
    ps = dds_alloc(nps * sizeof(*ps));
    bufcopy = ddsrt_expand_envvars_sh(buf, 0);
    i = 0; bc = bufcopy;
    while (1) {
        ps[i++] = bc;
        while (*bc && *bc != ',') bc++;
        if (*bc == 0) break;
        *bc++ = 0;
    }
    assert(i == nps);
    *p_ps = ps;
    *p_bufcopy = bufcopy;
    return nps;
}

static int set_pub_partition(dds_entity_t pub, const char *buf) {
    const char **ps;
    char *bufcopy;
    unsigned nps = split_partitions(&ps, &bufcopy, buf);
    dds_return_t rc = change_publisher_partitions(pub, nps, ps);
    error_report(rc, "set_pub_partition failed: ");
    dds_free(bufcopy);
    dds_free((char **)ps);
    return 0;
}

#if 0
static int set_sub_partition(dds_entity_t sub, const char *buf) {
    const char **ps;
    char *bufcopy;
    unsigned nps = split_partitions(&ps, &bufcopy, buf);
    dds_return_t rc = change_subscriber_partitions(sub, nps, ps);
    error_report(rc, "set_partition failed: %s (%d)\n");
    dds_free(bufcopy);
    dds_free(ps);
    return 0;
}
#endif

static int read_int(char *buf, int bufsize, int pos, int accept_minus) {
    int c = EOF;
    while (pos < bufsize-1 && (c = getc(stdin)) != EOF && (isdigit((unsigned char) c) || (c == '-' && accept_minus))) {
        accept_minus = 0;
        buf[pos++] = (char) c;
    }
    buf[pos] = 0;
    if (c == EOF || isspace((unsigned char) c)) {
        return (pos > 0);
    } else if (!isdigit((unsigned char) c)) {
        fprintf (stderr, "%c: unexpected character\n", c);
        return 0;
    } else if (pos == bufsize-1) {
        fprintf (stderr, "integer too long\n");
        return 0;
    }
    return 1;
}

static int read_int_w_tstamp(struct tstamp_t *tstamp, char *buf, int bufsize, int pos) {
    int c;
    assert(pos < bufsize - 2);
    c = getc(stdin);
    if (c == EOF)
        return 0;
    else if (c == '@') {
        int posoff = 0;
        c = getc(stdin);
        if (c == EOF)
            return 0;
        else if (c == '=')
            tstamp->isabs = 1;
        else {
            buf[pos] = (char) c;
            posoff = 1;
        }
        if (read_int(buf, bufsize, pos + posoff, 1))
            tstamp->t = atoi(buf + pos) * DDS_NSECS_IN_SEC;
        else
            return 0;
        while ((c = getc(stdin)) != EOF && isspace((unsigned char) c))
            ;
        if (!isdigit((unsigned char) c))
            return 0;
    }
    buf[pos++] = (char) c;
    while (pos < bufsize-1 && (c = getc(stdin)) != EOF && isdigit((unsigned char) c))
        buf[pos++] = (char) c;
    buf[pos] = 0;
    if (c == EOF || isspace((unsigned char) c))
        return (pos > 0);
    else if (!isdigit((unsigned char) c)) {
        fprintf (stderr, "%c: unexpected character\n", c);
        return 0;
    } else if (pos == bufsize-1) {
        fprintf (stderr, "integer too long\n");
        return 0;
    }
    return 1;
}

static int read_value(char *command, int *key, struct tstamp_t *tstamp, char **arg) {
    char buf[1024];
    int c;
    if (*arg) { dds_free(*arg); *arg = NULL; }
    tstamp->isabs = 0;
    tstamp->t = 0;
    do {
        while ((c = getc(stdin)) != EOF && isspace((unsigned char) c))
            ;
        if (c == EOF)
            return 0;
        switch (c) {
        case '-':
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            buf[0] = (char) c;
            if (read_int(buf, sizeof(buf), 1, 0)) {
                *command = 'w';
                *key = atoi(buf);
                return 1;
            }
            break;
        case 'w': case 'd': case 'D': case 'u': case 'r':
            *command = (char) c;
            if (read_int_w_tstamp(tstamp, buf, sizeof(buf), 0)) {
                *key = atoi(buf);
                return 1;
            }
            break;
        case 'z': case 's':
            *command = (char) c;
            if (read_int(buf, sizeof(buf), 0, 0)) {
                *key = atoi(buf);
                return 1;
            }
            break;
        case 'p': case 'S': case 'C': case ':': case 'Q': {
            int i = 0;
            *command = (char) c;
            while ((c = getc(stdin)) != EOF && !isspace((unsigned char) c)) {
                assert(i < (int) sizeof(buf) - 1);
                buf[i++] = (char) c;
            }
            buf[i] = 0;
            *arg = dds_string_dup(buf);
            ungetc(c, stdin);
            return 1;
        }
        case 'Y': case 'B': case 'E': case 'W':
            *command = (char) c;
            return 1;
        default:
            fprintf (stderr, "'%c': unexpected character\n", c);
            break;
        }
        while ((c = getc(stdin)) != EOF && !isspace((unsigned char) c))
            ;
    } while (c != EOF);
    return 0;
}

// TODO Upon support for ARB types, resolve the declaration of fdin
//static void getl_init_simple(struct getl_arg *arg, int fd) {
//    arg->use_editline = 0;
//    arg->u.s.fd = fd;
//    arg->u.s.lastline = NULL;
//}
//
//static char *getl_simple(int fd, int *count) {
//    size_t sz = 0, n = 0;
//    char *line;
//    int c;
//
//    if ((c = getc(stdin)) == EOF) {
//        *count = 0;
//        return NULL;
//    }
//
//    line = NULL;
//    do {
//        if (n == sz) line = dds_realloc(line, sz += 256);
//        line[n++] = (char) c;
//    } while ((c = getc(stdin)) != EOF && c != '\n');
//    if (n == sz) line = dds_realloc(line, sz += 256);
//    line[n++] = 0;
//    *count = (int) (n-1);
//    return line;
//}
//
//struct getl_arg {
//    int use_editline;
//    union {
//#if USE_EDITLINE
//    struct {
//        FILE *el_fp;
//        EditLine *el;
//        History *hist;
//        HistEvent ev;
//    } el;
//#endif
//    struct {
//        int fd;
//        char *lastline;
//    } s;
//    } u;
//};

#if USE_EDITLINE
static int el_getc_wrapper(EditLine *el, char *c) {
    void *fd;
    int in;
    el_get(el, EL_CLIENTDATA, &fd);
    in = fd_getc(*(int *)fd);
    if (in == EOF)
        return 0;
    else {
        *c = (char) in;
        return 1;
    }
}

static const char *prompt(EditLine *el __attribute__ ((unused))) {
    return "";
}

static void getl_init_editline(struct getl_arg *arg, int fd) {
    if (isatty (fdin)) {
        arg->use_editline = 1;
        arg->u.el.el_fp = fdopen(fd, "r");
        arg->u.el.hist = history_init();
        history(arg->u.el.hist, &arg->u.el.ev, H_SETSIZE, 800);
        arg->u.el.el = el_init("pubsub", arg->u.el.el_fp, stdout, stderr);
        el_source(arg->u.el.el, NULL);
        el_set(arg->u.el.el, EL_EDITOR, "emacs");
        el_set(arg->u.el.el, EL_PROMPT, prompt);
        el_set(arg->u.el.el, EL_SIGNAL, 1);
        el_set(arg->u.el.el, EL_CLIENTDATA, &fdin);
        el_set(arg->u.el.el, EL_GETCFN, el_getc_wrapper);
        el_set(arg->u.el.el, EL_HIST, history, arg->u.el.hist);
    } else {
        getl_init_simple(arg, fd);
    }
}
#endif

// TODO ARB type support
//static void getl_fini(struct getl_arg *arg) {
//    if (arg->use_editline) {
//#if USE_EDITLINE
//        el_end(arg->u.el.el);
//        history_end(arg->u.el.hist);
//        fclose(arg->u.el.el_fp);
//#endif
//    } else {
//        dds_free(arg->u.s.lastline);
//    }
//}
//
//static const char *getl(struct getl_arg *arg, int *count) {
//    if (arg->use_editline) {
//#if USE_EDITLINE
//        return el_gets(arg->u.el.el, count);
//#else
//        abort();
//        return NULL;
//#endif
//    } else {
//        dds_free(arg->u.s.lastline);
//        return arg->u.s.lastline = getl_simple(arg->u.s.fd, count);
//    }
//}
//
//static void getl_enter_hist(struct getl_arg *arg, const char *line) {
//#if USE_EDITLINE
//    if (arg->use_editline)
//        history(arg->u.el.hist, &arg->u.el.ev, H_ENTER, line);
//#endif
//}
//
//static char *skipspaces(const char *s) {
//    while (*s && isspace((unsigned char) *s))
//        s++;
//    return (char *) s;
//}

static char si2isc(const dds_sample_info_t *si) {
    switch (si->instance_state) {
    case DDS_IST_ALIVE: return 'A';
    case DDS_IST_NOT_ALIVE_DISPOSED: return 'D';
    case DDS_IST_NOT_ALIVE_NO_WRITERS: return 'U';
    default: return '?';
    }
}

static char si2ssc(const dds_sample_info_t *si) {
    switch (si->sample_state) {
    case DDS_SST_READ: return 'R';
    case DDS_SST_NOT_READ: return 'N';
    default: return '?';
    }
}

static char si2vsc(const dds_sample_info_t *si) {
    switch (si->view_state) {
    case DDS_VST_NEW: return 'N';
    case DDS_VST_OLD: return 'O';
    default: return '?';
    }
}

static int getkeyval_KS(dds_entity_t rd, int32_t *key, dds_instance_handle_t ih) {
    int result;
    KeyedSeq d_key;
    if ((result = dds_instance_get_key(rd, ih, &d_key)) == DDS_RETCODE_OK)
        *key = d_key.keyval;
    else
        *key = 0;
    return result;
}

static int getkeyval_K32(dds_entity_t rd, int32_t *key, dds_instance_handle_t ih) {
    int result = 0;
    Keyed32 d_key;
    if ((result = dds_instance_get_key(rd, ih, &d_key)) == DDS_RETCODE_OK)
        *key = d_key.keyval;
    else
        *key = 0;
    return result;
}

static int getkeyval_K64(dds_entity_t rd, int32_t *key, dds_instance_handle_t ih) {
    int result = 0;
    Keyed64 d_key;
    if ((result = dds_instance_get_key(rd, ih, &d_key)) == DDS_RETCODE_OK)
        *key = d_key.keyval;
    else
        *key = 0;
    return result;
}

static int getkeyval_K128(dds_entity_t rd, int32_t *key, dds_instance_handle_t ih) {
    int result = 0;
    Keyed128 d_key;
    if ((result = dds_instance_get_key(rd, ih, &d_key)) == DDS_RETCODE_OK)
        *key = d_key.keyval;
    else
        *key = 0;
    return result;
}

static int getkeyval_K256(dds_entity_t rd, int32_t *key, dds_instance_handle_t ih) {
    int result = 0;
    Keyed256 d_key;
    if ((result = dds_instance_get_key(rd, ih, &d_key)) == DDS_RETCODE_OK)
        *key = d_key.keyval;
    else
        *key = 0;
    return result;
}

// TODO Determine encoding of dds_instance_handle_t, and see what sort of value can be extracted from it, if any
//static void instancehandle_to_id(uint32_t *systemId, uint32_t *localId, dds_instance_handle_t h) {
//    /* Undocumented and unsupported trick */
//    union { struct { uint32_t systemId, localId; } s; dds_instance_handle_t h; } u;
//    u.h = h;
//    *systemId = u.s.systemId & ~0x80000000;
//    *localId = u.s.localId;
//}

static void print_sampleinfo(dds_time_t *tstart, dds_time_t tnow, const dds_sample_info_t *si, const char *tag) {
    dds_time_t relt;
//    uint32_t phSystemId, phLocalId, ihSystemId, ihLocalId;
    char isc = si2isc(si), ssc = si2ssc(si), vsc = si2vsc(si);
    const char *sep;
    int n = 0;
    if (*tstart == 0) {
        *tstart = tnow;
    }
    relt = tnow - *tstart;
//    instancehandle_to_id(&ihSystemId, &ihLocalId, si->instance_handle);
//    instancehandle_to_id(&phSystemId, &phLocalId, si->publication_handle);
    if (print_metadata & PM_PID) {
        n += printf ("%d", pid);
    }
    if (print_metadata & PM_TOPIC) {
        n += printf ("%s", tag);
    }
    if (print_metadata & PM_TIME) {
        n += printf ("%s%"PRId64".%09"PRId64, n > 0 ? " " : "", (relt / DDS_NSECS_IN_SEC), (relt % DDS_NSECS_IN_SEC));
    }
    sep = " : ";
    if (print_metadata & PM_PHANDLE) {
        n += printf ("%s%" PRIu64, n > 0 ? sep : "", si->publication_handle);
        sep = " ";
    }
    if (print_metadata & PM_IHANDLE) {
        n += printf ("%s%" PRIu64, n > 0 ? sep : "", si->instance_handle);
    }
    sep = " : ";
    if (print_metadata & PM_STIME) {
        n += printf ("%s%"PRId64".%09"PRId64, n > 0 ? sep : "", (si->source_timestamp/DDS_NSECS_IN_SEC), (si->source_timestamp%DDS_NSECS_IN_SEC));
    }
    sep = " : ";
    if (print_metadata & PM_DGEN) {
        n += printf ("%s%"PRIu32, n > 0 ? sep : "", si->disposed_generation_count);
        sep = " ";
    }
    if (print_metadata & PM_NWGEN) {
        n += printf ("%s%"PRIu32, n > 0 ? sep : "", si->no_writers_generation_count);
    }
    sep = " : ";
    if (print_metadata & PM_RANKS) {
        n += printf ("%s%"PRIu32" %"PRIu32" %"PRIu32, n > 0 ? sep : "", si->sample_rank, si->generation_rank, si->absolute_generation_rank);
    }
    sep = " : ";
    if (print_metadata & PM_STATE) {
        n += printf ("%s%c%c%c", n > 0 ? sep : "", isc, ssc, vsc);
    }
    if (n > 0) {
        printf(" : ");
    }
}

static void print_K(dds_time_t *tstart, dds_time_t tnow, dds_entity_t rd, const char *tag, const dds_sample_info_t *si, int32_t keyval, uint32_t seq, int (*getkeyval) (dds_entity_t rd, int32_t *key, dds_instance_handle_t ih)) {
    int result;
    ddsrt_mutex_lock(&output_mutex);
    print_sampleinfo(tstart, tnow, si, tag);
    if (si->valid_data) {
        if(printmode == TGPM_MULTILINE) {
            printf ("{\n%*.*s.seq = %"PRIu32",\n%*.*s.keyval = %"PRId32" }\n", 4, 4, "", seq, 4, 4, "", keyval);
        } else if(printmode == TGPM_DENSE) {
            printf ("{%"PRIu32",%"PRId32"}\n", seq, keyval);
        } else {
            printf ("{ .seq = %"PRIu32", .keyval = %"PRId32" }\n", seq, keyval);
        }
    } else {
        /* May not look at mseq->_buffer[i] but want the key value
        nonetheless.  Bummer.  Actually this leads to an interesting
        problem: if the instance is in the NOT_ALIVE state and the
        middleware releases all resources related to the instance
        after our taking the sample, get_key_value _will_ fail.  So
        the blanket statement "may not look at value" if valid_data
        is not set means you can't really use take ...  */
        int32_t d_key;
        if ((result = getkeyval(rd, &d_key, si->instance_handle)) == DDS_RETCODE_OK) {
            if(printmode == TGPM_MULTILINE) {
                printf ("{\n%*.*s.seq = NA,\n%*.*s.keyval = %"PRId32" }\n", 4, 4, "", 4, 4, "", keyval);
            } else if(printmode == TGPM_DENSE) {
                printf ("{NA,%"PRId32"}\n", keyval);
            } else {
                printf ("{ .seq = NA, .keyval = %"PRId32" }\n", keyval);
            }
        } else
            printf ("get_key_value: error (%s)\n", dds_err_str(result));
    }
    if (flushflag) {
        fflush (stdout);
    }
    ddsrt_mutex_unlock(&output_mutex);
}

static void print_seq_KS(dds_time_t *tstart, dds_time_t tnow, dds_entity_t rd, const char *tag, const dds_sample_info_t *iseq, KeyedSeq **mseq, int count) {
    int i;
    for (i = 0; i < count; i++)
        print_K(tstart, tnow, rd, tag, &iseq[i], mseq[i]->keyval, mseq[i]->seq, getkeyval_KS);
}

static void print_seq_K32(dds_time_t *tstart, dds_time_t tnow, dds_entity_t rd, const char *tag, const dds_sample_info_t *iseq, Keyed32 **mseq, int count) {
    int i;
    for (i = 0; i < count; i++)
        print_K(tstart, tnow, rd, tag, &iseq[i], mseq[i]->keyval, mseq[i]->seq, getkeyval_K32);
}

static void print_seq_K64(dds_time_t *tstart, dds_time_t tnow, dds_entity_t rd, const char *tag, const dds_sample_info_t *iseq, Keyed64 **mseq, int count) {
    int i;
    for (i = 0; i < count; i++)
        print_K(tstart, tnow, rd, tag, &iseq[i], mseq[i]->keyval, mseq[i]->seq, getkeyval_K64);
}

static void print_seq_K128(dds_time_t *tstart, dds_time_t tnow, dds_entity_t rd, const char *tag, const dds_sample_info_t *iseq, Keyed128 **mseq, int count) {
    int i;
    for (i = 0; i < count; i++)
        print_K(tstart, tnow, rd, tag, &iseq[i], mseq[i]->keyval, mseq[i]->seq, getkeyval_K128);
}

static void print_seq_K256(dds_time_t *tstart, dds_time_t tnow, dds_entity_t rd, const char *tag, const dds_sample_info_t *iseq, Keyed256 **mseq, int count) {
    int i;
    for (i = 0; i < count; i++)
        print_K(tstart, tnow, rd, tag, &iseq[i], mseq[i]->keyval, mseq[i]->seq, getkeyval_K256);
}

static void print_seq_OU(dds_time_t *tstart, dds_time_t tnow, dds_entity_t rd __attribute__ ((unused)), const char *tag, const dds_sample_info_t *si, const OneULong **mseq, int count) {
    int i;
    for (i = 0; i < count; i++)
    {
        ddsrt_mutex_lock(&output_mutex);
        print_sampleinfo(tstart, tnow, si, tag);
        if (si->valid_data) {
            if(printmode == TGPM_MULTILINE) {
                printf ("{\n%*.*s.seq = %"PRIu32" }\n", 4, 4, "", mseq[i]->seq);
            } else if(printmode == TGPM_DENSE) {
                printf ("{%"PRIu32"}\n", mseq[i]->seq);
            } else {
                printf ("{ .seq = %"PRIu32" }\n", mseq[i]->seq);
            }
        } else {
            printf ("NA\n");
        }
        if (flushflag) {
            fflush (stdout);
        }
        ddsrt_mutex_unlock(&output_mutex);
    }
}

static void print_seq_ARB(dds_time_t *tstart, dds_time_t tnow, dds_entity_t rd __attribute__ ((unused)), const char *tag, const dds_sample_info_t *iseq, const void **mseq, const struct tgtopic *tgtp) {
    (void)tnow;
    (void)tstart;
    (void)tag;
    (void)iseq;
    (void)mseq;
    (void)tgtp;
// TODO ARB type support
//    unsigned i;
//    for (i = 0; i < mseq->_length; i++)
//    {
//        dds_sample_info_t const * const si = &iseq->_buffer[i];
//        flockfile(stdout);
//        print_sampleinfo(tstart, tnow, si, tag);
//        if (si->valid_data)
//            tgprint(stdout, tgtp, (char *) mseq->_buffer + i * tgtp->size, printmode);
//        else
//            tgprintkey(stdout, tgtp, (char *) mseq->_buffer + i * tgtp->size, printmode);
//        printf ("\n");
//        funlockfile(stdout);
//    }
}

static void rd_on_liveliness_changed(dds_entity_t rd __attribute__ ((unused)), const dds_liveliness_changed_status_t status, void* arg  __attribute__ ((unused))) {
    printf ("[liveliness-changed: alive=(%"PRIu32" change %"PRId32") not_alive=(%"PRIu32" change %"PRId32") handle=%"PRIu64"]\n",
            status.alive_count, status.alive_count_change,
            status.not_alive_count, status.not_alive_count_change,
            status.last_publication_handle);
    if (flushflag) {
      fflush (stdout);
    }
}

static void rd_on_sample_lost(dds_entity_t rd __attribute__ ((unused)), const dds_sample_lost_status_t status, void* arg  __attribute__ ((unused))) {
    printf ("[sample-lost: total=(%"PRIu32" change %"PRId32")]\n", status.total_count, status.total_count_change);
    if (flushflag) {
      fflush (stdout);
    }
}

static void rd_on_sample_rejected(dds_entity_t rd __attribute__ ((unused)), const dds_sample_rejected_status_t status, void* arg  __attribute__ ((unused))) {
    const char *reasonstr = "?";
    switch (status.last_reason) {
    case DDS_NOT_REJECTED: reasonstr = "not_rejected"; break;
    case DDS_REJECTED_BY_INSTANCES_LIMIT: reasonstr = "instances"; break;
    case DDS_REJECTED_BY_SAMPLES_LIMIT: reasonstr = "samples"; break;
    case DDS_REJECTED_BY_SAMPLES_PER_INSTANCE_LIMIT: reasonstr = "samples_per_instance"; break;
    }
    printf ("[sample-rejected: total=(%"PRIu32" change %"PRId32") reason=%s handle=%"PRIu64"]\n",
            status.total_count, status.total_count_change,
            reasonstr,
            status.last_instance_handle);
    if (flushflag) {
      fflush (stdout);
    }
}

static void rd_on_subscription_matched(dds_entity_t rd __attribute__((unused)), const dds_subscription_matched_status_t status, void* arg  __attribute__((unused))) {
    printf ("[subscription-matched: total=(%"PRIu32" change %"PRId32") current=(%"PRIu32" change %"PRId32") handle=%"PRIu64"]\n",
            status.total_count, status.total_count_change,
            status.current_count, status.current_count_change,
            status.last_publication_handle);
    if (flushflag) {
      fflush (stdout);
    }
}

static void rd_on_requested_deadline_missed(dds_entity_t rd __attribute__((unused)), const dds_requested_deadline_missed_status_t status, void* arg  __attribute__ ((unused))) {
    printf ("[requested-deadline-missed: total=(%"PRIu32" change %"PRId32") handle=%"PRIu64"]\n",
            status.total_count, status.total_count_change,
            status.last_instance_handle);
    if (flushflag) {
      fflush (stdout);
    }
}

static const char *policystr(uint32_t id) {
    switch (id) {
    case DDS_USERDATA_QOS_POLICY_ID: return DDS_USERDATA_QOS_POLICY_NAME;
    case DDS_DURABILITY_QOS_POLICY_ID: return DDS_DURABILITY_QOS_POLICY_NAME;
    case DDS_PRESENTATION_QOS_POLICY_ID: return DDS_PRESENTATION_QOS_POLICY_NAME;
    case DDS_DEADLINE_QOS_POLICY_ID: return DDS_DEADLINE_QOS_POLICY_NAME;
    case DDS_LATENCYBUDGET_QOS_POLICY_ID: return DDS_LATENCYBUDGET_QOS_POLICY_NAME;
    case DDS_OWNERSHIP_QOS_POLICY_ID: return DDS_OWNERSHIP_QOS_POLICY_NAME;
    case DDS_OWNERSHIPSTRENGTH_QOS_POLICY_ID: return DDS_OWNERSHIPSTRENGTH_QOS_POLICY_NAME;
    case DDS_LIVELINESS_QOS_POLICY_ID: return DDS_LIVELINESS_QOS_POLICY_NAME;
    case DDS_TIMEBASEDFILTER_QOS_POLICY_ID: return DDS_TIMEBASEDFILTER_QOS_POLICY_NAME;
    case DDS_PARTITION_QOS_POLICY_ID: return DDS_PARTITION_QOS_POLICY_NAME;
    case DDS_RELIABILITY_QOS_POLICY_ID: return DDS_RELIABILITY_QOS_POLICY_NAME;
    case DDS_DESTINATIONORDER_QOS_POLICY_ID: return DDS_DESTINATIONORDER_QOS_POLICY_NAME;
    case DDS_HISTORY_QOS_POLICY_ID: return DDS_HISTORY_QOS_POLICY_NAME;
    case DDS_RESOURCELIMITS_QOS_POLICY_ID: return DDS_RESOURCELIMITS_QOS_POLICY_NAME;
    case DDS_ENTITYFACTORY_QOS_POLICY_ID: return DDS_ENTITYFACTORY_QOS_POLICY_NAME;
    case DDS_WRITERDATALIFECYCLE_QOS_POLICY_ID: return DDS_WRITERDATALIFECYCLE_QOS_POLICY_NAME;
    case DDS_READERDATALIFECYCLE_QOS_POLICY_ID: return DDS_READERDATALIFECYCLE_QOS_POLICY_NAME;
    case DDS_TOPICDATA_QOS_POLICY_ID: return DDS_TOPICDATA_QOS_POLICY_NAME;
    case DDS_GROUPDATA_QOS_POLICY_ID: return DDS_GROUPDATA_QOS_POLICY_NAME;
    case DDS_TRANSPORTPRIORITY_QOS_POLICY_ID: return DDS_TRANSPORTPRIORITY_QOS_POLICY_NAME;
    case DDS_LIFESPAN_QOS_POLICY_ID: return DDS_LIFESPAN_QOS_POLICY_NAME;
    case DDS_DURABILITYSERVICE_QOS_POLICY_ID: return DDS_DURABILITYSERVICE_QOS_POLICY_NAME;
    case DDS_SUBSCRIPTIONKEY_QOS_POLICY_ID: return DDS_SUBSCRIPTIONKEY_QOS_POLICY_NAME;
    case DDS_VIEWKEY_QOS_POLICY_ID: return DDS_VIEWKEY_QOS_POLICY_NAME;
    case DDS_READERLIFESPAN_QOS_POLICY_ID: return DDS_READERLIFESPAN_QOS_POLICY_NAME;
    case DDS_SHARE_QOS_POLICY_ID: return DDS_SHARE_QOS_POLICY_NAME;
    case DDS_SCHEDULING_QOS_POLICY_ID: return DDS_SCHEDULING_QOS_POLICY_NAME;
    case DDS_PROPERTY_QOS_POLICY_ID: return DDS_PROPERTY_QOS_POLICY_NAME;
    default: return "?";
    }
}

// TODO Decide on whether to work around the lack of DDS_QosPolicyCount, or get rid of this bit.
//static void format_policies(char *polstr, size_t polsz, const DDS_QosPolicyCount *xs, unsigned nxs) {
//    char *ps = polstr;
//    unsigned i;
//    for (i = 0; i < nxs && ps < polstr + polsz; i++)
//    {
//        const DDS_QosPolicyCount *x = &xs[i];
//        int n = snprintf (ps, polstr + polsz - ps, "%s%s:%d", i == 0 ? "" : ", ", policystr(x->policy_id), x->count);
//        ps += n;
//    }
//}

static void rd_on_requested_incompatible_qos(dds_entity_t rd __attribute__((unused)), const dds_requested_incompatible_qos_status_t status, void* arg __attribute__((unused))) {
    printf ("[requested-incompatible-qos: total=(%"PRIu32" change %"PRId32") last_policy=%s]\n",
            status.total_count, status.total_count_change, policystr(status.last_policy_id));
    if (flushflag) {
      fflush (stdout);
    }
}

static void wr_on_offered_incompatible_qos(dds_entity_t wr __attribute__((unused)), const dds_offered_incompatible_qos_status_t status, void* arg __attribute__((unused))) {
    printf ("[offered-incompatible-qos: total=(%"PRIu32" change %"PRId32") last_policy=%s]\n",
            status.total_count, status.total_count_change, policystr(status.last_policy_id));
    if (flushflag) {
      fflush (stdout);
    }
}

static void wr_on_liveliness_lost(dds_entity_t wr __attribute__((unused)), const dds_liveliness_lost_status_t status, void* arg  __attribute__ ((unused))) {
    printf ("[liveliness-lost: total=(%"PRIu32" change %"PRId32")]\n",
            status.total_count, status.total_count_change);
    if (flushflag) {
      fflush (stdout);
    }
}

static void wr_on_offered_deadline_missed(dds_entity_t wr __attribute__((unused)), const dds_offered_deadline_missed_status_t status, void* arg __attribute__((unused))) {
    printf ("[offered-deadline-missed: total=(%"PRIu32" change %"PRId32") handle=%"PRIu64"]\n",
            status.total_count, status.total_count_change, status.last_instance_handle);
    if (flushflag) {
      fflush (stdout);
    }
}

static void wr_on_publication_matched(dds_entity_t wr __attribute__((unused)), const dds_publication_matched_status_t status, void* arg __attribute__((unused))) {
    printf ("[publication-matched: total=(%"PRIu32" change %"PRId32") current=(%"PRIu32" change %"PRId32") handle=%"PRIu64"]\n",
            status.total_count, status.total_count_change,
            status.current_count, status.current_count_change,
            status.last_subscription_handle);
    if (flushflag) {
      fflush (stdout);
    }
}

static dds_return_t register_instance_wrapper(dds_entity_t wr, const void *d, const dds_time_t tstamp) {
    dds_instance_handle_t handle;
    (void)tstamp;
    return dds_register_instance(wr, &handle, d);
}

static write_oper_t get_write_oper(char command) {
    switch (command) {
    case 'w': return dds_write_ts;
    case 'd': return dds_dispose_ts;
    case 'D': return dds_writedispose_ts;
    case 'u': return dds_unregister_instance_ts;
    case 'r': return register_instance_wrapper;
    default:  return 0;
    }
}

static const char *get_write_operstr(char command) {
    switch (command) {
    case 'w': return "write";
    case 'd': return "dispose";
    case 'D': return "writedispose";
    case 'u': return "unregister_instance";
    case 'r': return "register_instance";
    default:  return 0;
    }
}

static void non_data_operation(char command, dds_entity_t wr) {
    dds_return_t rc = 0;
    switch (command) {
    case 'Y':
        printf ("Dispose all: not supported\n");
        if (flushflag) {
            fflush (stdout);
        }
//        TODO Implement application side tracking of alive instances for use with a 'dispose all' function
//        if ((result = DDS_Topic_dispose_all_data(DDS_DataWriter_get_topic(wr))) != DDS_RETCODE_OK)
//            error ("DDS_Topic_dispose_all: error %d\n", (int) result);
        break;
    case 'B':
        rc = dds_begin_coherent(wr);
        error_report(rc, "dds_begin_coherent:");
        break;
    case 'E':
        rc = dds_end_coherent(wr);
        error_report(rc, "dds_end_coherent:");
        break;
    case 'W': {
        dds_duration_t inf = DDS_INFINITY;
        rc = dds_wait_for_acks(wr, inf);
        error_report(rc, "dds_wait_for_acks:");
        break;
    }
    default:
        abort();
    }
}

static int accept_error(char command, int retcode) {
    if (retcode == DDS_RETCODE_TIMEOUT)
        return 1;
    if ((command == 'd' || command == 'u') && retcode == DDS_RETCODE_PRECONDITION_NOT_MET)
        return 1;
    return 0;
}

union data {
    uint32_t seq;
    struct { uint32_t seq; int32_t keyval; } seq_keyval;
    KeyedSeq ks;
    Keyed32 k32;
    Keyed64 k64;
    Keyed128 k128;
    Keyed256 k256;
    OneULong ou;
};

static void pub_do_auto(const struct writerspec *spec) {
    int result;
    dds_instance_handle_t *handle = (dds_instance_handle_t*) dds_alloc(sizeof(dds_instance_handle_t)*nkeyvals);
    dds_time_t ntot = 0, tfirst, tlast, tprev, tfirst0, tstop;
    struct hist *hist = hist_new(30, 1000, 0);
    int k = 0;
    union data d;
    memset(&d, 0, sizeof(d));

    switch (spec->topicsel) {
    case UNSPEC:
        assert(0);
    case KS:
        d.ks.baggage._maximum = d.ks.baggage._length = spec->baggagesize;
        d.ks.baggage._buffer = (uint8_t *) dds_alloc(spec->baggagesize);
        memset(d.ks.baggage._buffer, 0xee, spec->baggagesize);
        break;
    case K32:
        memset(d.k32.baggage, 0xee, sizeof(d.k32.baggage));
        break;
    case K64:
        memset(d.k64.baggage, 0xee, sizeof(d.k64.baggage));
        break;
    case K128:
        memset(d.k128.baggage, 0xee, sizeof(d.k128.baggage));
        break;
    case K256:
        memset(d.k256.baggage, 0xee, sizeof(d.k256.baggage));
        break;
    case OU:
        break;
    case ARB:
        break;
    }

    for (k = 0; (uint32_t) k < nkeyvals; k++) {
        d.seq_keyval.keyval = k;
        if(spec->register_instances) {
            (void) dds_register_instance(spec->wr, &handle[k], &d);
        }
    }

    dds_sleepfor(DDS_SECS(1)); // TODO is this sleep necessary?
    d.seq_keyval.keyval = 0;
    tfirst0 = tfirst = tprev = dds_time();
    if (dur != 0.0) {
        dds_duration_t dds_dur = 0;
        (void) double_to_dds_duration(&dds_dur, dur);
        tstop = tfirst0 + dds_dur;
    } else
        tstop = INT64_MAX;

    if (nkeyvals == 0) {
        while (!termflag && tprev < tstop) {
            dds_sleepfor(DDS_MSECS(100));
        }
    } else if (spec->writerate <= 0) {
        while (!termflag && tprev < tstop) {
            if ((result = dds_write(spec->wr, &d)) != DDS_RETCODE_OK) {
                printf ("write: error %d (%s)\n", (int) result, dds_err_str(result));
                if (flushflag) {
                    fflush (stdout);
                }
                if (result != DDS_RETCODE_TIMEOUT)
                    break;
            } else {
                d.seq_keyval.keyval = (d.seq_keyval.keyval + 1) % (int32_t)nkeyvals;
                d.seq++;
                ntot++;
                if ((d.seq % 16) == 0) {
                    dds_time_t t = dds_time();
                    hist_record(hist, (uint64_t)((t - tprev) / 16), 16);
                    if (t < tfirst + DDS_SECS(4)) {
                        tprev = t;
                    } else {
                        tlast = t;
                        hist_print(hist, tlast - tfirst, 1);
                        tfirst = tprev;
                        tprev = dds_time();
                    }
                }
            }
        }
    } else {
        unsigned bi = 0;
        while (!termflag && tprev < tstop) {
            if ((result = dds_write(spec->wr, &d)) != DDS_RETCODE_OK) {
                printf ("write: error %d (%s)\n", (int) result, dds_err_str(result));
                if (flushflag) {
                    fflush (stdout);
                }
                if (result != DDS_RETCODE_TIMEOUT)
                    break;
            }

            {
                dds_time_t t = dds_time();
                d.seq_keyval.keyval = (d.seq_keyval.keyval + 1) % (int32_t)nkeyvals;
                d.seq++;
                ntot++;
                hist_record(hist, (uint64_t)(t - tprev), 1);
                if (t >= tfirst + DDS_SECS(4)) {
                    tlast = t;
                    hist_print(hist, tlast - tfirst, 1);
                    tfirst = tprev;
                    t = dds_time();
                }
                if (++bi == spec->burstsize) {
                    while (((double)(ntot / spec->burstsize) / ((double)(t - tfirst0) / 1e9 + 5e-3)) > spec->writerate && !termflag) {
                        /* FIXME: only doing this manually because batching is not yet implemented properly */
                        dds_write_flush(spec->wr);
                        dds_sleepfor(DDS_MSECS(10));
                        t = dds_time();
                    }
                    bi = 0;
                }
                tprev = t;
            }
        }
    }
    tlast = dds_time();
    hist_print(hist, tlast - tfirst, 0);
    hist_free(hist);
    printf ("total writes: %" PRId64 " (%e/s)\n", ntot, (double)ntot * 1e9 / (double)(tlast - tfirst0));
    if (flushflag) {
        fflush (stdout);
    }
    if (spec->topicsel == KS) {
        dds_free(d.ks.baggage._buffer);
    }
    dds_free(handle);
}

static void do_deafmute(const char *args)
{
    const char *a = args;
    bool deaf = false;
    bool mute = false;
    dds_duration_t duration = 0;
    double durfloat;
    int pos;
    if (strncmp(a, "self", 4) == 0) {
        a += 4;
    } else {
        printf ("deafmute: invalid args: %s\n", args);
        return;
    }
    if (*a++ != ';') {
        printf ("deafmute: invalid args: %s\n", args);
        return;
    }
    while (*a && *a != ';') {
        switch (*a++) {
            case 'm': mute = true; break;
            case 'd': deaf = true; break;
            default: printf ("deafmute: invalid flags: %s\n", args); return;
        }
    }
    if (*a++ != ';') {
        printf ("deafmute: invalid args: %s\n", args);
        return;
    }
    if (strcmp(a, "inf") == 0) {
        duration = DDS_INFINITY;
    } else if (sscanf(a, "%lf%n", &durfloat, &pos) == 1 && a[pos] == 0) {
        if (durfloat <= 0.0) {
            printf ("deafmute: invalid duration (<= 0): %s\n", args);
            return;
        }
        if (durfloat > (double) (DDS_INFINITY / DDS_NSECS_IN_SEC))
            duration = DDS_INFINITY;
        else
            duration = (dds_duration_t) (durfloat * 1e9);
    } else {
        printf ("deafmute: invalid args: %s\n", args);
        return;
    }
    dds_domain_set_deafmute (dp, deaf, mute, duration);
}

static char *pub_do_nonarb(const struct writerspec *spec, uint32_t *seq) {
    struct tstamp_t tstamp_spec = { .isabs = 0, .t = 0 };
    int result;
    union data d;
    char command;
    char *arg = NULL;
    int k = 0;
    memset(&d, 0, sizeof(d));
    switch (spec->topicsel) {
    case UNSPEC:
        assert(0);
    case KS:
        d.ks.baggage._maximum = d.ks.baggage._length = spec->baggagesize;
        d.ks.baggage._buffer = (uint8_t *) dds_alloc(spec->baggagesize);
        memset(d.ks.baggage._buffer, 0xee, spec->baggagesize);
        break;
    case K32:
        memset(d.k32.baggage, 0xee, sizeof(d.k32.baggage));
        break;
    case K64:
        memset(d.k64.baggage, 0xee, sizeof(d.k64.baggage));
        break;
    case K128:
        memset(d.k128.baggage, 0xee, sizeof(d.k128.baggage));
        break;
    case K256:
        memset(d.k256.baggage, 0xee, sizeof(d.k256.baggage));
        break;
    case OU:
        break;
    case ARB:
        break;
    }
    d.seq = *seq;
    command = 0;
    while (command != ':' && read_value(&command, &k, &tstamp_spec, &arg)) {
        d.seq_keyval.keyval = k;
        switch (command) {
        case 'w': case 'd': case 'D': case 'u': case 'r': {
            write_oper_t fn = get_write_oper(command);
            dds_time_t tstamp = 0;
            if (!tstamp_spec.isabs) {
                tstamp = dds_time();
                tstamp_spec.t += tstamp;
            }
            tstamp = (tstamp_spec.t % DDS_NSECS_IN_SEC) + ((int) (tstamp_spec.t / DDS_NSECS_IN_SEC) * DDS_NSECS_IN_SEC);
            if ((result = fn(spec->wr, &d, tstamp)) != DDS_RETCODE_OK) {
                printf ("%s %d: error %d (%s)\n", get_write_operstr(command), k, (int) result, dds_err_str(result));
                if (flushflag) {
                    fflush (stdout);
                }
                if (!accept_error(command, result))
                    exit(1);
            }
            /* FIXME: only doing this manually because batching is not yet implemented properly */
            dds_write_flush(spec->wr);
            if (spec->dupwr && (result = fn(spec->dupwr, &d, tstamp)) != DDS_RETCODE_OK) {
                printf ("%s %d(dup): error %d (%s)\n", get_write_operstr(command), k, (int) result, dds_err_str(result));
                if (flushflag) {
                    fflush (stdout);
                }
                if (!accept_error(command, result))
                    exit(1);
            }
            if (spec->dupwr) {
                /* FIXME: only doing this manually because batching is not yet implemented properly */
                dds_write_flush(spec->wr);
            }
            d.seq++;
            break;
        }
        case 'z':
            if (spec->topicsel != KS) {
                printf ("payload size cannot be set for selected type\n");
                if (flushflag) {
                    fflush (stdout);
                }
            } else if (k < 12 && k != 0) {
                printf ("invalid payload size: %d\n", k);
                if (flushflag) {
                    fflush (stdout);
                }
            } else {
                uint32_t baggagesize = (k != 0) ? (uint32_t) (k - 12) : 0;
                if (d.ks.baggage._buffer)
                    dds_free (d.ks.baggage._buffer);
                d.ks.baggage._maximum = d.ks.baggage._length = baggagesize;
                d.ks.baggage._buffer = (uint8_t *) dds_alloc(baggagesize);
                memset(d.ks.baggage._buffer, 0xee, d.ks.baggage._length);
            }
            break;
        case 'p':
            set_pub_partition(spec->pub, arg);
            break;
        case 's':
            if (k < 0) {
                printf ("invalid sleep duration: %ds\n", k);
                if (flushflag) {
                    fflush (stdout);
                }
            } else {
                dds_sleepfor(DDS_SECS(k));
            }
            break;
        case 'Q': {
            dds_qos_t *qos = dds_create_qos ();
            setqos_from_args (DDS_KIND_PARTICIPANT, qos, 1, (const char **) &arg);
            (void) dds_set_qos (dp, qos);
            dds_delete_qos (qos);
            break;
          }
        case 'Y': case 'B': case 'E': case 'W':
            non_data_operation(command, spec->wr);
            break;
        case 'C':
            do_deafmute(arg);
            break;
        case ':':
            break;
        default:
            abort();
        }
    }
    if (spec->topicsel == KS)
        dds_free(d.ks.baggage._buffer);
    *seq = d.seq;
    if (command == ':')
        return arg;
    else {
        dds_free(arg);
        return NULL;
    }
}

// TODO ARB type support
//static char *pub_do_arb_line(const struct writerspec *spec, const char *line) {
//    int result;
//    struct tstamp_t tstamp_spec;
//    char *ret = NULL;
//    char command;
//    int k, pos;
//    while (line && *(line = skipspaces(line)) != 0) {
//        tstamp_spec.isabs = 0; tstamp_spec.t = 0;
//        command = 'w';
//        switch (*line) {
//        case 'w': case 'd': case 'D': case 'u': case 'r':
//            command = *line++;
//            if (*line == '@') {
//                if (*++line == '=') { ++line; tstamp_spec.isabs = 1; }
//                tstamp_spec.t = DDS_NSECS_IN_SEC * strtol(line, (char **) &line, 10);
//            }
//        case '{': {
//            write_oper_t fn = get_write_oper(command);
//            void *arb;
//            char *endp;
//            if ((arb = tgscan(spec->tgtp, line, &endp)) == NULL) {
//                line = NULL;
//            } else {
//                dds_time_t tstamp;
//                int diddodup = 0;
//                if (!tstamp_spec.isabs) {
//                    DDS_DomainParticipant_get_current_time(dp, &tstamp);
//                    tstamp_spec.t += tstamp.sec * DDS_NSECS_IN_SEC + tstamp.nanosec;
//                }
//                tstamp.sec = (int) (tstamp_spec.t / DDS_NSECS_IN_SEC);
//                tstamp.nanosec = (unsigned) (tstamp_spec.t % DDS_NSECS_IN_SEC);
//                line = endp;
//                result = fn(spec->wr, arb, DDS_HANDLE_NIL, &tstamp);
//                if (result == DDS_RETCODE_OK && spec->dupwr) {
//                    diddodup = 1;
//                    result = fn(spec->dupwr, arb, DDS_HANDLE_NIL, &tstamp);
//                }
//                tgfreedata(spec->tgtp, arb);
//                if (result != DDS_RETCODE_OK) {
//                    printf ("%s%s: error %d (%s)\n", get_write_operstr(command), diddodup ? "(dup)" : "", (int) result, dds_err_str(result));
//                    if (!accept_error(command, result)) {
//                        line = NULL;
//                        if (!isatty(fdin))
//                            exit(1);
//                        break;
//                    }
//                }
//            }
//            break;
//        }
//        case 'p':
//            set_pub_partition(DDS_DataWriter_get_publisher(spec->wr), line+1);
//            line = NULL;
//            break;
//        case 's':
//            if (sscanf(line+1, "%d%n", &k, &pos) != 1 || k < 0) {
//                printf ("invalid sleep duration: %ds\n", k);
//                line = NULL;
//            } else {
//                sleep((unsigned) k);
//                line += 1 + pos;
//            }
//            break;
//        case 'Y': case 'B': case 'E': case 'W':
//            non_data_operation(*line, spec->wr);
//            break;
//        case 'C':
//            do_deafmute(arg);
//            line = NULL;
//            break;
//        case 'S':
//            make_persistent_snapshot(line+1);
//            line = NULL;
//            break;
//        case ':':
//            ret = dds_string_dup(line+1);
//            line = NULL;
//            break;
//        default:
//            printf ("unrecognised command: %s\n", line);
//            line = NULL;
//            break;
//        }
//    }
//    return ret;
//}
//
//static char *pub_do_arb(const struct writerspec *spec, struct getl_arg *getl_arg) {
//    const char *orgline;
//    char *ret = NULL;
//    int count;
//    while (ret == NULL && (orgline = getl(getl_arg, &count)) != NULL) {
//        const char *line = skipspaces(orgline);
//        if (*line) getl_enter_hist(getl_arg, orgline);
//        ret = pub_do_arb_line(spec, line);
//    }
//    return ret;
//}

static uint32_t pubthread_auto(void *vspec) {
    const struct writerspec *spec = vspec;
    assert(spec->topicsel != UNSPEC && spec->topicsel != ARB);
    pub_do_auto(spec);
    return 0;
}

static uint32_t pubthread(void *vwrspecs) {
    struct wrspeclist *wrspecs = vwrspecs;
    uint32_t seq = 0;
// TODO Upon support for ARB types, resolve the declaration of fdin
//    struct getl_arg getl_arg;
//#if USE_EDITLINE
//    getl_init_editline(&getl_arg, fdin);
//#else
//    getl_init_simple(&getl_arg, fdin);
//#endif

    struct wrspeclist *cursor = wrspecs;
    struct writerspec *spec = cursor->spec;
    char *nextspec = NULL;
    do {
        if (spec->topicsel != ARB)
            nextspec = pub_do_nonarb(spec, &seq);
//        else
//            nextspec = pub_do_arb(spec, &getl_arg);
        if (nextspec == NULL)
            spec = NULL;
        else
        {
            int cnt, pos;
            char *tmp = nextspec + strlen(nextspec);
            while (tmp > nextspec && isspace((unsigned char)tmp[-1]))
                *--tmp = 0;
            if ((sscanf(nextspec, "+%d%n", &cnt, &pos) == 1 && nextspec[pos] == 0) || ((void)(cnt = 1), strcmp(nextspec, "+") == 0)) {
                while (cnt--) cursor = cursor->next;
            } else if ((sscanf(nextspec, "-%d%n", &cnt, &pos) == 1 && nextspec[pos] == 0) || ((void)(cnt = 1), strcmp(nextspec, "-") == 0)) {
                while (cnt--) cursor = cursor->prev;
            } else if (sscanf(nextspec, "%d%n", &cnt, &pos) == 1 && nextspec[pos] == 0) {
                cursor = wrspecs; while (cnt--) cursor = cursor->next;
            } else {
                struct wrspeclist *endm = cursor, *cand = NULL;
                do {
                    if (strncmp (cursor->spec->tpname, nextspec, strlen(nextspec)) == 0) {
                        if (cand == NULL)
                            cand = cursor;
                        else {
                            printf ("%s: ambiguous writer specification\n", nextspec);
                            if (flushflag) {
                                fflush (stdout);
                            }
                            break;
                        }
                    }
                    cursor = cursor->next;
                } while (cursor != endm);
                if (cand == NULL) {
                    printf ("%s: no matching writer specification\n", nextspec);
                    if (flushflag) {
                        fflush (stdout);
                    }
                } else if (cursor != endm) { /* ambiguous case */
                    cursor = endm;
                } else {
                    cursor = cand;
                }
            }
            spec = cursor != NULL ? cursor->spec : NULL;
        }
    } while (spec);

    return 0;
}

struct eseq_admin {
    unsigned nkeys;
    unsigned nph;
    dds_instance_handle_t *ph;
    unsigned **eseq;
};

static void init_eseq_admin(struct eseq_admin *ea, unsigned nkeys) {
    ea->nkeys = nkeys;
    ea->nph = 0;
    ea->ph = NULL;
    ea->eseq = NULL;
}

static void fini_eseq_admin(struct eseq_admin *ea) {
    dds_free(ea->ph);
    for (unsigned i = 0; i < ea->nph; i++)
        dds_free(ea->eseq[i]);
    dds_free(ea->eseq);
}

static int check_eseq(struct eseq_admin *ea, unsigned seq, unsigned keyval, const dds_instance_handle_t pubhandle) {
    unsigned *eseq;
    if (keyval >= ea->nkeys)
    {
        printf ("received key %u >= nkeys %u\n", keyval, ea->nkeys);
        exit(2);
    }
    for (unsigned i = 0; i < ea->nph; i++)
        if (pubhandle == ea->ph[i])
        {
            unsigned e = ea->eseq[i][keyval];
            ea->eseq[i][keyval] = seq + ea->nkeys;
            return seq == e;
        }
    ea->ph = dds_realloc(ea->ph, (ea->nph + 1) * sizeof(*ea->ph));
    ea->ph[ea->nph] = pubhandle;
    ea->eseq = dds_realloc(ea->eseq, (ea->nph + 1) * sizeof(*ea->eseq));
    ea->eseq[ea->nph] = dds_alloc(ea->nkeys * sizeof(*ea->eseq[ea->nph]));
    eseq = ea->eseq[ea->nph];
    for (unsigned i = 0; i < ea->nkeys; i++)
        eseq[i] = seq + (i - keyval) + (i <= keyval ? ea->nkeys : 0);
    ea->nph++;
    return 1;
}

// TODO coherency - Reintroduce this into application logic where needed. dds.h has this, but returns UNSUPPORTED, so expect that for now
//static int subscriber_needs_access(dds_entity_t sub) {
//    dds_qos_t *qos;
//    int x;
//    if ((qos = dds_create_qos()) == NULL)
//        return DDS_RETCODE_OUT_OF_RESOURCES;
//    dds_qos_get(sub, qos);
//    if (qos == NULL)
//        error ("DDS_Subscriber_get_qos: error\n");
//
//    dds_presentation_access_scope_kind_t access_scope;
//    bool coherent_access;
//    bool ordered_access;
//    dds_qget_presentation(qos, &access_scope, &coherent_access, &ordered_access);
//    x = (access_scope == DDS_PRESENTATION_GROUP && coherent_access);
//    dds_free(qos);
//    return x;
//}

static uint32_t subthread(void *vspec) {
    const struct readerspec *spec = vspec;
    dds_entity_t rd = spec->rd;
// TODO coherency support
//    dds_entity_t sub = spec->sub;
//    const int need_access = subscriber_needs_access(sub);
    dds_entity_t ws;
    dds_entity_t rdcondA = 0, rdcondD = 0;
    dds_entity_t stcond = 0;
    dds_return_t rc;
    uintptr_t exitcode = 0;
    char tag[270];
    char tn[256];
    size_t nxs = 0;

    rc = dds_get_name(dds_get_topic(rd), tn, sizeof(tn));
    error_report(rc, "dds_get_name failed");
    (void)snprintf(tag, sizeof(tag), "[%u:%s]", spec->idx, tn);

    if (wait_hist_data) {
        rc = dds_reader_wait_for_historical_data(rd, wait_hist_data_timeout);
        error_report(rc, "dds_reader_wait_for_historical_data");
    }

    ws = dds_create_waitset(dp);
    rc = dds_waitset_attach(ws, termcond, termcond);
    error_abort(rc, "dds_waitset_attach(termcond)");
    nxs++;
    switch (spec->mode) {
    case MODE_NONE:
    case MODE_ZEROLOAD:
        /* no triggers */
        break;
    case MODE_PRINT:
        /* complicated triggers */
        rdcondA = dds_create_readcondition(rd, spec->use_take ? (DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ALIVE_INSTANCE_STATE | DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE)
                : (DDS_NOT_READ_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ALIVE_INSTANCE_STATE | DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE));
        error_abort(rdcondA, "dds_readcondition_create(rdcondA)");

        rc = dds_waitset_attach(ws, rdcondA, rdcondA);
        error_abort(rc, "dds_waitset_attach(rdcondA)");
        nxs++;

        rdcondD = dds_create_readcondition(rd, (DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE));
        error_abort(rdcondD, "dds_readcondition_create(rdcondD)");

        rc = dds_waitset_attach(ws, rdcondD, rdcondD);
        error_abort(rc, "dds_waitset_attach(rdcondD)");
        nxs++;
        break;
    case MODE_CHECK:
    case MODE_DUMP:
        if (!spec->polling) {
            /* fastest trigger we have */
            rc = dds_set_status_mask(rd, DDS_DATA_AVAILABLE_STATUS);
            error_abort(rc, "dds_set_status_mask(stcond)");
            rc = dds_waitset_attach(ws, rd, rd);
            error_abort(rc, "dds_waitset_attach(rd)");
            nxs++;
        }
        break;
    }

    {
        void **mseq = (void **) dds_alloc(sizeof(void*) * (spec->read_maxsamples));

        dds_sample_info_t *iseq = (dds_sample_info_t *) dds_alloc(sizeof(dds_sample_info_t) * spec->read_maxsamples);
        dds_attach_t *xs = dds_alloc(sizeof(dds_attach_t) * nxs);

        dds_time_t tstart = 0, tfirst = 0, tprint = 0;
        long long out_of_seq = 0, nreceived = 0, last_nreceived = 0;
        long long nreceived_bytes = 0, last_nreceived_bytes = 0;
        struct eseq_admin eseq_admin;
        init_eseq_admin(&eseq_admin, nkeyvals);

        int ii = 0;
        for(ii = 0; ii < (int32_t) spec->read_maxsamples; ii++) {
            mseq[ii] = NULL;
        }

        while (!termflag && !once_mode) {
            dds_time_t tnow;
            unsigned gi;

            if (spec->polling) {
                dds_sleepfor(DDS_MSECS(1)); /* 1ms sleep interval, so a bit less than 1kHz poll freq */
            } else {
                rc = dds_waitset_wait(ws, xs, nxs, DDS_INFINITY);
                if (rc < DDS_RETCODE_OK) {
                    printf ("wait: error %d\n", (int) rc);
                    if (flushflag) {
                        fflush (stdout);
                    }
                    break;
                } else if (rc == DDS_RETCODE_OK) {
                    continue;
                }
            }

            tnow = dds_time();
            for (gi = 0; gi < (spec->polling ? 1 : nxs); gi++) {
                dds_entity_t cond = !spec->polling && xs[gi] != 0 ? (dds_entity_t) xs[gi] : 0;
                dds_return_t nread;
                int32_t i;

                if (cond == termcond)
                    continue;
                if (cond == 0 && !spec->polling) {
                    break;
                }

                if (spec->print_match_pre_read) {
                    dds_subscription_matched_status_t status;
                    rc = dds_get_subscription_matched_status(rd, &status);
                    error_report(rc, "dds_get_subscription_matched_status failed");
                    if (rc == DDS_RETCODE_OK) {
                        printf("[pre-read: subscription-matched: total=(%"PRIu32" change %"PRId32") current=(%"PRIu32" change %"PRId32") handle=%"PRIu64"]\n",
                                status.total_count, status.total_count_change,
                                status.current_count,
                                status.current_count_change,
                                status.last_publication_handle);
                        if (flushflag) {
                            fflush (stdout);
                        }
                    }
                }

                /* Always take NOT_ALIVE_DISPOSED data because it means the
                 instance has reached its end-of-life.

                 NO_WRITERS I usually don't care for (though there certainly
                 are situations in which it is useful information).  But you
                 can't have a NO_WRITERS with invalid_data set:

                 - either the reader contains the instance without data in
                 the disposed state, but in that case it stays in the
                 NOT_ALIVED_DISPOSED state;

                 - or the reader doesn't have the instance yet, in which
                 case the unregister is silently discarded.

                 However, receiving an unregister doesn't turn the sample
                 into a NEW one, though.  So HOW AM I TO TRIGGER ON IT
                 without triggering CONTINUOUSLY?
                 */
                // TODO coherency support
//                if (need_access && (result = DDS_Subscriber_begin_access(sub)) != DDS_RETCODE_OK)
//                    error ("DDS_Subscriber_begin_access: %d (%s)\n", (int) result, dds_err_str(result));

                if (spec->mode == MODE_CHECK || (spec->mode == MODE_DUMP && spec->use_take) || spec->polling) {
                    nread = dds_take_mask(rd, mseq, iseq, spec->read_maxsamples, spec->read_maxsamples, DDS_ANY_STATE);
                } else if (spec->mode == MODE_DUMP) {
                    nread = dds_read_mask(rd, mseq, iseq, spec->read_maxsamples, spec->read_maxsamples, DDS_ANY_STATE);
                } else if (spec->use_take || cond == rdcondD) {
                    nread = dds_take(cond, mseq, iseq, spec->read_maxsamples, spec->read_maxsamples);
                } else {
                    nread = dds_read(cond, mseq, iseq, spec->read_maxsamples, spec->read_maxsamples);
                }

                if (nread < 1) {
                    if (spec->polling && nread == 0) {
                        ; /* expected */
                    } else if (spec->mode == MODE_CHECK || spec->mode == MODE_DUMP || spec->polling) {
                        printf ("%s: %d (%s) on %s\n", (!spec->use_take && spec->mode == MODE_DUMP) ? "read" : "take", (int) nread, dds_err_str(nread), spec->polling ? "poll" : "stcond");
                        if (flushflag) {
                            fflush (stdout);
                        }
                    } else {
                        printf ("%s: %d (%s) on rdcond%s\n", spec->use_take ? "take" : "read", (int) nread, dds_err_str(nread), (cond == rdcondA) ? "A" : (cond == rdcondD) ? "D" : "?");
                        if (flushflag) {
                            fflush (stdout);
                        }
                    }
                    continue;
                }

//                TODO coherency support
//                if (need_access && (result = DDS_Subscriber_end_access(sub)) != DDS_RETCODE_OK)
//                    error ("DDS_Subscriber_end_access: %d (%s)\n", (int) result, dds_err_str(result));

                switch (spec->mode) {
                case MODE_PRINT:
                case MODE_DUMP:
                    switch (spec->topicsel) {
                    case UNSPEC: assert(0);
                    case KS:   print_seq_KS(&tstart, tnow, rd, tag, iseq, (KeyedSeq **)mseq, nread); break;
                    case K32:  print_seq_K32(&tstart, tnow, rd, tag, iseq, (Keyed32 **)mseq, nread); break;
                    case K64:  print_seq_K64(&tstart, tnow, rd, tag, iseq, (Keyed64 **)mseq, nread); break;
                    case K128: print_seq_K128(&tstart, tnow, rd, tag, iseq, (Keyed128 **)mseq, nread); break;
                    case K256: print_seq_K256(&tstart, tnow, rd, tag, iseq, (Keyed256 **)mseq, nread); break;
                    case OU:   print_seq_OU(&tstart, tnow, rd, tag, iseq, (const OneULong **)mseq, nread); break;
                    case ARB:  print_seq_ARB(&tstart, tnow, rd, tag, iseq, (const void **)mseq, spec->tgtp); break;
                    }
                    break;

                case MODE_CHECK:
                    for (i = 0; i < nread; i++) {
                        int keyval = 0;
                        unsigned seq = 0;
                        unsigned size = 0;
                        if (!iseq[i].valid_data)
                            continue;
                        switch (spec->topicsel) {
                        case UNSPEC: assert(0);
                        case KS:   { KeyedSeq *d = (KeyedSeq *) mseq[i];  keyval = d->keyval; seq = d->seq; size = 12 + d->baggage._length; } break;
                        case K32:  { Keyed32 *d  = (Keyed32 *)  mseq[i];  keyval = d->keyval; seq = d->seq; size = 32; } break;
                        case K64:  { Keyed64 *d  = (Keyed64 *)  mseq[i];  keyval = d->keyval; seq = d->seq; size = 64; } break;
                        case K128: { Keyed128 *d = (Keyed128 *) mseq[i];  keyval = d->keyval; seq = d->seq; size = 128; } break;
                        case K256: { Keyed256 *d = (Keyed256 *) mseq[i];  keyval = d->keyval; seq = d->seq; size = 256; } break;
                        case OU:   { OneULong *d = (OneULong *) mseq[i];  keyval = 0;         seq = d->seq; size = 4; } break;
                        case ARB:  assert(0); break; /* can't check what we don't know */
                        }
                        if (!check_eseq(&eseq_admin, seq, (unsigned)keyval, iseq[i].publication_handle))
                            out_of_seq++;
                        if (nreceived == 0) {
                            tfirst = tnow;
                            tprint = tfirst;
                        }
                        nreceived++;
                        nreceived_bytes += size;
                        if (tnow - tprint >= DDS_SECS(1)) {
                            const dds_time_t tdelta_ns = tnow - tfirst;
                            const dds_time_t tdelta_s = tdelta_ns / DDS_NSECS_IN_SEC;
                            const dds_time_t tdelta_ms = ((tdelta_ns % DDS_NSECS_IN_SEC) + 500000) / DDS_NSECS_IN_MSEC;
                            const long long ndelta = nreceived - last_nreceived;
                            const double rate_Mbps = (double)(nreceived_bytes - last_nreceived_bytes) * 8 / 1e6;
                            printf ("%"PRId64".%03"PRId64" ntot %lld nseq %lld ndelta %lld rate %.2f Mb/s\n",
                                    tdelta_s, tdelta_ms, nreceived, out_of_seq, ndelta, rate_Mbps);
                            if (flushflag) {
                                fflush (stdout);
                            }
                            last_nreceived = nreceived;
                            last_nreceived_bytes = nreceived_bytes;
                            tprint = tnow;
                        }
                    }
                    break;

                case MODE_NONE:
                case MODE_ZEROLOAD:
                    break;
                }
                rc = dds_return_loan(rd, mseq, nread);
                error_report(rc, "dds_return_loan failed");
                if (spec->sleep_ns) {
                    dds_sleepfor(spec->sleep_ns);
                }
            }
        }
        dds_free(xs);

        if (spec->mode == MODE_PRINT || spec->mode == MODE_DUMP || once_mode) {
            // TODO coherency support
//            if (need_access && (result = DDS_Subscriber_begin_access (sub)) != DDS_RETCODE_OK)
//                error ("DDS_Subscriber_begin_access: %d (%s)\n", (int) result, dds_err_str (result));

            /* This is the final Read/Take */
            dds_return_t nread;
            nread = dds_take_mask(rd, mseq, iseq, spec->read_maxsamples, spec->read_maxsamples, DDS_ANY_STATE);
            if (nread == 0) {
                if (!once_mode) {
                    printf ("-- final take: data reader empty --\n");
                    if (flushflag) {
                        fflush (stdout);
                    }
                } else {
                    exitcode = 1;
                }
            } else if (nread < DDS_RETCODE_OK) {
                if (!once_mode) {
                    error_report(rc, "-- final take --\n");
                } else {
                    error_report(rc, "read/take");
                }
            } else {
                if (!once_mode)
                    printf ("-- final contents of data reader --\n");
                if (spec->mode == MODE_PRINT || spec->mode == MODE_DUMP) {
                    switch (spec->topicsel) {
                    case UNSPEC: assert(0);
                    case KS:   print_seq_KS(&tstart, dds_time(), rd, tag, iseq, (KeyedSeq **) mseq, nread); break;
                    case K32:  print_seq_K32(&tstart, dds_time(), rd, tag, iseq, (Keyed32 **) mseq, nread); break;
                    case K64:  print_seq_K64(&tstart, dds_time(), rd, tag, iseq, (Keyed64 **) mseq, nread); break;
                    case K128: print_seq_K128(&tstart, dds_time(), rd, tag, iseq, (Keyed128 **) mseq, nread); break;
                    case K256: print_seq_K256(&tstart, dds_time(), rd, tag, iseq, (Keyed256 **) mseq, nread); break;
                    case OU:   print_seq_OU(&tstart, dds_time(), rd, tag, iseq, (const OneULong **) mseq, nread); break;
                    case ARB:  print_seq_ARB(&tstart, dds_time(), rd, tag, iseq, (const void **) mseq, spec->tgtp); break;
                    }
                }
            }
            // TODO coherency support
//            if (need_access && (result = DDS_Subscriber_end_access(sub)) != DDS_RETCODE_OK)
//                error ("DDS_Subscriber_end_access: %d (%s)\n", (int) result, dds_err_str(result));
            rc = dds_return_loan(rd, mseq, nread);
            error_report(rc, "dds_return_loan failed");
        }
        dds_free(iseq);
        dds_free(mseq);
        if (spec->mode == MODE_CHECK) {
            printf ("received: %lld, out of seq: %lld\n", nreceived, out_of_seq);
            if (flushflag) {
                fflush (stdout);
            }
        }
        fini_eseq_admin(&eseq_admin);
    }

    switch (spec->mode) {
    case MODE_NONE:
    case MODE_ZEROLOAD:
        break;
    case MODE_PRINT:
        dds_waitset_detach(ws, rdcondA);
        dds_delete(rdcondA);
        dds_waitset_detach(ws, rdcondD);
        dds_delete(rdcondD);
        break;
    case MODE_CHECK:
    case MODE_DUMP:
        if (!spec->polling)
            dds_waitset_detach(ws, stcond);
        break;
    }

//    TODO Confirm that dds_delete(participant) takes care of this
//    ret = dds_waitset_detach(ws, termcond);
//    ret = dds_delete(ws);

    if (once_mode) {
        /* trigger EOF for writer side, so we actually do terminate */
        terminate();
    }
    return (uint32_t)exitcode;
}

static uint32_t autotermthread(void *varg __attribute__((unused))) {
    dds_time_t tstop, tnow;
    dds_return_t rc;
    dds_entity_t ws;

    dds_attach_t wsresults[1];
    size_t wsresultsize = 1u;

    assert(dur > 0);

    tnow = dds_time();
    dds_duration_t dds_dur = 0;
    (void) double_to_dds_duration(&dds_dur, dur);
    tstop = tnow + dds_dur;

    ws = dds_create_waitset(dp);
    rc = dds_waitset_attach(ws, termcond, termcond);
    error_abort(rc, "dds_waitset_attach(termcomd)");

    tnow = dds_time();
    while (!termflag && tnow < tstop) {
        dds_time_t dt = tstop - tnow;
        dds_duration_t timeout;
        int64_t xsec = dt / DDS_NSECS_IN_SEC;
        int64_t xnanosec = dt % DDS_NSECS_IN_SEC;
        timeout = DDS_SECS(xsec)+xnanosec;

        if ((rc = dds_waitset_wait(ws, wsresults, wsresultsize, timeout)) < DDS_RETCODE_OK) {
            printf ("wait: error %s\n", dds_err_str(rc));
            if (flushflag) {
                fflush (stdout);
            }
            break;
        }
        tnow = dds_time();
    }

    (void) dds_waitset_detach(ws, termcond);
    (void) dds_delete(ws);
    return 0;
}

static const char *execname(int argc, char *argv[]) {
    const char *p;
    if (argc == 0 || argv[0] == NULL)
        return "";
    else if ((p = strrchr(argv[0], '/')) != NULL)
        return p + 1;
    else
        return argv[0];
}

static char *read_line_from_textfile(FILE *fp) {
    char *str = NULL;
    size_t sz = 0, n = 0;
    int c;
    while ((c = fgetc(fp)) != EOF && c != '\n') {
        if (n == sz) str = dds_realloc(str, sz += 256);
        str[n++] = (char)c;
    }
    if (c != EOF || n > 0) {
        if (n == sz) str = dds_realloc(str, sz + 1);
        str[n] = 0;
    } else if (ferror(fp)) {
        error_exit("error reading file, errno = %d\n", errno);
    }
    return str;
}

static int get_metadata(char **metadata, char **typename, char **keylist, const char *file) {
    FILE *fp;
    if ((fp = fopen(file, "r")) == NULL)
        error_exit("%s: can't open for reading metadata\n", file);
    *typename = read_line_from_textfile(fp);
    *keylist = read_line_from_textfile(fp);
    *metadata = read_line_from_textfile(fp);
    if (*typename == NULL || *keylist == NULL || *typename == NULL)
        error_exit("%s: invalid metadata file\n", file);
    fclose(fp);
    return 1;
}

#if 0
static dds_entity_t find_topic(dds_entity_t dpFindTopic, const char *name, const dds_duration_t *timeout) {
    dds_entity_t tp;
    (void)timeout;
//    TODO ARB type support
//    int isbuiltin = 0;

    /* A historical accident has caused subtle issues with a generic reader for the built-in topics included in the DDS spec. */
//    if (strcmp(name, "DCPSParticipant") == 0 || strcmp(name, "DCPSTopic") == 0 ||
//            strcmp(name, "DCPSSubscription") == 0 || strcmp(name, "DCPSPublication") == 0) {
//        dds_entity_t sub;
//        if ((sub = DDS_DomainParticipant_get_builtin_subscriber(dp)) == NULL)
//            error("DDS_DomainParticipant_get_builtin_subscriber failed\n");
//        if (DDS_Subscriber_lookup_datareader(sub, name) == NULL)
//            error("DDS_Subscriber_lookup_datareader failed\n");
//        if ((result = DDS_Subscriber_delete_contained_entities(sub)) != DDS_RETCODE_OK)
//            error("DDS_Subscriber_delete_contained_entities failed: error %d (%s)\n", (int) result, dds_err_str(result));
//        isbuiltin = 1;
//    }

//    TODO Note: the implementation for dds_topic_find blocks infinitely if the topic does not exist in the domain
    if (!(tp = dds_find_topic(dpFindTopic, name))) {
        printf ("topic %s not found\n", name);
        if (flushflag) {
            fflush (stdout);
        }
    }

//    if (!isbuiltin) {
//        char *tn = DDS_Topic_get_type_name(tp);
//        char *kl = DDS_Topic_get_keylist(tp);
//        char *md = DDS_Topic_get_metadescription(tp);
//        DDS_ReturnCode_t result;
//        DDS_TypeSupport ts;
//        if ((ts = DDS_TypeSupport__alloc(tn, kl ? kl : "", md)) == NULL)
//            error("DDS_TypeSupport__alloc(%s) failed\n", tn);
//        if ((result = DDS_TypeSupport_register_type(ts, dp, tn)) != DDS_RETCODE_OK)
//            error("DDS_TypeSupport_register_type(%s) failed: %d (%s)\n", tn, (int) result, dds_err_str(result));
//        DDS_free(md);
//        DDS_free(kl);
//        DDS_free(tn);
//        DDS_free(ts);
//
//        /* Work around a double-free-at-shutdown issue caused by a find_topic
//        without a type support having been register */
//        if ((result = DDS_DomainParticipant_delete_topic(dp, tp)) != DDS_RETCODE_OK) {
//            error("DDS_DomainParticipant_find_topic failed: %d (%s)\n", (int) result, dds_err_str(result));
//        }
//        if ((tp = DDS_DomainParticipant_find_topic(dp, name, timeout)) == NULL) {
//            error("DDS_DomainParticipant_find_topic(2) failed\n");
//        }
//    }

    return tp;
}
#endif

static void set_systemid_env(void) {
//    TODO Determine encoding of dds_instance_handle_t, and see what sort of value can be extracted from it, if any
//    Unsupported

    /*uint32_t systemId, localId;
    char str[128];
    instancehandle_to_id(&systemId, &localId, DDS_Entity_get_instance_handle(dp));
    snprintf (str, sizeof(str), "%u", systemId);
    setenv("SYSTEMID", str, 1);
    snprintf (str, sizeof(str), "__NODE%08x BUILT-IN PARTITION__", systemId);
    setenv("NODE_BUILTIN_PARTITION", str, 1);*/
}

struct spec {
    dds_entity_t tp;
    dds_entity_t cftp;
    const char *topicname;
    const char *cftp_expr;
    char *metadata;
    char *typename;
    char *keylist;
    dds_duration_t findtopic_timeout;
    struct readerspec rd;
    struct writerspec wr;
    ddsrt_thread_t rdtid;
    ddsrt_thread_t wrtid;
};

static void addspec(unsigned whatfor, unsigned *specsofar, unsigned *specidx, struct spec **spec, int want_reader) {
    if (*specsofar & whatfor)
    {
        struct spec *s;
        (*specidx)++;
        *spec = dds_realloc(*spec, (*specidx + 1) * sizeof(**spec));
        s = &(*spec)[*specidx];
        s->tp = 0;
        s->cftp = 0;
        s->topicname = NULL;
        s->cftp_expr = NULL;
        s->metadata = NULL;
        s->typename = NULL;
        s->keylist = NULL;
        s->findtopic_timeout = 10;
        s->rd = def_readerspec;
        s->wr = def_writerspec;

//        TODO Upon support for ARB types, resolve the declaration of fdin
//        if (fdin == -1 && fdservsock == -1)
//            s->wr.mode = WRM_NONE;
        if (!want_reader)
            s->rd.mode = MODE_NONE;
        *specsofar = 0;
    }
    *specsofar |= whatfor;
}

static void set_print_mode(const char *modestr) {
    char *copy = dds_string_dup(modestr), *cursor = copy, *tok;
    while ((tok = ddsrt_strsep(&cursor, ",")) != NULL) {
        int enable;
        if (strncmp(tok, "no", 2) == 0) {
            enable = 0; tok += 2;
        } else {
            enable = 1;
        }
        if (strcmp(tok, "type") == 0)
            printtype = enable;
        else if (strcmp(tok, "dense") == 0)
            printmode = TGPM_DENSE;
        else if (strcmp(tok, "space") == 0)
            printmode = TGPM_SPACE;
        else if (strcmp(tok, "fields") == 0)
            printmode = TGPM_FIELDS;
        else if (strcmp(tok, "multiline") == 0)
            printmode = TGPM_MULTILINE;
        else
        {
            static struct { const char *name; unsigned flag; } tab[] = {
                    { "meta", ~0u },
                    { "trad", PM_PID | PM_TIME | PM_PHANDLE | PM_STIME | PM_STATE },
                    { "pid", PM_PID },
                    { "topic", PM_TOPIC },
                    { "time", PM_TIME },
                    { "phandle", PM_PHANDLE },
                    { "ihandle", PM_IHANDLE },
                    { "stime", PM_STIME },
                    { "dgen", PM_DGEN },
                    { "nwgen", PM_NWGEN },
                    { "ranks", PM_RANKS },
                    { "state", PM_STATE }
            };
            size_t i;
            for (i = 0; i < sizeof(tab)/sizeof(tab[0]); i++)
                if (strcmp(tok, tab[i].name) == 0)
                    break;
            if (i < sizeof(tab)/sizeof(tab[0])) {
                if (enable)
                    print_metadata |= tab[i].flag;
                else
                    print_metadata &= ~tab[i].flag;
            } else {
                fprintf (stderr, "-P %s: invalid print mode\n", modestr);
                exit(2);
            }
        }
    }
    dds_free(copy);
}

int main(int argc, char *argv[]) {
    dds_entity_t sub = 0;
    dds_entity_t pub = 0;
    dds_listener_t *rdlistener = dds_create_listener(NULL);
    dds_listener_t *wrlistener = dds_create_listener(NULL);

    dds_qos_t *qos;
    const char **qtopic = (const char **) dds_alloc(sizeof(char *) * (unsigned)argc);
    const char **qreader = (const char **) dds_alloc(sizeof(char *) * (2+(unsigned)argc));
    const char **qwriter = (const char **) dds_alloc(sizeof(char *) * (2+(unsigned)argc));
    const char **qpublisher = (const char **) dds_alloc(sizeof(char *) * (2+(unsigned)argc));
    const char **qsubscriber = (const char **) dds_alloc(sizeof(char *) * (2+(unsigned)argc));
    int nqtopic = 0, nqreader = 0, nqwriter = 0;
    int nqpublisher = 0, nqsubscriber = 0;
    int opt, pos;
    uintptr_t exitcode = 0;
    int want_reader = 1;
    int want_writer = 1;
    bool isWriterListenerSet = false;
//    int disable_signal_handlers = 0;  // TODO signal handler support
    long long sleep_at_end = 0;
    ddsrt_thread_t sigtid;
    ddsrt_thread_t inptid;
    #define SPEC_TOPICSEL 1
    #define SPEC_TOPICNAME 2
    unsigned spec_sofar = 0;
    unsigned specidx = 0;
    unsigned i;
    double wait_for_matching_reader_timeout = 0.0;
    const char *wait_for_matching_reader_arg = NULL;
    struct spec *spec = NULL;
    struct wrspeclist *wrspecs = NULL;
    memset (&sigtid, 0, sizeof(sigtid));
    memset (&inptid, 0, sizeof(inptid));
    ddsrt_mutex_init(&output_mutex);

    if (ddsrt_strcasecmp(execname(argc, argv), "sub") == 0)
        want_writer = 0;
    else if(ddsrt_strcasecmp(execname(argc, argv), "pub") == 0)
        want_reader = 0;

    save_argv0 (argv[0]);
    pid = (int) ddsrt_getpid();

    qreader[0] = "k=all";
    qreader[1] = "R=10000/inf/inf";
    nqreader = 2;

    qwriter[0] = "k=all";
    qwriter[1] = "R=100/inf/inf";
    nqwriter = 2;

    spec_sofar = SPEC_TOPICSEL;
    specidx--;
    addspec(SPEC_TOPICSEL, &spec_sofar, &specidx, &spec, want_reader);
    spec_sofar = 0;
    assert(specidx == 0);

    while ((opt = getopt(argc, argv, "!@*:FK:T:D:q:m:M:n:OP:rRs:S:U:W:w:z:")) != EOF) {
        switch (opt) {
        case '!':
//            disable_signal_handlers = 1; // TODO signal handler support
            break;
        case '@':
            spec[specidx].wr.duplicate_writer_flag = 1;
            break;
        case '*':
            {
                sleep_at_end = 0;
                (void)ddsrt_atoll(optarg, &sleep_at_end);
            }
            break;
        case 'M':
            if (sscanf(optarg, "%lf:%n", &wait_for_matching_reader_timeout, &pos) != 1) {
                fprintf (stderr, "-M %s: invalid timeout\n", optarg);
                exit(2);
            }
            wait_for_matching_reader_arg = optarg + pos;
            break;
        case 'F':
            flushflag = 1;
            break;
        case 'K':
            addspec(SPEC_TOPICSEL, &spec_sofar, &specidx, &spec, want_reader);
            if (ddsrt_strcasecmp(optarg, "KS") == 0)
                spec[specidx].rd.topicsel = spec[specidx].wr.topicsel = KS;
            else if (ddsrt_strcasecmp(optarg, "K32") == 0)
                spec[specidx].rd.topicsel = spec[specidx].wr.topicsel = K32;
            else if (ddsrt_strcasecmp(optarg, "K64") == 0)
                spec[specidx].rd.topicsel = spec[specidx].wr.topicsel = K64;
            else if (ddsrt_strcasecmp(optarg, "K128") == 0)
                spec[specidx].rd.topicsel = spec[specidx].wr.topicsel = K128;
            else if (ddsrt_strcasecmp(optarg, "K256") == 0)
                spec[specidx].rd.topicsel = spec[specidx].wr.topicsel = K256;
            else if (ddsrt_strcasecmp(optarg, "OU") == 0)
                spec[specidx].rd.topicsel = spec[specidx].wr.topicsel = OU;
            else if (ddsrt_strcasecmp(optarg, "ARB") == 0)
                spec[specidx].rd.topicsel = spec[specidx].wr.topicsel = ARB;
            else if (get_metadata(&spec[specidx].metadata, &spec[specidx].typename, &spec[specidx].keylist, optarg))
                spec[specidx].rd.topicsel = spec[specidx].wr.topicsel = ARB;
            else {
                fprintf (stderr, "-K %s: unknown type\n", optarg);
                exit(2);
            }
            break;
        case 'T': {
            char *p;
            addspec(SPEC_TOPICNAME, &spec_sofar, &specidx, &spec, want_reader);
            spec[specidx].topicname = (const char *) dds_string_dup(optarg);
            if ((p = strchr(spec[specidx].topicname, ':')) != NULL) {
                double d;
                int dpos, have_to = 0;
                *p++ = 0;
                if (strcmp (p, "inf") == 0 || strncmp (p, "inf:", 4) == 0) {
                    have_to = 1;
                    set_infinite_dds_duration(&spec[specidx].findtopic_timeout);
                } else if (sscanf(p, "%lf%n", &d, &dpos) == 1 && (p[dpos] == 0 || p[dpos] == ':')) {
                    if (double_to_dds_duration(&spec[specidx].findtopic_timeout, d) < 0)
                        error_exit("-T %s: %s: duration invalid\n", optarg, p);
                    have_to = 1;
                } else {
                    /* assume content filter */
                }
                if (have_to && (p = strchr(p, ':')) != NULL) {
                    p++;
                }
            }
            if (p != NULL) {
                spec[specidx].cftp_expr = p;
            }
            break;
        }
        case 'q':
            if (strncmp(optarg, "provider=", 9) == 0) {
                set_qosprovider(optarg+9);
            } else {
                size_t n = strspn(optarg, "atrwps");
                const char *colon = strchr(optarg, ':');
                if (colon == NULL || n == 0 || n != (size_t) (colon - optarg)) {
                    fprintf (stderr, "-q %s: flags indicating to which entities QoS's apply must match regex \"[^atrwps]+:\"\n", optarg);
                    exit(2);
                } else {
                    const char *q = colon+1;
                    for (const char *flag = optarg; flag != colon; flag++)
                        switch (*flag) {
                        case 't': qtopic[nqtopic++] = q; break;
                        case 'r': qreader[nqreader++] = q; break;
                        case 'w': qwriter[nqwriter++] = q; break;
                        case 'p': qpublisher[nqpublisher++] = q; break;
                        case 's': qsubscriber[nqsubscriber++] = q; break;
                        case 'a':
                            qtopic[nqtopic++] = q;
                            qreader[nqreader++] = q;
                            qwriter[nqwriter++] = q;
                            qpublisher[nqpublisher++] = q;
                            qsubscriber[nqsubscriber++] = q;
                            break;
                        default:
                            assert(0);
                        }
                }
            }
            break;
        case 'D':
            dur = atof(optarg);
            break;
        case 'm':
            spec[specidx].rd.polling = 0;
            if (strcmp(optarg, "0") == 0) {
                spec[specidx].rd.mode = MODE_NONE;
            } else if (strcmp(optarg, "p") == 0) {
                spec[specidx].rd.mode = MODE_PRINT;
            } else if (strcmp(optarg, "pp") == 0) {
                spec[specidx].rd.mode = MODE_PRINT; spec[specidx].rd.polling = 1;
            } else if (strcmp(optarg, "c") == 0) {
                spec[specidx].rd.mode = MODE_CHECK;
            } else if (sscanf(optarg, "c:%u%n", &nkeyvals, &pos) == 1 && optarg[pos] == 0) {
                spec[specidx].rd.mode = MODE_CHECK;
            } else if (strcmp(optarg, "cp") == 0) {
                spec[specidx].rd.mode = MODE_CHECK; spec[specidx].rd.polling = 1;
            } else if (sscanf(optarg, "cp:%u%n", &nkeyvals, &pos) == 1 && optarg[pos] == 0) {
                spec[specidx].rd.mode = MODE_CHECK; spec[specidx].rd.polling = 1;
            } else if (strcmp(optarg, "z") == 0) {
                spec[specidx].rd.mode = MODE_ZEROLOAD;
            } else if (strcmp(optarg, "d") == 0) {
                spec[specidx].rd.mode = MODE_DUMP;
            } else if (strcmp(optarg, "dp") == 0) {
                spec[specidx].rd.mode = MODE_DUMP; spec[specidx].rd.polling = 1;
            } else {
                fprintf (stderr, "-m %s: invalid mode\n", optarg);
                exit(2);
            }
            break;
        case 'w': {
            int port;
            spec[specidx].wr.writerate = 0.0;
            spec[specidx].wr.burstsize = 1;
            if (strcmp(optarg, "-") == 0) {
                spec[specidx].wr.mode = WRM_INPUT;
            } else if (sscanf(optarg, "%u%n", &nkeyvals, &pos) == 1 && optarg[pos] == 0) {
                spec[specidx].wr.mode = (nkeyvals == 0) ? WRM_NONE : WRM_AUTO;
            } else if (sscanf(optarg, "%u:%lf*%u%n", &nkeyvals, &spec[specidx].wr.writerate, &spec[specidx].wr.burstsize, &pos) == 3
                    && optarg[pos] == 0) {
                spec[specidx].wr.mode = (nkeyvals == 0) ? WRM_NONE : WRM_AUTO;
            } else if (sscanf(optarg, "%u:%lf%n", &nkeyvals, &spec[specidx].wr.writerate, &pos) == 2
                    && optarg[pos] == 0) {
                spec[specidx].wr.mode = (nkeyvals == 0) ? WRM_NONE : WRM_AUTO;
            } else if (sscanf(optarg, ":%d%n", &port, &pos) == 1 && optarg[pos] == 0) {
                fprintf (stderr, "listen on TCP port P: not supported\n");
                exit(1);
            } else {
                spec[specidx].wr.mode = WRM_INPUT;
                fprintf (stderr, "%s: can't open\n", optarg);
                exit(1);
            }
            break;
        }
        case 'n':
            spec[specidx].rd.read_maxsamples = (uint32_t)atoi(optarg);
            break;
        case 'O':
            once_mode = 1;
            break;
        case 'P':
            set_print_mode(optarg);
            break;
        case 'R':
            spec[specidx].rd.use_take = 0;
            break;
        case 'r':
            spec[specidx].wr.register_instances = 1;
            break;
        case 's':
            spec[specidx].rd.sleep_ns = DDS_MSECS((int64_t) atoi(optarg));
            break;
        case 'W': {
            double t;
            wait_hist_data = 1;
            if (strcmp(optarg, "inf") == 0)
                set_infinite_dds_duration(&wait_hist_data_timeout);
            else if (sscanf(optarg, "%lf%n", &t, &pos) == 1 && optarg[pos] == 0 && t >= 0)
                double_to_dds_duration(&wait_hist_data_timeout, t);
            else {
                fprintf (stderr, "-W %s: invalid duration\n", optarg);
                exit(2);
            }
        }
        break;
        case 'S': {
            char *copy = dds_string_dup(optarg), *tok, *cursor = copy;
            if (copy == NULL)
                abort();
            tok = ddsrt_strsep(&cursor, ",");
            while (tok) {
                if (strcmp(tok, "pr") == 0 || strcmp(tok, "pre-read") == 0)
                    spec[specidx].rd.print_match_pre_read = 1;
                else if (strcmp(tok, "sl") == 0 || strcmp(tok, "sample-lost") == 0)
                    dds_lset_sample_lost(rdlistener, rd_on_sample_lost);
                else if (strcmp(tok, "sr") == 0 || strcmp(tok, "sample-rejected") == 0)
                    dds_lset_sample_rejected(rdlistener, rd_on_sample_rejected);
                else if (strcmp(tok, "lc") == 0 || strcmp(tok, "liveliness-changed") == 0)
                    dds_lset_liveliness_changed(rdlistener, rd_on_liveliness_changed);
                else if (strcmp(tok, "sm") == 0 || strcmp(tok, "subscription-matched") == 0)
                    dds_lset_subscription_matched(rdlistener, rd_on_subscription_matched);
                else if (strcmp(tok, "ll") == 0 || strcmp(tok, "liveliness-lost") == 0) {
                    dds_lset_liveliness_lost(wrlistener, wr_on_liveliness_lost);
                    isWriterListenerSet = true;
                } else if (strcmp(tok, "odm") == 0 || strcmp(tok, "offered-deadline-missed") == 0) {
                    dds_lset_offered_deadline_missed(wrlistener, wr_on_offered_deadline_missed);
                    isWriterListenerSet = true;
                } else if (strcmp(tok, "pm") == 0 || strcmp(tok, "publication-matched") == 0) {
                    dds_lset_publication_matched(wrlistener, wr_on_publication_matched);
                    isWriterListenerSet = true;
                } else if (strcmp(tok, "rdm") == 0 || strcmp(tok, "requested-deadline-missed") == 0)
                    dds_lset_requested_deadline_missed(rdlistener, rd_on_requested_deadline_missed);
                else if (strcmp(tok, "riq") == 0 || strcmp(tok, "requested-incompatible-qos") == 0)
                    dds_lset_requested_incompatible_qos(rdlistener, rd_on_requested_incompatible_qos);
                else if (strcmp(tok, "oiq") == 0 || strcmp(tok, "offered-incompatible-qos") == 0) {
                    dds_lset_offered_incompatible_qos(wrlistener, wr_on_offered_incompatible_qos);
                    isWriterListenerSet = true;
                } else {
                    fprintf (stderr, "-S %s: invalid event\n", tok);
                    exit(2);
                }
                tok = ddsrt_strsep(&cursor, ",");
            }
            dds_free(copy);
        }
        break;
        case 'z': {
            /* payload is int32 int32 seq<octet>, which we count as 16+N,
                for a 4 byte sequence length */
            int tmp = atoi(optarg);
            if (tmp != 0 && tmp < 12) {
                fprintf (stderr, "-z %s: minimum is 12\n", optarg);
                exit(1);
            } else if (tmp == 0)
                spec[specidx].wr.baggagesize = 0;
            else
                spec[specidx].wr.baggagesize = (unsigned) (tmp - 12);
            break;
        }
        default:
            usage(argv[0]);
        }
    }

    if (argc - optind < 1) {
        usage(argv[0]);
    }

    for (i = 0; i <= specidx; i++) {
        assert(spec[i].rd.topicsel == spec[i].wr.topicsel);

        if (spec[i].rd.topicsel == UNSPEC)
            spec[i].rd.topicsel = spec[i].wr.topicsel = KS;

        if (spec[i].topicname == NULL) {
            switch (spec[i].rd.topicsel) {
            case UNSPEC: assert(0);
            case KS: spec[i].topicname = dds_string_dup("PubSub"); break;
            case K32: spec[i].topicname = dds_string_dup("PubSub32"); break;
            case K64: spec[i].topicname = dds_string_dup("PubSub64"); break;
            case K128: spec[i].topicname = dds_string_dup("PubSub128"); break;
            case K256: spec[i].topicname = dds_string_dup("PubSub256"); break;
            case OU: spec[i].topicname = dds_string_dup("PubSubOU"); break;
            case ARB: error_exit("-K ARB requires specifying a topic name\n"); break;
            }
            assert(spec[i].topicname != NULL);
        }
        assert(spec[i].rd.topicsel != UNSPEC && spec[i].rd.topicsel == spec[i].wr.topicsel);
    }

    if (!isWriterListenerSet) {
        want_writer = 0;
        want_reader = 0;
        for (i = 0; i <= specidx; i++) {
            if (spec[i].rd.mode != MODE_NONE)
                want_reader = 1;
            switch(spec[i].wr.mode) {
            case WRM_NONE:
                break;
            case WRM_AUTO:
                want_writer = 1;
                if (spec[i].wr.topicsel == ARB)
                    error_exit("auto-write mode requires non-ARB topic\n");
                break;
            case WRM_INPUT:
                want_writer = 1;
            }
        }
    }

    for (i = 0; i <= specidx; i++) {
        if (spec[i].rd.topicsel == OU) {
            /* by definition only 1 instance for OneULong type */
            nkeyvals = 1;
            if (spec[i].rd.topicsel == ARB) {
//                TODO ARB type support
//                if (((spec[i].rd.mode != MODE_PRINT || spec[i].rd.mode != MODE_DUMP) && spec[i].rd.mode != MODE_NONE) || (fdin == -1 && fdservsock == -1))
//                    error("-K ARB requires readers in PRINT or DUMP mode and writers in interactive mode\n");
//                if (nqtopic != 0 && spec[i].metadata == NULL)
//                    error("-K ARB disallows specifying topic QoS when using find_topic\n");
            }
        }
        if (spec[i].rd.mode == MODE_ZEROLOAD)
        {
            /* need to change to keep-last-1 (unless overridden by user) */
            qreader[0] = "k=1";
        }
    }

    common_init(argv[0]);
    set_systemid_env();
    dds_write_set_batch(true); // FIXME: hack (the global batching flag is a hack anyway)

    {
        char **ps = (char **) dds_alloc(sizeof(char *) * (uint32_t)(argc - optind));
        for (i = 0; i < (unsigned) (argc - optind); i++)
            ps[i] = ddsrt_expand_envvars_sh(argv[(unsigned) optind + i], 0);
        if (want_reader) {
            qos = dds_create_qos();
            setqos_from_args(DDS_KIND_SUBSCRIBER, qos, nqsubscriber, qsubscriber);
            sub = new_subscriber(qos, (unsigned) (argc - optind), (const char **) ps);
            dds_delete_qos(qos);
        }
        if (want_writer) {
            qos = dds_create_qos();
            setqos_from_args(DDS_KIND_PUBLISHER, qos, nqpublisher, qpublisher);
            pub = new_publisher(qos, (unsigned) (argc - optind), (const char **) ps);
            dds_delete_qos(qos);
        }
        for (i = 0; i < (unsigned) (argc - optind); i++)
            dds_free(ps[i]);
        dds_free(ps);
    }


    for (i = 0; i <= specidx; i++) {
        qos = new_tqos();
        setqos_from_args(DDS_KIND_TOPIC, qos, nqtopic, qtopic);
        switch (spec[i].rd.topicsel) {
        case UNSPEC: assert(0); break;
        case KS:   spec[i].tp = new_topic(spec[i].topicname, ts_KeyedSeq, qos); break;
        case K32:  spec[i].tp = new_topic(spec[i].topicname, ts_Keyed32, qos); break;
        case K64:  spec[i].tp = new_topic(spec[i].topicname, ts_Keyed64, qos); break;
        case K128: spec[i].tp = new_topic(spec[i].topicname, ts_Keyed128, qos); break;
        case K256: spec[i].tp = new_topic(spec[i].topicname, ts_Keyed256, qos); break;
        case OU:   spec[i].tp = new_topic(spec[i].topicname, ts_OneULong, qos); break;
        case ARB:
            // TODO ARB type support
#if 1
            error_exit("Currently doesn't support ARB type\n");
#else
            if (spec[i].metadata == NULL) {
                if (!(spec[i].tp = find_topic(dp, spec[i].topicname, &spec[i].findtopic_timeout)))
                    error_exit("topic %s not found\n", spec[i].topicname);
            } else  {
//                const dds_topic_descriptor_t* ts = dds_topic_descriptor_create(spec[i].typename, spec[i].keylist, spec[i].metadata); //Todo: Not available in cham dds.h
                const dds_topic_descriptor_t* ts = NULL;
                if(ts == NULL)
                    error_exit("dds_topic_descriptor_create(%s) failed\n",spec[i].typename);
                spec[i].tp = new_topic(spec[i].topicname, ts, qos);
//                dds_topic_descriptor_delete((dds_topic_descriptor_t*) ts);
            }
//            spec[i].rd.tgtp = spec[i].wr.tgtp = tgnew(spec[i].tp, printtype);
#endif
            break;
        }
        assert(spec[i].tp);
//        assert(spec[i].rd.topicsel != ARB || spec[i].rd.tgtp != NULL);
//        assert(spec[i].wr.topicsel != ARB || spec[i].wr.tgtp != NULL);
        dds_delete_qos(qos);

        if (spec[i].cftp_expr == NULL)
            spec[i].cftp = spec[i].tp;
        else {
            fprintf (stderr,"C99 API doesn't support the creation of content filtered topic.\n");
            spec[i].cftp = spec[i].tp;
//            TODO Content Filtered Topic support
//            char name[40], *expr = expand_envvars_sh(spec[i].cftp_exp, 0);
//            DDS_StringSeq *params = DDS_StringSeq__alloc();
//            snprintf (name, sizeof (name), "cft%u", i);
//            if ((spec[i].cftp = DDS_DomainParticipant_create_contentfilteredtopic(dp, name, spec[i].tp, expr, params)) == NULL)
//                error("DDS_DomainParticipant_create_contentfiltered_topic failed\n");
//            DDS_free(params);
//            free(expr);
        }

        if (spec[i].rd.mode != MODE_NONE) {
            qos = new_rdqos(spec[i].cftp);
            setqos_from_args(DDS_KIND_READER, qos, nqreader, qreader);
            spec[i].rd.rd = new_datareader_listener(sub, spec[i].cftp, qos, rdlistener);
            spec[i].rd.sub = sub;
            dds_delete_qos(qos);
        }

        if (spec[i].wr.mode != WRM_NONE) {
            qos = new_wrqos(spec[i].tp);
            setqos_from_args(DDS_KIND_WRITER, qos, nqwriter, qwriter);
            spec[i].wr.wr = new_datawriter_listener(pub, spec[i].tp, qos, wrlistener);
            spec[i].wr.pub = pub;
            if (spec[i].wr.duplicate_writer_flag) {
                spec[i].wr.dupwr = dds_create_writer(pub, spec[i].tp, qos, NULL);
                error_abort(spec[i].wr.dupwr, "dds_writer_create failed");
            }
            dds_delete_qos(qos);
        }
    }

    if (want_writer && wait_for_matching_reader_arg) {
        printf("Wait for matching reader: unsupported\n");
        if (flushflag) {
            fflush (stdout);
        }
//        TODO Reimplement wait_for_matching_reader functionality via wait on status subscription matched
//        struct qos *q = NULL;
//        uint64_t tnow = dds_time();
//        uint64_t tend = tnow + (uint64_t) (wait_for_matching_reader_timeout >= 0 ? (wait_for_matching_reader_timeout * 1e9 + 0.5) : 0);
//        DDS_InstanceHandleSeq *sh = DDS_InstanceHandleSeq__alloc();
//        dds_instance_handle_t pphandle;
//        DDS_ReturnCode_t ret;
//        DDS_ParticipantBuiltinTopicData *ppdata = DDS_ParticipantBuiltinTopicData__alloc();
//        const DDS_UserDataQosPolicy *udqos;
//        unsigned m;
//        if ((pphandle = DDS_DomainParticipant_get_instance_handle(dp)) == 0)
//            error("DDS_DomainParticipant_get_instance_handle failed\n");
//        if ((ret = DDS_DomainParticipant_get_discovered_participant_data(dp, ppdata, pphandle)) != DDS_RETCODE_OK)
//            error("DDS_DomainParticipant_get_discovered_participant_data failed: %d (%s)\n", (int) ret, dds_err_str(ret));
//        q = new_wrqos(pub, spec[0].tp);
//        qos_user_data(q, wait_for_matching_reader_arg);
//        udqos = &qos_datawriter(q)->user_data;
//        do {
//            for (i = 0, m = specidx + 1; i <= specidx; i++) {
//                if (spec[i].wr.mode == WM_NONE)
//                    --m;
//                else if ((ret = DDS_DataWriter_get_matched_subscriptions(spec[i].wr.wr, sh)) != DDS_RETCODE_OK)
//                    error("DDS_DataWriter_get_matched_subscriptions failed: %d (%s)\n", (int) ret, dds_err_str(ret));
//                else {
//                    unsigned j;
//                    for(j = 0; j < sh->_length; j++) {
//                        DDS_SubscriptionBuiltinTopicData *d = DDS_SubscriptionBuiltinTopicData__alloc();
//                        if ((ret = DDS_DataWriter_get_matched_subscription_data(spec[i].wr.wr, d, sh->_buffer[j])) != DDS_RETCODE_OK)
//                            error("DDS_DataWriter_get_matched_subscription_data(wr %u ih %llx) failed: %d (%s)\n", specidx, sh->_buffer[j], (int) ret, dds_err_str(ret));
//                        if (memcmp(d->participant_key, ppdata->key, sizeof(ppdata->key)) != 0 &&
//                                d->user_data.value._length == udqos->value._length &&
//                                (d->user_data.value._length == 0 || memcmp(d->user_data.value._buffer, udqos->value._buffer, udqos->value._length) == 0)) {
//                            --m;
//                            DDS_free(d);
//                            break;
//                        }
//                        DDS_free(d);
//                    }
//                }
//            }
//            tnow = dds_time();
//            if (m != 0 && tnow < tend) {
//                uint64_t tdelta = (tend-tnow) < DDS_NSECS_IN_SEC/10 ? tend-tnow : DDS_NSECS_IN_SEC/10;
//                os_time delay = { (os_timeSec) (tdelta / DDS_NSECS_IN_SEC), (os_int32) (tdelta % DDS_NSECS_IN_SEC)};
//                os_nanoSleep(delay);
//                tnow = dds_time();
//            }
//        } while(m != 0 && tnow < tend);
//        free_qos(q);
//        DDS_free(ppdata);
//        DDS_free(sh);
//        if (m != 0)
//            error("timed out waiting for matching subscriptions\n");
    }

    termcond = dds_create_waitset(dp); // Waitset serves as GuardCondition here.
    error_abort(termcond, "dds_create_waitset failed");

    ddsrt_threadattr_t attr;
    ddsrt_threadattr_init(&attr);
    dds_return_t osres;

    if (want_writer) {
        for (i = 0; i <= specidx; i++) {
            struct wrspeclist *wsl;
            switch (spec[i].wr.mode) {
            case WRM_NONE:
                break;
            case WRM_AUTO:
                osres = ddsrt_thread_create(&spec[i].wrtid, "pubthread_auto", &attr, pubthread_auto, &spec[i].wr);
                os_error_exit(osres, "Error: cannot create thread pubthread_auto");
                break;
            case WRM_INPUT:
                wsl = dds_alloc(sizeof(*wsl));
                spec[i].wr.tpname = dds_string_dup(spec[i].topicname);
                wsl->spec = &spec[i].wr;
                if (wrspecs) {
                    wsl->next = wrspecs->next;
                    wrspecs->next = wsl;
                } else {
                    wsl->next = wsl;
                }
                wrspecs = wsl;
                break;
            }
        }
        if (wrspecs) { /* start with first wrspec */
            wrspecs = wrspecs->next;
            osres = ddsrt_thread_create(&inptid, "pubthread", &attr, pubthread, wrspecs);
            os_error_exit(osres, "Error: cannot create thread pubthread");
        }
    } else if (dur > 0) { /* note: abusing inptid */
        osres = ddsrt_thread_create(&inptid, "autotermthread", &attr, autotermthread, NULL);
        os_error_exit(osres, "Error: cannot create thread autotermthread");
    }

    for (i = 0; i <= specidx; i++) {
        if (spec[i].rd.mode != MODE_NONE) {
            spec[i].rd.idx = i;
            osres = ddsrt_thread_create(&spec[i].rdtid, "subthread", &attr, subthread, &spec[i].rd);
            os_error_exit(osres, "Error: cannot create thread subthread");
        }
    }

    if (want_writer || dur > 0) {
        int term_called = 0;
        if (!want_writer || wrspecs) {
            (void)ddsrt_thread_join(inptid, NULL);
            term_called = 1;
            terminate();
        }
        for (i = 0; i <= specidx; i++) {
            if (spec[i].wr.mode == WRM_AUTO)
                (void)ddsrt_thread_join(spec[i].wrtid, NULL);
        }
        if (!term_called)
            terminate();
    }

    if (want_reader) {
        uint32_t ret;
        exitcode = 0;
        for (i = 0; i <= specidx; i++) {
            if (spec[i].rd.mode != MODE_NONE) {
                (void)ddsrt_thread_join(spec[i].rdtid, &ret);
                if ((uintptr_t) ret > exitcode)
                    exitcode = (uintptr_t) ret;
            }
        }
    }

    if (wrspecs) {
        struct wrspeclist *m;
        m = wrspecs->next;
        wrspecs->next = NULL;
        wrspecs = m;
        while ((m = wrspecs) != NULL) {
            wrspecs = wrspecs->next;
            dds_free(m);
        }
    }

    dds_delete_listener(wrlistener);
    dds_delete_listener(rdlistener);

    dds_free((char **) qtopic);
    dds_free((char **) qpublisher);
    dds_free((char **) qsubscriber);
    dds_free((char **) qreader);
    dds_free((char **) qwriter);

    for (i = 0; i <= specidx; i++) {
        if(spec[i].topicname) dds_free((char *)spec[i].topicname);
        if(spec[i].cftp_expr) dds_free((char *)spec[i].cftp_expr);
        if(spec[i].metadata) dds_free(spec[i].metadata);
        if(spec[i].typename) dds_free(spec[i].typename);
        if(spec[i].keylist) dds_free(spec[i].keylist);
        assert(spec[i].wr.tgtp == spec[i].rd.tgtp); /* so no need to free both */
//        TODO ARB type support
//        if (spec[i].rd.tgtp)
//            tgfree(spec[i].rd.tgtp);
//        if (spec[i].wr.tgtp)
//            tgfree(spec[i].wr.tgtp);
        if (spec[i].wr.tpname)
            dds_string_free(spec[i].wr.tpname);
    }
    dds_free(spec);

//    dds_delete(termcond);
    common_fini ();
    if (sleep_at_end) {
        dds_sleepfor(DDS_SECS(sleep_at_end));
    }
    ddsrt_mutex_destroy(&output_mutex);
    return (int) exitcode;
}
