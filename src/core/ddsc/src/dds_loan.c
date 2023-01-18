#include "dds/ddsc/dds_loan_api.h"

#include "dds__entity.h"
#include "dds__loan.h"
#include "dds__reader.h"
#include "dds__types.h"
#include "dds__writer.h"

#include "dds/ddsi/ddsi_sertype.h"

#ifdef DDS_HAS_SHM
#include "dds/ddsi/ddsi_shm_transport.h"
#include "iceoryx_binding_c/chunk.h"
#endif

bool dds_is_shared_memory_available(const dds_entity_t entity) {
  bool ret = false;
#ifdef DDS_HAS_SHM
  dds_entity *e;

  if (DDS_RETCODE_OK != dds_entity_pin(entity, &e)) {
    return ret;
  }

  switch (dds_entity_kind(e)) {
  case DDS_KIND_READER: {
    struct dds_reader const *const rd = (struct dds_reader *)e;
    // only if SHM is enabled correctly (i.e. iox subscriber is initialized)
    ret = (rd->m_iox_sub != NULL);
    break;
  }
  case DDS_KIND_WRITER: {
    struct dds_writer const *const wr = (struct dds_writer *)e;
    // only if SHM is enabled correctly (i.e. iox publisher is initialized)
    ret = (wr->m_iox_pub != NULL);
    break;
  }
  default:
    break;
  }

  dds_entity_unpin(e);
#endif
  (void)entity;
  return ret;
}

bool dds_is_loan_available(const dds_entity_t entity) {
  bool ret = false;
#ifdef DDS_HAS_SHM
  dds_entity *e;

  if (DDS_RETCODE_OK != dds_entity_pin(entity, &e)) {
    return ret;
  }

  switch (dds_entity_kind(e)) {
  case DDS_KIND_READER: {
    struct dds_reader const *const rd = (struct dds_reader *)e;
    // only if SHM is enabled correctly (i.e. iox subscriber is initialized) and
    // the type is fixed
    ret = (rd->m_iox_sub != NULL) && (rd->m_topic->m_stype->fixed_size);
    break;
  }
  case DDS_KIND_WRITER: {
    struct dds_writer const *const wr = (struct dds_writer *)e;
    // only if SHM is enabled correctly (i.e. iox publisher is initialized) and
    // the type is fixed
    ret = (wr->m_iox_pub != NULL) && (wr->m_topic->m_stype->fixed_size);
    break;
  }
  default:
    break;
  }

  dds_entity_unpin(e);
#endif
  (void)entity;
  return ret;
}

#ifdef DDS_HAS_SHM

static void release_iox_chunk(dds_writer *wr, void *sample) {
  iox_pub_release_chunk(wr->m_iox_pub, sample);
}

void dds_register_pub_loan(dds_writer *wr, void *pub_loan) {
  for (uint32_t i = 0; i < MAX_PUB_LOANS; ++i) {
    if (!wr->m_iox_pub_loans[i]) {
      wr->m_iox_pub_loans[i] = pub_loan;
      return;
    }
  }
  /* The loan pool should be big enough to store the maximum number of open
   * IceOryx loans. So if IceOryx grants the loan, we should be able to store
   * it.
   */
  assert(false);
}

bool dds_deregister_pub_loan(dds_writer *wr, const void *pub_loan) {
  for (uint32_t i = 0; i < MAX_PUB_LOANS; ++i) {
    if (wr->m_iox_pub_loans[i] == pub_loan) {
      wr->m_iox_pub_loans[i] = NULL;
      return true;
    }
  }
  return false;
}

static void *dds_writer_loan_chunk(dds_writer *wr, size_t size) {
  void *chunk = shm_create_chunk(wr->m_iox_pub, size);
  if (chunk) {
    dds_register_pub_loan(wr, chunk);
    // NB: we set this since the user can use this chunk not only with write
    // where we check whether it was loaned before.
    // It is only possible to loan for fixed size types as of now.

    // Unfortunate, since the chunk is not actually filled at this point.
    // We should ensure that we cannot circumvent the write API
    // (API redesign).
    shm_set_data_state(chunk, IOX_CHUNK_CONTAINS_RAW_DATA);
    return chunk;
  }
  return NULL;
}

#endif
// we do not register this loan (we do not need to for the use with
// dds_writecdr)
dds_return_t dds_loan_shared_memory_buffer(dds_entity_t writer, size_t size,
                                           void **buffer) {
#ifndef DDS_HAS_SHM
  (void)writer;
  (void)size;
  (void)buffer;
  return DDS_RETCODE_UNSUPPORTED;
#else
  dds_return_t ret;
  dds_writer *wr;

  if (!buffer)
    return DDS_RETCODE_BAD_PARAMETER;

  if ((ret = dds_writer_lock(writer, &wr)) != DDS_RETCODE_OK)
    return ret;

  if (wr->m_iox_pub) {
    *buffer = shm_create_chunk(wr->m_iox_pub, size);
    if (*buffer == NULL) {
      ret = DDS_RETCODE_ERROR; // could not obtain buffer memory
    }
    shm_set_data_state(*buffer, IOX_CHUNK_UNINITIALIZED);
  } else {
    ret = DDS_RETCODE_UNSUPPORTED;
  }

  dds_writer_unlock(wr);
  return ret;
#endif
}

dds_return_t dds_loan_sample(dds_entity_t writer, void **sample) {
#ifndef DDS_HAS_SHM
  (void)writer;
  (void)sample;
  return DDS_RETCODE_UNSUPPORTED;
#else
  dds_return_t ret;
  dds_writer *wr;

  if (!sample)
    return DDS_RETCODE_BAD_PARAMETER;

  if ((ret = dds_writer_lock(writer, &wr)) != DDS_RETCODE_OK)
    return ret;

  // the loaning is only allowed if SHM is enabled correctly and if the type is
  // fixed
  if (wr->m_iox_pub && wr->m_topic->m_stype->fixed_size) {
    *sample = dds_writer_loan_chunk(wr, wr->m_topic->m_stype->iox_size);
    if (*sample == NULL) {
      ret = DDS_RETCODE_ERROR; // could not obtain a sample
    }
  } else {
    ret = DDS_RETCODE_UNSUPPORTED;
  }

  dds_writer_unlock(wr);
  return ret;
#endif
}

dds_return_t dds_return_writer_loan(dds_writer *writer, void **buf,
                                    int32_t bufsz) {
#ifndef DDS_HAS_SHM
  (void)writer;
  (void)buf;
  (void)bufsz;
  return DDS_RETCODE_UNSUPPORTED;
#else
  // Iceoryx publisher pointer is a constant so we can check outside the locks
  // returning loan is only valid if SHM is enabled correctly (i.e. iox
  // publisher is initialized) and the type is fixed
  if (writer->m_iox_pub == NULL || !writer->m_topic->m_stype->fixed_size)
    return DDS_RETCODE_UNSUPPORTED;
  if (bufsz <= 0) {
    // analogous to long-standing behaviour for the reader case, where it makes
    // (some) sense as it allows passing in the result of a read/take operation
    // regardless of whether that operation was successful
    return DDS_RETCODE_OK;
  }

  ddsrt_mutex_lock(&writer->m_entity.m_mutex);
  dds_return_t ret = DDS_RETCODE_OK;
  for (int32_t i = 0; i < bufsz; i++) {
    if (buf[i] == NULL) {
      ret = DDS_RETCODE_BAD_PARAMETER;
      break;
    } else if (!dds_deregister_pub_loan(writer, buf[i])) {
      ret = DDS_RETCODE_PRECONDITION_NOT_MET;
      break;
    } else {
      release_iox_chunk(writer, buf[i]);
      // return loan on the reader nulls buf[0], but here it makes more sense to
      // clear all successfully returned ones: then, on failure, the application
      // can figure out which ones weren't returned by looking for the first
      // non-null pointer
      buf[i] = NULL;
    }
  }
  ddsrt_mutex_unlock(&writer->m_entity.m_mutex);
  return ret;
#endif
}
