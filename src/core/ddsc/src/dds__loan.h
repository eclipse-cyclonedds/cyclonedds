#ifndef _DDS_LOAN_H_
#define _DDS_LOAN_H_

#if defined(__cplusplus)
extern "C" {
#endif

#ifdef DDS_HAS_SHM

// NB: required in dds_write.c and dds_loan.c for now

void register_pub_loan(dds_writer *wr, void *pub_loan);

bool deregister_pub_loan(dds_writer *wr, const void *pub_loan);

#endif

#if defined(__cplusplus)
}

#endif
#endif
