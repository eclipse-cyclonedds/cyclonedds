#include <dds/features.h>
#include <dds/ddsi/ddsi_plist.h>

#include <stdbool.h>

#if defined(__cplusplus)
extern "C" {
#endif
bool fuzz_handshake_init(void);
void fuzz_handshake_reset(bool initiate_remote);
void fuzz_handshake_handle_timeout(void);
void fuzz_handshake_handle_force_handshake_request(void);
void fuzz_handshake_handle_request(ddsi_dataholder_t *token);
void fuzz_handshake_handle_reply(ddsi_dataholder_t *token);
void fuzz_handshake_handle_final(ddsi_dataholder_t *token);
void fuzz_handshake_handle_crypto_tokens(void);
void fuzz_handshake_wait_for_event(uint32_t event);
void fuzz_handshake_wait_for_completion(void);
#if defined(__cplusplus)
}
#endif
