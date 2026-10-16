"""Statically translate the isolated A711 ROM control-flow graph into C.

This deliberately preserves 6502 flags and byte arithmetic. It is a faithful
first-stage C translation, not an idiomatic rewrite or a runtime CPU emulator.
Every reachable instruction becomes a fixed C statement and named goto.
"""
import json
from build_paths import ASSETS, GENERATED
ROM = ASSETS / 'RIVER_A8.BIN'
OPS = {}


def group(name, modes, codes):
    for mode, code in zip(modes.split(), codes.split()):
        OPS[int(code, 16)] = (name, mode)


for name, base in [('ORA', 0x00), ('AND', 0x20), ('EOR', 0x40),
                   ('ADC', 0x60), ('STA', 0x80), ('LDA', 0xa0),
                   ('CMP', 0xc0), ('SBC', 0xe0)]:
    for mode, offset in zip('ix zp imm abs iy zpx absy absx'.split(),
                            (1, 5, 9, 13, 17, 21, 25, 29)):
        if name != 'STA' or mode != 'imm':
            OPS[base + offset] = (name, mode)
for name, codes in [('ASL', '0a 06 16 0e 1e'), ('ROL', '2a 26 36 2e 3e'),
                    ('LSR', '4a 46 56 4e 5e'), ('ROR', '6a 66 76 6e 7e')]:
    group(name, 'acc zp zpx abs absx', codes)
group('STX', 'zp zpy abs', '86 96 8e')
group('STY', 'zp zpx abs', '84 94 8c')
group('LDX', 'imm zp zpy abs absy', 'a2 a6 b6 ae be')
group('LDY', 'imm zp zpx abs absx', 'a0 a4 b4 ac bc')
group('DEC', 'zp zpx abs absx', 'c6 d6 ce de')
group('INC', 'zp zpx abs absx', 'e6 f6 ee fe')
group('BIT', 'zp abs', '24 2c')
group('CPX', 'imm zp abs', 'e0 e4 ec')
group('CPY', 'imm zp abs', 'c0 c4 cc')
group('JMP', 'abs ind', '4c 6c')
group('JSR', 'abs', '20')
for code, name in [(0x10, 'BPL'), (0x30, 'BMI'), (0x50, 'BVC'), (0x70, 'BVS'),
                   (0x90, 'BCC'), (0xb0, 'BCS'), (0xd0, 'BNE'), (0xf0, 'BEQ')]:
    OPS[code] = (name, 'rel')
for code, name in [(0x60, 'RTS'), (0x48, 'PHA'), (0x68, 'PLA'),
                   (0x18, 'CLC'), (0x38, 'SEC'), (0xb8, 'CLV'),
                   (0x88, 'DEY'), (0xc8, 'INY'), (0xca, 'DEX'), (0xe8, 'INX'),
                   (0xaa, 'TAX'), (0xa8, 'TAY'), (0x8a, 'TXA'), (0x98, 'TYA')]:
    OPS[code] = (name, 'impl')
LENGTH = {mode: 1 if mode in ('acc', 'impl') else
          3 if mode in ('abs', 'absx', 'absy', 'ind') else 2
          for _, mode in OPS.values()}


def decode(rom, pc):
    name, mode = OPS[rom[pc - 0xa000]]
    length = LENGTH[mode]
    operand = int.from_bytes(rom[pc-0xa000+1:pc-0xa000+length], 'little')
    return name, mode, operand, pc + length


def reachable(rom):
    pending = [0xa711]
    found, returns = {}, set()
    while pending:
        pc = pending.pop()
        if pc in found:
            continue
        assert 0xa000 <= pc < 0xc000, hex(pc)
        instruction = decode(rom, pc)
        found[pc] = instruction
        name, mode, operand, end = instruction
        if name == 'JMP':
            assert mode == 'abs', 'Indirect control flow needs manual analysis'
            pending.append(operand)
        elif name == 'JSR':
            returns.add(end)
            pending.extend((operand, end))
        elif mode == 'rel':
            pending.extend((end, end + (operand if operand < 128 else operand-256)))
        elif name != 'RTS':
            pending.append(end)
    return found, returns


def address(mode, operand):
    v = f'0x{operand:04x}'
    if mode in ('zp', 'abs'):
        return v
    if mode in ('zpx', 'zpy'):
        return f'(uint8_t)({v} + {mode[-1]})'
    if mode in ('absx', 'absy'):
        return f'(uint16_t)({v} + {mode[-1]})'
    if mode == 'ix':
        return f'zp_word(g, (uint8_t)({v} + x))'
    if mode == 'iy':
        return f'(uint16_t)(zp_word(g, {v}) + y)'
    raise ValueError(mode)


def statement(name, mode, operand, end):
    value = f'0x{operand:02x}' if mode == 'imm' else 'a' if mode == 'acc' else (
        f'm[{address(mode, operand)}]' if mode not in ('impl', 'rel') else '')
    if name in ('LDA', 'LDX', 'LDY'):
        r = name[-1].lower()
        return f'{r} = {value}; NZ({r});'
    if name in ('STA', 'STX', 'STY'):
        return f'{value} = {name[-1].lower()};'
    if name in ('AND', 'ORA', 'EOR'):
        op = {'AND': '&', 'ORA': '|', 'EOR': '^'}[name]
        return f'a {op}= {value}; NZ(a);'
    if name in ('ADC', 'SBC'):
        rhs = value if name == 'ADC' else f'((uint8_t)~({value}))'
        return (f't = {rhs}; w = (unsigned)a + t + c; '
                'v = (~(a ^ t) & (a ^ w) & 128) != 0; '
                'c = w > 255; a = (uint8_t)w; NZ(a);')
    if name in ('CMP', 'CPX', 'CPY'):
        r = {'CMP': 'a', 'CPX': 'x', 'CPY': 'y'}[name]
        return f't = {value}; c = {r} >= t; NZ((uint8_t)({r} - t));'
    if name == 'BIT':
        return f't = {value}; z = (a & t) == 0; n = (t & 128) != 0; v = (t & 64) != 0;'
    if name in ('ASL', 'LSR', 'ROL', 'ROR'):
        if name in ('ASL', 'ROL'):
            expr = '(t << 1)' + (' | c' if name == 'ROL' else '')
            carry = '(t & 128) != 0'
        else:
            expr = '(t >> 1)' + (' | (c << 7)' if name == 'ROR' else '')
            carry = 't & 1'
        return f't = {value}; {value} = (uint8_t)({expr}); c = {carry}; NZ({value});'
    if name in ('INC', 'DEC'):
        return f'{value} = (uint8_t)({value} {"+" if name == "INC" else "-"} 1); NZ({value});'
    if name in ('INX', 'INY', 'DEX', 'DEY'):
        r = name[-1].lower()
        return f'{r} = (uint8_t)({r} {"+" if name.startswith("IN") else "-"} 1); NZ({r});'
    if name in ('TAX', 'TAY', 'TXA', 'TYA'):
        src, dst = name[1].lower(), name[2].lower()
        return f'{dst} = {src}; NZ({dst});'
    if name in ('SEC', 'CLC', 'CLV'):
        return {'SEC': 'c = 1;', 'CLC': 'c = 0;', 'CLV': 'v = 0;'}[name]
    if name == 'PHA':
        return 'push(g, a);'
    if name == 'PLA':
        return 'a = pop(g); NZ(a);'
    if name == 'JMP':
        return f'goto L{operand:04x};'
    if name == 'JSR':
        return f'push(g, 0x{(end-1)>>8:02x}); push(g, 0x{(end-1)&255:02x}); goto L{operand:04x};'
    if name == 'RTS':
        return 'goto return_from_subroutine;'
    if mode == 'rel':
        dest = end + (operand if operand < 128 else operand - 256)
        cond = {'BEQ': 'z', 'BNE': '!z', 'BMI': 'n', 'BPL': '!n',
                'BCS': 'c', 'BCC': '!c', 'BVS': 'v', 'BVC': '!v'}[name]
        return f'if ({cond}) goto L{dest:04x};'
    raise ValueError((name, mode))


def build_core(actors=False):
    rom = bytearray(ROM.read_bytes())
    assert rom[0xe94:0xe99] == bytes.fromhex('c6 32 4c 01 a7')
    if not actors:
        rom[0xe94] = 0x60
    rom[0xe96] = 0x60
    found, returns = reachable(rom)
    lines = ['/* Generated by lift_generator.py. Original ROM addresses are labels. */',
             'int a8_core_A711'+('_actors' if actors else '')+'(A8Generator *g)', '{',
             '    uint8_t *m = g->memory;',
             '    uint8_t a = g->a, x = g->x, y = g->y, t;',
             '    unsigned c = 0, z = 0, n = 0, v = 0, w;',
             '    uint16_t ret;',
             '    push(g, 0xff); push(g, 0xff);',
             '    goto La711;']
    for pc, instruction in sorted(found.items()):
        name, mode, operand, end = instruction
        lines.append(f'L{pc:04x}: /* {name} {mode} ${operand:04x} */')
        if pc == 0xad9d:
            # Observe the newly inserted actor without changing original CPU state.
            lines.extend(('    g->spawned = 1; g->spawn_type = m[0x50b+x];',
                          '    g->spawn_frame = m[0x500+x]; g->spawn_flags = m[0x42+x];',
                          '    g->spawn_y = m[0x0c+x]; g->spawn_x = m[0x516+x];',
                          '    g->spawn_size = m[0x521+x];'))
        if pc == 0xadca:
            lines.append('    g->tank_spawned = 1; g->tank_frame = m[0x38]; g->tank_x = m[0x39]; g->tank_y = m[0x3a];')
        if pc == 0xae8e:
            lines.append('    g->tank_spawned = 1; g->tank_frame = m[0x38]; g->tank_x = m[0x39]; g->tank_y = m[0x3a];')
        lines.append('    '+statement(*instruction))
        if name not in ('JMP', 'JSR', 'RTS'):
            lines.append(f'    goto L{end:04x};')
    lines.extend(('return_from_subroutine:',
                  '    ret = pop(g); ret |= (uint16_t)pop(g) << 8;',
                  '    ret = (uint16_t)(ret + 1);',
                  '    switch (ret) {',
                  '    case 0: g->a = a; g->x = x; g->y = y; return 1;'))
    lines.extend(f'    case 0x{ret:04x}: goto L{ret:04x};' for ret in sorted(returns))
    lines.extend(('    default: return 0;', '    }', '}'))
    suffix = '_actors' if actors else ''
    (GENERATED/('generator_core'+suffix+'.inc')).write_text('\n'.join(lines)+'\n')
    (GENERATED/('generator_core'+suffix+'.json')).write_text(json.dumps({
        'entry': 'A711', 'instructions': len(found),
        'subroutine_return_addresses': [f'{v:04X}' for v in sorted(returns)],
        'scheduler_exit_patches': ['AE96=RTS'] if actors else ['AE94=RTS', 'AE96=RTS'],
        'hardware_reads': [f'{pc:04X}' for pc, (_, mode, operand, _) in found.items()
                           if mode.startswith('abs') and 0xd000 <= operand <= 0xd7ff]
    }, indent=2)+'\n')
    print(f'Translated {len(found)} reachable ROM instructions into static C')


def main():
    build_core()
    build_core(True)


if __name__ == '__main__':
    main()
