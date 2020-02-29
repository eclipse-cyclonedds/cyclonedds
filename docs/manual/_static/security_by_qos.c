dds_qos_t * qos = dds_create_qos();

dds_qset_prop(qos, "dds.sec.auth.library.path", "dds_security_auth");
dds_qset_prop(qos, "dds.sec.auth.library.init", "init_authentication");
dds_qset_prop(qos, "dds.sec.auth.library.finalize", "finalize_authentication");
dds_qset_prop(qos, "dds.sec.auth.identity_ca", "file:/path/to/example_id_ca_cert.pem");
dds_qset_prop(qos, "dds.sec.auth.private_key", "file:/path/to/example_alice_priv_key.pem");
dds_qset_prop(qos, "dds.sec.auth.identity_certificate", "file:/path/to/example_alice_cert.pem");

dds_qset_prop(qos, "dds.sec.crypto.library.path", "dds_security_crypto");
dds_qset_prop(qos, "dds.sec.crypto.library.init", "init_crypto");
dds_qset_prop(qos, "dds.sec.crypto.library.finalize", "finalize_crypto");

dds_qset_prop(qos, "dds.sec.access.library.path", "dds_security_ac");
dds_qset_prop(qos, "dds.sec.access.library.init", "init_access_control");
dds_qset_prop(qos, "dds.sec.access.library.finalize", "finalize_access_control");
dds_qset_prop(qos, "dds.sec.access.permissions_ca", "file:/path/to/example_perm_ca_cert.pem");
dds_qset_prop(qos, "dds.sec.access.governance", "file:/path/to/example_governance.p7s");
dds_qset_prop(qos, "dds.sec.access.permissions", "file:/path/to/example_permissions.p7s");

dds_entity_t participant = dds_create_participant(0, qos, NULL);
