"""Deterministic packed-build A/B: maximum scroll, pulsed fire, STE 512 KiB.

Requires the previous release/debug executable in build/godlib_baseline.
Inputs and collision/fuel toggles are injected into RAM at frame breakpoints;
this is a controlled rendering/cycle regression, not an input-device test.
"""
from pathlib import Path
import bisect
import hashlib
import json
import shutil
import struct
import subprocess
import sys
import time

ROOT=Path(__file__).resolve().parent.parent
OUT=ROOT/'build/godlib_comparison'
OUT.mkdir(exist_ok=True)
sys.path.insert(0,str(Path.home()/'.agents/skills/hatari-debugging/scripts'))
from hatari_session import HatariSession,runtime_addresses
from hrdb_profile import Rdb,profile_lines,nm_symbols

def measure(label,packed,debug):
    folder=OUT/label
    folder.mkdir(exist_ok=True)
    program=folder/'RR.PRG'
    shutil.copy2(packed,program)
    with HatariSession(str(program),machine='ste',hatari='/opt/homebrew/bin/hatari-hrdb',
            screenshot_dir=str(folder),debug_except='bus,address,illegal',
            extra_args=['--memsize','0','--cpulevel','0','--cpuclock','8',
                        '--monitor','rgb','--confirm-quit','false']) as h:
        h.wait(12)
        base=h.basepage()['text']
        addresses=runtime_addresses(str(debug),base,nm='m68k-atari-mintelf-nm')
        r=Rdb()
        r.cmd('break')
        def stop(tick=None,profile=False):
            r.cmd('bp all')
            condition='' if tick is None else ' && ($%x).w = %d'%(addresses['ticks'],tick)
            r.cmd('bp pc = $%x%s :once'%(addresses['RRPlayer_Restore'],condition))
            r.take('!profile')
            if profile:r.cmd('profile 1')
            r.cmd('run',wait=.01)
            deadline=time.monotonic()+40
            while time.monotonic()<deadline:
                r.msgs=[m for m in r.msgs if m and m[0]!=b'OK']
                r.cmd('status',wait=.04)
                if any(len(m)==3 and m[0]==b'OK' and m[1]==b'0' for m in r.msgs):break
            else:raise AssertionError((label,'breakpoint timeout',tick))
            # status polling must not discard the profile event.
            if profile:
                r.pump(.2)
                message=r.take('!profile')
                assert message, 'Missing CPU profile'
                lines=profile_lines(message)
                r.cmd('profile 0')
                return lines
        def dump(name):
            path=folder/(name+'.ram')
            path.unlink(missing_ok=True)
            r.cmd('savebin 0 80000 '+str(path),wait=.1)
            deadline=time.monotonic()+5
            while not path.exists() or path.stat().st_size!=0x80000:
                assert time.monotonic()<deadline
                r.pump(.05)
            return path.read_bytes()
        def write(address,value,size=2):
            data=int(value).to_bytes(size,'big')
            r.cmd('memset %x %x %s'%(address,len(data),data.hex()),wait=.02)
        def word(ram,name):return struct.unpack_from('>H',ram,addresses[name])[0]
        def long(ram,name):return struct.unpack_from('>I',ram,addresses[name])[0]
        def pixels(ram):
            start=long(ram,'gScreenPhysicGraphic')+word(ram,'gScroll')*160
            panel=long(ram,'gpPanel')
            return ram[start:start+160*160]+ram[panel:panel+6400]
        stop()
        ready=dump('ready')
        assert word(ready,'gPlayerState')==4
        ready_pixels=pixels(ready)
        write(addresses['gPlayerState'],0)
        write(addresses['gPlayerCollisionEnabled'],0)
        write(addresses['gFuelEnabled'],0)
        write(addresses['gFlight'],254)
        # sIKBD.mKbdPressTable offset 64, scancodes UP=$48, SHIFT=$2a.
        write(addresses['gIKBD']+64+0x48,1,1)
        stop(1)
        first=dump('first_frame')
        cycles=[]
        symbols=nm_symbols(str(debug),nm='m68k-atari-mintelf-nm')
        positions=[base+offset for offset,name in symbols]
        text_bytes=struct.unpack_from('>I',debug.read_bytes(),2)[0]
        for block in range(8):
            write(addresses['gIKBD']+64+0x2a,1 if block%2==0 else 0,1)
            lines=stop(1+(block+1)*8,profile=True)
            game_cycles=0
            for address,instructions,count in lines:
                i=bisect.bisect_right(positions,address)-1
                if i>=0 and base<=address<base+text_bytes:
                    if symbols[i][1]!='Vbl_WaitVbl':game_cycles+=count
            cycles.append(game_cycles)
        last=dump('last_frame')
        assert word(last,'ticks')==65 and word(last,'gPlayerState')==0
        result=dict(packed_bytes=program.stat().st_size,
            ready_sha256=hashlib.sha256(ready_pixels).hexdigest(),
            flight_sha256=hashlib.sha256(pixels(last)).hexdigest(),
            frames=64,VBLs=struct.unpack_from('>I',last,0x466)[0]-struct.unpack_from('>I',first,0x466)[0],
            program_cycles_excluding_Vbl_WaitVbl=sum(cycles),blocks=cycles,
            distance=long(last,'distance'),score=long(last,'gScore'))
        assert not any(s in '\n'.join(h.lines).lower() for s in ('address error','bus error','illegal instruction'))
        r.s.close()
    print(label,json.dumps(result),flush=True)
    return result

baseline=measure('before',ROOT/'build/godlib_baseline/RELEASE.PRG',ROOT/'build/godlib_baseline/RR.PRG')
current=measure('after',ROOT/'release/RR.PRG',ROOT/'build/RR.PRG')
for key in ('ready_sha256','flight_sha256','distance','score'):
    assert baseline[key]==current[key],(key,baseline[key],current[key])
assert current['VBLs']<=baseline['VBLs'], 'Frame pacing regressed'
assert current['program_cycles_excluding_Vbl_WaitVbl']<=baseline['program_cycles_excluding_Vbl_WaitVbl']*1.01
(OUT/'report.json').write_text(json.dumps(dict(before=baseline,after=current,
    limits='64 controlled maximum-speed frames, pulsed SHIFT every eight frames; collision/fuel disabled; no full-campaign performance claim.'),indent=2)+'\n')
print('PASS: identical terrain/sprite/HUD bytes and no frame/cycle regression on STE 512 KiB')
