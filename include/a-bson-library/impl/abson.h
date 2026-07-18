// SPDX-FileCopyrightText: 2026 Andy Curtis <contactandyc@gmail.com>
// SPDX-License-Identifier: Apache-2.0
//
// Maintainer: Andy Curtis <contactandyc@gmail.com>

#include "the-macro-library/macro_map.h"
#include "the-macro-library/macro_bsearch.h"

/* ===========================
 * Core Struct Definitions
 * =========================== */

struct abson_s {
  uint32_t type;
  uint32_t length;
  abson_t *parent;
  union {
    char *str;
    uint8_t *bin;
    int32_t i32;
    int64_t i64;
    double dbl;
    bool bval;
  } v;
};

struct absona_s {
  abson_t *value;
  absona_t *next;
  absona_t *previous;
};

struct absono_s {
  macro_map_t map;
  char *key;
  abson_t *value;
  absono_t *next;
  absono_t *previous;
};

typedef struct {
  uint32_t type;
  uint32_t num_entries;
  abson_t *parent;
  absona_t **array;
  absona_t *head;
  absona_t *tail;
  aml_pool_t *pool;
} _absona_t;

typedef struct {
  uint32_t type;
  uint32_t num_entries;
  abson_t *parent;
  macro_map_t *root;
  size_t num_sorted_entries;
  absono_t *head;
  absono_t *tail;
  aml_pool_t *pool;
} _absono_t;

/* ===========================
 * Internal Search Macros
 * =========================== */

static inline int absono_compare(const char *key, const absono_t **o) {
  return strcmp(key, (*o)->key);
}

static inline int absono_compare2(const char *key, const absono_t *o) {
  return strcmp(key, o->key);
}

static inline int absono_insert_compare(const absono_t *a, const absono_t *b) {
  return strcmp(a->key, b->key);
}

static inline
macro_map_find_kv(__abson_find, char, absono_t, absono_compare2)

static inline macro_map_insert(__abson_insert, absono_t, absono_insert_compare)

static inline macro_bsearch_first_kv(__abson_search, char, absono_t *, absono_compare)

/* ===========================
 * Type Checking
 * =========================== */

static inline abson_type_t abson_type(abson_t *b) {
  return b ? (abson_type_t)(b->type) : ABSON_ERROR;
}

static inline bool abson_is_error(abson_t *b) { return b && b->type == ABSON_ERROR; }
static inline bool abson_is_document(abson_t *b) { return b && b->type == ABSON_DOCUMENT; }
static inline bool abson_is_array(abson_t *b) { return b && b->type == ABSON_ARRAY; }
static inline bool abson_is_null(abson_t *b) { return b && b->type == ABSON_NULL; }
static inline bool abson_is_bool(abson_t *b) { return b && b->type == ABSON_BOOLEAN; }
static inline bool abson_is_string(abson_t *b) { return b && b->type == ABSON_STRING; }
static inline bool abson_is_objectid(abson_t *b) { return b && b->type == ABSON_OBJECTID; }
static inline bool abson_is_binary(abson_t *b) { return b && b->type == ABSON_BINARY; }

static inline bool abson_is_number(abson_t *b) {
  return b && (b->type == ABSON_INT32 || b->type == ABSON_INT64 || b->type == ABSON_DOUBLE);
}

/* ===========================
 * Node Constructors
 * =========================== */

static inline abson_t *abson_true(aml_pool_t *pool) {
  abson_t *b = (abson_t *)aml_pool_zalloc(pool, sizeof(abson_t));
  b->type = ABSON_BOOLEAN;
  b->v.bval = true;
  return b;
}

static inline abson_t *abson_false(aml_pool_t *pool) {
  abson_t *b = (abson_t *)aml_pool_zalloc(pool, sizeof(abson_t));
  b->type = ABSON_BOOLEAN;
  b->v.bval = false;
  return b;
}

static inline abson_t *abson_bool(aml_pool_t *pool, bool v) {
  return v ? abson_true(pool) : abson_false(pool);
}

static inline abson_t *abson_null(aml_pool_t *pool) {
  abson_t *b = (abson_t *)aml_pool_zalloc(pool, sizeof(abson_t));
  b->type = ABSON_NULL;
  return b;
}

static inline abson_t *abson_int32(aml_pool_t *pool, int32_t v) {
  abson_t *b = (abson_t *)aml_pool_zalloc(pool, sizeof(abson_t));
  b->type = ABSON_INT32;
  b->v.i32 = v;
  return b;
}

static inline abson_t *abson_int64(aml_pool_t *pool, int64_t v) {
  abson_t *b = (abson_t *)aml_pool_zalloc(pool, sizeof(abson_t));
  b->type = ABSON_INT64;
  b->v.i64 = v;
  return b;
}

static inline abson_t *abson_datetime(aml_pool_t *pool, int64_t ms_since_epoch) {
  abson_t *b = (abson_t *)aml_pool_zalloc(pool, sizeof(abson_t));
  b->type = ABSON_DATETIME;
  b->v.i64 = ms_since_epoch;
  return b;
}

static inline abson_t *abson_double(aml_pool_t *pool, double v) {
  abson_t *b = (abson_t *)aml_pool_zalloc(pool, sizeof(abson_t));
  b->type = ABSON_DOUBLE;
  b->v.dbl = v;
  return b;
}

static inline abson_t *abson_string(aml_pool_t *pool, const char *s, uint32_t length) {
  if(!s) return NULL;
  abson_t *b = (abson_t *)aml_pool_zalloc(pool, sizeof(abson_t));
  b->type = ABSON_STRING;
  b->length = length;
  b->v.str = (char *)s;
  return b;
}

static inline abson_t *abson_str(aml_pool_t *pool, const char *s) {
  return s ? abson_string(pool, s, strlen(s) + 1) : NULL;
}

static inline abson_t *abson_objectid(aml_pool_t *pool, const uint8_t *oid) {
  if(!oid) return NULL;
  abson_t *b = (abson_t *)aml_pool_zalloc(pool, sizeof(abson_t));
  b->type = ABSON_OBJECTID;
  b->length = 12;
  b->v.bin = (uint8_t *)oid;
  return b;
}

static inline abson_t *abson_binary(aml_pool_t *pool, uint8_t subtype, const uint8_t *data, uint32_t length) {
  (void)subtype;
  if(!data) return NULL;
  /* Format: [length (4)] [subtype (1)] [payload...] -> Length covers payload only in modern BSON */
  abson_t *b = (abson_t *)aml_pool_zalloc(pool, sizeof(abson_t));
  b->type = ABSON_BINARY;
  b->length = length;

  /* Hack to store subtype inline cleanly without a new struct: we can allocate
     a fresh block storing [subtype][data...] if it wasn't pre-packed, or rely
     on the user providing a packed block. For this zero-copy AST, we assume
     the pointer 'data' points to the subtype byte. */
  b->v.bin = (uint8_t *)data;
  return b;
}

/* ===========================
 * Casting / Value Extraction
 * =========================== */

static inline int64_t abson_to_int64(abson_t *b, int64_t default_value) {
  if (!b) return default_value;
  switch(b->type) {
    case ABSON_INT32:     return (int64_t)b->v.i32;
    case ABSON_INT64:
    case ABSON_DATETIME:
    case ABSON_TIMESTAMP: return b->v.i64;
    case ABSON_DOUBLE:    return (int64_t)b->v.dbl;
    case ABSON_BOOLEAN:   return b->v.bval ? 1 : 0;
    default:              return default_value;
  }
}

static inline int32_t abson_to_int32(abson_t *b, int32_t default_value) {
  if (!b) return default_value;
  switch(b->type) {
    case ABSON_INT32:     return b->v.i32;
    case ABSON_INT64:
    case ABSON_DATETIME:
    case ABSON_TIMESTAMP: return (int32_t)b->v.i64;
    case ABSON_DOUBLE:    return (int32_t)b->v.dbl;
    case ABSON_BOOLEAN:   return b->v.bval ? 1 : 0;
    default:              return default_value;
  }
}

static inline double abson_to_double(abson_t *b, double default_value) {
  if (!b) return default_value;
  switch(b->type) {
    case ABSON_DOUBLE:    return b->v.dbl;
    case ABSON_INT32:     return (double)b->v.i32;
    case ABSON_INT64:
    case ABSON_DATETIME:  return (double)b->v.i64;
    case ABSON_BOOLEAN:   return b->v.bval ? 1.0 : 0.0;
    default:              return default_value;
  }
}

static inline bool abson_to_bool(abson_t *b, bool default_value) {
  if (!b) return default_value;
  switch(b->type) {
    case ABSON_BOOLEAN:   return b->v.bval;
    case ABSON_INT32:     return b->v.i32 != 0;
    case ABSON_INT64:     return b->v.i64 != 0;
    case ABSON_DOUBLE:    return b->v.dbl != 0.0;
    default:              return default_value;
  }
}

static inline int      abson_to_int(abson_t *b, int default_value)           { return (int)abson_to_int32(b, default_value); }
static inline uint32_t abson_to_uint32(abson_t *b, uint32_t default_value)   { return (uint32_t)abson_to_int32(b, default_value); }
static inline uint64_t abson_to_uint64(abson_t *b, uint64_t default_value)   { return (uint64_t)abson_to_int64(b, default_value); }
static inline float    abson_to_float(abson_t *b, float default_value)       { return (float)abson_to_double(b, default_value); }

static inline char *abson_to_str(abson_t *b, const char *default_value) {
  return (b && b->type == ABSON_STRING) ? b->v.str : (char *)default_value;
}

static inline const uint8_t *abson_to_objectid(abson_t *b) {
  return (b && b->type == ABSON_OBJECTID) ? b->v.bin : NULL;
}

static inline int64_t abson_to_datetime(abson_t *b, int64_t default_value) {
  return (b && b->type == ABSON_DATETIME) ? b->v.i64 : default_value;
}

static inline const uint8_t *abson_to_binary(abson_t *b, uint8_t *out_subtype, uint32_t *out_length) {
  if (b && b->type == ABSON_BINARY) {
    if (out_subtype) *out_subtype = b->v.bin[0];
    if (out_length) *out_length = b->length;
    return b->v.bin + 1; // Assuming AST stores subtype as first byte
  }
  return NULL;
}

/* ===========================
 * Try Conversions
 * =========================== */

static inline bool abson_try_to_int64(abson_t *b, int64_t *out) {
  if (!b || !out) return false;
  if (b->type == ABSON_INT64 || b->type == ABSON_DATETIME || b->type == ABSON_TIMESTAMP) { *out = b->v.i64; return true; }
  if (b->type == ABSON_INT32)  { *out = (int64_t)b->v.i32; return true; }
  if (b->type == ABSON_DOUBLE) { *out = (int64_t)b->v.dbl; return true; }
  return false;
}

static inline bool abson_try_to_int32(abson_t *b, int32_t *out) {
  if (!b || !out) return false;
  if (b->type == ABSON_INT32)  { *out = b->v.i32; return true; }
  if (b->type == ABSON_INT64)  { *out = (int32_t)b->v.i64; return true; }
  if (b->type == ABSON_DOUBLE) { *out = (int32_t)b->v.dbl; return true; }
  return false;
}

static inline bool abson_try_to_double(abson_t *b, double *out) {
  if (!b || !out) return false;
  if (b->type == ABSON_DOUBLE) { *out = b->v.dbl; return true; }
  if (b->type == ABSON_INT32)  { *out = (double)b->v.i32; return true; }
  if (b->type == ABSON_INT64)  { *out = (double)b->v.i64; return true; }
  return false;
}

static inline bool abson_try_to_bool(abson_t *b, bool *out) {
  if (!b || !out) return false;
  if (b->type == ABSON_BOOLEAN) { *out = b->v.bval; return true; }
  return false;
}

static inline bool abson_try_to_int(abson_t *b, int *out)            { return abson_try_to_int32(b, (int32_t*)out); }
static inline bool abson_try_to_uint32(abson_t *b, uint32_t *out)    { return abson_try_to_int32(b, (int32_t*)out); }
static inline bool abson_try_to_uint64(abson_t *b, uint64_t *out)    { return abson_try_to_int64(b, (int64_t*)out); }
static inline bool abson_try_to_float(abson_t *b, float *out) {
  double d;
  if (abson_try_to_double(b, &d)) { *out = (float)d; return true; }
  return false;
}

/* ===========================
 * Arrays
 * =========================== */

static inline abson_t *absona(aml_pool_t *pool) {
  _absona_t *a = (_absona_t *)aml_pool_zalloc(pool, sizeof(_absona_t));
  a->type = ABSON_ARRAY;
  a->pool = pool;
  return (abson_t *)a;
}

static inline void _absona_fill(_absona_t *arr) {
  arr->array = (absona_t **)aml_pool_alloc(arr->pool, sizeof(absona_t *) * arr->num_entries);
  absona_t **awp = arr->array;
  absona_t *n = arr->head;
  while (n) { *awp++ = n; n = n->next; }
  arr->num_entries = awp - arr->array;
}

static inline abson_t *absona_nth(abson_t *b, int nth) {
  _absona_t *arr = (_absona_t *)b;
  if (nth < 0 || (uint32_t)nth >= arr->num_entries) return NULL;
  if (!arr->array) _absona_fill(arr);
  return arr->array[nth]->value;
}

static inline absona_t *absona_nth_node(abson_t *b, int nth) {
  _absona_t *arr = (_absona_t *)b;
  if (nth < 0 || (uint32_t)nth >= arr->num_entries) return NULL;
  if (!arr->array) _absona_fill(arr);
  return arr->array[nth];
}

static inline abson_t *absona_scan(abson_t *b, int nth) {
  _absona_t *arr = (_absona_t *)b;
  if (nth < 0 || (uint32_t)nth >= arr->num_entries) return NULL;
  if ((uint32_t)nth > (arr->num_entries >> 1)) {
    nth = arr->num_entries - nth - 1;
    absona_t *n = arr->tail;
    while (nth--) n = n->previous;
    return n->value;
  } else {
    absona_t *n = arr->head;
    while (nth--) n = n->next;
    return n->value;
  }
}

static inline void absona_append(abson_t *b, abson_t *item) {
  if (!b || !item || b->type != ABSON_ARRAY) return;
  _absona_t *arr = (_absona_t *)b;
  arr->array = NULL;
  absona_t *node = (absona_t *)aml_pool_zalloc(arr->pool, sizeof(*node));
  node->value = item;
  item->parent = b;
  if (arr->tail) {
    node->previous = arr->tail;
    arr->tail->next = node;
    arr->tail = node;
  } else {
    arr->head = arr->tail = node;
  }
  arr->num_entries++;
}

static inline int absona_count(abson_t *b) { return b ? ((_absona_t *)b)->num_entries : 0; }
static inline absona_t *absona_first(abson_t *b) { return b ? ((_absona_t *)b)->head : NULL; }
static inline absona_t *absona_last(abson_t *b) { return b ? ((_absona_t *)b)->tail : NULL; }
static inline absona_t *absona_next(absona_t *b) { return b->next; }
static inline absona_t *absona_previous(absona_t *b) { return b->previous; }

static inline void absona_erase(absona_t *n) {
  _absona_t *arr = (_absona_t *)(n->value->parent);
  arr->num_entries--;
  if (n->previous) n->previous->next = n->next;
  else arr->head = n->next;
  if (n->next) n->next->previous = n->previous;
  else arr->tail = n->previous;
  arr->array = NULL;
  n->next = n->previous = NULL;
  if (n->value) n->value->parent = NULL;
}

static inline void absona_clear(abson_t *b) {
  if (!b || b->type != ABSON_ARRAY) return;
  _absona_t *arr = (_absona_t *)b;
  for (absona_t *n = arr->head; n; ) {
    absona_t *next = n->next;
    if (n->value) n->value->parent = NULL;
    n->next = n->previous = NULL;
    n = next;
  }
  arr->head = arr->tail = NULL;
  arr->num_entries = 0;
  arr->array = NULL;
}

/* ===========================
 * Documents
 * =========================== */

static inline abson_t *absono(aml_pool_t *pool) {
  _absono_t *obj = (_absono_t *)aml_pool_zalloc(pool, sizeof(_absono_t));
  obj->type = ABSON_DOCUMENT;
  obj->pool = pool;
  return (abson_t *)obj;
}

static inline int absono_count(abson_t *b) { return b ? ((_absono_t *)b)->num_entries : 0; }
static inline absono_t *absono_first(abson_t *b) { return b ? ((_absono_t *)b)->head : NULL; }
static inline absono_t *absono_last(abson_t *b) { return b ? ((_absono_t *)b)->tail : NULL; }
static inline absono_t *absono_next(absono_t *b) { return b->next; }
static inline absono_t *absono_previous(absono_t *b) { return b->previous; }

void _absono_fill(_absono_t *o);

static inline absono_t *absono_get_node(abson_t *b, const char *key) {
  _absono_t *o = (_absono_t *)b;
  if (!o->root) {
    if (o->head) _absono_fill(o);
    else return NULL;
  }
  absono_t **res = __abson_search((char *)key, (const absono_t **)o->root, o->num_sorted_entries);
  return res ? *res : NULL;
}

static inline abson_t *absono_get(abson_t *b, const char *key) {
  _absono_t *o = (_absono_t *)b;
  if (!o->root || o->num_sorted_entries == 0) {
    if (o->head) _absono_fill(o);
    else return NULL;
  }
  absono_t **res = __abson_search(key, (const absono_t **)o->root, o->num_sorted_entries);
  return res ? (*res ? (*res)->value : NULL) : NULL;
}

static inline abson_t *absono_scan(abson_t *b, const char *key) {
  if (!b || b->type != ABSON_DOCUMENT) return NULL;
  _absono_t *o = (_absono_t *)b;
  for (absono_t *r = o->head; r; r = r->next) {
    if (!strcmp(r->key, key)) return r->value;
  }
  return NULL;
}

static inline abson_t *absono_scanr(abson_t *b, const char *key) {
  if (!b || b->type != ABSON_DOCUMENT) return NULL;
  _absono_t *o = (_absono_t *)b;
  for (absono_t *r = o->tail; r; r = r->previous) {
    if (!strcmp(r->key, key)) return r->value;
  }
  return NULL;
}

static inline void _absono_fill_tree(_absono_t *o) {
  o->root = NULL;
  o->num_sorted_entries = 0;
  for (absono_t *r = o->head; r; r = r->next) {
    __abson_insert(&(o->root), r);
  }
}

static inline absono_t *absono_find_node(abson_t *b, const char *key) {
  _absono_t *o = (_absono_t *)b;
  if (!o->root || o->num_sorted_entries) {
    if (o->head) _absono_fill_tree(o);
    else return NULL;
  }
  return __abson_find(o->root, key);
}

static inline abson_t *absono_find(abson_t *b, const char *key) {
  absono_t *r = absono_find_node(b, key);
  return r ? r->value : NULL;
}

static inline void absono_append(abson_t *b, const char *key, abson_t *item, bool copy_key) {
  if (!item) return;
  _absono_t *o = (_absono_t *)b;
  absono_t *on;
  if (copy_key) {
    on = (absono_t *)aml_pool_zalloc(o->pool, sizeof(absono_t) + strlen(key) + 1);
    on->key = (char *)(on + 1);
    strcpy(on->key, key);
  } else {
    on = (absono_t *)aml_pool_zalloc(o->pool, sizeof(absono_t));
    on->key = (char *)key;
  }
  on->value = item;
  item->parent = b;

  o->num_entries++;
  if (!o->head) {
    o->head = o->tail = on;
  } else {
    on->previous = o->tail;
    o->tail->next = on;
    o->tail = on;
  }
}

static inline absono_t *absono_insert(abson_t *b, const char *key, abson_t *item, bool copy_key) {
  if (!item) return NULL;
  absono_t *res = absono_find_node(b, key);
  if (res) {
    item->parent = b;
    res->value = item;
  } else {
    absono_append(b, key, item, copy_key);
    _absono_t *o = (_absono_t *)b;
    __abson_insert(&(o->root), o->tail);
  }
  return res;
}

static inline absono_t *absono_set(abson_t *b, const char *key, abson_t *item, bool copy_key) {
  if (!b || b->type != ABSON_DOCUMENT || !key || !item) return NULL;
  _absono_t *o = (_absono_t *)b;

  for (absono_t *n = o->head; n; n = n->next) {
    if (!strcmp(n->key, key)) {
      n->value = item;
      item->parent = b;
      return n;
    }
  }
  absono_append(b, key, item, copy_key);
  if (o->root) {
    if (o->num_sorted_entries) {
      o->root = NULL;
      o->num_sorted_entries = 0;
    } else {
      __abson_insert(&(o->root), o->tail);
    }
  }
  return o->tail;
}

static inline void absono_erase(absono_t *n) {
  _absono_t *o = (_absono_t *)(n->value->parent);
  o->num_entries--;
  if (o->root) {
    if (o->num_sorted_entries) {
      o->root = NULL;
      o->num_sorted_entries = 0;
    } else {
      macro_map_erase(&o->root, (macro_map_t *)n);
    }
  }
  if (n->previous) {
    n->previous->next = n->next;
    if (n->next) n->next->previous = n->previous;
    else o->tail = n->previous;
  } else {
    o->head = n->next;
    if (n->next) n->next->previous = NULL;
    else o->head = o->tail = NULL;
  }
}

static inline bool absono_remove(abson_t *b, const char *key) {
  if (!b || b->type != ABSON_DOCUMENT || !key) return false;
  _absono_t *o = (_absono_t *)b;
  for (absono_t *n = o->head; n; n = n->next) {
    if (!strcmp(n->key, key)) {
      absono_erase(n);
      return true;
    }
  }
  return false;
}

/* ===========================
 * Path Navigation
 * =========================== */

static inline abson_t *absono_path(aml_pool_t *pool, abson_t *b, const char *path) {
  size_t num_paths = 0;
  char **paths = aml_pool_split_with_escape2(pool, &num_paths, '.', '\\', path);
  for (size_t i = 0; i < num_paths; i++) {
    if (abson_is_array(b)) {
      char *value = strchr(paths[i], '=');
      if (value) {
        *value++ = '\0';
        abson_t *next = NULL;
        for (absona_t *iter = absona_first(b); iter; iter = absona_next(iter)) {
          char *v = abson_to_str(absono_scan(iter->value, paths[i]), NULL);
          if (v && !strcmp(v, value)) {
            next = iter->value;
            break;
          }
        }
        b = next;
      } else {
        size_t num = 0;
        if (sscanf(paths[i], "%zu", &num) != 1) return NULL;
        b = absona_scan(b, num);
      }
    } else {
      b = absono_scan(b, paths[i]);
    }
    if (!b) return NULL;
  }
  return b;
}

static inline char *absono_pathv(aml_pool_t *pool, abson_t *b, const char *path) {
  return abson_to_str(absono_path(pool, b, path), NULL);
}

/* ===========================
 * Document Try Macros
 * =========================== */

#define ABSONO_TRY_TYPED(API, CTYPE, CONVFN)                                  \
static inline bool absono_##API##_try_##CONVFN(abson_t *b, const char *key,   \
                                               CTYPE *out) {                  \
  if (!out) return false;                                                     \
  abson_t *n = absono_##API(b, key);                                          \
  if (!n) return false;                                                       \
  return abson_try_to_##CONVFN(n, out);                                       \
}

ABSONO_TRY_TYPED(scan, int,      int)
ABSONO_TRY_TYPED(get,  int,      int)
ABSONO_TRY_TYPED(find, int,      int)

ABSONO_TRY_TYPED(scan, int32_t,  int32)
ABSONO_TRY_TYPED(get,  int32_t,  int32)
ABSONO_TRY_TYPED(find, int32_t,  int32)

ABSONO_TRY_TYPED(scan, uint32_t, uint32)
ABSONO_TRY_TYPED(get,  uint32_t, uint32)
ABSONO_TRY_TYPED(find, uint32_t, uint32)

ABSONO_TRY_TYPED(scan, int64_t,  int64)
ABSONO_TRY_TYPED(get,  int64_t,  int64)
ABSONO_TRY_TYPED(find, int64_t,  int64)

ABSONO_TRY_TYPED(scan, uint64_t, uint64)
ABSONO_TRY_TYPED(get,  uint64_t, uint64)
ABSONO_TRY_TYPED(find, uint64_t, uint64)

ABSONO_TRY_TYPED(scan, float,    float)
ABSONO_TRY_TYPED(get,  float,    float)
ABSONO_TRY_TYPED(find, float,    float)

ABSONO_TRY_TYPED(scan, double,   double)
ABSONO_TRY_TYPED(get,  double,   double)
ABSONO_TRY_TYPED(find, double,   double)

ABSONO_TRY_TYPED(scan, bool,     bool)
ABSONO_TRY_TYPED(get,  bool,     bool)
ABSONO_TRY_TYPED(find, bool,     bool)

#define ABSONO_TRY_STR_COMMON(API)                                             \
static inline bool absono_##API##_try_str(abson_t *b, const char *key,         \
                                          const char *default_value, char **out) { \
  if (!out) return false;                                                      \
  abson_t *n = absono_##API(b, key);                                           \
  if (!n) { *out = (char *)default_value; return false; }                      \
  *out = abson_to_str(n, default_value);                                       \
  return true;                                                                 \
}

ABSONO_TRY_STR_COMMON(scan)
ABSONO_TRY_STR_COMMON(get)
ABSONO_TRY_STR_COMMON(find)

#undef ABSONO_TRY_STR_COMMON
#undef ABSONO_TRY_TYPED

/* ===========================
 * Document Extraction (Value)
 * =========================== */

#define ABSONO_VAL_TYPED(API, CTYPE, CONVFN)                                  \
static inline CTYPE absono_##API##_##CONVFN(abson_t *b, const char *key,      \
                                            CTYPE default_value) {            \
  return abson_to_##CONVFN(absono_##API(b, key), default_value);              \
}

ABSONO_VAL_TYPED(scan, int,      int)
ABSONO_VAL_TYPED(get,  int,      int)
ABSONO_VAL_TYPED(find, int,      int)

ABSONO_VAL_TYPED(scan, int32_t,  int32)
ABSONO_VAL_TYPED(get,  int32_t,  int32)
ABSONO_VAL_TYPED(find, int32_t,  int32)

ABSONO_VAL_TYPED(scan, uint32_t, uint32)
ABSONO_VAL_TYPED(get,  uint32_t, uint32)
ABSONO_VAL_TYPED(find, uint32_t, uint32)

ABSONO_VAL_TYPED(scan, int64_t,  int64)
ABSONO_VAL_TYPED(get,  int64_t,  int64)
ABSONO_VAL_TYPED(find, int64_t,  int64)

ABSONO_VAL_TYPED(scan, uint64_t, uint64)
ABSONO_VAL_TYPED(get,  uint64_t, uint64)
ABSONO_VAL_TYPED(find, uint64_t, uint64)

ABSONO_VAL_TYPED(scan, float,    float)
ABSONO_VAL_TYPED(get,  float,    float)
ABSONO_VAL_TYPED(find, float,    float)

ABSONO_VAL_TYPED(scan, double,   double)
ABSONO_VAL_TYPED(get,  double,   double)
ABSONO_VAL_TYPED(find, double,   double)

ABSONO_VAL_TYPED(scan, bool,     bool)
ABSONO_VAL_TYPED(get,  bool,     bool)
ABSONO_VAL_TYPED(find, bool,     bool)

#undef ABSONO_VAL_TYPED

static inline char *absono_scan_str(abson_t *b, const char *key, const char *default_value) {
  return abson_to_str(absono_scan(b, key), default_value);
}
static inline char *absono_get_str(abson_t *b, const char *key, const char *default_value) {
  return abson_to_str(absono_get(b, key), default_value);
}
static inline char *absono_find_str(abson_t *b, const char *key, const char *default_value) {
  return abson_to_str(absono_find(b, key), default_value);
}

/* ===========================
 * Basic Extraction Helpers
 * =========================== */

static inline char *abson_extract_string(abson_t *node) { return abson_to_str(node, ""); }
static inline int abson_extract_int(abson_t *node) { return abson_to_int(node, 0); }
static inline bool abson_extract_bool(abson_t *node) { return abson_to_bool(node, false); }
static inline uint32_t abson_extract_uint32(abson_t *node) { return abson_to_uint32(node, 0); }

static inline char **abson_extract_string_array(size_t *count, aml_pool_t *pool, abson_t *node) {
  if (!node) { *count = 0; return NULL; }
  if (!abson_is_array(node)) {
    char **result = (char **)aml_pool_alloc(pool, 2 * sizeof(char *));
    result[0] = abson_extract_string(node);
    result[1] = NULL;
    *count = 1;
    return result;
  }
  size_t n = absona_count(node);
  char **result = (char **)aml_pool_alloc(pool, (n + 1) * sizeof(char *));
  for (size_t i = 0; i < n; ++i) {
    result[i] = abson_extract_string(absona_nth(node, i));
  }
  *count = n;
  result[n] = NULL;
  return result;
}

static inline float *abson_extract_float_array(size_t *num, aml_pool_t *pool, abson_t *node) {
  if (!node || !abson_is_array(node)) { *num = 0; return NULL; }
  size_t num_f = absona_count(node);
  if (!num_f) { *num = 0; return NULL; }

  float *f = (float *)aml_pool_alloc(pool, sizeof(float) * num_f);
  absona_t *ev = absona_first(node);
  float *fp = f;
  while (ev) {
    *fp++ = abson_to_float(ev->value, 0.0);
    ev = absona_next(ev);
  }
  *num = num_f;
  return f;
}
