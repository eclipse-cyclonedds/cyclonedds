// Copyright(c) 2023 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDS__PSMX_H
#define DDS__PSMX_H

#include "dds/ddsi/ddsi_sertype.h"
#include "dds/ddsc/dds_psmx.h"
#include "dds/ddsc/dds_public_impl.h"

struct ddsi_domaingv;
struct dds_domain;
struct dds_ktopic;
struct dds_endpoint;

/**
 * @brief linked list describing a number of topics
 */
struct dds_psmx_topic_list_elem {
  struct dds_psmx_topic * topic; //!< the current element in the list
  struct dds_psmx_topic_list_elem * prev; //!< the previous element in the list
  struct dds_psmx_topic_list_elem * next; //!< the next element in the list
};

/**
 * @brief linked list describing a number of endpoints
 */
struct dds_psmx_endpoint_list_elem {
  struct dds_psmx_endpoint * endpoint; //!< the current element in the list
  struct dds_psmx_endpoint_list_elem * prev; //!< the previous element in the list
  struct dds_psmx_endpoint_list_elem * next; //!< the next element in the list
};

/**
 * @brief Definition for the function to load a PSMX instance
 *
 * This function is exported from the PSMX plugin library.
 *
 * @returns a DDS return code
 */
typedef dds_return_t (*dds_psmx_create_fn) (
  struct dds_psmx **pubsub_message_exchange, // output for the PSMX instance to be created
  dds_psmx_instance_id_t identifier, // the unique identifier for this PSMX
  const char *config // PSMX specific configuration
);

char *dds_pubsub_message_exchange_configstr (const char *config);

dds_return_t dds_pubsub_message_exchange_init (const struct ddsi_domaingv *gv, struct dds_domain *domain);

dds_return_t dds_pubsub_message_exchange_fini (struct dds_domain *domain);

dds_return_t dds_endpoint_add_psmx_endpoint (struct dds_endpoint *ep, const dds_qos_t *qos, struct dds_psmx_topics_set *psmx_topics, dds_psmx_endpoint_type_t endpoint_type);
void dds_endpoint_remove_psmx_endpoints (struct dds_endpoint *ep);

struct ddsi_psmx_locators_set *dds_get_psmx_locators_set (const dds_qos_t *qos, const struct dds_psmx_set *psmx_instances);
void dds_psmx_locators_set_free (struct ddsi_psmx_locators_set *psmx_locators_set);

/**
 * @brief Request a loan
 *
 * @param[in] psmx_endpoint  the endpoint to request a loan for
 * @param[in] sz    size of the loan
 * @return a loaned sample
 */
dds_loaned_sample_t * dds_psmx_endpoint_request_loan (struct dds_psmx_endpoint *psmx_endpoint, uint32_t sz)
  ddsrt_nonnull_all ddsrt_attribute_warn_unused_result;

#endif // DDS__PSMX_H
