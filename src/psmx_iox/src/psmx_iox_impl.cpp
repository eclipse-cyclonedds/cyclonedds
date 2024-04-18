// Copyright(c) 2023 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>
#include <inttypes.h>
#include <string>
#include <memory>
#include <optional>

#include "dds/ddsrt/align.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/mh3.h"
#include "dds/ddsrt/random.h"
#include "dds/ddsrt/strtol.h"
#include "dds/ddsc/dds_loaned_sample.h"
#include "dds/ddsc/dds_psmx.h"

#include "iceoryx_hoofs/posix_wrapper/signal_watcher.hpp"
#include "iceoryx_posh/popo/untyped_publisher.hpp"
#include "iceoryx_posh/popo/untyped_subscriber.hpp"
#include "iceoryx_posh/popo/listener.hpp"
#include "iceoryx_posh/runtime/posh_runtime.hpp"

#include "psmx_iox_impl.hpp"
#include "machineid.hpp"

#define ERROR_PREFIX "=== [ICEORYX] "

static std::optional<dds_psmx_node_identifier_t> to_node_identifier(const std::string& str);

/*forward declarations of functions*/
namespace iox_psmx {

static bool iox_type_qos_supported(struct dds_psmx * psmx, dds_psmx_endpoint_type_t forwhat, dds_data_type_properties_t data_type, const struct dds_qos * qos);
static struct dds_psmx_topic* iox_create_topic(struct dds_psmx * psmx, const char * topic_name, const char * type_name, dds_data_type_properties_t data_type_props);
static dds_return_t iox_delete_topic(struct dds_psmx_topic * psmx_topic);
static dds_return_t iox_psmx_deinit(struct dds_psmx * self);
static dds_psmx_node_identifier_t iox_psmx_get_node_id(const struct dds_psmx * psmx);
static dds_psmx_features_t iox_supported_features(const struct dds_psmx *psmx);

static const dds_psmx_ops_t psmx_ops = {
  .type_qos_supported = iox_type_qos_supported,
  .create_topic = iox_create_topic,
  .delete_topic = iox_delete_topic,
  .deinit = iox_psmx_deinit,
  .get_node_id = iox_psmx_get_node_id,
  .supported_features = iox_supported_features
};


static struct dds_psmx_endpoint * iox_create_endpoint(struct dds_psmx_topic * psmx_topic, const struct dds_qos *qos, dds_psmx_endpoint_type_t endpoint_type);
static dds_return_t iox_delete_endpoint(struct dds_psmx_endpoint * psmx_endpoint);

static const dds_psmx_topic_ops_t psmx_topic_ops = {
  .create_endpoint = iox_create_endpoint,
  .delete_endpoint = iox_delete_endpoint
};


static dds_loaned_sample_t * iox_req_loan(struct dds_psmx_endpoint *psmx_endpoint, uint32_t size_requested);
static dds_return_t iox_write(struct dds_psmx_endpoint * psmx_endpoint, dds_loaned_sample_t * data);
static dds_loaned_sample_t * iox_take(struct dds_psmx_endpoint * psmx_endpoint);
static dds_return_t iox_on_data_available(struct dds_psmx_endpoint * psmx_endpoint, dds_entity_t reader);

static const dds_psmx_endpoint_ops_t psmx_ep_ops = {
  .request_loan = iox_req_loan,
  .write = iox_write,
  .take = iox_take,
  .on_data_available = iox_on_data_available
};


static void iox_loaned_sample_free(dds_loaned_sample_t *to_fini);

static const dds_loaned_sample_ops_t ls_ops = {
  .free = iox_loaned_sample_free
};


static bool is_wildcard_partition(const char *str)
{
  return strchr(str, '*') || strchr(str, '?');
}

struct iox_psmx : public dds_psmx_t
{
  iox_psmx(dds_psmx_instance_id_t identifier, const std::string& service_name, const dds_psmx_node_identifier_t& node_id, bool support_keyed_topics);
  ~iox_psmx();
  bool _support_keyed_topics;
  iox::capro::IdString_t _service_name;
  std::unique_ptr<iox::popo::Listener> _listener;  //the listener needs to be created after iox runtime has been initialized
  dds_psmx_node_identifier_t _node_id = { 0 };
  std::shared_ptr<iox::popo::UntypedPublisher> _node_id_publisher;
};

iox_psmx::iox_psmx(dds_psmx_instance_id_t identifier, const std::string& service_name, const dds_psmx_node_identifier_t& node_id, bool support_keyed_topics) :
  dds_psmx_t {
    .ops = psmx_ops,
    .instance_name = dds_string_dup ("CycloneDDS-IOX-PSMX"),
    .priority = 0,
    .locator = nullptr,
    .instance_id = identifier,
    .psmx_topics = nullptr
  },
  _support_keyed_topics{support_keyed_topics},
  _service_name{iox::capro::IdString_t(iox::cxx::TruncateToCapacity, service_name)},
  _listener{}
{
  uint64_t instance_hash = (uint64_t) ddsrt_random() << 32 | ddsrt_random();
  char iox_runtime_name[64];
  snprintf(iox_runtime_name, sizeof(iox_runtime_name), "CycloneDDS-iox_psmx-%016" PRIx64, instance_hash);
  iox::runtime::PoshRuntime::initRuntime(iox_runtime_name);
  _listener = std::unique_ptr<iox::popo::Listener>(new iox::popo::Listener());

  _node_id = node_id;
  dds_psmx_init_generic(this);
}

iox_psmx::~iox_psmx()
{
  if (dds_psmx_cleanup_generic(this) != DDS_RETCODE_OK)
  {
    std::cerr << ERROR_PREFIX "error during dds_psmx_cleanup_generic" << std::endl;
    assert(false);
  }
}

struct iox_psmx_topic : public dds_psmx_topic_t
{
  iox_psmx_topic(iox_psmx& psmx, const char * topic_name, const char * type_name, dds_data_type_properties_t data_type_props);
  ~iox_psmx_topic();
  iox_psmx &_parent;
  char _data_type_str[64];
};

iox_psmx_topic::iox_psmx_topic(iox_psmx& psmx, const char * topic_name, const char * type_name, dds_data_type_properties_t data_type_props) :
  _parent(psmx) // FIXME: superfluous
{
  dds_psmx_topic_init_generic(this, &psmx_topic_ops, &psmx, topic_name, type_name, data_type_props);
  if (ddsrt_strlcpy (_data_type_str, type_name, sizeof (_data_type_str)) >= sizeof (_data_type_str))
    snprintf(_data_type_str, sizeof (_data_type_str), "CycloneDDS iox_datatype %08X", data_type);
  if (dds_add_psmx_topic_to_list(this, &psmx.psmx_topics) != DDS_RETCODE_OK)
  {
    std::cerr << ERROR_PREFIX "could not add PSMX topic to list" << std::endl;
    assert(false);
  }
}

iox_psmx_topic::~iox_psmx_topic()
{
  if (dds_psmx_topic_cleanup_generic(static_cast<struct dds_psmx_topic *>(this)) != DDS_RETCODE_OK)
  {
    std::cerr << ERROR_PREFIX "could not remove PSMX from list" << std::endl;
    assert(false);
  }
}

struct iox_psmx_endpoint : public dds_psmx_endpoint_t
{
  iox_psmx_endpoint(iox_psmx_topic& topic, const struct dds_qos * qos, dds_psmx_endpoint_type_t endpoint_type);
  ~iox_psmx_endpoint();
  iox_psmx_topic &_parent;
  void *_iox_endpoint = nullptr;
  dds_entity_t cdds_endpoint;
  std::mutex lock; // Iceoryx isn't thread safe ...
private:
  char *get_partition_topic(const char * partition, const char * topic_name);
};

char *iox_psmx_endpoint::get_partition_topic(const char * partition, const char * topic_name)
{
  assert (partition);
  assert (!is_wildcard_partition(partition));

  // compute combined string length, allowing for escaping dots
  size_t size = 1 + strlen(topic_name) + 1; // dot & terminating 0
  for (char const *src = partition; *src; src++)
  {
    if (*src == '\\' || *src == '.')
      size++;
    size++;
  }
  char *combined = static_cast<char *>(dds_alloc(size));
  if (combined == NULL)
    return NULL;
  char *dst = combined;
  for (char const *src = partition; *src; src++)
  {
    if (*src == '\\' || *src == '.')
      *dst++ = '\\';
    *dst++ = *src;
  }
  *dst++ = '.';
  strcpy(dst, topic_name);
  return combined;
}

iox_psmx_endpoint::iox_psmx_endpoint(iox_psmx_topic &psmx_topic, const struct dds_qos * qos, dds_psmx_endpoint_type_t endpoint_type):
  dds_psmx_endpoint_t
  {
    .ops = psmx_ep_ops,
    .psmx_topic = &psmx_topic,
    .endpoint_type = endpoint_type
  }, _parent(psmx_topic)
{
  char *partition_topic;
  uint32_t n_partitions = 0;
  char **partitions = NULL;
  (void) dds_qget_partition(qos, &n_partitions, &partitions);
  assert (n_partitions == 0 || n_partitions == 1);
  partition_topic = get_partition_topic((n_partitions == 0) ? "" : partitions[0], psmx_topic.topic_name);
  if (n_partitions > 0)
  {
    dds_free(partitions[0]);
    dds_free(partitions);
  }

  char iox_event_name[64];
  if (strlen(partition_topic) < 63)
  {
    strcpy(iox_event_name, partition_topic);
  }
  else
  {
    strncpy(iox_event_name, partition_topic, sizeof(iox_event_name) - 9);
    uint32_t partition_topic_hash = ddsrt_mh3(partition_topic, strlen(partition_topic), 0);
    snprintf(iox_event_name + sizeof(iox_event_name) - 9, 9, "%08X", partition_topic_hash);
  }
  dds_free(partition_topic);

  switch (endpoint_type)
  {
    case DDS_PSMX_ENDPOINT_TYPE_READER: {
      iox::popo::SubscriberOptions suboptions;
      if (psmx_topic.data_type_props & DDS_DATA_TYPE_CONTAINS_KEY)
      {
        suboptions.queueCapacity = iox::popo::SubscriberChunkQueueData_t::MAX_CAPACITY;
        suboptions.queueFullPolicy = iox::popo::QueueFullPolicy::BLOCK_PRODUCER;
      }
      else
      {
        dds_history_kind h_kind;
        dds_durability_kind_t d_kind;
        int32_t h_depth;
        dds_qget_durability(qos, &d_kind);
        dds_qget_history(qos, &h_kind, &h_depth);
        if (h_kind != DDS_HISTORY_KEEP_LAST || (uint64_t) h_depth > iox::popo::SubscriberChunkQueueData_t::MAX_CAPACITY) {
            suboptions.queueCapacity = iox::popo::SubscriberChunkQueueData_t::MAX_CAPACITY;
            suboptions.queueFullPolicy = iox::popo::QueueFullPolicy::BLOCK_PRODUCER;
          } else {
            suboptions.queueCapacity = (uint64_t) h_depth;
            suboptions.queueFullPolicy = iox::popo::QueueFullPolicy::DISCARD_OLDEST_DATA;
          }
        if (d_kind != DDS_DURABILITY_VOLATILE)
          suboptions.requiresPublisherHistorySupport = true;
      }
      _iox_endpoint = new iox::popo::UntypedSubscriber({_parent._parent._service_name, _parent._data_type_str, iox_event_name }, suboptions);
      break;
    }
    case DDS_PSMX_ENDPOINT_TYPE_WRITER: {
      iox::popo::PublisherOptions puboptions;
      dds_durability_kind_t d_kind;
      dds_qget_durability(qos, &d_kind);
      if (d_kind == DDS_DURABILITY_TRANSIENT_LOCAL)
      {
        int32_t ds_history_depth;
        dds_qget_durability_service(qos, nullptr, nullptr, &ds_history_depth, nullptr, nullptr, nullptr);
        puboptions.historyCapacity = (uint64_t) ds_history_depth; // iox_qos_supported() checked the range
      }
      // Does this indeed only block if the subscriber says BLOCK_PRODUCER?
      puboptions.subscriberTooSlowPolicy = iox::popo::ConsumerTooSlowPolicy::WAIT_FOR_CONSUMER;
      _iox_endpoint = new iox::popo::UntypedPublisher({_parent._parent._service_name, _parent._data_type_str, iox_event_name }, puboptions);
      break;
    }
    default:
      std::cerr << ERROR_PREFIX "PSMX endpoint type not accepted" << std::endl;
      assert(false);
  }

  if (dds_add_psmx_endpoint_to_list(this, &psmx_topic.psmx_endpoints) != DDS_RETCODE_OK)
  {
    std::cerr << ERROR_PREFIX "could not add PSMX endpoint to list" << std::endl;
    assert(false);
  }
}

iox_psmx_endpoint::~iox_psmx_endpoint()
{
  switch (endpoint_type)
  {
    case DDS_PSMX_ENDPOINT_TYPE_READER:
      {
        auto sub = static_cast<iox::popo::UntypedSubscriber *>(_iox_endpoint);
        this->_parent._parent._listener->detachEvent(*sub, iox::popo::SubscriberEvent::DATA_RECEIVED);
        delete sub;
      }
      break;
    case DDS_PSMX_ENDPOINT_TYPE_WRITER:
      delete static_cast<iox::popo::UntypedPublisher *>(_iox_endpoint);
      break;
    default:
      std::cerr << ERROR_PREFIX "PSMX endpoint type not accepted" << std::endl;
      assert(false);
  }
}

struct iox_loaned_sample : public dds_loaned_sample_t
{
  iox_loaned_sample(struct dds_psmx_endpoint * origin, const void * iox_payload);
  ~iox_loaned_sample();
};

iox_loaned_sample::iox_loaned_sample(struct dds_psmx_endpoint * origin, const void * iox_payload):
  dds_loaned_sample_t {
    .ops = ls_ops,
    .loan_origin = { .origin_kind = DDS_LOAN_ORIGIN_KIND_PSMX, .psmx_endpoint = origin },
    .metadata = const_cast<dds_psmx_metadata_t *>(static_cast<const dds_psmx_metadata_t *>(iox::mepoo::ChunkHeader::fromUserPayload(iox_payload)->userHeader())),
    .sample_ptr = const_cast<void *>(iox_payload),
    .refc = { .v = 1 }
  }
{
}

iox_loaned_sample::~iox_loaned_sample()
{
  assert(loan_origin.origin_kind == DDS_LOAN_ORIGIN_KIND_PSMX);
  auto cpp_ep_ptr = static_cast<iox_psmx_endpoint*>(loan_origin.psmx_endpoint);
  if (sample_ptr)
  {
    switch (cpp_ep_ptr->endpoint_type)
    {
      case DDS_PSMX_ENDPOINT_TYPE_READER: {
        const std::lock_guard<std::mutex> lock(cpp_ep_ptr->lock);
        static_cast<iox::popo::UntypedSubscriber *>(cpp_ep_ptr->_iox_endpoint)->release(sample_ptr);
        break;
      }
      case DDS_PSMX_ENDPOINT_TYPE_WRITER: {
        const std::lock_guard<std::mutex> lock(cpp_ep_ptr->lock);
        static_cast<iox::popo::UntypedPublisher *>(cpp_ep_ptr->_iox_endpoint)->release(sample_ptr);
        break;
      }
      default: {
        std::cerr << ERROR_PREFIX "PSMX endpoint type not accepted" << std::endl;
        assert(false);
      }
    }
  }
}

// dds_psmx_ops_t implementation

static bool iox_type_qos_supported(struct dds_psmx * psmx, dds_psmx_endpoint_type_t forwhat, dds_data_type_properties_t data_type_props, const struct dds_qos * qos)
{
  if (data_type_props & DDS_DATA_TYPE_CONTAINS_KEY)
  {
    auto iox_psmx = static_cast<struct iox_psmx *>(psmx);
    if (!iox_psmx->_support_keyed_topics)
      return false;
  }
  // Everything else is really dependent on the endpoint QoS, not the topic QoS, given
  // that we don't support transient/persistent
  if (forwhat == DDS_PSMX_ENDPOINT_TYPE_UNSET)
  {
    return true;
  }

  dds_durability_kind_t d_kind = DDS_DURABILITY_VOLATILE;
  dds_qget_durability (qos, &d_kind);
  if (d_kind != DDS_DURABILITY_VOLATILE && d_kind != DDS_DURABILITY_TRANSIENT_LOCAL)
    return false;
  if (d_kind != DDS_DURABILITY_VOLATILE)
  {
    if (data_type_props & DDS_DATA_TYPE_CONTAINS_KEY)
      return false;
    dds_history_kind_t ds_history_kind = DDS_HISTORY_KEEP_LAST;
    int32_t ds_history_depth = 1;
    dds_qget_durability_service(qos, NULL, &ds_history_kind, &ds_history_depth, NULL, NULL, NULL);
    if (ds_history_kind == DDS_HISTORY_KEEP_LAST && ds_history_depth > static_cast<int32_t>(iox::MAX_PUBLISHER_HISTORY))
      return false;
  }

  uint32_t n_partitions;
  char **partitions;
  if (dds_qget_partition(qos, &n_partitions, &partitions))
  {
    bool supported = n_partitions == 0 || (n_partitions == 1 && !is_wildcard_partition(partitions[0]));
    for (uint32_t n = 0; n < n_partitions; n++)
      dds_free(partitions[n]);
    if (n_partitions > 0)
      dds_free(partitions);
    if (!supported)
      return false;
  }

  dds_ignorelocal_kind_t ignore_local;
  if (dds_qget_ignorelocal(qos, &ignore_local) && ignore_local != DDS_IGNORELOCAL_NONE)
    return false;
  dds_liveliness_kind_t liveliness_kind;
  if (dds_qget_liveliness(qos, &liveliness_kind, NULL) && liveliness_kind != DDS_LIVELINESS_AUTOMATIC)
    return false;
  dds_duration_t deadline_duration;
  if (dds_qget_deadline(qos, &deadline_duration) && deadline_duration != DDS_INFINITY)
    return false;
  return true;
}

static struct dds_psmx_topic* iox_create_topic(struct dds_psmx * psmx, const char * topic_name, const char * type_name, dds_data_type_properties_t data_type_props)
{
  assert(psmx);
  auto cpp_psmx_ptr = static_cast<iox_psmx *>(psmx);
  return new iox_psmx_topic(*cpp_psmx_ptr, topic_name, type_name, data_type_props);
}

static dds_return_t iox_delete_topic(struct dds_psmx_topic * psmx_topic)
{
  assert(psmx_topic);
  delete static_cast<iox_psmx_topic *>(psmx_topic);
  return DDS_RETCODE_OK;
}

static dds_return_t iox_psmx_deinit(struct dds_psmx * psmx)
{
  assert(psmx);
  delete static_cast<iox_psmx *>(psmx);
  return DDS_RETCODE_OK;
}

static dds_psmx_node_identifier_t iox_psmx_get_node_id(const struct dds_psmx * psmx)
{
  return static_cast<const iox_psmx *>(psmx)->_node_id;
}

static dds_psmx_features_t iox_supported_features(const struct dds_psmx * psmx)
{
  (void) psmx;
  return DDS_PSMX_FEATURE_SHARED_MEMORY | DDS_PSMX_FEATURE_ZERO_COPY;
}

// dds_psmx_topic_ops_t implementation

static struct dds_psmx_endpoint * iox_create_endpoint(struct dds_psmx_topic * psmx_topic, const struct dds_qos * qos, dds_psmx_endpoint_type_t endpoint_type)
{
  assert(psmx_topic);
  auto cpp_topic_ptr = static_cast<iox_psmx_topic *>(psmx_topic);
  return new iox_psmx_endpoint(*cpp_topic_ptr, qos, endpoint_type);
}

static dds_return_t iox_delete_endpoint(struct dds_psmx_endpoint * psmx_endpoint)
{
  assert(psmx_endpoint);
  delete static_cast<iox_psmx_endpoint *>(psmx_endpoint);
  return DDS_RETCODE_OK;
}

// dds_psmx_endpoint_ops_t implementation

static dds_loaned_sample_t * iox_req_loan(struct dds_psmx_endpoint *psmx_endpoint, uint32_t size_requested)
{
  auto cpp_ep_ptr = static_cast<iox_psmx_endpoint *>(psmx_endpoint);
  dds_loaned_sample_t *loaned_sample = nullptr;
  if (psmx_endpoint->endpoint_type == DDS_PSMX_ENDPOINT_TYPE_WRITER)
  {
    auto publisher = static_cast<iox::popo::UntypedPublisher *>(cpp_ep_ptr->_iox_endpoint);
    const std::lock_guard<std::mutex> lock(cpp_ep_ptr->lock);
    publisher->loan(size_requested, iox::CHUNK_DEFAULT_USER_PAYLOAD_ALIGNMENT, sizeof(dds_psmx_metadata_t), alignof(dds_psmx_metadata_t))
      .and_then([&](const void* iox_payload) {
        loaned_sample = new iox_loaned_sample(psmx_endpoint, iox_payload);
      })
      .or_else([&](auto& error) {
        std::cerr << ERROR_PREFIX "failure getting loan" << iox::popo::asStringLiteral(error) << std::endl;
      });
  }

  return loaned_sample;
}

static dds_return_t iox_write(struct dds_psmx_endpoint * psmx_endpoint, dds_loaned_sample_t * data)
{
  assert(psmx_endpoint->endpoint_type == DDS_PSMX_ENDPOINT_TYPE_WRITER);
  auto cpp_ep_ptr = static_cast<iox_psmx_endpoint *>(psmx_endpoint);
  auto publisher = static_cast<iox::popo::UntypedPublisher*>(cpp_ep_ptr->_iox_endpoint);

  const std::lock_guard<std::mutex> lock(cpp_ep_ptr->lock);
  publisher->publish(data->sample_ptr);

  // Clear metadata/sample_ptr so that any attempt to use it will cause a crash.  This gives no
  // guarantee whatsoever, but in practice it does help in discovering use of a iox writer loan
  // after publishing it.
  //
  // It also prevents the destructor from freeing it
  data->metadata = nullptr;
  data->sample_ptr = nullptr;
  return DDS_RETCODE_OK;
}

static dds_loaned_sample_t * incoming_sample_to_loan(iox_psmx_endpoint *psmx_endpoint, const void *iox_payload)
{
  return new iox_loaned_sample(psmx_endpoint, iox_payload);
}

static dds_loaned_sample_t * iox_take(struct dds_psmx_endpoint * psmx_endpoint)
{
  assert(psmx_endpoint->endpoint_type == DDS_PSMX_ENDPOINT_TYPE_READER);
  auto cpp_ep_ptr = static_cast<iox_psmx_endpoint *>(psmx_endpoint);
  auto subscriber = static_cast<iox::popo::UntypedSubscriber*>(cpp_ep_ptr->_iox_endpoint);
  assert(subscriber);
  dds_loaned_sample_t *loaned_sample = nullptr;
  const std::lock_guard<std::mutex> lock(cpp_ep_ptr->lock);
  subscriber->take()
    .and_then([cpp_ep_ptr, &loaned_sample](const void * iox_payload) {
      loaned_sample = incoming_sample_to_loan(cpp_ep_ptr, iox_payload);
    });
  return loaned_sample;
}

static void on_incoming_data_callback(iox::popo::UntypedSubscriber * subscriber, iox_psmx_endpoint * psmx_endpoint)
{
  psmx_endpoint->lock.lock();
  while (subscriber->hasData())
  {
    subscriber->take().and_then([psmx_endpoint](auto& sample) {
      psmx_endpoint->lock.unlock();
      auto data = incoming_sample_to_loan(psmx_endpoint, sample);
      (void) dds_reader_store_loaned_sample(psmx_endpoint->cdds_endpoint, data);
      dds_loaned_sample_unref(data);
      psmx_endpoint->lock.lock();
    });
  }
  psmx_endpoint->lock.unlock();
}

static dds_return_t iox_on_data_available(struct dds_psmx_endpoint * psmx_endpoint, dds_entity_t reader)
{
  auto cpp_ep_ptr = static_cast<iox_psmx_endpoint *>(psmx_endpoint);
  assert(cpp_ep_ptr && cpp_ep_ptr->endpoint_type == DDS_PSMX_ENDPOINT_TYPE_READER);

  cpp_ep_ptr->cdds_endpoint = reader;
  auto iox_subscriber = static_cast<iox::popo::UntypedSubscriber *>(cpp_ep_ptr->_iox_endpoint);

  dds_return_t returnval = DDS_RETCODE_ERROR;
  const std::lock_guard<std::mutex> lock(cpp_ep_ptr->lock);
  cpp_ep_ptr->_parent._parent._listener->attachEvent(
    *iox_subscriber,
    iox::popo::SubscriberEvent::DATA_RECEIVED,
    iox::popo::createNotificationCallback(on_incoming_data_callback, *cpp_ep_ptr))
      .and_then([&returnval]() { returnval = DDS_RETCODE_OK; })
      .or_else([](auto) { std::cerr << ERROR_PREFIX "failed to attach subscriber" << std::endl; });
  return returnval;
}


// dds_loaned_sample_ops_t implementation

static void iox_loaned_sample_free(dds_loaned_sample_t *loan)
{
  assert(loan);
  delete static_cast<iox_loaned_sample *>(loan);
}


};  //namespace iox_psmx


static std::optional<std::string> get_config_option_value(const char *conf, const char *option_name, bool tolower = false)
{
  char *copy = dds_string_dup (conf), *cursor = copy, *tok;
  while ((tok = ddsrt_strsep (&cursor, ",/|;")) != nullptr)
  {
    if (strlen(tok) == 0)
      continue;
    char *name = ddsrt_strsep (&tok, "=");
    if (name == nullptr || tok == nullptr)
    {
      dds_free(copy);
      return std::nullopt;
    }
    if (strcmp (name, option_name) == 0)
    {
      std::string ret{tok};
      dds_free(copy);
      if (tolower)
        transform(ret.begin(), ret.end(), ret.begin(), ::tolower);
      return ret;
    }
  }
  dds_free(copy);
  return std::nullopt;
}

static std::optional<iox::log::LogLevel> to_loglevel(const std::string& str)
{
  if (str == "off") return iox::log::LogLevel::kOff;
  if (str == "fatal") return iox::log::LogLevel::kFatal;
  if (str == "error") return iox::log::LogLevel::kError;
  if (str == "warn") return iox::log::LogLevel::kWarn;
  if (str == "info") return iox::log::LogLevel::kInfo;
  if (str == "debug") return iox::log::LogLevel::kDebug;
  if (str == "verbose") return iox::log::LogLevel::kVerbose;
  return std::nullopt;
}

static std::optional<dds_psmx_node_identifier_t> to_node_identifier(const std::string& str)
{
  dds_psmx_node_identifier_t id;
  if (str.length() != 2 * sizeof (id.x))
    return std::nullopt;
  for (uint32_t n = 0; n < 2 * sizeof (id.x); n++)
  {
    int32_t num;
    if ((num = ddsrt_todigit(str[n])) < 0 || num >= 16)
      return std::nullopt;
    if ((n % 2) == 0)
      id.x[n / 2] = (uint8_t) (num << 4);
    else
      id.x[n / 2] |= (uint8_t) num;
  }
  return id;
}

dds_return_t iox_create_psmx(struct dds_psmx **psmx, dds_psmx_instance_id_t instance_id, const char *config)
{
  assert(psmx);

  // C++23 would have or_else/and_then, but C++23 is too new ...
  auto opt_loglevel = get_config_option_value(config, "LOG_LEVEL", true);
  if (opt_loglevel.has_value()) {
    auto loglevel = to_loglevel (opt_loglevel.value());
    if (!loglevel.has_value())
      return DDS_RETCODE_ERROR;
    iox::log::LogManager::GetLogManager().SetDefaultLogLevel(loglevel.value(), iox::log::LogLevelOutput::kHideLogLevel);
  }

  auto opt_service_name = get_config_option_value(config, "SERVICE_NAME");
  std::string service_name;
  if (opt_service_name.has_value())
    service_name = opt_service_name.value();
  else {
    // FIXME: replace with hash of _instance_name and domain id?
    service_name = std::string("CycloneDDS iox_psmx ") + std::to_string(instance_id);
  }

  auto opt_node_id = get_config_option_value(config, "LOCATOR");
  std::optional<dds_psmx_node_identifier_t> node_id = std::nullopt;
  if (opt_node_id.has_value()) {
    node_id = to_node_identifier(opt_node_id.value());
  } else {
    node_id = get_machineid ();
  }
  if (!node_id.has_value())
    return DDS_RETCODE_ERROR;

  auto opt_keyed_topics = get_config_option_value(config, "KEYED_TOPICS", true);
  bool keyed_topics = false;
  if (opt_keyed_topics.has_value()) {
    if (opt_keyed_topics.value() == "true")
      keyed_topics = true;
    else if (opt_keyed_topics.value() != "false")
      return DDS_RETCODE_ERROR;
  }

  *psmx = new iox_psmx::iox_psmx(instance_id, service_name, node_id.value(), keyed_topics);
  return *psmx ? DDS_RETCODE_OK :  DDS_RETCODE_ERROR;
}
