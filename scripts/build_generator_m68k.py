"""Translate the A711 ROM graph directly to fixed 68000 code (no runtime decoder)."""
import argparse
import hashlib
import importlib.util
import re
from rom_data_layout import RANGES, packed_offset
from build_paths import ROOT, GENERATED

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compact', action='store_true', help='Emit row commands directly instead of A8 bitmap writes')
parser.add_argument('--sparse', action='store_true', help='Keep low RAM and ROM, without ANTIC/LMS bookkeeping')
parser.add_argument('--test-labels', action='store_true', help='Export fragment entry points in a separate test assembly')
parser.add_argument('--reference-calls', action='store_true', help='Keep the old virtual 6502 return dispatcher for comparison')
args = parser.parse_args()
native_calls = args.sparse and not args.reference_calls
in_line = False
assert not args.sparse or args.compact
assert not args.test_labels or args.sparse
SOURCE = ROOT/'scripts/lift_generator.py'
spec = importlib.util.spec_from_file_location('rom6502', SOURCE)
rom6502 = importlib.util.module_from_spec(spec)
spec.loader.exec_module(rom6502)
rom = bytearray(rom6502.ROM.read_bytes())
assert rom[0xe94:0xe99] == bytes.fromhex('c6 32 4c 01 a7')
rom[0xe96] = 0x60
graph, returns = rom6502.reachable(rom)
if args.sparse:
    # ANTIC row addressing and display-list patches cannot affect row contents.
    # Keep B4 (the source-column accumulator), but not the bitmap destination.
    redirects = {0xa9d7: 0xaa7a, 0xaa7e: 0xaa88, 0xb04c: 0xb055}
    removed = ((0xa9d8, 0xaa7a), (0xaa7f, 0xaa88),
               (0xb04d, 0xb055), (0xb0c7, 0xb0e9))
    graph = {pc: instruction for pc, instruction in graph.items()
             if not any(lo <= pc < hi for lo, hi in removed)}
    for pc, target in redirects.items(): graph[pc] = ('JMP', 'abs', target, pc+3)
    returns = {end for name, mode, operand, end in graph.values() if name == 'JSR'}

if native_calls:
    # Production checkpoints seed $100..$4ff once. Keep this a build invariant,
    # not an assumption that silently breaks when a ROM/backend changes.
    stores={'STA','STX','STY','INC','DEC','ASL','LSR','ROL','ROR'}
    for pc,(name,mode,operand,end) in graph.items():
        if pc==0xafff or 0xb01f<=pc<0xb024 or 0xb041<=pc<0xb049: continue
        if name not in stores or mode in ('acc','zp','zpx','zpy'): continue
        assert mode not in ('iy','ix'),('unclassified indirect write',hex(pc))
        extent=255 if mode in ('absx','absy') else 0
        assert operand>0x4ff or operand+extent<0x100,('immutable checkpoint page write',hex(pc))

# Match each native subroutine's exits to its actual call continuations.
# Following JSR's continuation here (not its callee) handles shared tail code.
return_edges = {}
if native_calls:
    for caller,(name,mode,target,end) in graph.items():
        if name != 'JSR': continue
        pending,visited = [target],set()
        while pending:
            node=pending.pop()
            if node in visited: continue
            visited.add(node)
            op,kind,value,after=graph[node]
            if op=='RTS' or node==0xae94:
                return_edges.setdefault(node,set()).add(end)
                if op=='RTS': continue
            if op=='JMP': pending.append(value)
            elif kind=='rel': pending.extend((after,after+(value if value<128 else value-256)))
            else: pending.append(after)

# Conservative flag liveness; the reference retains the old all-RTS graph.
# Stores, hooks, address calculations and the native return dispatcher do not
# change the saved 6502 flags. Avoid capturing values killed before their use.
reads, writes, successors = {}, {}, {}
for pc, (name, mode, operand, end) in graph.items():
    reads[pc] = set()
    writes[pc] = set()
    if name in ('LDA','LDX','LDY','AND','ORA','EOR','ADC','SBC','CMP','CPX','CPY',
                'BIT','ASL','LSR','ROL','ROR','INC','DEC','INX','INY','DEX','DEY',
                'TAX','TAY','TXA','TYA','PLA'):
        writes[pc] |= {'N','Z'}
    if name in ('ADC','SBC','CMP','CPX','CPY','ASL','LSR','ROL','ROR','SEC','CLC'):
        writes[pc].add('C')
    if name in ('ADC','SBC','BIT','CLV'): writes[pc].add('V')
    if name in ('ADC','SBC','ROL','ROR'): reads[pc].add('C')
    if mode == 'rel':
        reads[pc].add({'BEQ':'Z','BNE':'Z','BMI':'N','BPL':'N',
                      'BCS':'C','BCC':'C','BVS':'V','BVC':'V'}[name])
        successors[pc] = {end,end+(operand if operand<128 else operand-256)}
    elif name in ('JSR','JMP'): successors[pc] = {operand}
    elif name == 'RTS': successors[pc] = return_edges.get(pc,set()) if native_calls else returns
    else: successors[pc] = {end}
    if pc == 0xae94: successors[pc] = successors[pc] | (return_edges.get(pc,set()) if native_calls else returns)
live_in = {pc:set() for pc in graph}
live_out = {pc:set() for pc in graph}
changed = True
while changed:
    changed = False
    for pc in reversed(sorted(graph)):
        after = set().union(*(live_in[n] for n in successors[pc]))
        before = reads[pc] | (after-writes[pc])
        if before != live_in[pc]: changed = True
        live_in[pc],live_out[pc] = before,after

folded_nz = {}
if native_calls:
    incoming = {pc:set() for pc in graph}
    for pc,targets in successors.items():
        for target in targets: incoming[target].add(pc)
    nz_writers = {'LDA','LDX','LDY','AND','ORA','EOR','ADC','SBC','CMP','CPX','CPY',
                  'ASL','LSR','ROL','ROR','INC','DEC','INX','INY','DEX','DEY',
                  'TAX','TAY','TXA','TYA','PLA'}
    for branch,(name,mode,operand,end) in graph.items():
        if name not in ('BEQ','BNE','BMI','BPL') or len(incoming[branch])!=1: continue
        producer=next(iter(incoming[branch]))
        op,_,_,after=graph[producer]
        if after==branch and op in nz_writers and not 0xaff0<=producer<0xb060:
            folded_nz[branch]=producer
dead_nz_snapshots={producer for branch,producer in folded_nz.items()
                   if not live_out[branch]&{'N','Z'}}

# a4 = mapped low RAM, a5 = native unwind frame (reference: metadata), a6 = ROM base.
# Full-memory baseline instead uses a6 = RAM+$8000.
# d0/d1/d2 = A/X/Y (upper bytes zero), d4 bit0 = carry,
# d5 = saved 68000 NZ bits, d6 bit 6 = 6502 overflow; d3/d7/a0 = scratch.
line_api = 'a8_generator_compact_line' if args.compact else 'a8_generator_line'
init_api = 'a8_generator_compact_init' if args.compact else 'a8_generator_init'
lines = ['; Generated directly from River Raid 6502 ROM by build_generator_m68k.py.',
         '; ROM SHA256: '+hashlib.sha256(rom6502.ROM.read_bytes()).hexdigest(),
         '; Original PC labels, byte wrap and little-endian data pointers preserved.',
         '; Native BSR/RTS/PHA/PLA; A8 stack page is dead scratch.' if native_calls else
         '; Reference backend preserves virtual A8 stack and return dispatcher.',
         '\txdef '+line_api, '\txdef '+init_api, '\tsection text']
if args.test_labels:
    lines += ['\txdef rr_b01d,rr_b024,rr_b03f,rr_b049,rr_unmapped,rr_afff,rr_fail']
if args.sparse: lines += ['\txref rrGeneratorTables']

def emit(*code):
    if native_calls and in_line:
        # A5 is the native unwind frame; metadata remains in the same RAM bytes.
        code = tuple(re.sub(r'(\d*)\(a5\)',
                    lambda m: f'${0x600+int(m[1] or 0):x}(a4)', c) for c in code)
    lines.extend('\t'+c for c in code)

def label(name):
    lines.append(name+':')

def direct(address):
    if args.sparse:
        if address < 0x600: return f'${address:x}(a4)'
        assert 0xa000 <= address < 0xc000, hex(address)
        return f'${packed_offset(address):x}(a6)'
    return f'${address:x}(a4)' if address < 32768 else f'{address-32768}(a6)'

def indirect():
    if args.sparse: emit('bsr.w rr_map_address')
    else: emit('movea.l a4,a0', 'adda.l d7,a0')

def ea(mode, operand):
    if mode == 'imm': return f'#${operand:02x}'
    if mode == 'acc': return 'd0'
    if mode in ('zp', 'abs'): return direct(operand)
    if mode in ('zpx', 'zpy'):
        emit('moveq #0,d7', f'move.b d{1 if mode[-1]=="x" else 2},d7',
             f'addi.b #${operand:02x},d7')
        return '(a4,d7.w)'
    if mode in ('absx', 'absy'):
        index = 'd1' if mode[-1] == 'x' else 'd2'
        if operand <= 0xff00:
            emit(f'lea {direct(operand)},a0')
            return f'(a0,{index}.w)'
        emit('moveq #0,d7', f'move.w #${operand:04x},d7', f'add.w {index},d7',
             'andi.l #$ffff,d7')
        indirect()
        return '(a0)'
    if mode == 'iy':
        emit('moveq #0,d7', f'move.b {direct((operand+1)&255)},d7',
             'lsl.w #8,d7', f'move.b {direct(operand)},d7', 'add.w d2,d7',
             'andi.l #$ffff,d7')
        indirect()
        return '(a0)'
    if mode == 'ix':
        emit('moveq #0,d7', 'move.b d1,d7', f'addi.b #${operand:02x},d7',
             'moveq #0,d3', 'move.b (a4,d7.w),d3', 'addq.b #1,d7',
             'move.b (a4,d7.w),d7', 'lsl.w #8,d7', 'or.w d3,d7',
             'andi.l #$ffff,d7')
        indirect()
        return '(a0)'
    raise ValueError(mode)

def nz():
    if live_out[pc] & {'N','Z'} and pc not in dead_nz_snapshots: emit('move.w sr,d5')

def carry(inverse=False):
    if 'C' in live_out[pc]:
        emit(('scc' if inverse else 'scs')+' d4')
        if not native_calls: emit('neg.b d4')

def push(value):
    emit('moveq #0,d7', 'move.b 3(a5),d7', 'lea $100(a4),a0',
         f'move.b {value},(a0,d7.w)', 'subq.b #1,3(a5)')

def pop(dst):
    emit('addq.b #1,3(a5)', 'moveq #0,d7', 'move.b 3(a5),d7',
         'lea $100(a4),a0', f'move.b (a0,d7.w),{dst}')

# GCC ELF -mshort -mfastcall: a0/a1, third pointer at 4(sp), int result d0.w.
label(init_api)
state_offset = 0x600 if args.sparse else 65536
state_longs = (state_offset+20)//4
emit('movem.l d2-d7/a2-a6,-(sp)', 'movea.l 48(sp),a2', 'movea.l a0,a6', 'movea.l a0,a4',
     'movea.l a1,a3', 'moveq #0,d0', f'move.w #{state_longs-1},d7')
label('rr_init_clear')
emit('move.l d0,(a4)+', 'dbra d7,rr_init_clear', 'movea.l a6,a4',
     'move.w #$600,d0', 'bsr.w rr_init_copy')
if not args.sparse:
    emit('lea $2000(a6),a4', 'lea $1a00(a3),a3',
         'move.w #$2000,d0', 'bsr.w rr_init_copy',
         'movea.l a6,a4', 'adda.l #$a000,a4')
    emit('movea.l a2,a3', 'move.w #$2000,d0', 'bsr.w rr_init_copy',
         'movea.l a6,a0', 'adda.l #$a000,a0',
         'move.b #$60,$e94(a0)', 'move.b #$60,$e96(a0)')
emit('movea.l a6,a5', f'adda.l #{state_offset},a5',
     'move.b #$ff,1(a5)', 'move.b #$ff,3(a5)',
     'movem.l (sp)+,d2-d7/a2-a6', 'rts')
label('rr_init_copy')
emit('move.l a3,d7', 'move.l a4,d3', 'or.l d3,d7', 'btst #0,d7',
     'bne.w rr_init_bytes', 'lsr.w #2,d0', 'subq.w #1,d0')
label('rr_init_longs')
emit('move.l (a3)+,(a4)+', 'dbra d0,rr_init_longs', 'rts')
label('rr_init_bytes')
emit('subq.w #1,d0')
label('rr_init_byte_loop')
emit('move.b (a3)+,(a4)+', 'dbra d0,rr_init_byte_loop', 'rts')

label(line_api)
in_line = True
emit('movem.l d2-d7/a2-a6,-(sp)', 'movea.l a0,a4', 'movea.l a1,a3',
     *(['movea.l sp,a5'] if native_calls else ['movea.l a4,a5', f'adda.l #{state_offset},a5']))
if args.sparse: emit('lea rrGeneratorTables,a6')
else: emit('movea.l a4,a6', 'adda.l #32768,a6')
emit(
     'clr.b 8(a5)', 'clr.b 16(a5)', 'tst.b 15(a5)', 'beq.w rr_no_retire',
     'cmpi.b #$bc,$3a(a4)', 'bcs.w rr_no_retire', 'clr.b $38(a4)', 'clr.b $3a(a4)')
label('rr_no_retire')
if args.compact:
    # a3 is the record base, a2 the command cursor; neither is used by the ROM graph.
    emit(*(['move.b #48,(a3)'] if native_calls else ['subq.l #2,sp', 'move.w #48,(sp)']),
         'lea 1(a3),a2', 'lea 97(a3),a1')
emit('moveq #0,d0', 'moveq #0,d1', 'moveq #0,d2', 'moveq #0,d4',
     'moveq #0,d5', 'moveq #0,d6', 'move.b (a5),d0', 'move.b 1(a5),d1',
     'move.b 2(a5),d2')
if native_calls:
    emit('bsr.w rr_a711', 'bra.w rr_core_done')
else:
    push('#$ff'); push('#$ff')
    emit('bra.w rr_a711')

remaining = '(a3)' if native_calls else '(sp)'
remaining_size = 'b' if native_calls else 'w'

for pc, instruction in sorted(graph.items()):
    if args.sparse and (0xb01f <= pc < 0xb024 or 0xb041 <= pc < 0xb049):
        # Unreachable bitmap stores replaced by the compact literal emitters.
        continue
    name, mode, operand, end = instruction
    label(f'rr_{pc:04x}')
    lines.append(f'; ${pc:04X}: {name} {mode} ${operand:04X}')
    if pc == 0xad9d:
        emit('move.b #1,8(a5)')
        for addr, off in ((0x50b,9),(0x500,10),(0x42,11),(0xc,12),(0x516,13),(0x521,14)):
            emit(f'lea {direct(addr)},a0', 'move.b (a0,d1.w),d3', f'move.b d3,{off}(a5)')
    if pc in (0xadca,0xae8e):
        emit('move.b #1,16(a5)')
        for addr, off in ((0x38,17),(0x39,18),(0x3a,19)):
            emit(f'move.b {direct(addr)},d3', f'move.b d3,{off}(a5)')
    if pc == 0xae94:
        emit('tst.b 15(a5)', 'beq.w rr_return')
    if args.compact and pc == 0xafff:
        # Replace the descending fill loop, retaining its Y=-1 result and
        # original B004/B04B pointer/stack/flag side effects.
        emit('move.w d2,d7', 'addq.w #1,d7')
        if native_calls: emit('tst.b d7', 'beq.w rr_fail') # Y=FF means 256, not a zero-byte span.
        emit(f'cmp.{remaining_size} {remaining},d7',
             'bhi.w rr_fail', f'sub.{remaining_size} d7,{remaining}', 'cmpa.l a1,a2', 'bcc.w rr_fail',
             'move.b d0,d3',
             'andi.b #3,d3', 'lsl.b #6,d3', 'or.b d7,d3',
             'move.b d3,(a2)+', 'move.b #$ff,d2', 'bra.w rr_b004')
        continue
    if args.compact and pc == 0xb01d:
        # Copy the literal source directly to the command. Sources are not
        # assumed aligned or in ROM; preserve 16-bit source wrapping.
        emit('moveq #0,d3', 'move.w d2,d3', 'addq.w #1,d3')
        if native_calls: emit('tst.b d3', 'beq.w rr_fail')
        emit(f'cmp.{remaining_size} {remaining},d3', 'bhi.w rr_fail', f'sub.{remaining_size} d3,{remaining}',
             'move.l a2,d7', 'add.l d3,d7', 'addq.l #2,d7',
             'cmp.l a1,d7', 'bhi.w rr_fail',
             'clr.b (a2)+', 'move.b d3,(a2)+', 'moveq #0,d7', 'move.b $bb(a4),d7',
             'lsl.w #8,d7', 'move.b $ba(a4),d7')
        if args.sparse:
            # Check the whole ROM span once, then copy with postincrement.
            # Other mapped regions retain the general checked path below.
            emit('subi.w #$bc06,d7', 'cmpi.w #$2e7,d7',
                 'bhi.w rr_literal_checked', 'move.w d7,d0', 'add.w d3,d0',
                 'cmpi.w #$2e8,d0', 'bhi.w rr_unmapped',
                 'lea $20(a6,d7.w),a0', 'moveq #0,d0',
                 'move.b (a0),d0', 'subq.w #1,d3')
            label('rr_literal_rom')
            emit('move.b (a0)+,(a2)+', 'dbra d3,rr_literal_rom',
                 'st d2', 'bra.w rr_b024')
            label('rr_literal_checked')
            emit('addi.w #$bc06,d7')
        indirect()
        emit('move.b (a0),d0', 'subq.w #1,d3')
        label('rr_compact_literal')
        emit('move.b (a0),(a2)+', 'addq.w #1,d7', 'andi.l #$ffff,d7')
        indirect()
        emit('dbra d3,rr_compact_literal', 'move.b #$ff,d2', 'bra.w rr_b024')
        continue
    if args.compact and pc == 0xb03f:
        # Two exact sub-byte bank fragments, in ascending destination order.
        emit(f'cmpi.{remaining_size} #2,{remaining}', 'bcs.w rr_fail', f'subq.{remaining_size} #2,{remaining}',
             'lea 4(a2),a0', 'cmpa.l a1,a0', 'bhi.w rr_fail',
             'clr.b (a2)+', 'move.b #2,(a2)+', 'moveq #0,d7',
             'move.b $a2(a4),d7', 'lsl.w #8,d7', 'move.b $a1(a4),d7')
        if args.sparse:
            emit('subi.w #$b800,d7', 'cmpi.w #$1e,d7',
                 'bhi.w rr_bank_checked', 'lea (a6,d7.w),a0',
                 'move.b (a0)+,d0', 'move.b d0,(a2)+', 'move.b (a0),(a2)+',
                 'st d2', 'bra.w rr_b049')
            label('rr_bank_checked')
            emit('addi.w #$b800,d7')
        indirect()
        emit('move.b (a0),d0', 'move.b d0,(a2)+', 'addq.w #1,d7', 'andi.l #$ffff,d7')
        indirect()
        emit('move.b (a0),(a2)+', 'move.b #$ff,d2', 'bra.w rr_b049')
        continue
    if name in ('LDA','LDX','LDY'):
        dst = {'LDA':'d0','LDX':'d1','LDY':'d2'}[name]
        src = ea(mode,operand)
        emit(f'moveq #{operand},{dst}' if native_calls and mode=='imm' and operand<128
             else f'move.b {src},{dst}'); nz()
    elif name in ('STA','STX','STY'):
        dest = ea(mode,operand)
        emit(f'move.b '+{'STA':'d0','STX':'d1','STY':'d2'}[name]+','+dest)
    elif name in ('AND','ORA','EOR'):
        src = ea(mode,operand)
        if native_calls and mode=='imm': emit({'AND':'andi.b','ORA':'ori.b','EOR':'eori.b'}[name]+f' {src},d0')
        else: emit(f'move.b {src},d3', {'AND':'and.b','ORA':'or.b','EOR':'eor.b'}[name]+' d3,d0')
        nz()
    elif name in ('ADC','SBC'):
        src = ea(mode,operand)
        emit(f'moveq #{operand},d3' if native_calls and mode=='imm' and operand<128 else f'move.b {src},d3')
        if name == 'SBC': emit('eori.b #1,d4')
        emit('lsr.b #1,d4', ('addx.b' if name=='ADC' else 'subx.b')+' d3,d0')
        if 'V' in live_out[pc]: emit('svs d6')
        # ADDX Z is cumulative. TST obtains normal NZ, but destroys native C/V.
        # Capture C/V first, then recompute NZ after normalizing the saved carry.
        carry(name=='SBC')
        if live_out[pc] & {'N','Z'}: emit('tst.b d0'); nz()
    elif name in ('CMP','CPX','CPY'):
        src = ea(mode,operand)
        dst={'CMP':'d0','CPX':'d1','CPY':'d2'}[name]
        if native_calls and mode=='imm': emit(f'cmpi.b {src},{dst}')
        else: emit(f'move.b {src},d3', f'cmp.b d3,{dst}')
        nz(); carry(True)
    elif name == 'BIT':
        src = ea(mode,operand)
        emit(f'move.b {src},d3')
        if 'V' in live_out[pc]: emit('move.b d3,d6')
        if live_out[pc] & {'N','Z'}:
            emit('move.b d3,d7', 'and.b d0,d3')
            nz()
            emit('andi.w #$fff7,d5', 'lsr.b #4,d7', 'andi.b #8,d7', 'or.b d7,d5')
    elif name in ('ASL','LSR','ROL','ROR'):
        src = ea(mode,operand)
        emit(f'move.b {src},d3')
        if name in ('ROL','ROR'): emit('lsr.b #1,d4')
        emit({'ASL':'lsl.b','LSR':'lsr.b','ROL':'roxl.b','ROR':'roxr.b'}[name]+' #1,d3')
        nz(); carry()
        emit(f'move.b d3,{src}')
    elif name in ('INC','DEC'):
        dest = ea(mode,operand)
        emit(('addq.b' if name=='INC' else 'subq.b')+f' #1,{dest}'); nz()
    elif name in ('INX','INY','DEX','DEY'):
        emit(('addq.b' if name.startswith('IN') else 'subq.b')+' #1,'+('d1' if name[-1]=='X' else 'd2')); nz()
    elif name in ('TAX','TAY','TXA','TYA'):
        src, dest = {'TAX':('d0','d1'),'TAY':('d0','d2'),'TXA':('d1','d0'),'TYA':('d2','d0')}[name]
        emit(f'move.b {src},{dest}'); nz()
    elif name in ('SEC','CLC','CLV'):
        emit({'SEC':'moveq #1,d4','CLC':'moveq #0,d4','CLV':'moveq #0,d6'}[name])
    elif name == 'PHA':
        if native_calls: emit('move.w d0,-(sp)')
        else: push('d0')
    elif name == 'PLA':
        if native_calls: emit('move.w (sp)+,d0')
        else: pop('d0')
        emit('tst.b d0'); nz()
    elif name == 'JSR':
        if native_calls:
            emit(f'bsr.w rr_{operand:04x}')
            ordered = sorted(graph)
            if ordered[ordered.index(pc)+1] != end: emit(f'bra.w rr_{end:04x}')
        else:
            push(f'#${(end-1)>>8:02x}'); push(f'#${(end-1)&255:02x}')
            emit(f'bra.w rr_{operand:04x}')
    elif name == 'JMP':
        assert mode == 'abs'
        emit(f'bra.w rr_{operand:04x}')
    elif name == 'RTS': emit('bra.w rr_return')
    elif mode == 'rel':
        dest = end+(operand if operand < 128 else operand-256)
        if pc in folded_nz: emit(name.lower()+f'.w rr_{dest:04x}')
        else:
            flag,bit = ('d4',0) if name in ('BCS','BCC') else ('d6',6) if name in ('BVS','BVC') else ('d5',2 if name in ('BEQ','BNE') else 3)
            emit(f'btst #{bit},{flag}', ('bne.w' if name in ('BCS','BVS','BEQ','BMI') else 'beq.w')+f' rr_{dest:04x}')
    else: raise ValueError(instruction)
    if name not in ('JSR','JMP','RTS') and end != pc+rom6502.LENGTH[mode]:
        raise AssertionError(instruction)
    if name not in ('JSR','JMP','RTS') and end not in graph:
        raise AssertionError(hex(end))
    if name not in ('JSR','JMP','RTS'):
        # Fall through only when the next emitted ROM instruction is adjacent.
        ordered = sorted(graph)
        if ordered[ordered.index(pc)+1] != end: emit(f'bra.w rr_{end:04x}')

label('rr_return')
if native_calls: emit('rts')
else:
    emit('moveq #0,d3')
    pop('d3'); pop('d7')
    emit('lsl.w #8,d7', 'or.w d7,d3', 'addq.w #1,d3', 'beq.w rr_core_done')

def dispatch(values, name):
    label(name)
    if not values: emit('bra.w rr_fail'); return
    mid = len(values)//2
    pc = values[mid]
    emit(f'cmpi.w #${pc:04x},d3', f'beq.w rr_{pc:04x}',
         f'bcs.w {name}_lo', f'bra.w {name}_hi')
    dispatch(values[:mid],name+'_lo'); dispatch(values[mid+1:],name+'_hi')

if not native_calls: dispatch(sorted(returns), 'rr_dispatch')
label('rr_core_done')
emit('move.b d0,(a5)', 'move.b d1,1(a5)', 'move.b d2,2(a5)')
if args.compact:
    emit(f'tst.{remaining_size} {remaining}', 'bne.w rr_fail',
         'move.l a2,d0', 'sub.l a3,d0', 'move.w d0,d3', 'subq.w #1,d3',
         'cmpi.w #96,d3', 'bhi.w rr_fail', 'move.b d3,(a3)',
         'addq.l #1,4(a5)', 'bra.w rr_leave')
else:
    emit(
     'moveq #0,d7', 'move.b $3f05(a4),d7', 'lsl.w #8,d7',
     'move.b $3f04(a4),d7', 'cmpi.w #$ffd0,d7', 'bhi.w rr_fail',
     'andi.l #$ffff,d7', 'movea.l a4,a0', 'adda.l d7,a0',
     'move.l a0,d7', 'move.l a3,d3', 'or.l d3,d7', 'btst #0,d7',
         'bne.w rr_row_bytes', 'moveq #11,d7')
    label('rr_row_longs')
    emit('move.l (a0)+,(a3)+', 'dbra d7,rr_row_longs', 'bra.w rr_success')
    label('rr_row_bytes')
    emit('moveq #47,d7')
    label('rr_row_byte_loop')
    emit('move.b (a0)+,(a3)+', 'dbra d7,rr_row_byte_loop')
    label('rr_success')
    emit('addq.l #1,4(a5)', 'moveq #1,d0', 'bra.w rr_leave')
label('rr_fail')
emit('moveq #0,d0')
label('rr_leave')
if native_calls: emit('movea.l a5,sp')
elif args.compact:
    emit('addq.l #2,sp')
emit('movem.l (sp)+,d2-d7/a2-a6', 'rts')

if args.sparse:
    # Reject accesses to removed address space instead of silently aliasing it.
    # d7 stays an original 16-bit A8 address; NZ/C live in d4/d5/d6.
    label('rr_map_address')
    emit('movea.l a4,a0', 'cmpi.w #$600,d7', 'bcs.w rr_map_low')
    label('rr_map_rom')
    for index,(name,start,size) in enumerate(sorted(RANGES,key=lambda r:r[1])):
        emit(f'cmpi.w #${start:x},d7', 'bcs.w rr_unmapped',
             f'cmpi.w #${start+size:x},d7', f'bcs.w rr_map_table_{index}')
    emit('bra.w rr_unmapped')
    for index,(name,start,size) in enumerate(sorted(RANGES,key=lambda r:r[1])):
        label(f'rr_map_table_{index}')
        emit('movea.l a6,a0', 'adda.l d7,a0',
             f'suba.l #{start-packed_offset(start)},a0', 'rts')
    label('rr_map_low')
    emit('adda.l d7,a0', 'rts')
    label('rr_unmapped')
    emit('illegal')
output = GENERATED/('gen_sparse_m68k.s' if args.sparse else
              'gen_compact_m68k.s' if args.compact else 'gen_m68k.s')
if args.test_labels: output = GENERATED/'gen_sparse_test.s'
if args.sparse:
    assert not any(f'${address:x}(a4)' in line
                   for address in range(0xa3, 0xa9) for line in lines)
output.write_text('\n'.join(lines)+'\n')
print(f'Generated native m68k graph: {len(graph)} ROM instructions, {len(returns)} return sites, shared terrain/actor path')
