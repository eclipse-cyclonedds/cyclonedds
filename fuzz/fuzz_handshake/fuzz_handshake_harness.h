#include <stdbool.h>
bool fuzz_handshake_init(void);
void fuzz_handshake_reset(void);
void fuzz_handshake_handle_timeout(void);
void fuzz_handshake_handle_request(void);
void fuzz_handshake_handle_reply(void);
void fuzz_handshake_handle_final(void);
void fuzz_handshake_handle_crypto_tokens(void);
