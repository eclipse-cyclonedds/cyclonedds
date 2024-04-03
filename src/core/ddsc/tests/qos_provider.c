// Copyright(c) 2024 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#include "CUnit/Theory.h"
#include "dds/dds.h"

#include "dds/ddsc/dds_public_qos_provider.h"

#include "dds/ddsrt/hopscotch.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/random.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/time.h"
#include "dds/ddsrt/io.h"

#include "dds__qos.h"
#include "dds__sysdef_model.h"
#include "dds__qos_provider.h"

#define DEF(libs) \
  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<dds>"libs"\n</dds>"
#define LIB(lib_name,profiles) \
  "\n <qos_library name=\""#lib_name"\">"profiles"\n </qos_library>"
#define N_LIB(profiles) \
  "\n <qos_library>"profiles"\n </qos_library>"
#define PRO(prof_name,ents) \
  "\n  <qos_profile name=\""#prof_name"\">"ents"\n  </qos_profile>"
#define N_PRO(ents) \
  "\n  <qos_profile>"ents"\n  </qos_profile>"
#define ENT(qos,kind) \
  "\n   <" #kind "_qos>"qos"\n   </" #kind "_qos>"
#define ENT_N(nm,qos,kind) \
  "\n   <" #kind "_qos name=\""#nm"\">"qos"\n   </" #kind "_qos>"


CU_TheoryDataPoints(qos_provider, create) = {
  // The various of sysdef configuration files
  CU_DataPoints(char *,
    DEF(LIB(lib0,PRO(pro0,ENT("",datareader)ENT("",datawriter)
                          ENT("",publisher)ENT("",subscriber)
                          ENT("",domain_participant)ENT("",topic)))),
    DEF(LIB(lib0,PRO(pro0,ENT("",datareader)
                          ENT_N(rd0,"",datareader)))),
    DEF(LIB(lib0,PRO(pro0,ENT_N(rd0,"",datareader)
                          ENT_N(rd0,"",datareader)))),
    DEF(LIB(lib0,PRO(pro0,ENT("",datareader)
                          ENT("",datareader)))),
    DEF(LIB(lib0,PRO(pro0,ENT("",datareader))
                 PRO(pro1,ENT("",datareader)))),
    DEF(LIB(lib0,PRO(pro0,ENT("",datareader)))
        LIB(lib1,PRO(pro0,ENT("",datareader)))),
    DEF(LIB(lib0,PRO(pro0,ENT("",datareader))
                 PRO(pro0,ENT("",datareader)))),
    DEF(LIB(lib0,PRO(pro0,ENT("",datareader)))
        LIB(lib0,PRO(pro1,ENT("",datareader)))),
    DEF(LIB(lib0,N_PRO(   ENT("",datareader)))),
    DEF(N_LIB(   PRO(pro0,ENT("",datareader)))),
  ),
  // Expected retcodes
  CU_DataPoints(int32_t,
    DDS_RETCODE_OK,
    DDS_RETCODE_OK,
    DDS_RETCODE_BAD_PARAMETER,
    DDS_RETCODE_BAD_PARAMETER,
    DDS_RETCODE_OK,
    DDS_RETCODE_OK,
    DDS_RETCODE_BAD_PARAMETER,
    DDS_RETCODE_BAD_PARAMETER,
    DDS_RETCODE_ERROR,
    DDS_RETCODE_ERROR,
  )
};
// @brief This tests creating qos_provider with different sysdef files.
CU_Theory((char *configuration, dds_return_t ret), qos_provider, create)
{
  dds_qos_provider_t *provider = NULL;
  // init qos provider with given configuration file
  dds_return_t ret_ac = dds_create_qos_provider(configuration, &provider);
  if (ret_ac == DDS_RETCODE_OK)
    dds_delete_qos_provider(provider);
  CU_ASSERT_EQUAL(ret, ret_ac);
}

#define NO_QOS_PROVIDER_CONF \
  DEF( \
    LIB(lib0, \
      PRO(pro00, \
        ENT("",datareader)ENT("",datawriter) \
        ENT_N(rd1,"",datareader) \
        ENT("",publisher)ENT("",subscriber) \
        ENT("",domain_participant)ENT("",topic)) \
      PRO(pro01, \
        ENT_N(rd0,"",datareader)ENT("",datawriter) \
        ENT("",publisher)ENT("",subscriber) \
        ENT("",domain_participant)ENT("",topic)) \
      PRO(pro02, \
        ENT_N(rd0,"",datareader)ENT_N(wr0,"",datawriter) \
        ENT("",publisher)ENT("",subscriber) \
        ENT("",domain_participant)ENT("",topic)) \
      PRO(pro03, \
        ENT_N(rd0,"",datareader)ENT_N(wr0,"",datawriter) \
        ENT_N(rd1,"",datareader)ENT_N(wr1,"",datawriter) \
        ENT_N(pb0,"",publisher)ENT("",subscriber) \
        ENT("",domain_participant)ENT("",topic)) \
    ) \
    LIB(lib1, \
      PRO(pro00, \
        ENT("",datareader)ENT("",datawriter) \
        ENT("",publisher)ENT("",subscriber) \
        ENT("",domain_participant)ENT("",topic)) \
      PRO(pro01, \
        ENT("",datareader)ENT("",datawriter) \
        ENT("",publisher)ENT("",subscriber) \
        ENT("",domain_participant)ENT_N(tp0,"",topic)) \
      PRO(pro02, \
        ENT("",datareader)ENT("",datawriter) \
        ENT("",publisher)ENT("",subscriber) \
        ENT_N(pp0,"",domain_participant)ENT_N(tp0,"",topic)) \
      PRO(pro03, \
        ENT("",datareader)ENT("",datawriter) \
        ENT("",publisher)ENT_N(sb0,"",subscriber) \
        ENT_N(pp0,"",domain_participant)ENT_N(tp0,"",topic)) \
    ) \
    LIB(lib2, \
      PRO(pro01, \
        ENT_N(rd0,"",datareader)ENT_N(tp0,"",topic)) \
      PRO(pro02, \
        ENT_N(rd0,"",datareader)ENT_N(wr0,"",datawriter) \
        ENT_N(pp0,"",domain_participant)ENT_N(tp0,"",topic)) \
      PRO(pro03, \
        ENT_N(rd0,"",datareader)ENT_N(wr0,"",datawriter) \
        ENT_N(pb0,"",publisher)ENT_N(sb0,"",subscriber) \
        ENT_N(pp0,"",domain_participant)ENT_N(tp0,"",topic)) \
    ) \
  ) \

typedef struct cscope_tokens
{
  char *scope;
  char *expct;
} cscope_tokens_t;

#define SCOPE(scp,exp) {.scope=scp, .expct=exp}
#define N NO_QOS_PROVIDER_CONF
CU_TheoryDataPoints(qos_provider, create_scope) = {
  // The sysdef file with multiple libs/profiles/entity qos.
  CU_DataPoints(char *,
    N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N),
  // The scopes for initialize qos_provider (scope)
  // and pattern that all qos in qos_provider should match with (pattern)
  // SCOPE(<scope>,<pattern>)
  CU_DataPoints(cscope_tokens_t,
    SCOPE("*","::"), SCOPE("lib1","lib1::"), SCOPE("lib0::*","lib0::"),
    SCOPE("lib0::pro00","lib0::pro00"), SCOPE("lib0::pro00::","lib0::pro00"),
    SCOPE("*::","::"), SCOPE("*::pro00::*","::pro00"), SCOPE("::pro00","::pro00"),
    SCOPE("*::*::rd0","::rd0"), SCOPE("::::::","::"), SCOPE("::","::"),
    SCOPE("::::rd0","::rd0"), SCOPE("lib3",""), SCOPE("lib2::pro10",""),
    SCOPE("lib0::pro01::rd0","lib0::pro01::rd0"), SCOPE("lib2::*::tp0","::tp0"),
    SCOPE("lib2::*::sb0","lib2::pro03::sb0"), SCOPE("::pro03::rd0", "::pro03::rd0"),
    SCOPE("**", "::"), SCOPE("::!:absolutely:***::wrong*scope;", "")),
  // The number of expected qos that qos_provider contains when created with (scope) above.
  CU_DataPoints(int32_t,
    63, 24, 27,
    7, 7,
    63, 13, 13,
    6, 63, 63,
    6, 0, 0,
    1, 3,
    1, 2,
    63, 0)
};
#undef N
#undef SCOPE

struct cscope_inspect_arg
{
  char *scope;
  int32_t n;
};

static void inspect_qos_items(void *vnode, void *vargs)
{
  dds_qos_item_t *item = (dds_qos_item_t *) vnode;
  struct cscope_inspect_arg *arg = (struct cscope_inspect_arg *)vargs;
  CU_ASSERT_NOT_EQUAL(strstr(item->full_name, arg->scope), NULL);
  --arg->n;
}

// @brief This tests creating qos_provider with the same sysdef file but with different scope.
CU_Theory((char * configuration, cscope_tokens_t tok, int32_t n),qos_provider, create_scope)
{
  dds_qos_provider_t *provider = NULL;
  // init qos provider with given scope
  dds_return_t ret = dds_create_qos_provider_scope(configuration, &provider, tok.scope);
  CU_ASSERT_EQUAL(ret, DDS_RETCODE_OK);
  struct cscope_inspect_arg arg = {.scope = tok.expct, .n = n};
  // examinate qos in qos provider
  ddsrt_hh_enum(provider->keyed_qos, inspect_qos_items, &arg);
  CU_ASSERT_EQUAL(arg.n, 0);
  dds_delete_qos_provider(provider);
}

#define N NO_QOS_PROVIDER_CONF
#define RD DDS_READER_QOS
#define WR DDS_WRITER_QOS
#define PP DDS_PARTICIPANT_QOS
#define PB DDS_PUBLISHER_QOS
#define SB DDS_SUBSCRIBER_QOS
#define TP DDS_TOPIC_QOS
#define OK DDS_RETCODE_OK
#define BAD DDS_RETCODE_BAD_PARAMETER
CU_TheoryDataPoints(qos_provider, get_qos) = {
  // The sysdef file with multiple libs/profiles/entity qos
  CU_DataPoints(char *,
    N,N,N,N,N,N,
    N,N,N,N,N,
    N,N,N,N,N),
  // Keys which qos we would like to get
  CU_DataPoints(char *,
    "lib0::pro00","lib0::pro00","lib0::pro00","lib0::pro00","lib0::pro00","lib0::pro00",
    "lib0::pro00::pb0","lib2::pro01::sb0","lib0::pro01::rd0","lib0::pro03","lib0::pro03::rd1",
    "lib0::pro00","lib0::*","*::pro00::rd1","*","lib2::pro01"),
  // Type of entity for which qos we try to get
  CU_DataPoints(dds_qos_kind_t,
    RD,WR,PP,PB,SB,TP,
    PB,SB,RD,RD,RD,
    RD,WR,RD,PP,PP),
  // Expected retcodes
  CU_DataPoints(dds_return_t,
    OK,OK,OK,OK,OK,OK,
    BAD,BAD,OK,BAD,OK,
    OK,BAD,BAD,BAD,BAD)
};
#undef BAD
#undef OK
#undef TP
#undef SB
#undef PB
#undef PP
#undef WR
#undef RD
#undef N

CU_Theory((char *configuration, char *key, dds_qos_kind_t kind, dds_return_t code),qos_provider, get_qos)
{
  dds_qos_provider_t *provider = NULL;
  // init qos provider with provided configuration
  dds_return_t ret = dds_create_qos_provider(configuration, &provider);
  CU_ASSERT_EQUAL(ret, DDS_RETCODE_OK);
  const dds_qos_t *qos;
  // try to get qos with `key`, `kind`
  ret = dds_qos_provider_get_qos(provider, kind, key, &qos);
  CU_ASSERT_EQUAL(ret, code);
  // check that retcode eq to expected
  dds_delete_qos_provider(provider);
}

#define QOS_DURATION_FMT(unit) \
  "<"#unit">%lli</"#unit">"
#define QOS_DURATION_FMT_STR(unit) \
  "<"#unit">%s</"#unit">"
#define QOS_POLICY_DEADLINE_PERIOD_FMT(duration_fmt) \
  "<period>"duration_fmt"</period>"
#define QOS_POLICY_DEADLINE_FMT(unit) \
  "<deadline>"QOS_POLICY_DEADLINE_PERIOD_FMT(unit)"</deadline>"
#define QOS_POLICY_DESTINATION_ORDER_FMT(k) \
  "<destination_order><kind>"#k"</kind></destination_order>"
#define QOS_POLICY_DURABILITY_FMT(k) \
  "<durability><kind>"#k"</kind></durability>"
#define QOS_HISTORY_FMT(hk) \
  "<history><kind>"#hk"</kind><depth>%d</depth></history>"
#define QOS_SERVICE_CLEANUP_DELAY_FMT(duration_fmt) \
  "<service_cleanup_delay>"duration_fmt"</service_cleanup_delay>"
#define QOS_RESOURCE_LIMITS_MS(ms_f) \
  "<max_samples>"ms_f"</max_samples>"
#define QOS_RESOURCE_LIMITS_MI(mi_f) \
  "<max_instances>"mi_f"</max_instances>"
#define QOS_RESOURCE_LIMITS_MSPI(mspi_f) \
  "<max_samples_per_instance>"mspi_f"</max_samples_per_instance>"
#define QOS_RESOURCE_LIMITS \
  "%s%s%s"
#define QOS_DURABILITY_SERVICE_HISTORY(hk) \
  "<history_kind>"#hk"</history_kind><history_depth>%d</history_depth>"
#define QOS_POLICY_DURABILITY_SERVICE_FMT \
  "<durability_service>%s%s"QOS_RESOURCE_LIMITS"</durability_service>"
#define QOS_POLICY_ENTITYFACTORY_FMT(val) \
  "<entity_factory>" \
    "<autoenable_created_entities>"#val"</autoenable_created_entities>" \
  "</entity_factory>"
#define QOS_BASE64_VALUE \
  "<value>%s</value>"
#define QOS_POLICY_GOUPDATA_FMT \
  "<group_data>"QOS_BASE64_VALUE"</group_data>"
#define QOS_POLICY_HISTORY_FMT(hk) \
  QOS_HISTORY_FMT(hk)
#define QOS_POLICY_LATENCYBUDGET_FMT(duration_fmt) \
  "<latency_budget><duration>"duration_fmt"</duration></latency_budget>"
#define QOS_POLICY_LIFESPAN_FMT(duration_fmt) \
  "<lifespan><duration>"duration_fmt"</duration></lifespan>"
#define QOS_LIVELINESS_KIND(lk) \
  "<kind>"#lk"</kind>"
#define QOS_LIVELINESS_DURATION(duration_fmt) \
  "<lease_duration>"duration_fmt"</lease_duration>"
#define QOS_POLICY_LIVELINESS_FMT \
  "<liveliness>%s%s</liveliness>"
#define QOS_POLICY_OWNERSHIP_FMT(ok) \
  "<ownership><kind>"#ok"</kind></ownership>"
#define QOS_POLICY_OWNERSHIPSTRENGTH_FMT \
  "<ownership_strength><value>%d</value></ownership_strength>"
#define QOS_PARTITION_ELEMENT \
  "<element>%s</element>"
#define QOS_POLIC_PARTITION_FMT \
  "<partition><name>%s</name></partition>"
#define QOS_ACCESS_SCOPE_KIND(ask) \
  "<access_scope>"#ask"</access_scope>"
#define QOS_COHERENT_ACCESS(ca) \
  "<coherent_access>"#ca"</coherent_access>"
#define QOS_ORDERED_ACCESS(oa) \
 "<ordered_access>"#oa"</ordered_access>"
#define QOS_POLICY_PRESENTATION_FMT \
  "<presentation>%s%s%s</presentation>"
#define QOS_RELIABILITY_KIND(rk) \
  "<kind>"#rk"</kind>"
#define QOS_RELIABILITY_DURATION(duration_fmt) \
  "<max_blocking_time>"duration_fmt"</max_blocking_time>"
#define QOS_POLICY_RELIABILITY_FMT \
  "<reliability>%s%s</reliability>"
#define QOS_POLICY_RESOURCE_LIMITS_FMT \
  "<resource_limits>"QOS_RESOURCE_LIMITS"</resource_limits>"
#define QOS_POLICY_TIMEBASEDFILTER_FMT(duration_fmt) \
  "<time_based_filter><minimum_separation>" \
    duration_fmt \
  "</minimum_separation></time_based_filter>"
#define QOS_POLICY_TOPICDATA_FMT \
  "<topic_data>"QOS_BASE64_VALUE"</topic_data>"
#define QOS_POLICY_TRANSPORTPRIORITY_FMT \
  "<transport_priority><value>%d</value></transport_priority>"
#define QOS_POLICY_USERDATA_FMT \
  "<user_data>"QOS_BASE64_VALUE"</user_data>"
#define QOS_NOWRITER_DELAY(duration_fmt) \
  "<autopurge_nowriter_samples_delay>"duration_fmt"</autopurge_nowriter_samples_delay>"
#define QOS_DISPOSED_DELAY(duration_fmt) \
  "<autopurge_disposed_samples_delay>" \
    duration_fmt \
  "</autopurge_disposed_samples_delay>"
#define QOS_POLICY_READERDATALIFECYCLE_FMT \
  "<reader_data_lifecycle>%s%s</reader_data_lifecycle>"
#define QOS_POLICY_WRITERDATA_LIFECYCLE_FMT(aui) \
  "<writer_data_lifecycle>" \
    "<autodispose_unregistered_instances>"#aui"</autodispose_unregistered_instances>" \
  "</writer_data_lifecycle>"

enum duration_unit
{
  sec = 0,
  nsec = 1
};

typedef struct sysdef_qos_conf
{
  enum duration_unit deadline_unit;
  enum duration_unit durability_serv_unit;
  enum duration_unit latency_budget_unit;
  enum duration_unit lifespan_unit;
  enum duration_unit liveliness_unit;
  enum duration_unit reliability_unit;
  enum duration_unit time_based_filter_unit;
  enum duration_unit reader_data_lifecycle_nowriter_unit;
  enum duration_unit reader_data_lifecycle_disposed_unit;
} sysdef_qos_conf_t;

#define QOS_FORMAT "     "
#define CHECK_RET_OK(ret) \
  if (ret < 0) goto fail;
static inline dds_return_t qos_to_conf(dds_qos_t *qos, const sysdef_qos_conf_t *conf, char **out, dds_qos_kind_t kind, uint64_t *validate_mask, const bool ignore_ent)
{
  char *sysdef_qos = ddsrt_strdup("");
  dds_return_t ret = DDS_RETCODE_OK;
  if ((ignore_ent || (kind == DDS_TOPIC_QOS || kind == DDS_READER_QOS || kind == DDS_WRITER_QOS)) &&
      (ret >= 0) && (qos->present & DDSI_QP_DEADLINE))
  {
    char *deadline;
    if (qos->deadline.deadline == DDS_INFINITY)
    {
      if (conf->deadline_unit == sec)
        ret = ddsrt_asprintf(&deadline, QOS_POLICY_DEADLINE_FMT(QOS_DURATION_FMT_STR(sec)),
                             QOS_DURATION_INFINITY_SEC);
      else
        ret = ddsrt_asprintf(&deadline, QOS_POLICY_DEADLINE_FMT(QOS_DURATION_FMT_STR(nanosec)),
                             QOS_DURATION_INFINITY_NSEC);
    } else {
      if (conf->deadline_unit == sec)
        ret = ddsrt_asprintf(&deadline, QOS_POLICY_DEADLINE_FMT(QOS_DURATION_FMT(sec)),
                             (long long)(qos->deadline.deadline/DDS_NSECS_IN_SEC));
      else
        ret = ddsrt_asprintf(&deadline, QOS_POLICY_DEADLINE_FMT(QOS_DURATION_FMT(nanosec)),
                             (long long)qos->deadline.deadline);
    }
    CHECK_RET_OK(ret);
    char *tmp = sysdef_qos;
    ret = ddsrt_asprintf(&sysdef_qos, QOS_FORMAT"%s\n"QOS_FORMAT"%s", sysdef_qos, deadline);
    ddsrt_free(tmp);
    ddsrt_free(deadline);
    *validate_mask |= DDSI_QP_DEADLINE;
  }
  if ((ignore_ent || (kind == DDS_TOPIC_QOS || kind == DDS_READER_QOS || kind == DDS_WRITER_QOS)) &&
      (ret >= 0) && (qos->present & DDSI_QP_DESTINATION_ORDER))
  {
    char *dest_order;
    if (qos->destination_order.kind == DDS_DESTINATIONORDER_BY_RECEPTION_TIMESTAMP)
      ret = ddsrt_asprintf(&dest_order, "%s", QOS_POLICY_DESTINATION_ORDER_FMT(BY_RECEPTION_TIMESTAMP_DESTINATIONORDER_QOS));
    else
      ret = ddsrt_asprintf(&dest_order, "%s", QOS_POLICY_DESTINATION_ORDER_FMT(BY_SOURCE_TIMESTAMP_DESTINATIONORDER_QOS));
    CHECK_RET_OK(ret);
    char *tmp = sysdef_qos;
    ret = ddsrt_asprintf(&sysdef_qos, QOS_FORMAT"%s\n"QOS_FORMAT"%s", sysdef_qos, dest_order);
    ddsrt_free(tmp);
    ddsrt_free(dest_order);
    *validate_mask |= DDSI_QP_DESTINATION_ORDER;
  }
  if ((ignore_ent || (kind != DDS_SUBSCRIBER_QOS && kind != DDS_PUBLISHER_QOS && kind != DDS_PARTICIPANT_QOS)) &&
      (ret >= 0) && (qos->present & DDSI_QP_DURABILITY))
  {
    char *durability;
    if (qos->durability.kind == DDS_DURABILITY_VOLATILE)
      ret = ddsrt_asprintf(&durability, "%s", QOS_POLICY_DURABILITY_FMT(VOLATILE_DURABILITY_QOS));
    else if (qos->durability.kind == DDS_DURABILITY_TRANSIENT_LOCAL)
      ret = ddsrt_asprintf(&durability, "%s", QOS_POLICY_DURABILITY_FMT(TRANSIENT_LOCAL_DURABILITY_QOS));
    else if (qos->durability.kind == DDS_DURABILITY_TRANSIENT)
      ret = ddsrt_asprintf(&durability, "%s", QOS_POLICY_DURABILITY_FMT(TRANSIENT_DURABILITY_QOS));
    else
      ret = ddsrt_asprintf(&durability, "%s", QOS_POLICY_DURABILITY_FMT(PERSISTENT_DURABILITY_QOS));
    CHECK_RET_OK(ret);
    char *tmp = sysdef_qos;
    ret = ddsrt_asprintf(&sysdef_qos, QOS_FORMAT"%s\n"QOS_FORMAT"%s", sysdef_qos, durability);
    ddsrt_free(tmp);
    ddsrt_free(durability);
    *validate_mask |= DDSI_QP_DURABILITY;
  }
  if ((ignore_ent || (kind == DDS_WRITER_QOS || kind == DDS_TOPIC_QOS)) &&
      (ret >= 0) && qos->present & DDSI_QP_DURABILITY_SERVICE)
  {
    char *service_cleanup_delay;
    if (qos->durability_service.service_cleanup_delay == DDS_INFINITY)
    {
      if (conf->durability_serv_unit == sec)
        ret = ddsrt_asprintf(&service_cleanup_delay, QOS_SERVICE_CLEANUP_DELAY_FMT(QOS_DURATION_FMT_STR(sec)),
                             QOS_DURATION_INFINITY_SEC);
      else
        ret = ddsrt_asprintf(&service_cleanup_delay, QOS_SERVICE_CLEANUP_DELAY_FMT(QOS_DURATION_FMT_STR(nanosec)),
                             QOS_DURATION_INFINITY_NSEC);
    } else {
      if (conf->durability_serv_unit == sec)
        ret = ddsrt_asprintf(&service_cleanup_delay, QOS_SERVICE_CLEANUP_DELAY_FMT(QOS_DURATION_FMT(sec)),
                             (long long)qos->durability_service.service_cleanup_delay/DDS_NSECS_IN_SEC);
      else
        ret = ddsrt_asprintf(&service_cleanup_delay, QOS_SERVICE_CLEANUP_DELAY_FMT(QOS_DURATION_FMT(nanosec)),
                             (long long)qos->durability_service.service_cleanup_delay);
    }
    CHECK_RET_OK(ret);
    char *history;
    if (qos->durability_service.history.kind == DDS_HISTORY_KEEP_LAST)
      ret = ddsrt_asprintf(&history, QOS_DURABILITY_SERVICE_HISTORY(KEEP_LAST_HISTORY_QOS), qos->durability_service.history.depth);
    else
      ret = ddsrt_asprintf(&history, QOS_DURABILITY_SERVICE_HISTORY(KEEP_ALL_HISTORY_QOS), qos->durability_service.history.depth);
    CHECK_RET_OK(ret);
    char *durability_service;
    int32_t ms,mi,mspi;
    ms = qos->durability_service.resource_limits.max_samples;
    mi = qos->durability_service.resource_limits.max_instances;
    mspi = qos->durability_service.resource_limits.max_samples_per_instance;
    char *ms_f,*mi_f,*mspi_f;
    (void) ddsrt_asprintf(&ms_f,(ms<0? QOS_RESOURCE_LIMITS_MS(QOS_LENGTH_UNLIMITED): QOS_RESOURCE_LIMITS_MS("%d")), ms);
    (void) ddsrt_asprintf(&mi_f,(mi<0? QOS_RESOURCE_LIMITS_MI(QOS_LENGTH_UNLIMITED): QOS_RESOURCE_LIMITS_MI("%d")), mi);
    (void) ddsrt_asprintf(&mspi_f,(mspi<0? QOS_RESOURCE_LIMITS_MSPI(QOS_LENGTH_UNLIMITED): QOS_RESOURCE_LIMITS_MSPI("%d")), mspi);
    ret = ddsrt_asprintf(&durability_service, QOS_POLICY_DURABILITY_SERVICE_FMT, service_cleanup_delay, history, ms_f, mi_f, mspi_f);
    ddsrt_free(ms_f);ddsrt_free(mi_f);ddsrt_free(mspi_f);
    ddsrt_free(service_cleanup_delay);
    ddsrt_free(history);
    CHECK_RET_OK(ret);
    char *tmp = sysdef_qos;
    ret = ddsrt_asprintf(&sysdef_qos, QOS_FORMAT"%s\n"QOS_FORMAT"%s", sysdef_qos, durability_service);
    ddsrt_free(tmp);
    ddsrt_free(durability_service);
    *validate_mask |= DDSI_QP_DURABILITY_SERVICE;
  }
  if ((ignore_ent || (kind == DDS_PARTICIPANT_QOS || kind == DDS_PUBLISHER_QOS || kind == DDS_SUBSCRIBER_QOS)) &&
      (ret >= 0) && qos->present & DDSI_QP_ADLINK_ENTITY_FACTORY)
  {
    char *entity_factory;
    if (qos->entity_factory.autoenable_created_entities == 0)
      ret = ddsrt_asprintf(&entity_factory, "%s", QOS_POLICY_ENTITYFACTORY_FMT(false));
    else
      ret = ddsrt_asprintf(&entity_factory, "%s", QOS_POLICY_ENTITYFACTORY_FMT(true));
    CHECK_RET_OK(ret);
    char *tmp = sysdef_qos;
    ret = ddsrt_asprintf(&sysdef_qos, QOS_FORMAT"%s\n"QOS_FORMAT"%s", sysdef_qos, entity_factory);
    ddsrt_free(tmp);
    ddsrt_free(entity_factory);
    *validate_mask |= DDSI_QP_ADLINK_ENTITY_FACTORY;
  }
  if ((ignore_ent || (kind == DDS_PUBLISHER_QOS || kind == DDS_SUBSCRIBER_QOS)) &&
      (ret >= 0) && qos->present & DDSI_QP_GROUP_DATA)
  {
    if (qos->group_data.length > 0)
    {
      char *data = ddsrt_strdup("");
      for (uint32_t i = 0; i < qos->group_data.length; i++) {
        char *tmp = data;
        ret = ddsrt_asprintf(&data, "%s%c", data, qos->group_data.value[i]);
        ddsrt_free(tmp);
        CHECK_RET_OK(ret);
      }
      char *group_data;
      ret = ddsrt_asprintf(&group_data, QOS_POLICY_GOUPDATA_FMT, data);
      ddsrt_free(data);
      CHECK_RET_OK(ret);
      char *tmp = sysdef_qos;
      ret = ddsrt_asprintf(&sysdef_qos, QOS_FORMAT"%s\n"QOS_FORMAT"%s", sysdef_qos, group_data);
      ddsrt_free(tmp);
      ddsrt_free(group_data);
      *validate_mask |= DDSI_QP_GROUP_DATA;
    }
  }
  if ((ignore_ent || (kind == DDS_TOPIC_QOS || kind == DDS_READER_QOS || kind == DDS_WRITER_QOS)) &&
      (ret >= 0) && qos->present & DDSI_QP_HISTORY)
  {
    char *history;
    if (qos->history.kind == DDS_HISTORY_KEEP_LAST)
      ret = ddsrt_asprintf(&history, QOS_POLICY_HISTORY_FMT(KEEP_LAST_HISTORY_QOS), qos->history.depth);
    else
      ret = ddsrt_asprintf(&history, QOS_POLICY_HISTORY_FMT(KEEP_ALL_HISTORY_QOS), qos->history.depth);
    CHECK_RET_OK(ret);
    char *tmp = sysdef_qos;
    ret = ddsrt_asprintf(&sysdef_qos, QOS_FORMAT"%s\n"QOS_FORMAT"%s", sysdef_qos, history);
    ddsrt_free(tmp);
    ddsrt_free(history);
    *validate_mask |= DDSI_QP_HISTORY;
  }
  if ((ignore_ent || (kind == DDS_TOPIC_QOS || kind == DDS_WRITER_QOS || kind == DDS_READER_QOS)) &&
      (ret >= 0) && qos->present & DDSI_QP_LATENCY_BUDGET)
  {
    char *latency_budget;
    if (qos->latency_budget.duration == DDS_INFINITY)
    {
      if (conf->latency_budget_unit == sec)
        ret = ddsrt_asprintf(&latency_budget, QOS_POLICY_LATENCYBUDGET_FMT(QOS_DURATION_FMT_STR(sec)),
                             QOS_DURATION_INFINITY_SEC);
      else
        ret = ddsrt_asprintf(&latency_budget, QOS_POLICY_LATENCYBUDGET_FMT(QOS_DURATION_FMT_STR(nanosec)),
                             QOS_DURATION_INFINITY_NSEC);
    } else {
      if (conf->latency_budget_unit == sec)
        ret = ddsrt_asprintf(&latency_budget, QOS_POLICY_LATENCYBUDGET_FMT(QOS_DURATION_FMT(sec)),
                             (long long)qos->latency_budget.duration/DDS_NSECS_IN_SEC);
      else
        ret = ddsrt_asprintf(&latency_budget, QOS_POLICY_LATENCYBUDGET_FMT(QOS_DURATION_FMT(nanosec)),
                             (long long)qos->latency_budget.duration);
    }
    CHECK_RET_OK(ret);
    char *tmp = sysdef_qos;
    ret = ddsrt_asprintf(&sysdef_qos, QOS_FORMAT"%s\n"QOS_FORMAT"%s", sysdef_qos, latency_budget);
    ddsrt_free(tmp);
    ddsrt_free(latency_budget);
    *validate_mask |= DDSI_QP_LATENCY_BUDGET;
  }
  if ((ignore_ent || (kind == DDS_WRITER_QOS || kind == DDS_TOPIC_QOS)) &&
      (ret >= 0) && qos->present & DDSI_QP_LIFESPAN)
  {
    char *lifespan;
    if (qos->lifespan.duration == DDS_INFINITY)
    {
      if (conf->lifespan_unit == sec)
        ret = ddsrt_asprintf(&lifespan, QOS_POLICY_LIFESPAN_FMT(QOS_DURATION_FMT_STR(sec)),
                             QOS_DURATION_INFINITY_SEC);
      else
       ret = ddsrt_asprintf(&lifespan, QOS_POLICY_LIFESPAN_FMT(QOS_DURATION_FMT_STR(nanosec)),
                            QOS_DURATION_INFINITY_NSEC);
    } else {
      if (conf->lifespan_unit == sec)
        ret = ddsrt_asprintf(&lifespan, QOS_POLICY_LIFESPAN_FMT(QOS_DURATION_FMT(sec)),
                             (long long)qos->lifespan.duration/DDS_NSECS_IN_SEC);
      else
       ret = ddsrt_asprintf(&lifespan, QOS_POLICY_LIFESPAN_FMT(QOS_DURATION_FMT(nanosec)),
                            (long long)qos->lifespan.duration);
    }
    CHECK_RET_OK(ret);
    char *tmp = sysdef_qos;
    ret = ddsrt_asprintf(&sysdef_qos, QOS_FORMAT"%s\n"QOS_FORMAT"%s", sysdef_qos, lifespan);
    ddsrt_free(tmp);
    ddsrt_free(lifespan);
    *validate_mask |= DDSI_QP_LIFESPAN;
  }
  if ((ignore_ent || (kind != DDS_PUBLISHER_QOS && kind != DDS_SUBSCRIBER_QOS && kind != DDS_PARTICIPANT_QOS)) &&
      (ret >= 0) && qos->present & DDSI_QP_LIVELINESS)
  {
    char *duration;
    if (qos->liveliness.lease_duration == DDS_INFINITY)
    {
      if (conf->liveliness_unit == sec)
        ret = ddsrt_asprintf(&duration, QOS_LIVELINESS_DURATION(QOS_DURATION_FMT_STR(sec)),
                             QOS_DURATION_INFINITY_SEC);
      else
        ret = ddsrt_asprintf(&duration, QOS_LIVELINESS_DURATION(QOS_DURATION_FMT_STR(nanosec)),
                             QOS_DURATION_INFINITY_NSEC);
    } else {
      if (conf->liveliness_unit == sec)
        ret = ddsrt_asprintf(&duration, QOS_LIVELINESS_DURATION(QOS_DURATION_FMT(sec)),
                             (long long)qos->liveliness.lease_duration/DDS_NSECS_IN_SEC);
      else
        ret = ddsrt_asprintf(&duration, QOS_LIVELINESS_DURATION(QOS_DURATION_FMT(nanosec)),
                             (long long)qos->liveliness.lease_duration);
    }
    CHECK_RET_OK(ret);
    char *liveliness_kind;
    if (qos->liveliness.kind == DDS_LIVELINESS_AUTOMATIC)
      ret = ddsrt_asprintf(&liveliness_kind, "%s", QOS_LIVELINESS_KIND(AUTOMATIC_LIVELINESS_QOS));
    else if (qos->liveliness.kind == DDS_LIVELINESS_MANUAL_BY_PARTICIPANT)
      ret = ddsrt_asprintf(&liveliness_kind, "%s", QOS_LIVELINESS_KIND(MANUAL_BY_PARTICIPANT_LIVELINESS_QOS));
    else
      ret = ddsrt_asprintf(&liveliness_kind, "%s", QOS_LIVELINESS_KIND(MANUAL_BY_TOPIC_LIVELINESS_QOS));
    CHECK_RET_OK(ret);
    char *liveliness;
    ret = ddsrt_asprintf(&liveliness, QOS_POLICY_LIVELINESS_FMT, duration, liveliness_kind);
    ddsrt_free(duration);
    ddsrt_free(liveliness_kind);
    CHECK_RET_OK(ret);
    char *tmp = sysdef_qos;
    ret = ddsrt_asprintf(&sysdef_qos, QOS_FORMAT"%s\n"QOS_FORMAT"%s", sysdef_qos, liveliness);
    ddsrt_free(tmp);
    ddsrt_free(liveliness);
    *validate_mask |= DDSI_QP_LIVELINESS;
  }
  if ((ignore_ent || (kind != DDS_PUBLISHER_QOS && kind != DDS_SUBSCRIBER_QOS && kind != DDS_PARTICIPANT_QOS)) &&
      (ret >= 0) && qos->present & DDSI_QP_OWNERSHIP)
  {
    char *ownership;
    if (qos->ownership.kind == DDS_OWNERSHIP_SHARED)
      ret = ddsrt_asprintf(&ownership, "%s", QOS_POLICY_OWNERSHIP_FMT(SHARED_OWNERSHIP_QOS));
    else
      ret = ddsrt_asprintf(&ownership, "%s", QOS_POLICY_OWNERSHIP_FMT(EXCLUSIVE_OWNERSHIP_QOS));
    CHECK_RET_OK(ret);
    char *tmp = sysdef_qos;
    ret = ddsrt_asprintf(&sysdef_qos, QOS_FORMAT"%s\n"QOS_FORMAT"%s", sysdef_qos, ownership);
    ddsrt_free(tmp);
    ddsrt_free(ownership);
    *validate_mask |= DDSI_QP_OWNERSHIP;
  }
  if ((ignore_ent || (kind == DDS_WRITER_QOS)) &&
      (ret >= 0) && qos->present & DDSI_QP_OWNERSHIP_STRENGTH)
  {
    char *ownership_strength;
    ret = ddsrt_asprintf(&ownership_strength, QOS_POLICY_OWNERSHIPSTRENGTH_FMT, qos->ownership_strength.value);
    CHECK_RET_OK(ret);
    char *tmp = sysdef_qos;
    ret = ddsrt_asprintf(&sysdef_qos, QOS_FORMAT"%s\n"QOS_FORMAT"%s", sysdef_qos, ownership_strength);
    ddsrt_free(tmp);
    ddsrt_free(ownership_strength);
    *validate_mask |= DDSI_QP_OWNERSHIP_STRENGTH;
  }
  if ((ignore_ent || (kind == DDS_PUBLISHER_QOS || kind == DDS_SUBSCRIBER_QOS)) &&
      (ret >= 0) && qos->present & DDSI_QP_PARTITION)
  {
    if (qos->partition.n > 0)
    {
      char *part_elems = ddsrt_strdup("");
      for (uint32_t i = 0; i < qos->partition.n; i++) {
        char *tmp = part_elems;
        ret = ddsrt_asprintf(&part_elems, "%s"QOS_PARTITION_ELEMENT, part_elems, qos->partition.strs[i]);
        CHECK_RET_OK(ret);
        ddsrt_free(tmp);
      }
      CHECK_RET_OK(ret);
      char *partition;
      ret = ddsrt_asprintf(&partition, QOS_POLIC_PARTITION_FMT, part_elems);
      ddsrt_free(part_elems);
      CHECK_RET_OK(ret);
      char *tmp = sysdef_qos;
      ret = ddsrt_asprintf(&sysdef_qos, QOS_FORMAT"%s\n"QOS_FORMAT"%s", sysdef_qos, partition);
      ddsrt_free(tmp);
      ddsrt_free(partition);
      *validate_mask |= DDSI_QP_PARTITION;
    }
  }
  if ((ignore_ent || (kind == DDS_PUBLISHER_QOS || kind == DDS_SUBSCRIBER_QOS)) &&
      (ret >= 0) && qos->present & DDSI_QP_PRESENTATION)
  {
    char *access_scope_kind;
    if (qos->presentation.access_scope == DDS_PRESENTATION_INSTANCE)
      ret =  ddsrt_asprintf(&access_scope_kind, QOS_ACCESS_SCOPE_KIND(INSTANCE_PRESENTATION_QOS));
    else if (qos->presentation.access_scope == DDS_PRESENTATION_TOPIC)
      ret =  ddsrt_asprintf(&access_scope_kind, QOS_ACCESS_SCOPE_KIND(TOPIC_PRESENTATION_QOS));
    else
      ret =  ddsrt_asprintf(&access_scope_kind, QOS_ACCESS_SCOPE_KIND(GROUP_PRESENTATION_QOS));
    CHECK_RET_OK(ret);
    char *coherent_access;
    if (qos->presentation.coherent_access)
      ret = ddsrt_asprintf(&coherent_access, "%s", QOS_COHERENT_ACCESS(true));
    else
      ret = ddsrt_asprintf(&coherent_access, "%s", QOS_COHERENT_ACCESS(false));
    CHECK_RET_OK(ret);
    char *ordered_access;
    if (qos->presentation.ordered_access)
      ret = ddsrt_asprintf(&ordered_access, "%s", QOS_ORDERED_ACCESS(true));
    else
      ret = ddsrt_asprintf(&ordered_access, "%s", QOS_ORDERED_ACCESS(false));
    CHECK_RET_OK(ret);
    char *presentation;
    ret = ddsrt_asprintf(&presentation, QOS_POLICY_PRESENTATION_FMT, access_scope_kind, coherent_access, ordered_access);
    ddsrt_free(access_scope_kind);
    ddsrt_free(coherent_access);
    ddsrt_free(ordered_access);
    CHECK_RET_OK(ret);
    char *tmp = sysdef_qos;
    ret = ddsrt_asprintf(&sysdef_qos, QOS_FORMAT"%s\n"QOS_FORMAT"%s", sysdef_qos, presentation);
    ddsrt_free(tmp);
    ddsrt_free(presentation);
    *validate_mask |= DDSI_QP_PRESENTATION;
  }
  if ((ignore_ent || (kind == DDS_TOPIC_QOS || kind == DDS_READER_QOS || kind == DDS_WRITER_QOS)) &&
      (ret >= 0) && qos->present & DDSI_QP_RELIABILITY)
  {
    char *max_blocking_time;
    if (qos->reliability.max_blocking_time == DDS_INFINITY)
    {
      if (conf->reliability_unit == sec)
        ret = ddsrt_asprintf(&max_blocking_time, QOS_RELIABILITY_DURATION(QOS_DURATION_FMT_STR(sec)),
                             QOS_DURATION_INFINITY_SEC);
      else
        ret = ddsrt_asprintf(&max_blocking_time, QOS_RELIABILITY_DURATION(QOS_DURATION_FMT_STR(nanosec)),
                             QOS_DURATION_INFINITY_NSEC);
    } else {
      if (conf->reliability_unit == sec)
        ret = ddsrt_asprintf(&max_blocking_time, QOS_RELIABILITY_DURATION(QOS_DURATION_FMT(sec)),
                             (long long)qos->reliability.max_blocking_time/DDS_NSECS_IN_SEC);
      else
        ret = ddsrt_asprintf(&max_blocking_time, QOS_RELIABILITY_DURATION(QOS_DURATION_FMT(nanosec)),
                             (long long)qos->reliability.max_blocking_time);
    }
    CHECK_RET_OK(ret);
    char *reliability_kind;
    if (qos->reliability.kind == DDS_RELIABILITY_BEST_EFFORT)
      ret = ddsrt_asprintf(&reliability_kind, "%s", QOS_RELIABILITY_KIND(BEST_EFFORT_RELIABILITY_QOS));
    else
      ret = ddsrt_asprintf(&reliability_kind, "%s", QOS_RELIABILITY_KIND(RELIABLE_RELIABILITY_QOS));
    CHECK_RET_OK(ret);
    char *reliability;
    ret = ddsrt_asprintf(&reliability, QOS_POLICY_RELIABILITY_FMT, max_blocking_time, reliability_kind);
    ddsrt_free(max_blocking_time);
    ddsrt_free(reliability_kind);
    CHECK_RET_OK(ret);
    char *tmp = sysdef_qos;
    ret = ddsrt_asprintf(&sysdef_qos, QOS_FORMAT"%s\n"QOS_FORMAT"%s", sysdef_qos, reliability);
    ddsrt_free(tmp);
    ddsrt_free(reliability);
    *validate_mask |= DDSI_QP_RELIABILITY;
  }
  if ((ignore_ent || (kind == DDS_TOPIC_QOS || kind == DDS_READER_QOS || kind == DDS_WRITER_QOS)) &&
      (ret >= 0) && qos->present & DDSI_QP_RESOURCE_LIMITS)
  {
    int32_t ms,mi,mspi;
    ms = qos->resource_limits.max_samples;
    mi = qos->resource_limits.max_instances;
    mspi = qos->resource_limits.max_samples_per_instance;
    char *ms_f,*mi_f,*mspi_f;
    (void) ddsrt_asprintf(&ms_f,(ms<0? QOS_RESOURCE_LIMITS_MS(QOS_LENGTH_UNLIMITED): QOS_RESOURCE_LIMITS_MS("%d")), ms);
    (void) ddsrt_asprintf(&mi_f,(mi<0? QOS_RESOURCE_LIMITS_MI(QOS_LENGTH_UNLIMITED): QOS_RESOURCE_LIMITS_MI("%d")), mi);
    (void) ddsrt_asprintf(&mspi_f,(mspi<0? QOS_RESOURCE_LIMITS_MSPI(QOS_LENGTH_UNLIMITED): QOS_RESOURCE_LIMITS_MSPI("%d")), mspi);
    char *resource_limits;
    ret = ddsrt_asprintf(&resource_limits, QOS_POLICY_RESOURCE_LIMITS_FMT, ms_f, mi_f, mspi_f);
    ddsrt_free(ms_f);ddsrt_free(mi_f);ddsrt_free(mspi_f);
    CHECK_RET_OK(ret);
    char *tmp = sysdef_qos;
    ret = ddsrt_asprintf(&sysdef_qos, QOS_FORMAT"%s\n"QOS_FORMAT"%s", sysdef_qos, resource_limits);
    ddsrt_free(tmp);
    ddsrt_free(resource_limits);
    *validate_mask |= DDSI_QP_RESOURCE_LIMITS;
  }
  if ((ignore_ent || (kind == DDS_READER_QOS)) &&
      (ret >= 0) && qos->present & DDSI_QP_TIME_BASED_FILTER)
  {
    char *time_based_filter;
    if (qos->time_based_filter.minimum_separation == DDS_INFINITY)
    {
      if (conf->time_based_filter_unit == sec)
        ret = ddsrt_asprintf(&time_based_filter, QOS_POLICY_TIMEBASEDFILTER_FMT(QOS_DURATION_FMT_STR(sec)),
                             QOS_DURATION_INFINITY_SEC);
      else
        ret = ddsrt_asprintf(&time_based_filter, QOS_POLICY_TIMEBASEDFILTER_FMT(QOS_DURATION_FMT_STR(nanosec)),
                             QOS_DURATION_INFINITY_NSEC);
    } else {
      if (conf->time_based_filter_unit == sec)
        ret = ddsrt_asprintf(&time_based_filter, QOS_POLICY_TIMEBASEDFILTER_FMT(QOS_DURATION_FMT(sec)),
                             (long long)qos->time_based_filter.minimum_separation/DDS_NSECS_IN_SEC);
      else
        ret = ddsrt_asprintf(&time_based_filter, QOS_POLICY_TIMEBASEDFILTER_FMT(QOS_DURATION_FMT(nanosec)),
                             (long long)qos->time_based_filter.minimum_separation);
    }
    CHECK_RET_OK(ret);
    char *tmp = sysdef_qos;
    ret = ddsrt_asprintf(&sysdef_qos, QOS_FORMAT"%s\n"QOS_FORMAT"%s", sysdef_qos, time_based_filter);
    ddsrt_free(tmp);
    ddsrt_free(time_based_filter);
    *validate_mask |= DDSI_QP_TIME_BASED_FILTER;
  }
  if ((ignore_ent || (kind == DDS_TOPIC_QOS)) &&
      (ret >= 0) && qos->present & DDSI_QP_TOPIC_DATA)
  {
    if (qos->topic_data.length > 0)
    {
      char *data = ddsrt_strdup("");
      for (uint32_t i = 0; i < qos->topic_data.length; i++) {
        char *tmp = data;
        ret = ddsrt_asprintf(&data, "%s%c", data, qos->topic_data.value[i]);
        ddsrt_free(tmp);
        CHECK_RET_OK(ret);
      }
      char *topic_data;
      ret = ddsrt_asprintf(&topic_data, QOS_POLICY_TOPICDATA_FMT, data);
      ddsrt_free(data);
      CHECK_RET_OK(ret);
      char *tmp = sysdef_qos;
      ret = ddsrt_asprintf(&sysdef_qos, QOS_FORMAT"%s\n"QOS_FORMAT"%s", sysdef_qos, topic_data);
      ddsrt_free(tmp);
      ddsrt_free(topic_data);
      *validate_mask |= DDSI_QP_TOPIC_DATA;
    }
  }
  if ((ignore_ent || (kind == DDS_TOPIC_QOS || kind == DDS_WRITER_QOS)) &&
      (ret >= 0) && qos->present & DDSI_QP_TRANSPORT_PRIORITY)
  {
    char *priority;
    ret = ddsrt_asprintf(&priority, QOS_POLICY_TRANSPORTPRIORITY_FMT, qos->transport_priority.value);
    CHECK_RET_OK(ret);
    char *tmp = sysdef_qos;
    ret = ddsrt_asprintf(&sysdef_qos, QOS_FORMAT"%s\n"QOS_FORMAT"%s", sysdef_qos, priority);
    ddsrt_free(tmp);
    ddsrt_free(priority);
    *validate_mask |= DDSI_QP_TRANSPORT_PRIORITY;
  }
  if ((ignore_ent || (kind == DDS_PARTICIPANT_QOS || kind == DDS_READER_QOS || kind == DDS_WRITER_QOS)) &&
      (ret >= 0) && qos->present & DDSI_QP_USER_DATA)
  {
    if (qos->user_data.length > 0)
    {
      char *data = ddsrt_strdup("");
      for (uint32_t i = 0; i < qos->user_data.length; i++) {
        char *tmp = data;
        ret = ddsrt_asprintf(&data, "%s%c", data, qos->user_data.value[i]);
        ddsrt_free(tmp);
        CHECK_RET_OK(ret);
      }
      char *user_data;
      ret = ddsrt_asprintf(&user_data, QOS_POLICY_USERDATA_FMT, data);
      ddsrt_free(data);
      CHECK_RET_OK(ret);
      char *tmp = sysdef_qos;
      ret = ddsrt_asprintf(&sysdef_qos, QOS_FORMAT"%s\n"QOS_FORMAT"%s", sysdef_qos, user_data);
      ddsrt_free(tmp);
      ddsrt_free(user_data);
      *validate_mask |= DDSI_QP_USER_DATA;
    }
  }
  if ((ignore_ent || (kind == DDS_READER_QOS)) &&
      (ret >= 0) && qos->present & DDSI_QP_ADLINK_READER_DATA_LIFECYCLE)
  {
    char *nowriter_delay;
    if (qos->reader_data_lifecycle.autopurge_nowriter_samples_delay == DDS_INFINITY)
    {
      if (conf->reader_data_lifecycle_nowriter_unit == sec)
        ret = ddsrt_asprintf(&nowriter_delay, QOS_NOWRITER_DELAY(QOS_DURATION_FMT_STR(sec)),
                             QOS_DURATION_INFINITY_SEC);
      else
        ret = ddsrt_asprintf(&nowriter_delay, QOS_NOWRITER_DELAY(QOS_DURATION_FMT_STR(nanosec)),
                             QOS_DURATION_INFINITY_NSEC);
    } else {
      if (conf->reader_data_lifecycle_nowriter_unit == sec)
        ret = ddsrt_asprintf(&nowriter_delay, QOS_NOWRITER_DELAY(QOS_DURATION_FMT(sec)),
                             (long long)qos->reader_data_lifecycle.autopurge_nowriter_samples_delay/DDS_NSECS_IN_SEC);
      else
        ret = ddsrt_asprintf(&nowriter_delay, QOS_NOWRITER_DELAY(QOS_DURATION_FMT(nanosec)),
                             (long long)qos->reader_data_lifecycle.autopurge_nowriter_samples_delay);
    }
    CHECK_RET_OK(ret);
    char *disposed_delay;
    if (qos->reader_data_lifecycle.autopurge_disposed_samples_delay == DDS_INFINITY)
    {
      if (conf->reader_data_lifecycle_disposed_unit == sec)
        ret = ddsrt_asprintf(&disposed_delay, QOS_DISPOSED_DELAY(QOS_DURATION_FMT_STR(sec)),
                             QOS_DURATION_INFINITY_SEC);
      else
        ret = ddsrt_asprintf(&disposed_delay, QOS_DISPOSED_DELAY(QOS_DURATION_FMT_STR(nanosec)),
                             QOS_DURATION_INFINITY_NSEC);
    } else {
      if (conf->reader_data_lifecycle_disposed_unit == sec)
        ret = ddsrt_asprintf(&disposed_delay, QOS_DISPOSED_DELAY(QOS_DURATION_FMT(sec)),
                             (long long)qos->reader_data_lifecycle.autopurge_disposed_samples_delay/DDS_NSECS_IN_SEC);
      else
        ret = ddsrt_asprintf(&disposed_delay, QOS_DISPOSED_DELAY(QOS_DURATION_FMT(nanosec)),
                             (long long)qos->reader_data_lifecycle.autopurge_disposed_samples_delay);
    }
    CHECK_RET_OK(ret);
    char *reader_data_lifecycle;
    ret = ddsrt_asprintf(&reader_data_lifecycle, QOS_POLICY_READERDATALIFECYCLE_FMT, nowriter_delay, disposed_delay);
    ddsrt_free(nowriter_delay);
    ddsrt_free(disposed_delay);
    CHECK_RET_OK(ret);
    char *tmp = sysdef_qos;
    ret = ddsrt_asprintf(&sysdef_qos, QOS_FORMAT"%s\n"QOS_FORMAT"%s", sysdef_qos, reader_data_lifecycle);
    ddsrt_free(tmp);
    ddsrt_free(reader_data_lifecycle);
    *validate_mask |= DDSI_QP_ADLINK_READER_DATA_LIFECYCLE;
  }
  if ((ignore_ent || (kind == DDS_WRITER_QOS)) &&
      (ret >= 0) && qos->present & DDSI_QP_ADLINK_WRITER_DATA_LIFECYCLE)
  {
    char *writer_data_lifecycle;
    if (qos->writer_data_lifecycle.autodispose_unregistered_instances)
      ret = ddsrt_asprintf(&writer_data_lifecycle, "%s", QOS_POLICY_WRITERDATA_LIFECYCLE_FMT(true));
    else
      ret = ddsrt_asprintf(&writer_data_lifecycle, "%s", QOS_POLICY_WRITERDATA_LIFECYCLE_FMT(false));
    CHECK_RET_OK(ret);
    char *tmp = sysdef_qos;
    ret = ddsrt_asprintf(&sysdef_qos, QOS_FORMAT"%s\n"QOS_FORMAT"%s", sysdef_qos, writer_data_lifecycle);
    ddsrt_free(tmp);
    ddsrt_free(writer_data_lifecycle);
    *validate_mask |= DDSI_QP_ADLINK_WRITER_DATA_LIFECYCLE;
  }

  *out = sysdef_qos;
fail:
  return ret;
}

#undef QOS_FORMAT

static dds_qos_t get_supported_qos(dds_qos_t qos)
{
  qos.present &= ~DDSI_QP_ADLINK_ENTITY_FACTORY;
  return qos;
}

static dds_return_t get_single_configuration(dds_qos_t *qos, sysdef_qos_conf_t *conf, dds_qos_kind_t kind, char **out_conf, uint64_t *validate_mask)
{
  dds_return_t ret = DDS_RETCODE_OK;
  char *qos_conf = NULL;
  ret = qos_to_conf(qos, conf, &qos_conf, kind, validate_mask, false);
  CU_ASSERT_TRUE(ret >= 0);
  char *def = NULL;
  switch(kind)
  {
    case DDS_PARTICIPANT_QOS:
      def = DEF(LIB(lib1,PRO(pro00,ENT("%s",domain_participant))));
      break;
    case DDS_PUBLISHER_QOS:
      def = DEF(LIB(lib1,PRO(pro00,ENT("%s",publisher))));
      break;
    case DDS_SUBSCRIBER_QOS:
      def = DEF(LIB(lib1,PRO(pro00,ENT("%s",subscriber))));
      break;
    case DDS_TOPIC_QOS:
      def = DEF(LIB(lib1,PRO(pro00,ENT("%s",topic))));
      break;
    case DDS_READER_QOS:
      def = DEF(LIB(lib1,PRO(pro00,ENT("%s",datareader))));
      break;
    case DDS_WRITER_QOS:
      def = DEF(LIB(lib1,PRO(pro00,ENT("%s",datawriter))));
      break;
    default:
      ddsrt_free(qos_conf);
      CU_FAIL("unsupported QOS_KIND");
  }
  ret = ddsrt_asprintf(out_conf, def, qos_conf);
  ddsrt_free(qos_conf);
  CU_ASSERT_TRUE(ret >= 0);

  return ret;
}

#define N nsec
#define S sec
CU_TheoryDataPoints(qos_provider, get_qos_default) = {
  // The type of entity_qos that will be tested with it default qos.
  CU_DataPoints(dds_qos_kind_t,
    DDS_TOPIC_QOS,DDS_READER_QOS,DDS_WRITER_QOS),
  // In which format <sec>/<nanosec> `duration` will be presented in sysdef file.
  CU_DataPoints(sysdef_qos_conf_t,
    {N,N,S,S,N,N,S,S,N},{S,S,S,S,S,N,S,S,N},{S,N,S,N,S,N,S,N,S},
  ),
};
#undef N
#undef S

// @brief This test check sysdef file qos created correctly by qos_provider (in this case with default qos).
CU_Theory((dds_qos_kind_t kind, sysdef_qos_conf_t dur_conf), qos_provider, get_qos_default)
{
  dds_return_t ret = DDS_RETCODE_OK;
  char *full_configuration = NULL;
  dds_qos_t qos;
  switch(kind)
  {
    case DDS_PARTICIPANT_QOS:
      qos = get_supported_qos(ddsi_default_qos_participant);
      break;
    case DDS_PUBLISHER_QOS:
      qos = get_supported_qos(ddsi_default_qos_publisher_subscriber);
      break;
    case DDS_SUBSCRIBER_QOS:
      qos = get_supported_qos(ddsi_default_qos_publisher_subscriber);
      break;
    case DDS_TOPIC_QOS:
      qos = get_supported_qos(ddsi_default_qos_topic);
      break;
    case DDS_READER_QOS:
      qos = get_supported_qos(ddsi_default_qos_reader);
      break;
    case DDS_WRITER_QOS:
      qos = get_supported_qos(ddsi_default_qos_writer);
      break;
    default:
      CU_FAIL("oops");
      break;
  }
  uint64_t validate_mask = 0;
  // init configuraiton with qos of `kind` in sysdef format
  ret = get_single_configuration(&qos, &dur_conf, kind, &full_configuration, &validate_mask);
  CU_ASSERT_TRUE(ret >= 0);
  dds_qos_provider_t *provider;
  // init qos provider with create configuration
  ret = dds_create_qos_provider(full_configuration, &provider);
  ddsrt_free(full_configuration);
  CU_ASSERT_EQUAL(ret, DDS_RETCODE_OK);
  const dds_qos_t *act_qos;
  // get qos from provider
  ret = dds_qos_provider_get_qos(provider, kind, "lib1::pro00", &act_qos);
  CU_ASSERT_EQUAL(ret, DDS_RETCODE_OK);
  // calculate the difference between defined qos and qos from provider
  uint64_t res = ddsi_xqos_delta(&qos, act_qos, validate_mask);
  CU_ASSERT_EQUAL(act_qos->present, validate_mask);
  CU_ASSERT_EQUAL(res, 0);
  dds_delete_qos_provider(provider);

}

#define _C "abcdefghijklmnopqrstuvwxyz"
#define RND_UCHAR (unsigned char)(_C[ddsrt_random () % (uint32_t)(strlen(_C)-1)])
#define RND_UCHAR3 (unsigned char[]){RND_UCHAR, RND_UCHAR, RND_UCHAR}
#define RND_CHAR (char)(_C[ddsrt_random () % (uint32_t)(strlen(_C)-1)])
#define RND_CHAR4 (char[]){RND_CHAR, RND_CHAR, RND_CHAR, '\0'}
#define RND_CHAR3x4 (char *[]){RND_CHAR4, RND_CHAR4, RND_CHAR4}

#define Q_DATA4(kind) .kind##_data={.value=RND_UCHAR3,.length=3},
#define Q_DURABILITY(knd) .durability={.kind=knd},
#define Q_DEADLINE(tm) .deadline={.deadline=tm},
#define Q_LATENCYBUDGET(tm) .latency_budget={.duration=tm},
#define Q_OWNERSHIP(knd) .ownership={.kind=knd},
#define Q_LIVELINESS(knd,dur) .liveliness={.kind=knd,.lease_duration=dur},
#define Q_RELIABILITY(knd,tm) .reliability={.kind=knd,.max_blocking_time=tm},
#define Q_TRANSPORTPRIO(vl) .transport_priority={.value=vl},
#define Q_LIFESPAN(dur) .lifespan={.duration=dur},
#define Q_DESTINATIONORDER(knd) .destination_order={.kind=knd},
#define Q_HISTORY(knd,dp) .history={.kind=knd,.depth=dp},
#define Q_RESOURCELIMITS(ms,mi,mspmi) .resource_limits={.max_samples=ms,.max_instances=mi,.max_samples_per_instance=mspmi},
#define Q_DATA3x4(kind) .kind={.strs=RND_CHAR3x4,.n=3},
#define Q_PRESENATION(ask,oa,ca) .presentation={.access_scope=ask,.ordered_access=oa,.coherent_access=ca},
#define Q_TIMEBASEDFILTER(dur) .time_based_filter={.minimum_separation=dur},
#define Q_READERLIFECYCLE(adsd,ansd) .reader_data_lifecycle={.autopurge_disposed_samples_delay=adsd,.autopurge_nowriter_samples_delay=ansd},
#define Q_OWNERSHIPSTRENGTH(vl) .ownership_strength={.value=vl},
#define Q_WRITERLIFECYCLE(aui) .writer_data_lifecycle={.autodispose_unregistered_instances=aui},
#define Q_DURABILITYSERVICE(dur,hk,hd,ms,mi,mspi) .durability_service={.service_cleanup_delay=dur,Q_HISTORY(hk,hd)Q_RESOURCELIMITS(ms,mi,mspi)},

#define QOS_ALL_PRESENT .present = DDSI_QP_TOPIC_DATA | DDSI_QP_DURABILITY | \
      DDSI_QP_DEADLINE | DDSI_QP_LATENCY_BUDGET | \
      DDSI_QP_OWNERSHIP | DDSI_QP_LIVELINESS | \
      DDSI_QP_RELIABILITY | DDSI_QP_TRANSPORT_PRIORITY | \
      DDSI_QP_LIFESPAN | DDSI_QP_DESTINATION_ORDER | \
      DDSI_QP_HISTORY | DDSI_QP_RESOURCE_LIMITS | \
      DDSI_QP_USER_DATA | DDSI_QP_PARTITION | \
      DDSI_QP_PRESENTATION | DDSI_QP_GROUP_DATA | \
      DDSI_QP_TIME_BASED_FILTER | DDSI_QP_ADLINK_READER_DATA_LIFECYCLE | \
      DDSI_QP_OWNERSHIP_STRENGTH | DDSI_QP_ADLINK_WRITER_DATA_LIFECYCLE | \
      DDSI_QP_DURABILITY_SERVICE,

#define QOS_ALL_BASE { \
    QOS_ALL_PRESENT \
    Q_DATA4(topic)Q_DURABILITY(DDS_DURABILITY_VOLATILE) \
    Q_DEADLINE(DDS_SECS(1))Q_LATENCYBUDGET(DDS_SECS(1)) \
    Q_OWNERSHIP(DDS_OWNERSHIP_EXCLUSIVE)Q_LIVELINESS(DDS_LIVELINESS_AUTOMATIC,DDS_INFINITY) \
    Q_RELIABILITY(DDS_RELIABILITY_RELIABLE, DDS_SECS(1))Q_TRANSPORTPRIO(1000) \
    Q_LIFESPAN(DDS_SECS(1))Q_DESTINATIONORDER(DDS_DESTINATIONORDER_BY_SOURCE_TIMESTAMP) \
    Q_HISTORY(DDS_HISTORY_KEEP_LAST,1)Q_RESOURCELIMITS(1,1,1) \
    Q_DATA4(user)Q_DATA3x4(partition) \
    Q_PRESENATION(DDS_PRESENTATION_TOPIC,1,1)Q_DATA4(group) \
    Q_TIMEBASEDFILTER(DDS_SECS(1))Q_READERLIFECYCLE(DDS_SECS(1), DDS_SECS(1)) \
    Q_OWNERSHIPSTRENGTH(100)Q_WRITERLIFECYCLE(1) \
    Q_DURABILITYSERVICE(DDS_SECS(1),DDS_HISTORY_KEEP_ALL,-1,1,1,1) \
  }

CU_TheoryDataPoints(qos_provider, get_qos_all) = {
  // The type of entity_qos that will be tested with custom qos.
  CU_DataPoints(dds_qos_kind_t,
    DDS_PARTICIPANT_QOS,DDS_PUBLISHER_QOS,DDS_SUBSCRIBER_QOS,
    DDS_TOPIC_QOS,DDS_READER_QOS,DDS_WRITER_QOS),
};

#define Q QOS_ALL_BASE
// @brief This test check sysdef file qos created correctly by qos_provider
// (in this case with custom qos with all possible values presented).
CU_Theory((dds_qos_kind_t kind),qos_provider, get_qos_all)
{
  dds_return_t ret = DDS_RETCODE_OK;
  char *full_configuration = NULL;
  dds_qos_t qos = Q;
  sysdef_qos_conf_t dur_conf = {sec,sec,sec,sec,sec,sec,sec,sec,sec};
  uint64_t validate_mask = 0;
  // init configuraiton with qos of `kind` in sysdef format
  ret = get_single_configuration(&qos, &dur_conf, kind, &full_configuration, &validate_mask);
  CU_ASSERT_TRUE(ret >= 0);
  dds_qos_provider_t *provider;
  // init qos provider with create configuration
  ret = dds_create_qos_provider(full_configuration, &provider);
  ddsrt_free(full_configuration);
  CU_ASSERT_EQUAL(ret, DDS_RETCODE_OK);
  const dds_qos_t *act_qos;
  ret = dds_qos_provider_get_qos(provider, kind, "lib1::pro00", &act_qos);
  CU_ASSERT_EQUAL(ret, DDS_RETCODE_OK);
  // calculate the difference between defined qos and qos from provider
  uint64_t res = ddsi_xqos_delta(&qos, act_qos, validate_mask);
  CU_ASSERT_EQUAL(act_qos->present, validate_mask);
  CU_ASSERT_EQUAL(res, 0);
  dds_delete_qos_provider(provider);
}

CU_TheoryDataPoints(qos_provider, create_wrong_qos) = {
  // The type of entity_qos that will be tested with wrong qos
  CU_DataPoints(dds_qos_kind_t,
    DDS_PARTICIPANT_QOS,DDS_PUBLISHER_QOS,DDS_SUBSCRIBER_QOS,
    DDS_TOPIC_QOS,DDS_READER_QOS,DDS_WRITER_QOS),
  // Expected retcodes
  CU_DataPoints(dds_return_t,
    DDS_RETCODE_ERROR,DDS_RETCODE_ERROR,DDS_RETCODE_ERROR,
    DDS_RETCODE_ERROR,DDS_RETCODE_ERROR,DDS_RETCODE_ERROR),
};

#define QOS_TO_CONF_SNGL(excl_msk,knd) \
  def = DEF(LIB(lib1,PRO(pro00,ENT("%s",knd)))); \
  qos.present ^= (excl_msk); \
  ret = qos_to_conf(&qos, &conf, &qos_conf, kind, &validate_mask, true);

CU_Theory((dds_qos_kind_t kind, dds_return_t code),qos_provider, create_wrong_qos)
{
  dds_return_t ret = DDS_RETCODE_OK;
  char *full_configuration = NULL;
  dds_qos_t qos = Q;
  sysdef_qos_conf_t conf = {sec,sec,sec,sec,sec,sec,sec,sec,sec};
  uint64_t validate_mask = 0;
  char *def = NULL;
  char *qos_conf = NULL;
  // init sysdef configuration that contains qos that not related for this `kind` of entity
  // (wrong for this entity kind)
  switch(kind)
  {
    case DDS_PARTICIPANT_QOS:
      QOS_TO_CONF_SNGL(DDS_PARTICIPANT_QOS_MASK,domain_participant);
      break;
    case DDS_PUBLISHER_QOS:
      QOS_TO_CONF_SNGL(DDS_PUBLISHER_QOS_MASK,publisher);
      break;
    case DDS_SUBSCRIBER_QOS:
      QOS_TO_CONF_SNGL(DDS_SUBSCRIBER_QOS_MASK,subscriber);
      break;
    case DDS_TOPIC_QOS:
      QOS_TO_CONF_SNGL(DDS_TOPIC_QOS_MASK,topic);
      break;
    case DDS_READER_QOS:
      QOS_TO_CONF_SNGL(DDS_READER_QOS_MASK,datareader);
      break;
    case DDS_WRITER_QOS:
      QOS_TO_CONF_SNGL(DDS_WRITER_QOS_MASK,datawriter);
      break;
    default:
      CU_FAIL("unsupported QOS_KIND");
  }
  CU_ASSERT_TRUE(ret >= 0);
  ret = ddsrt_asprintf(&full_configuration, def, qos_conf);
  ddsrt_free(qos_conf);
  CU_ASSERT_TRUE(ret >= 0);
  dds_qos_provider_t *provider = NULL;
  // init qos provider with create configuration
  ret = dds_create_qos_provider(full_configuration, &provider);
  ddsrt_free(full_configuration);
  // check that retcode eq to expected
  CU_ASSERT_EQUAL(ret, code);
  // provider not initialized
  CU_ASSERT_PTR_NULL(provider);
}
#undef QOS_TO_CONF_SNGL
#undef Q
