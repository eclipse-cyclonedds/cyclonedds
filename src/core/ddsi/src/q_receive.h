#ifndef Q_DO_PACKET_H
#define Q_DO_PACKET_H

bool do_packet (struct thread_state1 * const ts1, struct ddsi_domaingv *gv, ddsi_tran_conn_t conn, const ddsi_guid_prefix_t *guidprefix, struct nn_rbufpool *rbpool);

#endif
