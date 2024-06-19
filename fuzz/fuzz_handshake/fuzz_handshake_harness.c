#include "fuzz_handshake_harness.h"
#include "fuzz_handshake_expose.h"

#include <string.h>
#include <dlfcn.h>

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
#include <ddsi__security_util.h>
#include <ddsi__xevent.h>
#include <ddsi__gc.h>
#include <dds__qos.h>
#include <dds/ddsi/ddsi_iid.h>
#include <dds/ddsi/ddsi_init.h>
#include <dds/ddsi/ddsi_participant.h>
#include <dds/ddsi/ddsi_entity_index.h>
#include <dds/ddsi/ddsi_unused.h>
#include <ddsi__security_omg.h>
#include <dds/security/core/dds_security_fsm.h>
#include <dds/security/core/dds_security_utils.h>
#include <dds/security/core/dds_security_serialize.h>
#include <common/test_identity.h>

#include <openssl/pem.h>

const char *sec_config =
    "<Domain id=\"any\">"
    "  <Discovery>"
    "    <Tag>${CYCLONEDDS_PID}</Tag>"
    "  </Discovery>"
    "  <Tracing><Verbosity>finest</></>"
    "  <Security>"
    "    <Authentication>"
    "      <Library initFunction=\"init_authentication\" finalizeFunction=\"finalize_authentication\" path=\"dds_security_auth\"/>"
    "      <IdentityCertificate>data:," TEST_IDENTITY1_CERTIFICATE "</IdentityCertificate>"
    "      <PrivateKey>data:," TEST_IDENTITY1_PRIVATE_KEY "</PrivateKey>"
    "      <IdentityCA>data:," TEST_IDENTITY_CA1_CERTIFICATE "</IdentityCA>"
    "    </Authentication>"
    "    <Cryptographic>"
    "      <Library initFunction=\"init_crypto\" finalizeFunction=\"finalize_crypto\" path=\"dds_security_crypto\"/>"
    "    </Cryptographic>"
    "    <AccessControl>"
    "      <Library initFunction=\"init_access_control\" finalizeFunction=\"finalize_access_control\" path=\"dds_security_ac\"/>"
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
EVP_PKEY *g_private_key;

#define FUZZ_HANDSHAKE_EVENT_HANDLED (-4)
ddsrt_atomic_uint32_t g_fsm_done;
ddsrt_atomic_uint32_t g_fuzz_events;

static ddsrt_dynlib_t auth_plugin_handle;

typedef DDS_Security_ValidationResult_t (*dhpkey2oct_t)(EVP_PKEY *, int, unsigned char **, uint32_t *, DDS_Security_SecurityException *);
static dhpkey2oct_t dh_public_key_to_oct_ptr = NULL;

typedef DDS_Security_ValidationResult_t (*gendhkeys_t)(EVP_PKEY **, int, DDS_Security_SecurityException *);
static gendhkeys_t generate_dh_keys_ptr = NULL;

typedef DDS_Security_ValidationResult_t (*cvas_t)(bool, EVP_PKEY *, const unsigned char *, const size_t, unsigned char **, size_t *, DDS_Security_SecurityException *);
static cvas_t create_validate_asymmetrical_signature_ptr = NULL;

struct {
    struct ddsi_participant *pp;
    struct ddsi_proxy_participant *proxy_pp;
    struct ddsi_handshake *hs;
} harness;

bool fuzz_handshake_init()
{
    // Load private key for remote identity
    BIO *bio;
    bio = BIO_new_mem_buf(TEST_IDENTITY3_PRIVATE_KEY, -1);
    if (!bio) abort();
    g_private_key = PEM_read_bio_PrivateKey(bio, NULL, NULL, "");
    if (!g_private_key) abort();
    BIO_free(bio);

    //ddsi_iid_init();
    ddsi_thread_states_init();
    // register the main thread, then claim it as spawned by Cyclone because the
    // internal processing has various asserts that it isn't an application thread
    // doing the dirty work
    thrst = ddsi_lookup_thread_state ();
    assert (thrst->state == DDSI_THREAD_STATE_LAZILY_CREATED);
    thrst->state = DDSI_THREAD_STATE_ALIVE;
    thrst->vtime.v = 0;
    ddsrt_atomic_stvoidp (&thrst->gv, &gv);
    memset(&gv, 0, sizeof(gv));
    ddsi_config_init_default(&gv.config);
    gv.config.transport_selector = DDSI_TRANS_NONE;
    ddsi_config_prep(&gv, NULL);
    ddsi_init(&gv, NULL);
    gv.handshake_include_optional = true;
    g_cfgst = ddsi_config_init(sec_config, &gv.config, 1);

    //FILE *fp = fopen("/dev/stdout", "w");
    //dds_log_cfg_init(&gv.logconfig, 1, DDS_LC_TRACE | DDS_LC_ERROR | DDS_LC_WARNING, NULL, fp);

    // Disable logging
    dds_log_cfg_init(&gv.logconfig, 1, 0, NULL, NULL);

    // We use a statically linked build, all functions we need to lookup need to be
    // exported from the plugin
    //
    // See src/security/builtin_plugins/authentication/CMakeLists.txt
    if (ddsrt_dlopen("dds_security_auth", true, &auth_plugin_handle) != DDS_RETCODE_OK)
      abort();
    if (ddsrt_dlsym(auth_plugin_handle, "generate_dh_keys", (void **)&generate_dh_keys_ptr) != DDS_RETCODE_OK)
      abort();
    if (ddsrt_dlsym(auth_plugin_handle, "dh_public_key_to_oct", (void **)&dh_public_key_to_oct_ptr) != DDS_RETCODE_OK)
      abort();
    if (ddsrt_dlsym(auth_plugin_handle, "create_validate_asymmetrical_signature", (void **)&create_validate_asymmetrical_signature_ptr) != DDS_RETCODE_OK)
      abort();

    // Create participant
    ddsi_thread_state_awake(ddsi_lookup_thread_state(), &gv);
    {
        ddsi_plist_t pplist;
        ddsi_plist_init_empty(&pplist);
        dds_qos_t *new_qos = dds_create_qos();
        ddsi_xqos_mergein_missing (new_qos, &gv.default_local_xqos_pp, ~(uint64_t)0);
        dds_apply_entity_naming(new_qos, NULL, &gv);
        ddsi_xqos_mergein_missing (&pplist.qos, new_qos, ~(uint64_t)0);
        dds_delete_qos(new_qos);

        ddsi_generate_participant_guid (&g_ppguid, &gv);
        dds_return_t ret = ddsi_new_participant(&g_ppguid, &gv, 0, &pplist);
        if(ret != DDS_RETCODE_OK) abort();
        harness.pp = ddsi_entidx_lookup_participant_guid(gv.entity_index, &g_ppguid);
        ddsi_plist_fini(&pplist);
    }
    ddsi_thread_state_asleep(ddsi_lookup_thread_state());

    return true;
}

static void hs_end_cb(UNUSED_ARG(struct ddsi_handshake *handshake),
                      UNUSED_ARG(struct ddsi_participant *pp),
                      UNUSED_ARG(struct ddsi_proxy_participant *proxy_pp),
                      UNUSED_ARG(enum ddsi_handshake_state state))
{
    //printf("HANDSHAKE END: %d\n", state); fflush(stdout);
}

static void fsm_debug_func(UNUSED_ARG(struct dds_security_fsm *fsm), DDS_SECURITY_FSM_DEBUG_ACT act, UNUSED_ARG(const dds_security_fsm_state *current), int event_id, UNUSED_ARG(void *arg))
{
    // This event is never generated by the state machine.
    // The fuzzer throws it in at the end of its events to signal that it's done
    // and the handshake can be abandoned.
    if (act == DDS_SECURITY_FSM_DEBUG_ACT_HANDLING && event_id == DDS_SECURITY_FSM_EVENT_DELETE) {
        ddsrt_atomic_st32(&g_fsm_done, 1);
    }

    // This event is used to serialize the fuzzer, as the state machine is driven by a
    // separate thread. The fuzzer generates events and waits for the state machine to
    // finish the resulting transition. This prevents triggering any race conditions which
    // may result from a bug in the fuzzing harness. Further, the handshake data structures
    // containing the messages from a remote participant are being reused, so overwriting them
    // before the state machine handled a message will lead nowhere. Lastly, the harness does
    // not need to concern itself with locking.
    if (act == DDS_SECURITY_FSM_DEBUG_ACT_HANDLING && event_id == FUZZ_HANDSHAKE_EVENT_HANDLED) {
        ddsrt_atomic_inc32(&g_fuzz_events);
    }
}

void fuzz_handshake_reset(bool initiate_remote) {
    ddsrt_atomic_st32(&g_fsm_done, 0);
    ddsrt_atomic_st32(&g_fuzz_events, 0);

    // Create proxy participant
    ddsi_plist_t pplist;
    ddsi_plist_init_empty(&pplist);
    pplist.present |= PP_IDENTITY_TOKEN;
    ddsi_token_t identity_token = {
        .class_id = strdup(DDS_SECURITY_AUTH_TOKEN_CLASS_ID),
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
    if (!initiate_remote) {
        g_proxy_ppguid.prefix.s[0] = 0xff;
        g_proxy_ppguid.prefix.s[1] = 0xff;
        g_proxy_ppguid.prefix.s[2] = 0xff;
    } else {
        g_proxy_ppguid.prefix.s[0] = 0x00;
        g_proxy_ppguid.prefix.s[1] = 0x00;
        g_proxy_ppguid.prefix.s[2] = 0x00;
    }

    ddsrt_wctime_t timestamp = { .v = dds_time() };

    // We avoid generating network traffic by using DDSI_TRANS_NONE
    // This is the only valid locator for that "network".
    const ddsi_locator_t loc = {
      .kind = 2147483647, .address = {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, .port = 0
    };
    struct ddsi_addrset *as = ddsi_new_addrset();
    ddsi_add_locator_to_addrset(&gv,as, &loc);
    assert(!ddsi_addrset_empty_uc(as));

    ddsi_thread_state_awake(ddsi_lookup_thread_state(), &gv);
    if (!ddsi_new_proxy_participant(&harness.proxy_pp, &gv,
            &g_proxy_ppguid,
            DDSI_DISC_BUILTIN_ENDPOINT_PARTICIPANT_SECURE_ANNOUNCER|
            DDSI_BUILTIN_ENDPOINT_PARTICIPANT_STATELESS_MESSAGE_ANNOUNCER|DDSI_BUILTIN_ENDPOINT_PARTICIPANT_STATELESS_MESSAGE_DETECTOR,
            NULL,
            as,
            ddsi_ref_addrset(as),
            &pplist,
            DDS_INFINITY,
            DDSI_VENDORID_ECLIPSE,
            DDSI_CF_PROXYPP_NO_SPDP,
            timestamp,
            0)) {
        abort();
    }
    ddsi_thread_state_asleep(ddsi_lookup_thread_state());
    ddsi_plist_fini(&pplist);

    harness.hs = ddsi_handshake_find(harness.pp, harness.proxy_pp);
    if (harness.hs == NULL) abort();

    // We abuse the debug function for the serialization of the fuzzer thread
    // and the handshake fsm thread.
    dds_security_fsm_set_debug(harness.hs->fsm, fsm_debug_func);
    harness.hs->end_cb = hs_end_cb;
    ddsi_handshake_release(harness.hs);
}

void fuzz_handshake_handle_timeout(void) {
    dds_security_fsm_dispatch(harness.hs->fsm, DDS_SECURITY_FSM_EVENT_TIMEOUT, false);
}

static DDS_Security_ValidationResult_t create_dh_key(int algo, unsigned char **data, uint32_t *len) {
    EVP_PKEY *key;
    DDS_Security_SecurityException ex = {0};
    DDS_Security_ValidationResult_t result = generate_dh_keys_ptr(&key, algo, &ex);
    if (result != DDS_SECURITY_VALIDATION_OK) goto out;

    result = dh_public_key_to_oct_ptr(key, algo, data, len, &ex);
out:
    EVP_PKEY_free(key);
    DDS_Security_Exception_reset(&ex);
    return result;
}

void fuzz_handshake_handle_request(ddsi_dataholder_t *token) {
    struct ddsi_participant_generic_message msg;
    msg.message_class_id = DDS_SECURITY_AUTH_HANDSHAKE;
    msg.message_data.n = 1;
    msg.message_data.tags = token;
    token->class_id = DDS_SECURITY_AUTH_HANDSHAKE_REQUEST_TOKEN_ID;

    for (uint32_t i = 0; i < token->binary_properties.n; i++) {
        dds_binaryproperty_t *binprop = &token->binary_properties.props[i];
        // To avoid fuzzing openssl, always use a valid certificate
        if (strcmp(binprop->name, "c.id") == 0) {
            free(binprop->value.value);
            binprop->value.length = strlen(TEST_IDENTITY3_CERTIFICATE);
            binprop->value.value = (unsigned char *) strdup(TEST_IDENTITY3_CERTIFICATE);
        }
        // Provide a valid dh public key
        if (strcmp(binprop->name, "dh1") == 0) {
            unsigned char *data;
            uint32_t len;
            if (create_dh_key(1, &data, &len) == DDS_SECURITY_VALIDATION_OK) {
                free(binprop->value.value);
                binprop->value.length = len;
                binprop->value.value = data;
            }
        }
    }
    ddsi_handshake_handle_message(harness.hs, harness.pp, harness.proxy_pp, &msg);
}

static void create_signature(const DDS_Security_BinaryProperty_t **bprops, uint32_t n_bprops, unsigned char **signature, size_t *signatureLen)
{
    unsigned char *buffer;
    size_t size;
    DDS_Security_Serializer serializer = DDS_Security_Serializer_new(4096, 4096);
    DDS_Security_Serialize_BinaryPropertyArray(serializer, bprops, n_bprops);
    DDS_Security_Serializer_buffer(serializer, &buffer, &size);
    DDS_Security_SecurityException ex;
    (void) create_validate_asymmetrical_signature_ptr(true, g_private_key, buffer, size, signature, signatureLen, &ex);
    free(buffer);
    DDS_Security_Serializer_free(serializer);
}

static void create_hash(const DDS_Security_DataHolder *dh, DDS_Security_BinaryProperty_t *bprop, const char *name)
{
    unsigned char *buffer;
    size_t size;

    DDS_Security_BinaryProperty_t *tokens = DDS_Security_BinaryPropertySeq_allocbuf(5);
    const DDS_Security_BinaryProperty_t *c_id = DDS_Security_DataHolder_find_binary_property(dh, "c.id");
    uint32_t tokidx = 0;
    if (c_id) DDS_Security_BinaryProperty_copy(&tokens[tokidx++], c_id);
    const DDS_Security_BinaryProperty_t *c_perm = DDS_Security_DataHolder_find_binary_property(dh, "c.perm");
    if (c_perm) DDS_Security_BinaryProperty_copy(&tokens[tokidx++], c_perm);
    const DDS_Security_BinaryProperty_t *c_pdata = DDS_Security_DataHolder_find_binary_property(dh, "c.pdata");
    if (c_pdata) DDS_Security_BinaryProperty_copy(&tokens[tokidx++], c_pdata);
    const DDS_Security_BinaryProperty_t *c_dsign_algo = DDS_Security_DataHolder_find_binary_property(dh, "c.dsign_algo");
    if (c_dsign_algo) DDS_Security_BinaryProperty_copy(&tokens[tokidx++], c_dsign_algo);
    const DDS_Security_BinaryProperty_t *c_kagree_algo = DDS_Security_DataHolder_find_binary_property(dh, "c.kagree_algo");
    if (c_kagree_algo) DDS_Security_BinaryProperty_copy(&tokens[tokidx++], c_kagree_algo);
    DDS_Security_BinaryPropertySeq seq = { ._length = tokidx, ._buffer = tokens };
    DDS_Security_Serializer serializer = DDS_Security_Serializer_new(4096, 4096);
    DDS_Security_Serialize_BinaryPropertySeq(serializer, &seq);
    DDS_Security_Serializer_buffer(serializer, &buffer, &size);

    unsigned char *hash = malloc(SHA256_DIGEST_LENGTH);
    SHA256(buffer, size, hash);
    free(buffer);
    DDS_Security_Serializer_free(serializer);

    bprop->name = strdup(name);
    bprop->value._length = SHA256_DIGEST_LENGTH;
    bprop->value._maximum = SHA256_DIGEST_LENGTH;
    bprop->value._buffer = hash;
    DDS_Security_BinaryPropertySeq_deinit(&seq);
}

void fuzz_handshake_handle_reply(ddsi_dataholder_t *token) {
    struct ddsi_participant_generic_message msg;
    msg.message_class_id = DDS_SECURITY_AUTH_HANDSHAKE;
    msg.message_data.n = 1;
    msg.message_data.tags = token;
    token->class_id = DDS_SECURITY_AUTH_HANDSHAKE_REPLY_TOKEN_ID;
    const DDS_Security_BinaryProperty_t *c1 = NULL;
    const DDS_Security_BinaryProperty_t *c2 = NULL;
    const DDS_Security_BinaryProperty_t *dh1 = NULL;
    const DDS_Security_BinaryProperty_t *dh2 = NULL;
    const DDS_Security_BinaryProperty_t *hash_c1 = NULL;
    const DDS_Security_BinaryProperty_t *kagree_algo = NULL;

    if (harness.hs->handshake_message_out != NULL) {
        // Using gv.handshake_include_optional = true;
        c1 = DDS_Security_DataHolder_find_binary_property(harness.hs->handshake_message_out, "challenge1");
        dh1 = DDS_Security_DataHolder_find_binary_property(harness.hs->handshake_message_out, "dh1");
        hash_c1 = DDS_Security_DataHolder_find_binary_property(harness.hs->handshake_message_out, "hash_c1");
        kagree_algo = DDS_Security_DataHolder_find_binary_property(harness.hs->handshake_message_out, "c.kagree_algo");
    }

    // First fix up the values necessary for the hash
    for (uint32_t i = 0; i < token->binary_properties.n; i++) {
        dds_binaryproperty_t *binprop = &token->binary_properties.props[i];
        // To avoid fuzzing openssl, always use a valid certificate
        if (strcmp(binprop->name, "c.id") == 0) {
            free(binprop->value.value);
            binprop->value.length = strlen(TEST_IDENTITY3_CERTIFICATE);
            binprop->value.value = (unsigned char *) strdup(TEST_IDENTITY3_CERTIFICATE);
        }
        if ((strcmp(binprop->name, "challenge1") == 0) && c1) {
            free(binprop->value.value);
            binprop->value.length = DDS_SECURITY_AUTHENTICATION_CHALLENGE_SIZE;
            binprop->value.value = malloc(DDS_SECURITY_AUTHENTICATION_CHALLENGE_SIZE);
            memcpy(binprop->value.value, c1->value._buffer, DDS_SECURITY_AUTHENTICATION_CHALLENGE_SIZE);
        }
        // Provide a valid dh public key
        if ((strcmp(binprop->name, "dh2") == 0) && kagree_algo) {
            int algo = 1;
            if (strncmp((const char *)kagree_algo->value._buffer, "ECDH+prime256v1-CEUM", kagree_algo->value._length) == 0) algo = 2;
            unsigned char *data;
            uint32_t len;
            if (create_dh_key(algo, &data, &len) == DDS_SECURITY_VALIDATION_OK) {
                free(binprop->value.value);
                binprop->value.length = len;
                binprop->value.value = data;
            }
        }
    }

    // Generate the hash to be signed
    DDS_Security_DataHolder token_holder;
    ddsi_omg_shallow_copyin_DataHolder(&token_holder, token);
    c2 = DDS_Security_DataHolder_find_binary_property(&token_holder, "challenge2");
    dh2 = DDS_Security_DataHolder_find_binary_property(&token_holder, "dh2");
    DDS_Security_BinaryProperty_t hash_c2 = {0};
    create_hash(&token_holder, &hash_c2, "hash_c2");

    for (uint32_t i = 0; i < token->binary_properties.n; i++) {
        dds_binaryproperty_t *binprop = &token->binary_properties.props[i];
        // Need to provide a valid signature for the handshake to succeed
        if (strcmp(binprop->name, "signature") == 0 && hash_c1 && c1 && c2 && dh1 && c2 && dh2 && hash_c2.value._buffer) {
            unsigned char *signature;
            size_t signatureLen;
            const DDS_Security_BinaryProperty_t *bprops[] = { &hash_c2, c2, dh2, c1, dh1, hash_c1};
            create_signature(bprops, 6, &signature, &signatureLen);
            free(binprop->value.value);
            binprop->value.length = (uint32_t) signatureLen;
            binprop->value.value = signature;
        }
    }
    DDS_Security_BinaryProperty_deinit(&hash_c2);
    ddsi_handshake_handle_message(harness.hs, harness.pp, harness.proxy_pp, &msg);
    ddsi_omg_shallow_free_DataHolder(&token_holder);
}

void fuzz_handshake_handle_final(ddsi_dataholder_t *token) {
    struct ddsi_participant_generic_message msg;
    msg.message_class_id = DDS_SECURITY_AUTH_HANDSHAKE;
    msg.message_data.n = 1;
    msg.message_data.tags = token;
    token->class_id = DDS_SECURITY_AUTH_HANDSHAKE_FINAL_TOKEN_ID;

    const DDS_Security_BinaryProperty_t *c1 = NULL;
    const DDS_Security_BinaryProperty_t *c2 = NULL;
    const DDS_Security_BinaryProperty_t *dh1 = NULL;
    const DDS_Security_BinaryProperty_t *dh2 = NULL;
    const DDS_Security_BinaryProperty_t *hash_c1 = NULL;
    const DDS_Security_BinaryProperty_t *hash_c2 = NULL;

    if (harness.hs->handshake_message_out != NULL) {
        // Using gv.handshake_include_optional = true;
        c1 = DDS_Security_DataHolder_find_binary_property(harness.hs->handshake_message_out, "challenge1");
        c2 = DDS_Security_DataHolder_find_binary_property(harness.hs->handshake_message_out, "challenge2");
        dh1 = DDS_Security_DataHolder_find_binary_property(harness.hs->handshake_message_out, "dh1");
        dh2 = DDS_Security_DataHolder_find_binary_property(harness.hs->handshake_message_out, "dh2");
        hash_c1 = DDS_Security_DataHolder_find_binary_property(harness.hs->handshake_message_out, "hash_c1");
        hash_c2 = DDS_Security_DataHolder_find_binary_property(harness.hs->handshake_message_out, "hash_c2");
    }

    for (uint32_t i = 0; i < token->binary_properties.n; i++) {
        dds_binaryproperty_t *binprop = &token->binary_properties.props[i];
        // Need to copy the correct values for challenge2 from the relation for the handshake to succeed
        if ((strcmp(binprop->name, "challenge1") == 0) && c1) {
            free(binprop->value.value);
            binprop->value.length = DDS_SECURITY_AUTHENTICATION_CHALLENGE_SIZE;
            binprop->value.value = malloc(DDS_SECURITY_AUTHENTICATION_CHALLENGE_SIZE);
            memcpy(binprop->value.value, c1->value._buffer, DDS_SECURITY_AUTHENTICATION_CHALLENGE_SIZE);
        }
        if ((strcmp(binprop->name, "challenge2") == 0) && c2) {
            free(binprop->value.value);
            binprop->value.length = DDS_SECURITY_AUTHENTICATION_CHALLENGE_SIZE;
            binprop->value.value = malloc(DDS_SECURITY_AUTHENTICATION_CHALLENGE_SIZE);
            memcpy(binprop->value.value, c2->value._buffer, DDS_SECURITY_AUTHENTICATION_CHALLENGE_SIZE);
        }
        // Need to provide a valid signature for the handshake to succeed
        if (strcmp(binprop->name, "signature") == 0 && hash_c1 && c1 && c2 && dh1 && c2 && dh2 && hash_c2) {
            unsigned char *signature;
            size_t signatureLen;
            const DDS_Security_BinaryProperty_t *bprops[] = { hash_c1, c1, dh1, c2, dh2, hash_c2 };
            create_signature(bprops, 6, &signature, &signatureLen);
            free(binprop->value.value);
            binprop->value.length = (uint32_t) signatureLen;
            binprop->value.value = signature;
        }
    }

    ddsi_handshake_handle_message(harness.hs, harness.pp, harness.proxy_pp, &msg);
}

void fuzz_handshake_handle_crypto_tokens(void) {
    ddsi_thread_state_awake(ddsi_lookup_thread_state(), &gv);
    ddsi_handshake_crypto_tokens_received(harness.hs);
    ddsi_thread_state_asleep(ddsi_lookup_thread_state());
}

void fuzz_handshake_wait_for_event(uint32_t event) {
    dds_security_fsm_dispatch(harness.hs->fsm, FUZZ_HANDSHAKE_EVENT_HANDLED, false);
    while(ddsrt_atomic_ld32(&g_fuzz_events) != event) {}
}

void fuzz_handshake_wait_for_completion(void) {
    dds_security_fsm_dispatch(harness.hs->fsm, DDS_SECURITY_FSM_EVENT_DELETE, false);
    while(ddsrt_atomic_ld32(&g_fsm_done) == 0) {}
    ddsrt_wctime_t timestamp = { .v = dds_time() };
    ddsi_thread_state_awake(ddsi_lookup_thread_state(), &gv);
    ddsi_delete_proxy_participant_by_guid(&gv, &g_proxy_ppguid, timestamp, 1);
    ddsi_thread_state_asleep(ddsi_lookup_thread_state());
    harness.proxy_pp = NULL;
    harness.hs = NULL;

    // To actually delete all we created we need to step the what is normally
    // done by 4 different threads in the background (multi-stage cleanup via
    // the GC involves bubbles sent through the delivery queues; and then one
    // has to "send" the messages queued by the handshake code).
    bool x;
    do {
      x = false;
      if (ddsi_gcreq_queue_step (gv.gcreq_queue))
        x = true;
      if (ddsi_dqueue_step_deaf (gv.builtins_dqueue))
        x = true;
      if (ddsi_dqueue_step_deaf (gv.user_dqueue))
        x = true;
      ddsi_xeventq_step (gv.xevents);
    } while (x);
}
