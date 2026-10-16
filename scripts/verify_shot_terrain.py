"""Execute the production 68000 terrain test: masks, row bounds and ABI."""
from pathlib import Path
import json
import struct
import subprocess
import machine68k as m

root=Path(__file__).resolve().parent.parent
out=root/'build/shot_terrain_test'
out.mkdir(exist_ok=True)
subprocess.run(['m68k-atari-mintelf-objcopy','-O','binary','-j','text',
    str(root/'build/shot_terrain.o'),str(out/'kernel.bin')],check=True)
machine=m.Machine(m.CPUType.M68000,2048)
cpu,mem=machine.cpu,machine.mem
mem.w_block(0x10000,(out/'kernel.bin').read_bytes())
base,stack,ret=0x30000,0xf0000,0x800
water=struct.pack('>4H',0xffff,0xffff,0,0)*20*416
registers=[getattr(m.Register,'D'+str(i)) for i in range(3,8)]
registers += [getattr(m.Register,'A'+str(i)) for i in range(2,7)]
cases=0
timing={}

def run(x,y,height,ink=None,pixel=None,row=None):
    global cases
    screen=bytearray(water)
    if ink is not None:
        at=(y+row)*160+(pixel//16)*8
        mask=0x8000>>(pixel&15)
        for plane in range(4):
            old=struct.unpack_from('>H',screen,at+plane*2)[0]
            value=old|mask if ink&(1<<plane) else old&~mask
            struct.pack_into('>H',screen,at+plane*2,value)
    expected=ink in (1,2,4,5,6) and x<=pixel<x+2 and 0<=row<height if ink is not None else False
    mem.w_block(base,screen)
    for i,r in enumerate(registers):cpu.w_reg(r,0x12340000+i*102)
    saved=[cpu.r_reg(r) for r in registers]
    cpu.w_reg(m.Register.A0,base+y*160+(x//16)*8)
    cpu.w_reg(m.Register.D0,x&15)
    cpu.w_reg(m.Register.D1,height)
    cpu.w_reg(m.Register.A7,stack)
    mem.w32(stack,ret)
    cpu.w_sr(0x2700)
    cpu.w_pc(0x10000)
    cycles=0
    for _ in range(1000):
        cycles+=cpu.execute(1)
        if cpu.r_pc()==ret:break
    else:raise AssertionError('no return')
    assert cpu.r_reg(m.Register.D0)==int(expected),(x,y,height,ink,pixel,row)
    assert saved==[cpu.r_reg(r) for r in registers]
    assert cpu.r_reg(m.Register.A7)==stack+4
    assert bytes(mem.r_block(base,len(screen)))==screen, 'collision test wrote terrain'
    cases+=1
    return cycles

for x in list(range(16,32))+[303,311,318]:
    for height in range(17):
        for ink in range(7):
            run(x,255,height,ink,x,0)
            run(x,255,height,ink,x+1,max(0,height-1))
        for pixel,row in ((x-1,0),(x+2,0),(x,16)):
            run(x,255,height,2,pixel,row)
for x in (23,31):timing[x&15]=run(x,255,16)
(out/'report.json').write_text(json.dumps(dict(cases=cases,miss_cycles_16_rows=timing),indent=2)+'\n')
print('PASS:',cases,'68000 cases: all shifts/inks, row bounds, >64K offsets, untouched terrain and ABI;',timing)
