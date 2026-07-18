# Overview of A BSON Library

ABSON is a highly efficient, zero-copy BSON processing library written in C. It provides a robust, AST-based API for parsing, constructing, and manipulating binary BSON (Binary JSON) documents. Built for extreme performance, ABSON parses native binary payloads directly into native C types (e.g., `int32`, `int64`, `double`) without intermediate string allocations. By tightly integrating with AML (A Memory Library), ABSON ensures minimal memory overhead while seamlessly supporting MongoDB-specific rich types like `ObjectId`, `DateTime`, and `Binary` data.

## Dependencies

* [A cmake library](https://github.com/knode-ai-open-source/a-cmake-library) needed for the cmake build
* [A memory library](https://github.com/knode-ai-open-source/a-memory-library) for the memory handling
* [The macro library](https://github.com/knode-ai-open-source/the-macro-library) for sorting, searching, and conversions

## Installation

### Clone the library and change to the directory

```bash
git clone [https://github.com/knode-ai-open-source/a-bson-library.git](https://github.com/knode-ai-open-source/a-bson-library.git)
cd a-bson-library

```

### Build and install library

```bash
mkdir -p build
cd build
cmake ..
make
make install

```

## An Example

```c
#include <stdio.h>
#include "a-bson-library/abson.h"
#include "a-memory-library/aml_pool.h"

int main() {
    // Assume `bson_payload` is a valid binary BSON buffer of size `payload_len`
    // representing: { "name": "John Doe", "age": 30, "is_active": true }
    extern const uint8_t *bson_payload;
    extern size_t payload_len;

    // Create a memory pool for efficient memory management
    aml_pool_t *pool = aml_pool_init(1024);

    // Parse the BSON payload (zero-copy: strings/binary point into the source payload)
    abson_t *doc = abson_parse(pool, bson_payload, payload_len);

    // Check for parsing errors or truncated data
    if (abson_is_error(doc) || !doc) {
        fprintf(stderr, "Error parsing BSON payload\n");
        aml_pool_destroy(pool);
        return 1;
    }

    // Access elements in the BSON document
    const char* name = absono_scan_str(doc, "name", "unknown");
    int32_t age = absono_scan_int32(doc, "age", -1);
    bool is_active = absono_scan_bool(doc, "is_active", false);

    // Print the values
    printf("Name: %s\n", name);
    printf("Age: %d\n", age);
    printf("Is Active: %s\n", is_active ? "true" : "false");

    // Clean up
    aml_pool_destroy(pool);
    return 0;
}

```

## Core Functions

* **abson_parse**: Parses a binary BSON payload into abson structures. Non-destructive (zero-copy).
* **abson_is_error**: Checks if the parsed BSON is marked as an error or malformed.

## Type Handling

* **abson_type**: Determines the exact type of a BSON node (e.g., `ABSON_INT32`, `ABSON_OBJECTID`, `ABSON_STRING`).
* **abson_is_document** / **abson_is_array**: Checks container types.
* **abson_is_string** / **abson_is_number** / **abson_is_bool**: Checks standard value types.
* **abson_is_objectid** / **abson_is_binary** / **abson_is_datetime**: Checks BSON-specific rich types.

## Output Functions

* **abson_dump**: Outputs the binary BSON structure directly to a `FILE *`.
* **abson_dump_to_buffer**: Appends the binary BSON structure to an `aml_buffer_t`.
* **abson_dump_to_memory**: Writes the binary BSON structure to a pre-allocated memory pointer.
* **abson_dump_estimate**: Calculates the exact byte size required to serialize the AST.
* **abson_stringify**: Compacts a BSON document as a single pool allocation.

## BSON Node Constructors

* **absono**: Creates a new empty BSON document.
* **absona**: Creates a new empty BSON array.
* **abson_int32** / **abson_int64**: Creates a native integer node.
* **abson_double**: Creates a native floating-point node.
* **abson_str** / **abson_string**: Creates a BSON string node.
* **abson_true** / **abson_false** / **abson_bool**: Creates a BSON boolean node.
* **abson_null**: Creates a BSON null node.
* **abson_objectid**: Creates a 12-byte ObjectId node.
* **abson_datetime**: Creates a 64-bit UTC DateTime node.
* **abson_binary**: Creates a BSON binary node with a specified subtype.

## BSON Array Functions

* **absona_count**: Counts elements in a BSON array.
* **absona_append**: Appends a node to the end of a BSON array.
* **absona_scan**: Retrieves an element from a BSON array using a linear scan.
* **absona_nth**: Retrieves an element from a BSON array using a direct-access index.
* **absona_clear**: Unlinks all items from a BSON array.

## BSON Object Access

* **absono_append**: Appends a key-value pair to a BSON document quickly without checking for duplicates.
* **absono_scan**: Retrieves a BSON node by key using a linear scan.
* **absono_get**: Retrieves a BSON node by key. Internally builds a sorted-array snapshot on the first call for fast repeated access.
* **absono_find**: Retrieves a BSON node by key. Internally converts the object into an RB-tree map on the first call.
* **absono_insert**: Inserts a BSON node by key using the RB-tree map.
* **absono_set**: Replaces the first matching key or appends if missing.
* **absono_remove**: Removes a specific key from the document.

## BSON Path Functions

* **absono_path**: Retrieves a BSON node using a dot-separated path (e.g., `user.address.0.city`).
* **absono_pathv**: Retrieves a string value directly from a JSON path.

## Conversion Functions

* **abson_to_int32** / **abson_to_uint32**: Safely extracts a 32-bit integer, casting from other numeric types if necessary.
* **abson_to_int64** / **abson_to_uint64**: Safely extracts a 64-bit integer.
* **abson_to_double** / **abson_to_float**: Safely extracts floating-point values.
* **abson_to_bool**: Converts a node to a boolean value.
* **abson_to_str**: Returns the string pointer for a string node.
* **abson_to_objectid**: Returns a pointer to the 12-byte ObjectId payload.
* **abson_to_datetime**: Returns the 64-bit Unix epoch millisecond timestamp.
* **abson_to_binary**: Returns a pointer to the binary payload and optionally fills the subtype and length.

### Try-Conversion Variants

Functions like `abson_try_to_int32` and `abson_try_to_double` allow for strict type extraction without relying on default fallbacks, returning `true` only if extraction succeeds.

## Object Helper Functions

These helpers combine document access (scan, get, find) with type extraction in a single call.

* **absono_scan_int32**: Scans for a key and converts the value to a 32-bit integer.
* **absono_scan_int64**: Scans for a key and converts the value to a 64-bit integer.
* **absono_scan_double**: Scans for a key and converts the value to a double.
* **absono_scan_bool**: Scans for a key and converts the value to a boolean.
* **absono_scan_str**: Scans for a key and returns the string value.

The same helper variants exist for snapshot (`get`) and tree-based (`find`) lookups:

* **absono_get_int64**: Looks up a key via the sorted-array snapshot and extracts a 64-bit int.
* **absono_find_double**: Looks up a key via the RB-tree map and extracts a double.
