#include <ddsi__serdata_cdr.h>
#include <dds/ddsc/dds_internal_api.h>
#include <dds/ddsrt/heap.h>
#include <dds/ddsrt/string.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <fuzz_samples.h>

#ifndef FUZZ_SAMPLE_DESER_XCDR_VERSION
#error "FUZZ_SAMPLE_DESER_XCDR_VERSION must be defined"
#endif

static void __attribute__((constructor)) print_idl_types_seed() {
    printf("IDL types seed: %s (XCDR%u)\n", idl_types_seed, idl_types_xcdr_version);
}

static void exercise_trusted_consumers(const struct dds_cdrstream_desc *desc, void *data, uint32_t size)
{
    dds_istream_t is;
    char printbuf[65536];

    void *sample = ddsrt_calloc(1, desc->size);
    dds_istream_init_well_formed (&is, size, data, FUZZ_SAMPLE_DESER_XCDR_VERSION);
    dds_stream_read_sample(&is, sample, &dds_cdrstream_default_allocator, desc);
    dds_stream_free_sample(sample, &dds_cdrstream_default_allocator, desc->ops.ops);
    ddsrt_free(sample);

    dds_istream_init_well_formed (&is, size, data, FUZZ_SAMPLE_DESER_XCDR_VERSION);
    (void) dds_stream_print_sample(&is, desc, printbuf, sizeof(printbuf));

    if (desc->keys.nkeys > 0) {
        dds_ostream_t key_os;
        dds_ostream_init(&key_os, &dds_cdrstream_default_allocator, 0, FUZZ_SAMPLE_DESER_XCDR_VERSION);

        dds_istream_init_well_formed (&is, size, data, FUZZ_SAMPLE_DESER_XCDR_VERSION);
        if (dds_stream_extract_key_from_data(&is, &key_os, &dds_cdrstream_default_allocator, desc)) {
            dds_istream_t key_is;
            dds_istream_init_well_formed (&key_is, key_os.m_index, key_os.m_buffer, FUZZ_SAMPLE_DESER_XCDR_VERSION);
            (void) dds_stream_print_key(&key_is, desc, printbuf, sizeof(printbuf));
        }

        dds_ostream_fini(&key_os, &dds_cdrstream_default_allocator);
    }
}

int LLVMFuzzerTestOneInput(void *data, size_t size);

int LLVMFuzzerTestOneInput(void *data, size_t size)
{
    uint32_t actual_size;

    if (size == 0 || size > UINT32_MAX) {
        return 0;
    }

    for(size_t i = 0; i < sizeof(fixed_types)/sizeof(fixed_types[0]); i++) {
        const struct dds_topic_descriptor *topic = fixed_types[i];
        struct dds_cdrstream_desc desc;
        dds_cdrstream_desc_from_topic_desc(&desc, topic);
        void *data_copy = ddsrt_memdup (data, size);
        if (dds_stream_normalize(data_copy, (uint32_t) size, false, FUZZ_SAMPLE_DESER_XCDR_VERSION, &desc, false, &actual_size) == DDS_STREAM_NORMALIZE_SUCCESS) {
            exercise_trusted_consumers(&desc, data_copy, actual_size);
        }
        ddsrt_free (data_copy);
        dds_cdrstream_desc_fini(&desc, &dds_cdrstream_default_allocator);
    }
    return 0;
}
