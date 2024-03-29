#include "fuzz_handshake_harness.h"
#include <string.h>

#include <dds/features.h>
#include <dds__types.h>
#include <dds__init.h>
#include <dds__domain.h>
#include <ddsi__addrset.h>
#include <ddsi__handshake.h>
#include <ddsi__thread.h>
#include <ddsi__plist.h>
#include <ddsi__proxy_participant.h>
#include <ddsi__vendor.h>
#include <ddsi__protocol.h>
#include <dds__qos.h>
#include <dds/ddsi/ddsi_iid.h>
#include <dds/ddsi/ddsi_init.h>
#include <dds/ddsi/ddsi_participant.h>
#include <dds/ddsi/ddsi_entity_index.h>
#include <ddsi__security_omg.h>
//#include <dds/security/core/dds_security_fsm.h>
#include <common/config_env.h>
#include <common/test_identity.h>

const char *sec_config =
    "<Domain id=\"any\">"
    "  <Discovery>"
    "    <Tag>${CYCLONEDDS_PID}</Tag>"
    "  </Discovery>"
    "  <Tracing><Verbosity>finest</></>"
    "  <Security>"
    "    <Authentication>"
    "      <Library initFunction=\"init_test_authentication_all_ok\" finalizeFunction=\"finalize_test_authentication_all_ok\" path=\"" WRAPPERLIB_PATH("dds_security_authentication_wrapper") "\"/>"
    "      <IdentityCertificate>"TEST_IDENTITY_CERTIFICATE_DUMMY"</IdentityCertificate>"
    "      <IdentityCA>"TEST_IDENTITY_CA_CERTIFICATE_DUMMY"</IdentityCA>"
    "      <PrivateKey>"TEST_IDENTITY_PRIVATE_KEY_DUMMY"</PrivateKey>"
    "      <Password>testtext_Password_testtext</Password>"
    "      <TrustedCADirectory>testtext_Dir_testtext</TrustedCADirectory>"
    "    </Authentication>"
    "    <Cryptographic>"
    "      <Library initFunction=\"init_test_cryptography_all_ok\" finalizeFunction=\"finalize_test_cryptography_all_ok\" path=\"" WRAPPERLIB_PATH("dds_security_cryptography_wrapper") "\"/>"
    "    </Cryptographic>"
    "    <AccessControl>"
    "      <Library initFunction=\"init_test_access_control_all_ok\" finalizeFunction=\"finalize_test_access_control_all_ok\" path=\"" WRAPPERLIB_PATH("dds_security_access_control_wrapper") "\"/>"
    "      <Governance></Governance>"
    "      <PermissionsCA></PermissionsCA>"
    "      <Permissions></Permissions>"
    "    </AccessControl>"
    "  </Security>"
    "</Domain>";
struct ddsi_cfgst *g_cfgst;
struct ddsi_guid g_ppguid;
struct ddsi_guid g_proxy_ppguid;
static struct ddsi_thread_state *thrst;

static struct ddsi_domaingv gv;

struct {
    struct ddsi_participant *pp;
    struct ddsi_proxy_participant *proxy_pp;
    struct ddsi_handshake *hs;
} harness;

bool fuzz_handshake_init() {
    ddsi_iid_init();
    ddsi_thread_states_init();
    // register the main thread, then claim it as spawned by Cyclone because the
    // internal processing has various asserts that it isn't an application thread
    // doing the dirty work
    thrst = ddsi_lookup_thread_state ();
    assert (thrst->state == DDSI_THREAD_STATE_LAZILY_CREATED);
    thrst->state = DDSI_THREAD_STATE_ALIVE;
    thrst->vtime.v = DDSI_VTIME_NEST_MASK;
    ddsrt_atomic_stvoidp (&thrst->gv, &gv);
    memset(&gv, 0, sizeof(gv));
    ddsi_config_init_default(&gv.config);
    gv.config.transport_selector = DDSI_TRANS_NONE;
    ddsi_config_prep(&gv, NULL);
    ddsi_init(&gv, NULL);
    g_cfgst = ddsi_config_init(sec_config, &gv.config, 1);

    printf("Security: %p\n", gv.security_context);

    // Create participant
    {
        ddsi_plist_t pplist;
        ddsi_plist_init_empty(&pplist);
        dds_qos_t *new_qos = dds_create_qos();
        ddsi_xqos_mergein_missing (new_qos, &gv.default_local_xqos_pp, ~(uint64_t)0);
        dds_apply_entity_naming(new_qos, NULL, &gv);
        ddsi_xqos_mergein_missing (&pplist.qos, new_qos, ~(uint64_t)0);
        dds_delete_qos(new_qos);

        dds_return_t ret = ddsi_new_participant(&g_ppguid, &gv, 0, &pplist);
        if(ret != DDS_RETCODE_OK) abort();
        harness.pp = ddsi_entidx_lookup_participant_guid(gv.entity_index, &g_ppguid);
        ddsi_plist_fini(&pplist);
    }

    // Create proxy participant
    {
        ddsi_plist_t pplist;
        ddsi_plist_init_empty(&pplist);
        pplist.present |= PP_IDENTITY_TOKEN;
        ddsi_token_t identity_token = {
            .class_id = NULL,
            .properties = {
                .n = 0,
                .props = NULL
            },
            .binary_properties = {
                .n = 0,
                .props = NULL
            }
        };
        pplist.identity_token = identity_token;
        union { uint64_t u64; uint32_t u32[2]; } u;
        u.u32[0] = gv.ppguid_base.prefix.u[1];
        u.u32[1] = gv.ppguid_base.prefix.u[2];
        u.u64 += ddsi_iid_gen ();
        g_proxy_ppguid.prefix.u[0] = gv.ppguid_base.prefix.u[0];
        g_proxy_ppguid.prefix.u[1] = u.u32[0];
        g_proxy_ppguid.prefix.u[2] = u.u32[1];
        g_proxy_ppguid.entityid.u = DDSI_ENTITYID_PARTICIPANT;
        ddsrt_wctime_t timestamp = { .v = dds_time() };
        if (!ddsi_new_proxy_participant(&gv,
                &g_proxy_ppguid,
                DDSI_DISC_BUILTIN_ENDPOINT_PARTICIPANT_SECURE_ANNOUNCER,
                NULL,
                ddsi_new_addrset(),
                ddsi_new_addrset(),
                &pplist,
                DDS_INFINITY,
                DDSI_VENDORID_ECLIPSE,
                DDSI_CF_PROXYPP_NO_SPDP,
                timestamp,
                0)) {
            abort();
        }

        harness.proxy_pp = ddsi_entidx_lookup_proxy_participant_guid(gv.entity_index, &g_proxy_ppguid);
        ddsi_plist_fini(&pplist);
    }
    printf("domaingv %p\n", &gv);
    printf("Participant %p\n", harness.pp);
    printf("Participant domaingv %p\n", harness.pp->e.gv);
    printf("Participant hsadmin %p\n", harness.pp->e.gv->hsadmin);
    printf("Participant sec_attr %p\n", harness.pp->sec_attr);
    printf("Proxy Participant %p\n", harness.proxy_pp);
    printf("Proxy Participant domaingv %p\n", harness.proxy_pp->e.gv);
    printf("Proxy Participant hsadmin %p\n", harness.proxy_pp->e.gv->hsadmin);
    harness.hs = ddsi_handshake_find(harness.pp, harness.proxy_pp);
    printf("Handshake %p\n", harness.hs);
    return true;
}

void hs_end_cb(struct ddsi_handshake *handshake, struct ddsi_participant *pp, struct ddsi_proxy_participant *proxy_pp, enum ddsi_handshake_state state)
{
    ddsi_handshake_release(handshake);
}

void fuzz_handshake_reset(void) {
    // Remove proxy participant, if created
    // Release our handshake
    ddsi_handshake_remove(harness.pp, harness.proxy_pp);
    ddsi_handshake_register(harness.pp, harness.proxy_pp, hs_end_cb);
    //harness.hs = ddsi_handshake_find(harness.pp, harness.proxy_pp);
    if (harness.hs == NULL) abort();
    //// Make a new proxy participant
    // Get the proxy participant
    //harness.proxy_pp = ...
    // Get the handshake
}


void fuzz_handshake_handle_timeout(void) {
    //dds_security_fsm_dispatch(harness.hs->fsm, DDS_SECURITY_FSM_EVENT_TIMEOUT, false);
}
void fuzz_handshake_handle_request(void) {}
void fuzz_handshake_handle_reply(void) {}
void fuzz_handshake_handle_final(void) {}
void fuzz_handshake_handle_crypto_tokens(void) {}
