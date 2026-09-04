/*
 * ctoon Go binding bridge — batched (single-CGo-call) encode/decode.
 *
 * Every CGo call has a fixed, non-trivial cost (a goroutine/OS-thread
 * transition — measured at roughly 200x a native Go function call on
 * typical hardware). The original binding walked the ctoon_val /
 * ctoon_mut_val tree one CGo call per node, so a deeply nested document
 * paid that cost hundreds of times over.
 *
 * This bridge instead exchanges the whole tree in ONE CGo call, using a
 * small binary "tape" format that both sides can build/parse without any
 * further FFI crossings:
 *
 *   - Encode direction (Go value -> TOON/JSON): the Go side serialises its
 *     own value tree into a tape buffer in pure Go (no CGo calls at all),
 *     then makes ONE call into ctoon_go_bridge_encode(), which parses the
 *     tape into a ctoon_mut_val tree and writes the final TOON/JSON output
 *     — the whole walk-and-build happens natively in C.
 *
 *   - Decode direction (TOON/JSON -> Go value): ONE call into
 *     ctoon_go_bridge_decode() parses the input and walks the resulting
 *     ctoon_val tree into a tape buffer natively in C, which the Go side
 *     then decodes into native maps/slices/etc. in pure Go.
 *
 * Tape format (see binding.go's tape* constants — MUST match exactly):
 *   TAPE_NULL  = 0                                    (no payload)
 *   TAPE_FALSE = 1                                    (no payload)
 *   TAPE_TRUE  = 2                                    (no payload)
 *   TAPE_SINT  = 3   + 8 bytes little-endian int64
 *   TAPE_UINT  = 4   + 8 bytes little-endian uint64
 *   TAPE_REAL  = 5   + 8 bytes little-endian raw double bit pattern
 *   TAPE_STR   = 6   + 4 bytes little-endian uint32 length + N raw bytes
 *   TAPE_ARR   = 7   + 4 bytes little-endian uint32 count, then that many
 *                      recursively-encoded values
 *   TAPE_OBJ   = 8   + 4 bytes little-endian uint32 count, then that many
 *                      (4-byte length + N raw bytes key, no tag byte —
 *                      always a string in this position) + recursively-
 *                      encoded value pairs
 *
 * This is a private wire format between binding.go and bridge.c only — it
 * is never exposed, persisted, or sent across a process boundary, so there
 * is no versioning/compatibility concern beyond keeping the two files in
 * sync with each other.
 */

#ifndef CTOON_GO_BRIDGE_H
#define CTOON_GO_BRIDGE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Parses `tape` (as described above) into a ctoon_mut_val tree and writes
 * it out as TOON (as_json = 0) or JSON (as_json = 1).
 *
 * `delimiter` is only meaningful for TOON output (ctoon_delimiter values);
 * ignored for JSON.
 *
 * On success, returns 1 and sets *out_data/*out_len to a buffer the caller
 * must free with ctoon_go_bridge_free(). On failure, returns 0 and sets
 * *out_err to a heap-allocated message the caller must free the same way
 * (or NULL if no message is available).
 */
int ctoon_go_bridge_encode(
    const uint8_t *tape, size_t tape_len,
    int as_json, int indent, uint32_t write_flag, uint32_t delimiter,
    char **out_data, size_t *out_len,
    char **out_err
);

/*
 * Parses `data` as TOON (as_json = 0) or JSON (as_json = 1) and walks the
 * resulting document into a tape buffer (format above).
 *
 * On success, returns 1 and sets *out_tape/*out_tape_len to a buffer the
 * caller must free with ctoon_go_bridge_free(). On failure, returns 0 and
 * sets *out_err the same way as ctoon_go_bridge_encode().
 */
int ctoon_go_bridge_decode(
    const char *data, size_t len,
    int as_json, uint32_t read_flag,
    uint8_t **out_tape, size_t *out_tape_len,
    char **out_err
);

/* Frees any buffer returned by the two functions above. */
void ctoon_go_bridge_free(void *ptr);

#ifdef __cplusplus
}
#endif

#endif /* CTOON_GO_BRIDGE_H */
