// Copyright(c) 2024 Robert Femmer
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#include "CUnit/CUnit.h"
#include "CUnit/Test.h"

#include "dds/security/core/dds_security_serialize.h"
#include "dds/ddsrt/heap.h"

CU_Test(dds_security_serialize, deserialize_octet_seq)
{
    unsigned char heapdata[] =
    {
        // transformation_kind (OctetArray) 4 bytes
        0x0, 0x0, 0x0, 0x0,
        // master_salt (OctetSeq)
        // length of octet seq
        0x0, 0x0, 0x0, 0x1,
        // the octet sequence
        0x0,
        // This is all the data we attempt to deserialize
        // Three padding bytes that the deserializer will skip
        0x0, 0x0, 0x0,
        // Suppose unknown heap memory starts here. This would be parsed into sender_key_id
        0x0, 0x0, 0x0, 0x0,
        // This would be the length of the next master_send_key.
        0x0, 0x0, 0x0, 0x0,
        // Next would be receiver_specific_key_id.
        0x0, 0x0, 0x0, 0x0,
        // master_receiver_specific_key OctetSeq. This could be any size up to 4GB, which will be allocated.
        // Here, we allocate 16 bytes, which is longer than the 9 bytes we feed to the deserializer.
        0x0, 0x0, 0x0, 0x10,
        // the key
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
    };

    // Pass the pointer to the prepared buffer and claim that length is only 9 bytes.
    DDS_Security_Deserializer dser = DDS_Security_Deserializer_new(heapdata, 9);
    DDS_Security_KeyMaterial_AES_GCM_GMAC data;
    int r = DDS_Security_Deserialize_KeyMaterial_AES_GCM_GMAC(dser, &data);
    // This should fail, because 9 bytes is not enough to deserialize the key material
    CU_ASSERT_EQUAL(r, 0);
    // Specifically, master_receiver_specific_key._buffer should be NULL, because
    // the octet sequence itself is longer than the serialized data according to dser->remain
    CU_ASSERT_EQUAL(data.master_receiver_specific_key._buffer, NULL);

    ddsrt_free(data.master_salt._buffer);
    ddsrt_free(data.master_sender_key._buffer);
    ddsrt_free(data.master_receiver_specific_key._buffer);
    DDS_Security_Deserializer_free(dser);
}
