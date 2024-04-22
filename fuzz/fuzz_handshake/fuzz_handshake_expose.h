/* The following struct definitions are opaque in ddsi, however the fuzzer needs
 * access to some of the fields and hence they are reproduced here in order to
 * expose them. This is not ideal, because any changes in those structs would 
 * need to be reflected here.
 */
#include <stdint.h>
#include <ddsi__security_omg.h>

struct handshake_entities {
  ddsi_guid_t lguid;
  ddsi_guid_t rguid;
};

struct ddsi_handshake
{
  ddsrt_avl_node_t avlnode;
  enum ddsi_handshake_state state;
  struct handshake_entities participants;
  DDS_Security_HandshakeHandle handshake_handle;
  ddsrt_atomic_uint32_t refc;
  ddsrt_atomic_uint32_t deleting;
  ddsi_handshake_end_cb_t end_cb;
  ddsrt_mutex_t lock;
  struct dds_security_fsm *fsm;
  const struct ddsi_domaingv *gv;
  dds_security_authentication *auth;

  DDS_Security_HandshakeMessageToken handshake_message_in_token;
  ddsi_message_identity_t handshake_message_in_id;
  DDS_Security_HandshakeMessageToken *handshake_message_out;
  DDS_Security_AuthRequestMessageToken local_auth_request_token;
  DDS_Security_AuthRequestMessageToken *remote_auth_request_token;
  DDS_Security_OctetSeq pdata;
  int64_t shared_secret;
};
