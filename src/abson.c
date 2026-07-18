// SPDX-FileCopyrightText: 2026 Andy Curtis <contactandyc@gmail.com>
// SPDX-License-Identifier: Apache-2.0
//
// Maintainer: Andy Curtis <contactandyc@gmail.com>

#include "a-bson-library/abson.h"
#include "a-memory-library/aml_pool.h"
#include "the-macro-library/macro_sort.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* =========================================================================
 * Internal Memory Access Helpers
 * (BSON specifies little-endian. memcpy prevents unaligned access faults)
 * ========================================================================= */

static inline int32_t read_i32(const uint8_t **p) {
  int32_t val;
  memcpy(&val, *p, 4);
  *p += 4;
  return val;
}

static inline int64_t read_i64(const uint8_t **p) {
  int64_t val;
  memcpy(&val, *p, 8);
  *p += 8;
  return val;
}

static inline double read_double(const uint8_t **p) {
  double val;
  memcpy(&val, *p, 8);
  *p += 8;
  return val;
}

static inline void write_i32(uint8_t **p, int32_t val) {
  memcpy(*p, &val, 4);
  *p += 4;
}

/* =========================================================================
 * Core Parser
 * ========================================================================= */

static abson_t *parse_document(aml_pool_t *pool, const uint8_t **p, const uint8_t *ep, bool is_array) {
  if (*p + 4 > ep) return NULL; /* Truncated document */

  int32_t doc_size = read_i32(p);
  const uint8_t *doc_end = (*p - 4) + doc_size;

  /* Validate bounds and strict BSON null-terminator requirement */
  if (doc_end > ep || *(doc_end - 1) != 0x00) {
    return NULL;
  }

  abson_t *root = is_array ? absona(pool) : absono(pool);

  /* Loop until we hit the document terminator '\x00' */
  while (*p < doc_end - 1) {
    uint8_t type = *(*p)++;

    /* Keys in BSON are standard C-strings */
    char *key = (char *)*p;
    while (*p < doc_end && *(*p) != '\0') (*p)++;
    if (*p >= doc_end) return NULL; /* Malformed key bounds */
    (*p)++; /* Skip the '\0' */

    abson_t *val = (abson_t *)aml_pool_zalloc(pool, sizeof(abson_t));
    val->type = type;
    val->parent = root;

    /* Extract Payload */
    switch (type) {
      case ABSON_DOUBLE:
        val->v.dbl = read_double(p);
        break;

      case ABSON_STRING:
        val->length = read_i32(p);
        val->v.str = (char *)*p;
        *p += val->length; /* Length includes the null terminator */
        break;

      case ABSON_DOCUMENT:
        val = parse_document(pool, p, ep, false);
        if (val) val->parent = root;
        break;

      case ABSON_ARRAY:
        val = parse_document(pool, p, ep, true);
        if (val) val->parent = root;
        break;

      case ABSON_BINARY:
        val->length = read_i32(p);
        val->v.bin = (uint8_t *)*p; /* Points directly to subtype, payload follows immediately */
        *p += 1 + val->length;
        break;

      case ABSON_OBJECTID:
        val->length = 12;
        val->v.bin = (uint8_t *)*p;
        *p += 12;
        break;

      case ABSON_BOOLEAN:
        val->v.bval = *(*p)++ != 0;
        break;

      case ABSON_DATETIME:
      case ABSON_TIMESTAMP:
      case ABSON_INT64:
        val->v.i64 = read_i64(p);
        break;

      case ABSON_NULL:
        /* No payload attached */
        break;

      case ABSON_INT32:
        val->v.i32 = read_i32(p);
        break;

      case ABSON_REGEX:
      case ABSON_CODEWSCOPE:
      case ABSON_UNDEFINED:
      case ABSON_DBPOINTER:
      case ABSON_JAVASCRIPT:
      case ABSON_SYMBOL:
      case ABSON_DECIMAL128:
      case ABSON_MINKEY:
      case ABSON_MAXKEY:
        /* BSON format requires skipping variable bytes for these if implemented natively.
           For this zero-copy AST implementation, we reject unhandled legacy types
           because skipping them requires type-specific logic. */
        return NULL;

      default:
        return NULL; /* Unknown BSON type signature */
    }

    if (is_array) {
      /* Array keys (e.g. "0", "1") are safely ignored, array structure handles order inherently */
      absona_append(root, val);
    } else {
      /* Zero copy: point directly to the source buffer's key */
      absono_append(root, key, val, false);
    }
  }

  *p = doc_end; /* Advance exactly to the end of this document */
  return root;
}

abson_t *abson_parse(aml_pool_t *pool, const void *data, size_t length) {
  const uint8_t *p = (const uint8_t *)data;
  return parse_document(pool, &p, p + length, false);
}


/* =========================================================================
 * Tree / Map Build & Sort (for rapid Document Key lookups)
 * ========================================================================= */

static inline bool absono_compare_sort(const absono_t **a, const absono_t **b) {
  return strcmp((*a)->key, (*b)->key) < 0;
}

macro_sort(__abson_sort, absono_t *, absono_compare_sort)

void _absono_fill(_absono_t *o) {
  o->root = (macro_map_t *)aml_pool_alloc(
      o->pool, sizeof(absono_t *) * o->num_entries);

  absono_t **base = (absono_t **)o->root;
  absono_t **awp = base;
  absono_t *n = o->head;
  while (n) {
    *awp++ = n;
    n = n->next;
  }

  o->num_sorted_entries = awp - base;
  if (o->num_sorted_entries)
    __abson_sort(base, o->num_sorted_entries);
  else
    o->root = NULL;
}


/* =========================================================================
 * Serialization (Dumping)
 * ========================================================================= */

size_t abson_dump_estimate(abson_t *b) {
  if (!b) return 0;

  size_t sz = 0;
  switch (b->type) {
    case ABSON_DOUBLE:     return 8;
    case ABSON_STRING:     return 4 + b->length;
    case ABSON_BINARY:     return 4 + 1 + b->length; /* length(4) + subtype(1) + payload */
    case ABSON_OBJECTID:   return 12;
    case ABSON_BOOLEAN:    return 1;
    case ABSON_DATETIME:
    case ABSON_TIMESTAMP:
    case ABSON_INT64:      return 8;
    case ABSON_NULL:       return 0;
    case ABSON_INT32:      return 4;

    case ABSON_DOCUMENT: {
      sz += 4; /* Doc size prefix */
      _absono_t *o = (_absono_t *)b;
      for (absono_t *n = o->head; n; n = n->next) {
        sz += 1; /* Type byte */
        sz += strlen(n->key) + 1; /* Key + \0 */
        sz += abson_dump_estimate(n->value);
      }
      sz += 1; /* Doc \0 terminator */
      break;
    }

    case ABSON_ARRAY: {
      sz += 4; /* Array size prefix */
      _absona_t *a = (_absona_t *)b;
      int i = 0;
      char key_buf[16];
      for (absona_t *n = a->head; n; n = n->next) {
        sz += 1; /* Type byte */
        int klen = snprintf(key_buf, sizeof(key_buf), "%d", i++);
        sz += klen + 1; /* Index Key + \0 */
        sz += abson_dump_estimate(n->value);
      }
      sz += 1; /* Array \0 terminator */
      break;
    }
    default: break;
  }
  return sz;
}


uint8_t *abson_dump_to_memory(uint8_t *s, abson_t *b) {
  if (!b) return s;

  switch (b->type) {
    case ABSON_DOUBLE:
      memcpy(s, &b->v.dbl, 8);
      return s + 8;

    case ABSON_STRING:
      write_i32(&s, b->length);
      memcpy(s, b->v.str, b->length);
      return s + b->length;

    case ABSON_BINARY:
      write_i32(&s, b->length);
      /* Copy subtype + payload together (subtype is first byte) */
      memcpy(s, b->v.bin, b->length + 1);
      return s + b->length + 1;

    case ABSON_OBJECTID:
      memcpy(s, b->v.bin, 12);
      return s + 12;

    case ABSON_BOOLEAN:
      *s++ = b->v.bval ? 1 : 0;
      return s;

    case ABSON_DATETIME:
    case ABSON_TIMESTAMP:
    case ABSON_INT64:
      memcpy(s, &b->v.i64, 8);
      return s + 8;

    case ABSON_NULL:
      return s;

    case ABSON_INT32:
      write_i32(&s, b->v.i32);
      return s;

    case ABSON_DOCUMENT: {
      uint8_t *start = s;
      s += 4; /* Reserve space for size backpatch */

      _absono_t *o = (_absono_t *)b;
      for (absono_t *n = o->head; n; n = n->next) {
        *s++ = (uint8_t)n->value->type;
        size_t klen = strlen(n->key);
        memcpy(s, n->key, klen + 1);
        s += klen + 1;
        s = abson_dump_to_memory(s, n->value);
      }

      *s++ = 0x00; /* End of document terminator */
      int32_t total_size = (int32_t)(s - start);
      memcpy(start, &total_size, 4); /* Backpatch correct size */
      return s;
    }

    case ABSON_ARRAY: {
      uint8_t *start = s;
      s += 4; /* Reserve space for size backpatch */

      _absona_t *a = (_absona_t *)b;
      int i = 0;
      char key_buf[16];
      for (absona_t *n = a->head; n; n = n->next) {
        *s++ = (uint8_t)n->value->type;
        int klen = snprintf(key_buf, sizeof(key_buf), "%d", i++);
        memcpy(s, key_buf, klen + 1);
        s += klen + 1;
        s = abson_dump_to_memory(s, n->value);
      }

      *s++ = 0x00; /* End of array terminator */
      int32_t total_size = (int32_t)(s - start);
      memcpy(start, &total_size, 4); /* Backpatch correct size */
      return s;
    }

    default:
      return s;
  }
}

void abson_dump_to_buffer(aml_buffer_t *bh, abson_t *b) {
  if (!b) return;
  size_t need = abson_dump_estimate(b);
  size_t old_len = aml_buffer_length(bh);
  aml_buffer_resize(bh, old_len + need);
  uint8_t *s = (uint8_t *)aml_buffer_data(bh) + old_len;
  abson_dump_to_memory(s, b);
}

uint8_t *abson_stringify(aml_pool_t *pool, abson_t *b, size_t *out_length) {
  size_t need = abson_dump_estimate(b);
  if (out_length) *out_length = need;
  uint8_t *s = (uint8_t *)aml_pool_alloc(pool, need);
  abson_dump_to_memory(s, b);
  return s;
}

void abson_dump(FILE *out, abson_t *b) {
  if (!b) return;
  size_t need = abson_dump_estimate(b);
  uint8_t *tmp = (uint8_t *)malloc(need);
  if (tmp) {
    abson_dump_to_memory(tmp, b);
    fwrite(tmp, 1, need, out);
    free(tmp);
  }
}
