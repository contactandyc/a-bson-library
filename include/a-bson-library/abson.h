// SPDX-FileCopyrightText: 2026 Andy Curtis <contactandyc@gmail.com>
// SPDX-License-Identifier: Apache-2.0
//
// Maintainer: Andy Curtis <contactandyc@gmail.com>

#ifndef _abson_H
#define _abson_H

#include "a-memory-library/aml_buffer.h"
#include "a-memory-library/aml_pool.h"

#include <inttypes.h>
#include <stdbool.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ===========================
 *  Design notes / contracts
 * ===========================
 *
 * - Speed first. Containers keep insertion order. Lookups use either a
 *   one-time sorted-array snapshot ("get") or an RB-tree map ("find").
 *
 * - Zero-copy strictly enforced. BSON is binary and already null-terminated
 *   for strings. The parser NEVER modifies the input buffer. Strings, keys,
 *   and binary blobs are pointer-mapped directly to the source buffer.
 *
 * - Native Types. Unlike JSON where numbers are stored as strings and parsed
 *   on demand, BSON provides strict int32, int64, and double payloads. These
 *   are parsed immediately into native unions in the AST.
 *
 * - Arrays in BSON are technically documents with keys "0", "1", "2". The
 *   abson_parse function intercepts ARRAY types, ignores the physical keys,
 *   and creates an absona_t linked list so the user experiences a standard
 *   array API seamlessly. When dumping, it automatically regenerates the keys.
 *
 * - Object key indexes are snapshots:
 *     * absono_get / absono_get_node build a sorted-array index **once**.
 *       Appends after that are invisible to this index until it rebuilds.
 *     * absono_find uses a tree map. Appends are **not** auto-inserted
 *       unless you used absono_insert/absono_set; plain absono_append
 *       will leave the tree stale.
 */

/* Forward decls */
struct abson_s;  typedef struct abson_s  abson_t;
struct absona_s; typedef struct absona_s absona_t;
struct absono_s; typedef struct absono_s absono_t;

/* Public type tag. Matches official BSON specs. */
typedef enum {
  ABSON_ERROR      = 0x00,
  ABSON_DOUBLE     = 0x01,
  ABSON_STRING     = 0x02,
  ABSON_DOCUMENT   = 0x03, // Object
  ABSON_ARRAY      = 0x04,
  ABSON_BINARY     = 0x05,
  ABSON_UNDEFINED  = 0x06, // Deprecated, but supported
  ABSON_OBJECTID   = 0x07,
  ABSON_BOOLEAN    = 0x08,
  ABSON_DATETIME   = 0x09,
  ABSON_NULL       = 0x0A,
  ABSON_REGEX      = 0x0B,
  ABSON_DBPOINTER  = 0x0C, // Deprecated
  ABSON_JAVASCRIPT = 0x0D,
  ABSON_SYMBOL     = 0x0E, // Deprecated
  ABSON_CODEWSCOPE = 0x0F,
  ABSON_INT32      = 0x10,
  ABSON_TIMESTAMP  = 0x11,
  ABSON_INT64      = 0x12,
  ABSON_DECIMAL128 = 0x13,
  ABSON_MINKEY     = 0xFF,
  ABSON_MAXKEY     = 0x7F
} abson_type_t;

/* =========
 * Parsing
 * ========= */

/** Parse BSON from a buffer. NON-destructive (zero-copy).
 *  - Stores string/binary pointers directly to `data`.
 *  - Fails and returns an error node if length bounds are violated.
 *  Lifetime: returned node points into the caller's buffer; keep it alive. */
abson_t *abson_parse(aml_pool_t *pool, const void *data, size_t length);

/** True iff b is an error node. */
static inline bool abson_is_error(abson_t *b);

/** Return the tag of a node. O(1). */
static inline abson_type_t abson_type(abson_t *b);

/* =================
 * Dump / stringify
 * ================= */

/** Dump BSON document to FILE*.
 *  Writes raw length-prefixed bytes following the BSON specification. */
void abson_dump(FILE *out, abson_t *b);

/** Dump BSON to aml_buffer. Backpatches length headers automatically. */
void abson_dump_to_buffer(aml_buffer_t *bh, abson_t *b);

/** Estimate bytes for binary dump. Calculates nested lengths perfectly. */
size_t abson_dump_estimate(abson_t *b);

/** Dump BSON into caller-provided buffer 's' (must have at least
 *  abson_dump_estimate(b) bytes). Returns end pointer. */
uint8_t *abson_dump_to_memory(uint8_t *s, abson_t *b);

/** Compact BSON document as a single pool allocation. */
uint8_t *abson_stringify(aml_pool_t *pool, abson_t *b, size_t *out_length);

/* ==========================
 * Node construction helpers
 * ========================== */

/** Literal nodes (small, fast). */
static inline abson_t *abson_true(aml_pool_t *pool);
static inline abson_t *abson_false(aml_pool_t *pool);
static inline abson_t *abson_bool(aml_pool_t *pool, bool v);
static inline abson_t *abson_null(aml_pool_t *pool);

/** String nodes. 's' must be null-terminated. length includes \0.
 *  Uses zero-copy unless the buffer is volatile, in which case caller
 *  should pass a pool-duplicated string. */
static inline abson_t *abson_string(aml_pool_t *pool, const char *s, uint32_t length);
static inline abson_t *abson_str(aml_pool_t *pool, const char *s);

/** Numeric and BSON-specific type constructors. */
static inline abson_t *abson_int32(aml_pool_t *pool, int32_t v);
static inline abson_t *abson_int64(aml_pool_t *pool, int64_t v);
static inline abson_t *abson_double(aml_pool_t *pool, double v);
static inline abson_t *abson_datetime(aml_pool_t *pool, int64_t ms_since_epoch);
static inline abson_t *abson_objectid(aml_pool_t *pool, const uint8_t *oid); /* 12 bytes */
static inline abson_t *abson_binary(aml_pool_t *pool, uint8_t subtype, const uint8_t *data, uint32_t length);

/* ======================
 * Type test convenience
 * ====================== */

static inline bool abson_is_document(abson_t *b);
static inline bool abson_is_array(abson_t *b);
static inline bool abson_is_null(abson_t *b);
static inline bool abson_is_bool(abson_t *b);
static inline bool abson_is_string(abson_t *b);
static inline bool abson_is_number(abson_t *b); /* INT32, INT64, DOUBLE */
static inline bool abson_is_objectid(abson_t *b);
static inline bool abson_is_binary(abson_t *b);

/* ======================
 * Value conversions
 * ======================
 * All functions:
 *   - Return 'default_value' if b is NULL or type mismatch.
 *   - Fast inline access to union properties.
 *   - Numeric extractors will gracefully cast across INT32/INT64/DOUBLE.
 */
static inline int      abson_to_int(abson_t *b, int default_value);
static inline int32_t  abson_to_int32(abson_t *b, int32_t default_value);
static inline uint32_t abson_to_uint32(abson_t *b, uint32_t default_value);
static inline int64_t  abson_to_int64(abson_t *b, int64_t default_value);
static inline uint64_t abson_to_uint64(abson_t *b, uint64_t default_value);
static inline float    abson_to_float(abson_t *b, float default_value);
static inline double   abson_to_double(abson_t *b, double default_value);
static inline bool     abson_to_bool(abson_t *b, bool default_value);

/** Return string view. Because BSON strings are inherently unescaped and null
 * terminated, we don't need a separate 'strd' decoded method like ajson. */
static inline char *abson_to_str(abson_t *b, const char *default_value);

/** BSON Specific Extractors */
static inline const uint8_t *abson_to_objectid(abson_t *b);
static inline const uint8_t *abson_to_binary(abson_t *b, uint8_t *out_subtype, uint32_t *out_length);
static inline int64_t        abson_to_datetime(abson_t *b, int64_t default_value);

/* “Try” variants (no defaults). Return true on success and fill *out. */
static inline bool abson_try_to_int    (abson_t *b, int      *out);
static inline bool abson_try_to_int32  (abson_t *b, int32_t  *out);
static inline bool abson_try_to_uint32 (abson_t *b, uint32_t *out);
static inline bool abson_try_to_int64  (abson_t *b, int64_t  *out);
static inline bool abson_try_to_uint64 (abson_t *b, uint64_t *out);
static inline bool abson_try_to_float  (abson_t *b, float    *out);
static inline bool abson_try_to_double (abson_t *b, double   *out);
static inline bool abson_try_to_bool   (abson_t *b, bool     *out);

/* ===========================
 * Small extraction helpers
 * =========================== */

static inline char    *abson_extract_string(abson_t *node);
static inline int      abson_extract_int(abson_t *node);
static inline bool     abson_extract_bool(abson_t *node);
static inline uint32_t abson_extract_uint32(abson_t *node);

/** Extract array of strings:
 *  - If node is not an array → returns a 1-element array.
 *  - Pointers point directly into BSON payload. Last element is NULL. */
static inline char **abson_extract_string_array(size_t *count, aml_pool_t *pool, abson_t *node);

/** Extract array of floats. Returns NULL if node not an array. */
static inline float *abson_extract_float_array(size_t *num, aml_pool_t *pool, abson_t *node);


/* =========
 * Arrays
 * ========= */

/** Create an empty array (pool-owned). */
static inline abson_t *absona(aml_pool_t *pool);

/** Count elements. O(1). */
static inline int absona_count(abson_t *b);

/** Linear scan to nth (0-based). O(n/2). */
static inline abson_t *absona_scan(abson_t *b, int nth);

/** Direct-access (builds/uses an internal snapshot table). O(1) after build. */
static inline abson_t  *absona_nth(abson_t *b, int nth);
static inline absona_t *absona_nth_node(abson_t *b, int nth);

/** Doubly-linked iteration helpers (preserve insertion order). */
static inline absona_t *absona_first(abson_t *b);
static inline absona_t *absona_last(abson_t *b);
static inline absona_t *absona_next(absona_t *b);
static inline absona_t *absona_previous(absona_t *b);

/** Append item (sets item->parent). Invalidates any direct-access table. */
static inline void absona_append(abson_t *b, abson_t *item);

/** Erase a node from its array. */
static inline void absona_erase(absona_t *n);

/** Clear an array by unlinking all items. Keeps pool allocations. */
static inline void absona_clear(abson_t *b);


/* ==========
 * Objects / Documents
 * ========== */

/** Create an empty document (pool-owned). */
static inline abson_t *absono(aml_pool_t *pool);

/** Count key-value pairs. O(1). */
static inline int absono_count(abson_t *b);

/** Ordered iteration (insertion order). */
static inline absono_t *absono_first(abson_t *b);
static inline absono_t *absono_last(abson_t *b);
static inline absono_t *absono_next(absono_t *b);
static inline absono_t *absono_previous(absono_t *b);

/** Append without checking for existing key (fast). Keeps insertion order.
 *  If copy_key == false, stores the pointer as-is. */
static inline void absono_append(abson_t *b, const char *key, abson_t *item, bool copy_key);

/** Set (replace first matching key, else append). Maintains indexes. */
static inline absono_t *absono_set(abson_t *b, const char *key, abson_t *item, bool copy_key);

/** Remove first matching key. Updates indexes appropriately. */
static inline bool absono_remove(abson_t *b, const char *key);

/** Linear scans (no index). */
static inline abson_t *absono_scan(abson_t *b, const char *key);
static inline abson_t *absono_scanr(abson_t *b, const char *key);

/** Snapshot-based lookup (builds a sorted-array index). */
static inline abson_t  *absono_get(abson_t *b, const char *key);
static inline absono_t *absono_get_node(abson_t *b, const char *key);

/** Tree-based lookup/insert. */
static inline absono_t *absono_find_node(abson_t *b, const char *key);
static inline abson_t  *absono_find(abson_t *b, const char *key);
static inline absono_t *absono_insert(abson_t *b, const char *key, abson_t *item, bool copy_key);

/** Erase a specific object entry. */
static inline void absono_erase(absono_t *n);


/* Document value helpers (scan/get/find + convert). */
static inline int      absono_scan_int(abson_t *b, const char *key, int default_value);
static inline int32_t  absono_scan_int32(abson_t *b, const char *key, int32_t default_value);
static inline uint32_t absono_scan_uint32(abson_t *b, const char *key, uint32_t default_value);
static inline int64_t  absono_scan_int64(abson_t *b, const char *key, int64_t default_value);
static inline uint64_t absono_scan_uint64(abson_t *b, const char *key, uint64_t default_value);
static inline float    absono_scan_float(abson_t *b, const char *key, float default_value);
static inline double   absono_scan_double(abson_t *b, const char *key, double default_value);
static inline bool     absono_scan_bool(abson_t *b, const char *key, bool default_value);
static inline char    *absono_scan_str(abson_t *b, const char *key, const char *default_value);

/* Document “try” helpers — scan (linear), get (snapshot), find (tree). */
static inline bool absono_scan_try_int    (abson_t *b, const char *key, int      *out);
static inline bool absono_scan_try_int32  (abson_t *b, const char *key, int32_t  *out);
static inline bool absono_scan_try_uint32 (abson_t *b, const char *key, uint32_t *out);
static inline bool absono_scan_try_int64  (abson_t *b, const char *key, int64_t  *out);
static inline bool absono_scan_try_uint64 (abson_t *b, const char *key, uint64_t *out);
static inline bool absono_scan_try_float  (abson_t *b, const char *key, float    *out);
static inline bool absono_scan_try_double (abson_t *b, const char *key, double   *out);
static inline bool absono_scan_try_bool   (abson_t *b, const char *key, bool     *out);
static inline bool absono_scan_try_str    (abson_t *b, const char *key, const char *default_value, char **out);


static inline int      absono_get_int(abson_t *b, const char *key, int default_value);
static inline int32_t  absono_get_int32(abson_t *b, const char *key, int32_t default_value);
static inline uint32_t absono_get_uint32(abson_t *b, const char *key, uint32_t default_value);
static inline int64_t  absono_get_int64(abson_t *b, const char *key, int64_t default_value);
static inline uint64_t absono_get_uint64(abson_t *b, const char *key, uint64_t default_value);
static inline float    absono_get_float(abson_t *b, const char *key, float default_value);
static inline double   absono_get_double(abson_t *b, const char *key, double default_value);
static inline bool     absono_get_bool(abson_t *b, const char *key, bool default_value);
static inline char    *absono_get_str(abson_t *b, const char *key, const char *default_value);

static inline bool absono_get_try_int    (abson_t *b, const char *key, int      *out);
static inline bool absono_get_try_int32  (abson_t *b, const char *key, int32_t  *out);
static inline bool absono_get_try_uint32 (abson_t *b, const char *key, uint32_t *out);
static inline bool absono_get_try_int64  (abson_t *b, const char *key, int64_t  *out);
static inline bool absono_get_try_uint64 (abson_t *b, const char *key, uint64_t *out);
static inline bool absono_get_try_float  (abson_t *b, const char *key, float    *out);
static inline bool absono_get_try_double (abson_t *b, const char *key, double   *out);
static inline bool absono_get_try_bool   (abson_t *b, const char *key, bool     *out);
static inline bool absono_get_try_str    (abson_t *b, const char *key, const char *default_value, char **out);


static inline int      absono_find_int(abson_t *b, const char *key, int default_value);
static inline int32_t  absono_find_int32(abson_t *b, const char *key, int32_t default_value);
static inline uint32_t absono_find_uint32(abson_t *b, const char *key, uint32_t default_value);
static inline int64_t  absono_find_int64(abson_t *b, const char *key, int64_t default_value);
static inline uint64_t absono_find_uint64(abson_t *b, const char *key, uint64_t default_value);
static inline float    absono_find_float(abson_t *b, const char *key, float default_value);
static inline double   absono_find_double(abson_t *b, const char *key, double default_value);
static inline bool     absono_find_bool(abson_t *b, const char *key, bool default_value);
static inline char    *absono_find_str(abson_t *b, const char *key, const char *default_value);

static inline bool absono_find_try_int    (abson_t *b, const char *key, int      *out);
static inline bool absono_find_try_int32  (abson_t *b, const char *key, int32_t  *out);
static inline bool absono_find_try_uint32 (abson_t *b, const char *key, uint32_t *out);
static inline bool absono_find_try_int64  (abson_t *b, const char *key, int64_t  *out);
static inline bool absono_find_try_uint64 (abson_t *b, const char *key, uint64_t *out);
static inline bool absono_find_try_float  (abson_t *b, const char *key, float    *out);
static inline bool absono_find_try_double (abson_t *b, const char *key, double   *out);
static inline bool absono_find_try_bool   (abson_t *b, const char *key, bool     *out);
static inline bool absono_find_try_str    (abson_t *b, const char *key, const char *default_value, char **out);

/* ==========
 * BSON path
 * ==========
 * Simple dotted path navigation over objects/arrays.
 * - Objects: dot-separated keys.
 * - Arrays:
 *     • "idx" selects by 0-based index (linear scan w/ sscanf).
 *     • "key=value" selects first element whose object's key equals value.
 * Return NULL on miss.
 */
static inline abson_t *absono_path(aml_pool_t *pool, abson_t *b, const char *path);
static inline char    *absono_pathv(aml_pool_t *pool, abson_t *b, const char *path);

/* Inline implementations */
#include "a-bson-library/impl/abson.h"

#ifdef __cplusplus
}
#endif

#endif /* _abson_H */
