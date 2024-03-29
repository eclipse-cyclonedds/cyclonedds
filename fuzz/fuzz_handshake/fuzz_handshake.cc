#include "fuzz_handshake.pb.h"
#include <src/libfuzzer/libfuzzer_macro.h>
extern "C" {
#include "fuzz_handshake_harness.h"
}
using fuzz_handshake::Event;

static bool g_init = false;
DEFINE_PROTO_FUZZER(const fuzz_handshake::FuzzMsg& message) {
    if (!g_init) g_init = fuzz_handshake_init();
    fuzz_handshake_reset();
    for (auto &event : message.events()) {
        switch (event.event_case()) {
            case Event::EventCase::kTimeout:
                fuzz_handshake_handle_timeout();
                break;
            case Event::EventCase::kRequest:
                fuzz_handshake_handle_request();
                break;
            case Event::EventCase::kReply:
                fuzz_handshake_handle_reply();
                break;
            case Event::EventCase::kFinal:
                fuzz_handshake_handle_final();
                break;
            case Event::EventCase::kCryptoTokens:
                fuzz_handshake_handle_crypto_tokens();
                break;
            default:
                break;
        }
    }
}
