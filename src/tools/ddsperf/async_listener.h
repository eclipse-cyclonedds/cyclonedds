#ifndef ASYNC_LISTENER_H
#define ASYNC_LISTENER_H

#include "dds/ddsrt/attributes.h"

struct async_listener;

struct async_listener *async_listener_new (void) ddsrt_attribute_warn_unused_result;
bool async_listener_start (struct async_listener *al) ddsrt_nonnull_all;
void async_listener_stop (struct async_listener *al) ddsrt_nonnull_all;
void async_listener_free (struct async_listener *al) ddsrt_nonnull_all;

void async_listener_enqueue_data_available (struct async_listener *al, dds_on_data_available_fn fn, dds_entity_t rd, void *arg) ddsrt_nonnull ((1, 2));
void async_listener_enqueue_subscription_matched (struct async_listener *al, dds_on_subscription_matched_fn fn, dds_entity_t rd, const dds_subscription_matched_status_t status, void *arg) ddsrt_nonnull ((1, 2));
void async_listener_enqueue_publication_matched (struct async_listener *al, dds_on_publication_matched_fn fn, dds_entity_t wr, const dds_publication_matched_status_t status, void *arg) ddsrt_nonnull ((1, 2));

#endif
