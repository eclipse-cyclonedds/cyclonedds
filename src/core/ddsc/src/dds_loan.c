#include "dds/ddsc/dds_loan.h"

#include "dds__entity.h"
#include "dds__reader.h"
#include "dds__types.h"
#include "dds__writer.h"

#include "dds/ddsi/ddsi_sertype.h"

#ifdef DDS_HAS_SHM
#include "dds/ddsi/shm_transport.h"
#endif

bool dds_writer_shared_memory_supported(dds_entity_t writer) {
#ifndef DDS_HAS_SHM
  (void)writer;
  return false;
#else
  dds_entity *e;
  if (DDS_RETCODE_OK != dds_entity_pin(writer, &e)) {
    return false;
  }

  dds_writer *wr = (struct dds_writer *)e;
  bool ret = wr->m_iox_pub != NULL;
  dds_entity_unpin(e);
  return ret;
#endif
}

bool dds_writer_loan_sample_supported(dds_entity_t writer) {
#ifndef DDS_HAS_SHM
  (void)writer;
  return false;
#else
  dds_entity *e;
  if (DDS_RETCODE_OK != dds_entity_pin(writer, &e)) {
    return false;
  }

  dds_writer *wr = (struct dds_writer *)e;
  bool ret = (wr->m_iox_pub != NULL) && (wr->m_topic->m_stype->fixed_size);
  dds_entity_unpin(e);
  return ret;
#endif
}

bool dds_reader_shared_memory_supported(dds_entity_t reader) {
#ifndef DDS_HAS_SHM
  (void)reader;
  return false;
#else
  dds_entity *e;
  if (DDS_RETCODE_OK != dds_entity_pin(reader, &e)) {
    return false;
  }

  dds_reader *rd = (struct dds_reader *)e;
  bool ret = rd->m_iox_sub != NULL;
  dds_entity_unpin(e);
  return ret;
#endif
}

bool dds_reader_loan_supported(dds_entity_t reader) {
#ifndef DDS_HAS_SHM
  (void)reader;
  return false;
#else
  dds_entity *e;
  if (DDS_RETCODE_OK != dds_entity_pin(reader, &e)) {
    return false;
  }

  dds_reader *rd = (struct dds_reader *)e;
  bool ret = (rd->m_iox_sub != NULL) && (rd->m_topic->m_stype->fixed_size);
  dds_entity_unpin(e);
  return ret;
#endif
}

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
    // only if SHM is enabled correctly (i.e. iox subscriber is initialized) and
    // the type is fixed
    ret = (rd->m_iox_sub != NULL);
    break;
  }
  case DDS_KIND_WRITER: {
    struct dds_writer const *const wr = (struct dds_writer *)e;
    // only if SHM is enabled correctly (i.e. iox publisher is initialized) and
    // the type is fixed
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

bool is_loan_available(const dds_entity_t entity) {
  return dds_is_loan_available(entity);
}
