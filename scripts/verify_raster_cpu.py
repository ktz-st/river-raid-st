"""Execute production 68000 raster kernels against pixel-wise reference output.

Requires machine68k only for this optional test, not for building the game.
"""
from pathlib import Path
import json
import random
import struct
import subprocess
import machine68k as m

root=Path(__file__).resolve().parent.parent
out=root/'build/raster_cpu_test'
out.mkdir(exist_ok=True)
obj=root/'build/raster_cpu.o'
subprocess.run(['m68k-atari-mintelf-objcopy','-O','binary','-j','text',
                str(obj),str(out/'kernel.bin')],check=True)
symbols={parts[2]:int(parts[0],16) for line in subprocess.check_output(
    ['m68k-atari-mintelf-nm',str(obj)],text=True).splitlines()
    if len(parts:=line.split())==3 and parts[1].lower()=='t'}
machine=m.Machine(m.CPUType.M68000,2048)
cpu,mem=machine.cpu,machine.mem
code,base,stack,ret=0x10000,0x30002,0xf0000,0x800
mem.w_block(code,(out/'kernel.bin').read_bytes())
assert (out/'kernel.bin').stat().st_size>0
randomizer=random.Random(68000)
original=randomizer.randbytes(160*200+128)
registers=[getattr(m.Register,'D'+str(i)) for i in range(3,8)]
registers += [getattr(m.Register,'A'+str(i)) for i in range(2,7)]
cases={}
cycles={}

def pixel(screen,x,y,ink):
    at=64+y*160+(x//16)*8
    mask=0x8000>>(x&15)
    for plane in range(4):
        value=struct.unpack_from('>H',screen,at+plane*2)[0]
        value=value|mask if ink&(1<<plane) else value&~mask
        struct.pack_into('>H',screen,at+plane*2,value)

def run(name,offset,d0,d1,expected,glyph=None):
    mem.w_block(base,original)
    for i,r in enumerate(registers):cpu.w_reg(r,0x12340000+i*102)
    saved=[cpu.r_reg(r) for r in registers]
    cpu.w_reg(m.Register.A0,base+offset)
    if glyph is not None:
        # Source glyphs are bytes and may have odd addresses. Screen never is.
        mem.w_block(0x20001,glyph)
        cpu.w_reg(m.Register.A1,0x20001)
    cpu.w_reg(m.Register.D0,d0)
    cpu.w_reg(m.Register.D1,d1)
    cpu.w_reg(m.Register.A7,stack)
    mem.w32(stack,ret)
    cpu.w_sr(0x2700)
    cpu.w_pc(code+symbols[name])
    elapsed=0
    for _ in range(30000):
        elapsed+=cpu.execute(1)
        if cpu.r_pc()==ret:break
    else:raise AssertionError(('no return',name))
    actual=bytes(mem.r_block(base,len(original)))
    assert actual==expected,(name,d0,d1,next(i for i,(a,b) in enumerate(zip(actual,expected)) if a!=b))
    assert saved==[cpu.r_reg(r) for r in registers],('ABI',name)
    assert cpu.r_reg(m.Register.A7)==stack+4,('stack',name)
    cases[name]=cases.get(name,0)+1
    cycles.setdefault(name,{})[str((d0,d1))]=elapsed

for ink in range(16):
    for rows in (0,1,2,40,160):
        expected=bytearray(original)
        words=struct.pack('>4H',*[65535 if ink&(1<<p) else 0 for p in range(4)])
        expected[64+7*160:64+(7+rows)*160]=words*20*rows
        run('RR_FillRowsCPU',64+7*160,ink,rows,expected)

for x in list(range(16,32))+[88+position*2 for position in range(32)]:
    expected=bytearray(original)
    for y in range(20,30):
        for px in range(x,x+4):pixel(expected,px,y,12)
    run('RR_FuelMarkerCPU',64+20*160+(x//16)*8,x&15,0,expected)

for bits in range(256):
    for half in (0,8):
        x=16*(bits%20)+half
        y=bits%180
        glyph=bytes((bits+17*row)&255 for row in range(8))
        expected=bytearray(original)
        for row in range(8):
            for bit in range(8):pixel(expected,x+bit,y+row,15 if glyph[row]&(128>>bit) else 0)
        run('RR_TextGlyphCPU',64+y*160+(x//16)*8,half,0,expected,glyph)

# Compact ASM literals still consume plane[], not only the uniform fill table.
# Execute the actual initializer to prevent mistaking this LUT for dead data.
lut_obj=root/'build/modee.o'
subprocess.run(['m68k-atari-mintelf-objcopy','-O','binary','-j','.text',
                str(lut_obj),str(out/'modee.bin')],check=True)
lut_symbols={p[2]:int(p[0],16) for line in subprocess.check_output(
    ['m68k-atari-mintelf-nm',str(lut_obj)],text=True).splitlines()
    if len(p:=line.split())==3 and p[1].lower()=='t'}
lut_code,lut_base,map_base=0x18000,0x60002,0x70001
mem.w_block(lut_code,(out/'modee.bin').read_bytes())
assert (out/'modee.bin').stat().st_size>0
lut_maps=((0,1,2,3),(5,12,15,0),(15,8,4,1),(3,2,1,0))
for mapping in lut_maps:
    expected=bytearray(1568)
    for byte in range(256):
        for part in range(4):
            selector=(byte>>(6-2*part))&3
            mask=0xc0>>(2*part)
            for plane in range(4):
                if mapping[selector]&(1<<plane):expected[byte*4+plane]|=mask
            if selector!=3:expected[1024+byte]|=mask
            if selector==1:expected[1280+byte]|=mask
    expected[1536:]=struct.pack('>16H',*[65535 if ink&(1<<p) else 0
                                        for ink in mapping for p in range(4)])
    guarded=bytes([0xa5])*32+bytes([0xcc])*1568+bytes([0x5a])*32
    mem.w_block(lut_base-32,guarded)
    mem.w_block(map_base,bytes(mapping))
    for i,r in enumerate(registers):cpu.w_reg(r,0x12340000+i*102)
    saved=[cpu.r_reg(r) for r in registers]
    cpu.w_reg(m.Register.A0,lut_base)
    cpu.w_reg(m.Register.A1,map_base)
    cpu.w_reg(m.Register.A7,stack)
    mem.w32(stack,ret)
    cpu.w_sr(0x2700)
    cpu.w_pc(lut_code+lut_symbols['modee_build_lut'])
    for _ in range(200000):
        cpu.execute(1)
        if cpu.r_pc()==ret:break
    else:raise AssertionError('LUT initializer did not return')
    actual=bytes(mem.r_block(lut_base-32,len(guarded)))
    wanted=guarded[:32]+expected+guarded[-32:]
    assert actual==wanted,(mapping,next((i,a,b) for i,(a,b) in enumerate(zip(actual,wanted)) if a!=b))
    assert saved==[cpu.r_reg(r) for r in registers]
    assert cpu.r_reg(m.Register.A7)==stack+4

(out/'report.json').write_text(json.dumps(dict(cases=cases,total=sum(cases.values()),
    byte_exact=True,ABI_preserved=True,LUT_cases=len(lut_maps),cycles=cycles,
    limits='Production ASM on emulated 68000 vs pixel reference; CPU cycles exclude Hatari/display contention.'),indent=2)+'\n')
print('PASS:',sum(cases.values()),'68000 raster cases: byte-exact output, guards, odd glyph source, even screen, ABI;',cases)
print('PASS:',len(lut_maps),'production LUT cases: literal planes, solid/bank masks, fill groups and guards')
