"""Audited generator data ranges, packed in RRGeneratorTables field order.

Indexed tables retain their original contiguous boundaries: in particular
BBEB+{5,6,8,10} deliberately reads into the following pointer list.
"""
RANGES = (
    ('bank_fragments', 0xb800, 32),
    ('decoration_rows', 0xbc06, 0x2e8),
    ('tank_patterns', 0xb7f4, 10),
    ('edge_delta', 0xba8f, 32),
    ('actor_classes', 0xbb1c, 8),
    ('actor_heights', 0xbb60, 16),
    ('spawn_modifiers', 0xbba2, 16),
    ('decoration_widths', 0xbbb2, 26),
    ('decoration_classes', 0xbbcc, 8),
    ('decoration_offsets', 0xbbd4, 23),
    ('decoration_lifetimes', 0xbbeb, 5),
    ('decoration_pointers_le', 0xbbf0, 22),
    ('tank_positions', 0xbfe4, 14),
)

def packed_offset(address):
    offset = 0
    for name, start, size in RANGES:
        if start <= address < start+size: return offset+address-start
        offset += size
    raise ValueError('No retained generator data at $%04x' % address)
