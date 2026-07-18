// SPDX-FileCopyrightText: 2026 Andy Curtis <contactandyc@gmail.com>
// SPDX-FileCopyrightText: 2026 Knode.ai
// SPDX-License-Identifier: Apache-2.0
//
// Maintainer: Andy Curtis <contactandyc@gmail.com>

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#include "a-bson-library/abson.h"
#include "a-memory-library/aml_pool.h"
#include "a-memory-library/aml_buffer.h"

#include "the-macro-library/macro_test.h"

static int is_ok(abson_t *b) { return b && !abson_is_error(b); }

/* --- BSON Byte Builder Helper --- */
static void build_simple_bson(aml_buffer_t *bh) {
    /* Builds: { "i": 42, "s": "abc" } */
    aml_buffer_clear(bh);
    size_t start = aml_buffer_length(bh);
    int32_t dummy = 0;
    aml_buffer_append(bh, &dummy, 4);

    /* "i": 42 */
    aml_buffer_appendc(bh, 0x10); /* Int32 */
    aml_buffer_appends(bh, "i"); aml_buffer_appendc(bh, '\0');
    int32_t val = 42; aml_buffer_append(bh, &val, 4);

    /* "s": "abc" */
    aml_buffer_appendc(bh, 0x02); /* String */
    aml_buffer_appends(bh, "s"); aml_buffer_appendc(bh, '\0');
    int32_t slen = 4; aml_buffer_append(bh, &slen, 4);
    aml_buffer_append(bh, "abc", 4);

    aml_buffer_appendc(bh, '\0'); /* End document */

    /* Patch length */
    int32_t total_len = (int32_t)(aml_buffer_length(bh) - start);
    memcpy((uint8_t*)aml_buffer_data(bh) + start, &total_len, 4);
}


/* ---------- 0) Predicates ---------- */

MACRO_TEST(abson_predicates_nullptr) {
    MACRO_ASSERT_FALSE(abson_is_error(NULL));
    MACRO_ASSERT_FALSE(abson_is_document(NULL));
    MACRO_ASSERT_FALSE(abson_is_array(NULL));
    MACRO_ASSERT_FALSE(abson_is_null(NULL));
    MACRO_ASSERT_FALSE(abson_is_bool(NULL));
    MACRO_ASSERT_FALSE(abson_is_string(NULL));
    MACRO_ASSERT_FALSE(abson_type(NULL) == ABSON_INT32);
    MACRO_ASSERT_FALSE(abson_type(NULL) == ABSON_INT64);
    MACRO_ASSERT_FALSE(abson_type(NULL) == ABSON_DOUBLE);
    MACRO_ASSERT_FALSE(abson_is_objectid(NULL));
    MACRO_ASSERT_FALSE(abson_is_binary(NULL));
    MACRO_ASSERT_FALSE(abson_type(NULL) == ABSON_DATETIME);
}

MACRO_TEST(abson_predicates_values) {
    aml_pool_t *pool = aml_pool_init(1 << 12);

    MACRO_ASSERT_TRUE(abson_is_null(abson_null(pool)));
    MACRO_ASSERT_TRUE(abson_is_bool(abson_true(pool)));
    MACRO_ASSERT_TRUE(abson_is_bool(abson_false(pool)));
    MACRO_ASSERT_TRUE(abson_is_string(abson_str(pool, "x")));
    MACRO_ASSERT_TRUE(abson_type(abson_int32(pool, 123)) == ABSON_INT32);
    MACRO_ASSERT_TRUE(abson_type(abson_int64(pool, 9999999999)) == ABSON_INT64);
    MACRO_ASSERT_TRUE(abson_type(abson_double(pool, 3.14)) == ABSON_DOUBLE);

    uint8_t oid[12] = {0};
    MACRO_ASSERT_TRUE(abson_is_objectid(abson_objectid(pool, oid)));

    abson_t *o = absono(pool);
    abson_t *a = absona(pool);
    MACRO_ASSERT_TRUE(abson_is_document(o));
    MACRO_ASSERT_TRUE(abson_is_array(a));
    MACRO_ASSERT_FALSE(abson_type(o) == ABSON_INT32);

    aml_pool_destroy(pool);
}

/* ---------- 1) Parsing & Value Extraction ---------- */

MACRO_TEST(abson_parse_basic_document) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    aml_buffer_t *bh = aml_buffer_init(256);
    build_simple_bson(bh);

    abson_t *b = abson_parse(pool, aml_buffer_data(bh), aml_buffer_length(bh));
    MACRO_ASSERT_TRUE(is_ok(b));
    MACRO_ASSERT_TRUE(abson_is_document(b));

    /* Extract and verify Int32 */
    abson_t *i_node = absono_scan(b, "i");
    MACRO_ASSERT_TRUE(abson_type(i_node) == ABSON_INT32);
    MACRO_ASSERT_EQ_INT(abson_to_int32(i_node, 0), 42);

    /* Extract and verify String */
    abson_t *s_node = absono_scan(b, "s");
    MACRO_ASSERT_TRUE(abson_is_string(s_node));
    MACRO_ASSERT_STREQ(abson_to_str(s_node, ""), "abc");

    aml_buffer_destroy(bh);
    aml_pool_destroy(pool);
}

/* ---------- 2) AST Building and Dumping ---------- */

MACRO_TEST(abson_build_and_dump) {
    aml_pool_t *pool = aml_pool_init(1 << 12);

    abson_t *doc = absono(pool);
    absono_append(doc, "a", abson_int32(pool, 1), true);
    absono_append(doc, "b", abson_true(pool), true);
    absono_append(doc, "c", abson_null(pool), true);

    size_t need = abson_dump_estimate(doc);
    uint8_t *buf = (uint8_t *)malloc(need);
    uint8_t *end_ptr = abson_dump_to_memory(buf, doc);
    size_t actual_len = (size_t)(end_ptr - buf);

    MACRO_ASSERT_EQ_SZ(actual_len, need);

    /* Document length header check */
    int32_t doc_len;
    memcpy(&doc_len, buf, 4);
    MACRO_ASSERT_EQ_INT(doc_len, (int32_t)actual_len);

    /* Last byte must be \0 */
    MACRO_ASSERT_EQ_INT(buf[actual_len - 1], 0x00);

    /* Verify we can parse what we just dumped (Roundtrip) */
    abson_t *parsed = abson_parse(pool, buf, actual_len);
    MACRO_ASSERT_TRUE(is_ok(parsed));
    MACRO_ASSERT_EQ_INT(absono_scan_int32(parsed, "a", 0), 1);
    MACRO_ASSERT_TRUE(absono_scan_bool(parsed, "b", false));
    MACRO_ASSERT_TRUE(abson_is_null(absono_scan(parsed, "c")));

    free(buf);
    aml_pool_destroy(pool);
}

/* ---------- 3) Array Operations ---------- */

MACRO_TEST(abson_arrays_append_and_nth) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    abson_t *arr = absona(pool);

    absona_append(arr, abson_int32(pool, 10));
    absona_append(arr, abson_int32(pool, 20));
    absona_append(arr, abson_int32(pool, 30));

    MACRO_ASSERT_EQ_INT(absona_count(arr), 3);
    MACRO_ASSERT_EQ_INT(abson_to_int32(absona_nth(arr, 1), 0), 20);

    absona_clear(arr);
    MACRO_ASSERT_EQ_INT(absona_count(arr), 0);
    MACRO_ASSERT_TRUE(absona_nth(arr, 0) == NULL);

    aml_pool_destroy(pool);
}

/* ---------- 4) Object Mutations (Scan, Get, Remove) ---------- */

MACRO_TEST(abson_object_indexes_and_mutation) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    abson_t *obj = absono(pool);

    absono_append(obj, "k1", abson_int32(pool, 100), true);
    absono_append(obj, "k2", abson_int32(pool, 200), true);

    /* Build snapshot used by _get */
    MACRO_ASSERT_EQ_INT(abson_to_int32(absono_get(obj, "k1"), 0), 100);

    /* Append after snapshot is built */
    absono_append(obj, "k3", abson_int32(pool, 300), true);
    MACRO_ASSERT_TRUE(absono_get(obj, "k3") == NULL);             /* snapshot misses it */
    MACRO_ASSERT_EQ_INT(abson_to_int32(absono_find(obj, "k3"), 0), 300); /* live tree finds it */

    /* Remove */
    MACRO_ASSERT_TRUE(absono_remove(obj, "k2"));
    MACRO_ASSERT_TRUE(absono_scan(obj, "k2") == NULL);

    aml_pool_destroy(pool);
}

/* ---------- 5) Rich BSON Types ---------- */

MACRO_TEST(abson_rich_types) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    abson_t *obj = absono(pool);

    uint8_t oid_in[12] = {0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88,0x99,0xAA,0xBB,0xCC};
    uint8_t bin_data[] = {0x80, 0xDE, 0xAD, 0xBE, 0xEF}; // First byte is subtype in your AST model

    absono_append(obj, "oid", abson_objectid(pool, oid_in), true);
    absono_append(obj, "dt", abson_datetime(pool, 1600000000000), true);
    absono_append(obj, "bin", abson_binary(pool, 0x80, bin_data, 4), true);

    /* Extraction */
    abson_t *o_node = absono_scan(obj, "oid");
    MACRO_ASSERT_TRUE(abson_is_objectid(o_node));
    const uint8_t *oid_out = abson_to_objectid(o_node);
    MACRO_ASSERT_TRUE(oid_out != NULL);
    MACRO_ASSERT_TRUE(memcmp(oid_out, oid_in, 12) == 0);

    abson_t *d_node = absono_scan(obj, "dt");
    MACRO_ASSERT_TRUE(abson_type(d_node) == ABSON_DATETIME);
    MACRO_ASSERT_EQ_INT(abson_to_datetime(d_node, 0), 1600000000000);

    abson_t *b_node = absono_scan(obj, "bin");
    MACRO_ASSERT_TRUE(abson_is_binary(b_node));

    uint8_t subtype = 0;
    uint32_t length = 0;
    const uint8_t *bin_payload = abson_to_binary(b_node, &subtype, &length);
    MACRO_ASSERT_TRUE(bin_payload != NULL);
    MACRO_ASSERT_EQ_INT(subtype, 0x80);
    MACRO_ASSERT_EQ_SZ(length, 4);

    aml_pool_destroy(pool);
}

/* ---------- 6) Error Handling / Invalid Payloads ---------- */

MACRO_TEST(abson_truncated_payload) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    aml_buffer_t *bh = aml_buffer_init(256);
    build_simple_bson(bh);

    /* Truncate the buffer heavily */
    abson_t *b = abson_parse(pool, aml_buffer_data(bh), aml_buffer_length(bh) / 2);

    /* Parser should return an error node or NULL on corrupted/truncated BSON */
    MACRO_ASSERT_TRUE(b == NULL || abson_is_error(b));

    aml_buffer_destroy(bh);
    aml_pool_destroy(pool);
}

MACRO_TEST(abson_corrupted_length) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    aml_buffer_t *bh = aml_buffer_init(256);
    build_simple_bson(bh);

    /* Mutate the root length header to lie about the document size */
    int32_t bad_len = 999999;
    memcpy(aml_buffer_data(bh), &bad_len, 4);

    abson_t *b = abson_parse(pool, aml_buffer_data(bh), aml_buffer_length(bh));
    MACRO_ASSERT_TRUE(b == NULL || abson_is_error(b));

    aml_buffer_destroy(bh);
    aml_pool_destroy(pool);
}

/* ---------- Register ---------- */

int main(void) {
    macro_test_case tests[64];
    size_t test_count = 0;

    MACRO_ADD(tests, abson_predicates_nullptr);
    MACRO_ADD(tests, abson_predicates_values);

    MACRO_ADD(tests, abson_parse_basic_document);
    MACRO_ADD(tests, abson_build_and_dump);

    MACRO_ADD(tests, abson_arrays_append_and_nth);
    MACRO_ADD(tests, abson_object_indexes_and_mutation);

    MACRO_ADD(tests, abson_rich_types);

    MACRO_ADD(tests, abson_truncated_payload);
    MACRO_ADD(tests, abson_corrupted_length);

    macro_run_all("abson", tests, test_count);
    return 0;
}
