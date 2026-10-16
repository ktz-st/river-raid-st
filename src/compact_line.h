#ifndef RR_COMPACT_LINE_H
#define RR_COMPACT_LINE_H

#include <stdint.h>

#define RR_LINE_BYTES 48
#define RR_LINE_PIXELS 192
#define RR_LINE_RECORD_MAX 49
#define RR_LINE_DIRECT_MAX 97

/* Variable-length record: header + payload. Header bit 7 selects a raw
 * 48-byte row; otherwise bits 0..6 give the encoded payload length.
 * Direct generation may use up to RR_LINE_DIRECT_MAX bytes: it preserves
 * individual source operations rather than recompressing their fragments.
 * A fill token is (selector << 6) | byte_count (1..48). Each byte describes
 * four equal Mode E pixels. Token 0 introduces count + literal bytes, which
 * preserve sub-byte boundaries, island edges and decoration exactly.
 * Store only the returned length, not a fixed RR_LINE_RECORD_MAX per row.
 * Palette/DLI state belongs to the caller and is not part of this record. */
uint16_t RR_LineEncode(const uint8_t *row, uint8_t *record);

/* Returns 1 only for a complete valid row/record. On failure output may be
 * partially written, but never beyond RR_LINE_BYTES. Supports odd addresses;
 * source and destination must not overlap. No generator state is modified. */
uint16_t RR_LineDecode(const uint8_t *record, uint16_t size, uint8_t *row);

/* Fast 68000 expansion of a trusted, complete record. No validation.
 * Use RR_LineDecode for external/untrusted data. Supports odd addresses. */
void RR_LineExpand(const uint8_t *record, uint8_t *row);

#endif
