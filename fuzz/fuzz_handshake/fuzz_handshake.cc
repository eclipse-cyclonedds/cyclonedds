#include "fuzz_handshake.pb.h"
#include <src/libfuzzer/libfuzzer_macro.h>
#include "fuzz_handshake_harness.h"

using fuzz_handshake::Event;

void to_property(const fuzz_handshake::Property &in, dds_property_t &out) {
    out.propagate = in.propagate();
    out.name = strdup(in.name().c_str());
    out.value = strdup(in.value().c_str());
}

void to_binaryproperty(const fuzz_handshake::BinaryProperty &in, dds_binaryproperty_t &out) {
    out.propagate = in.propagate();
    out.name = strdup(in.name().c_str());
    out.value.length = (uint32_t) in.value().size();
    out.value.value = (unsigned char *) malloc(out.value.length);
    memcpy(out.value.value, in.value().data(), out.value.length);
}

void to_dataholder(const fuzz_handshake::DataHolder &in, ddsi_dataholder_t &out) {
    out.properties.n = (uint32_t) in.properties().size();
    out.properties.props = (dds_property_t *) calloc(out.properties.n, sizeof(dds_property_t));
    for (int i = 0; i < in.properties().size(); i++) {
        to_property(in.properties().Get(i), out.properties.props[i]);
    }
    out.binary_properties.n = (uint32_t) in.binary_properties().size();
    out.binary_properties.props = (dds_binaryproperty_t *) calloc(out.binary_properties.n, sizeof(dds_binaryproperty_t));
    for (int i = 0; i < in.binary_properties().size(); i++) {
        to_binaryproperty(in.binary_properties().Get(i), out.binary_properties.props[i]);
    }
}

void free_properties(ddsi_dataholder_t &dh) {
    for (uint32_t i = 0; i < dh.properties.n; i++) {
        free(dh.properties.props[i].name);
        free(dh.properties.props[i].value);
    }
    free(dh.properties.props);
    for (uint32_t i = 0; i < dh.binary_properties.n; i++) {
        free(dh.binary_properties.props[i].name);
        free(dh.binary_properties.props[i].value.value);
    }
    free(dh.binary_properties.props);
}

static bool g_init = false;
DEFINE_PROTO_FUZZER(const fuzz_handshake::FuzzMsg& message) {
    if (!g_init) g_init = fuzz_handshake_init();
    fuzz_handshake_reset(message.initiate_remote());
    uint32_t n_event = 1;
    for (auto &event : message.events()) {
        switch (event.event_case()) {
            case Event::EventCase::kTimeout:
                fuzz_handshake_handle_timeout();
                break;
            case Event::EventCase::kRequest:
                {
                    ddsi_dataholder_t token;
                    to_dataholder(event.request().token(), token);
                    fuzz_handshake_handle_request(&token);
                    free_properties(token);
                } break;
            case Event::EventCase::kReply:
                {
                    ddsi_dataholder_t token;
                    to_dataholder(event.reply().token(), token);
                    fuzz_handshake_handle_reply(&token);
                    free_properties(token);
                } break;
            case Event::EventCase::kFinal:
                {
                    ddsi_dataholder_t token;
                    to_dataholder(event.final().token(), token);
                    fuzz_handshake_handle_final(&token);
                    free_properties(token);
                } break;
            case Event::EventCase::kCryptoTokens:
                fuzz_handshake_handle_crypto_tokens();
                break;
            default:
                continue;
        }
        fuzz_handshake_wait_for_event(n_event++);
    }
    fuzz_handshake_wait_for_completion();
}
