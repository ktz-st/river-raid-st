"""Packed 512K release: SPACE/F1, SHIFT and natural bridge/kamikaze respawn."""
from pathlib import Path
import hashlib
import json
import shutil
import struct
import sys
import time

ROOT=Path(__file__).resolve().parent.parent
OUT=ROOT/'build/tester_regression'; DISK=OUT/'disk'
DISK.mkdir(parents=True,exist_ok=True)
(DISK/'RR5LOG.TXT').unlink(missing_ok=True)
shutil.copy2(ROOT/'release/RR.PRG',DISK/'RR5.PRG')
sys.path.insert(0,str(Path.home()/'.agents/skills/hatari-debugging/scripts'))
from hatari_session import HatariSession,runtime_addresses
from hrdb_profile import Rdb,profile_lines

with HatariSession(str(DISK/'RR5.PRG'),machine='ste',hatari='/opt/homebrew/bin/hatari-hrdb',
        screenshot_dir=str(OUT/'shots'),debug_except='bus,address,illegal',
        extra_args=['--memsize','0','--cpulevel','0','--cpuclock','8',
                    '--monitor','rgb','--confirm-quit','false']) as h:
    h.wait(12)
    a=runtime_addresses(str(ROOT/'build/RR.PRG'),h.basepage()['text'],nm='m68k-atari-mintelf-nm')
    r=Rdb();r.cmd('break')
    results=[]
    def regs():
        r.msgs=[];r.cmd('regs',wait=.05)
        for m in reversed(r.msgs):
            if b'PC' in m:return int(m[m.index(b'PC')+1],16)
        raise AssertionError('No PC')
    def waitstop():
        end=time.monotonic()+90
        while time.monotonic()<end:
            r.msgs=[];r.cmd('status',wait=.04)
            if any(len(m)==3 and m[:3]==[b'OK',b'0',b''] for m in r.msgs):return
            if any(len(m)==3 and m[0]==b'OK' and m[1]==b'0' for m in r.msgs):return
        r.cmd('break');dump('timeout')
        raise AssertionError(('breakpoint timeout',hex(regs())))
    def stop(condition=''):
        r.cmd('bp all')
        r.msgs=[]
        r.cmd('bp pc = $%x %s :once'%(a['RRPlayer_Restore'],condition))
        assert not any(m and m[0]==b'ERROR' for m in r.msgs),r.msgs
        r.cmd('run',wait=.01);waitstop()
        assert regs()==a['RRPlayer_Restore']
    def stop_at(name,condition=''):
        r.cmd('bp all')
        r.cmd('bp pc = $%x %s :once'%(a[name],condition))
        r.cmd('run',wait=.01);waitstop()
        assert regs()==a[name]
    def dump(label):
        p=OUT/(label+'.ram');p.unlink(missing_ok=True)
        r.cmd('savebin 0 80000 '+str(p),wait=.1)
        end=time.monotonic()+5
        while not p.exists() or p.stat().st_size!=0x80000:
            assert time.monotonic()<end,label
            r.pump(.05)
        return p.read_bytes()
    def word(ram,name):return struct.unpack_from('>H',ram,a[name])[0]
    def long(ram,name):return struct.unpack_from('>I',ram,a[name])[0]
    def signed(ram,name):return struct.unpack_from('>i',ram,a[name])[0]
    def write(address,data):r.cmd('memset %x %x %s'%(address,len(data),data.hex()),wait=.02)
    def put(name,n,size=2):write(a[name],int(n).to_bytes(size,'big',signed=n<0))
    def check(label):results.append(label);print('PASS:',label,flush=True)
    def runkey(key,hold=.15):
        r.cmd('bp all');r.cmd('run',wait=.01)
        h.tap(key,hold=hold,after=.1)
        r.cmd('break')
    def escape():
        r.cmd('bp all');r.cmd('run',wait=.01)
        h.control('hatari-event keydown 01');h.wait(.4)
        h.control('hatari-event keyup 01');h.wait(.15);r.cmd('break')
    def restart():
        runkey('F1',.7);stop()
        ram=dump('restart')
        assert [word(ram,n) for n in ('gPaused','gPlayerState','gPlayerLives','gEnemyBridge','gFuel')]==[0,4,3,1,65535]
        assert long(ram,'gScore')==0 and word(ram,'gScroll')==0
        return ram
    def pause_case():
        runkey('SPACE',.7)
        one=dump('pause_begin');assert word(one,'gPaused')==1
        r.cmd('run',wait=.01);h.key_down('LEFT');h.wait(.5);h.key_up('LEFT');r.cmd('break')
        two=dump('pause_end')
        for n,size in [('gScroll',2),('distance',4),('gFuel',2),('ticks',2),
                       ('gPlayerX',2),('gPlayerState',2),('gPlayerDeathTicks',2),
                       ('gFlight',4),('gScore',4),('gShotY',2),('gEnemyBridge',2),
                       ('gScreenClass',16)]:
            assert one[a[n]:a[n]+size]==two[a[n]:a[n]+size],n
        assert two[a['gPsgRegisters']+8:a['gPsgRegisters']+11]==bytes(3)
        check('held SPACE freezes gameplay/display pages and mutes PSG; movement ignored')
        return one
    def approach():
        put('gPlayerCollisionEnabled',0);put('gFuelEnabled',0)
        runkey('LSHIFT')
        h.control('hatari-option --fast-forward true');h.key_down('UP')
        stop('&& ($%x).w = 1'%a['bridgePending'])
        h.key_up('UP');h.control('hatari-option --fast-forward false')
        ram=dump('bridge_spawn')
        world=signed(ram,'bridgeWorld')
        target=100-world
        stop('&& ($%x).l > $%x'%(a['distance'],target-1))
        return dump('bridge_approach')
    stop_at('IKBD_Update','&& ($%x).w = 1'%a['gTitleActive'])
    title=dump('initial_title')
    screen=long(title,'gScreenPhysicGraphic')
    assert title[screen:screen+32000]==(ROOT/'build/title.st4').read_bytes()
    assert title[a['gRRPalette']:a['gRRPalette']+32]==title[a['rrTitlePalette']:a['rrTitlePalette']+32]
    check('title decompresses byte-exactly into screen RAM on 512K')
    runkey('LSHIFT');stop()
    assert word(dump('title_fire'),'gPlayerState')==0
    restart()
    assert word(dump('initial'),'gPlayerState')==4
    if '--title-pause' in sys.argv or '--title-fades' in sys.argv:
        runkey('RSHIFT');stop()
        put('gPlayerLives',1);put('gPlayerX',0)
        stop('&& ($%x).w = 1'%a['gPlayerState'])
        assert word(dump('title_pause_explosion'),'gPlayerLives')==0
        h.key_down('LSHIFT')
        stop_at('Vbl_WaitVbl','&& ($%x).w = 75'%a['gGameOverDelay'])
        before=dump('title_pause_begin')
        began=struct.unpack_from('>I',before,0x466)[0]
        stop_at('RRPalette_FadeOut')
        ended=dump('title_pause_completed')
        elapsed=struct.unpack_from('>I',ended,0x466)[0]-began
        assert 75<=elapsed<=76,elapsed
        stop_at('RRPalette_Set','&& ($%x).w = 8 && ($%x).w = 1'%(a['gPaletteStep'],a['gPaletteFading']))
        mid=dump('title_fade_out_midpoint')
        def scale(palette,n):
            result=[]
            for colour in struct.unpack('>16H',palette):
                out=0
                for shift in (0,4,8):
                    part=(colour>>shift)&15
                    level=(((part&7)*2+(part>>3))*n)//16
                    out|=((level>>1)|((level&1)<<3))<<shift
                result.append(out)
            return struct.pack('>16H',*result)
        original=ended[a['gRRPalette']:a['gRRPalette']+32]
        assert mid[a['gRRPalette']:a['gRRPalette']+32]==scale(original,9)
        stop_at('RRTitle_Draw')
        black=dump('title_decompress_black')
        assert black[a['gRRPalette']:a['gRRPalette']+32]==bytes(32)
        stop_at('RRPalette_Set','&& ($%x).w = 8 && ($%x).w = 1'%(a['gPaletteStep'],a['gPaletteFading']))
        mid=dump('title_fade_in_midpoint')
        target=mid[a['rrTitlePalette']:a['rrTitlePalette']+32]
        assert mid[a['gRRPalette']:a['gRRPalette']+32]==scale(target,7)
        stop_at('IKBD_Update','&& ($%x).w = 1'%a['gTitleActive'])
        after=dump('title_pause_end')
        assert word(after,'gGameOverDelay')==0
        assert word(after,'gPlayerLives')==0
        assert after[a['gRRPalette']:a['gRRPalette']+32]==target
        for n,size in [('distance',4),('gScore',4),('gScroll',2),('gFuel',2)]:
            assert before[a[n]:a[n]+size]==after[a[n]:a[n]+size],n
        screen=long(after,'gScreenPhysicGraphic')
        assert after[screen:screen+32000]==(ROOT/'build/title.st4').read_bytes()
        r.cmd('bp all');r.cmd('run');h.wait(.4);r.cmd('break')
        assert word(dump('title_pause_held_fire'),'gTitleActive')==1
        h.key_up('LSHIFT');runkey('LEFT')
        assert word(dump('title_pause_arrow'),'gTitleActive')==1
        h.key_down('RSHIFT')
        stop_at('RR_Load')
        h.key_up('RSHIFT')
        loading=dump('game_loading_black')
        assert loading[a['gRRPalette']:a['gRRPalette']+32]==bytes(32)
        stop_at('RRPalette_FadeIn')
        prepared=dump('game_prepared_black')
        assert prepared[a['gRRPalette']:a['gRRPalette']+32]==bytes(32)
        assert word(prepared,'gPlayerVisible')==1
        stop()
        fresh=dump('title_pause_new_game')
        assert word(fresh,'gPlayerState')==0 and word(fresh,'gPlayerLives')==3
        assert word(fresh,'gEnemyBridge')==1 and long(fresh,'gScore')==0
        assert fresh[a['gRRPalette']:a['gRRPalette']+32]==fresh[a['rrPalette']:a['rrPalette']+32]
        check('last life: 75 frozen VBLs, title ignores held FIRE, fresh FIRE starts game')
        check('STE fades: intermediate DAC levels, black title decode/map preparation, exact final palettes')
        (OUT/'title_pause_report.json').write_text(json.dumps(dict(cases=results,
            pause_vbls=elapsed,RAM_bytes=524288,
            binary_sha256=hashlib.sha256((DISK/'RR5.PRG').read_bytes()).hexdigest()),indent=2)+'\n')
        r.s.close();sys.exit(0)
    if '--terrain-shots' in sys.argv:
        put('gPlayerCollisionEnabled',0);put('gFuelEnabled',0)
        put('gPlayerState',0);put('gEnemiesEnabled',0)
        for label,ink,x,px,dy,expected in (
            ('water',3,31,31,0,1),('background',0,31,31,0,1),
            ('green',1,31,31,0,0),('brown',2,31,31,0,0),
            ('road_dark',4,31,31,0,0),('road_gray',5,31,31,0,0),
            ('road_stripe',6,31,31,0,0),
            ('island_second_pixel',1,31,32,7,0),
            ('outside_left',2,31,30,7,1),('outside_right',2,31,33,7,1),
            ('old_tail',2,31,31,15,0),('sweep_last_exclusive',2,31,31,16,1),
            ('right_edge',2,318,319,4,0)):
            stop_at('RREnemies_Combat')
            before=dump('terrain_'+label+'_before')
            scroll=word(before,'gScroll')
            back=long(before,'gScreenBackGraphic')
            start=back+(scroll+72)*160
            original=before[start:start+17*160]
            patch=bytearray(struct.pack('>4H',65535,65535,0,0)*20*17)
            offset=dy*160+(px//16)*8;mask=0x8000>>(px&15)
            for plane in range(4):
                old=struct.unpack_from('>H',patch,offset+plane*2)[0]
                value=old|mask if ink&(1<<plane) else old&~mask
                struct.pack_into('>H',patch,offset+plane*2,value)
            write(start,patch)
            put('gShotX',x);put('gShotY',80);put('gShotActive',1)
            stop_at('RREnemies_Weapons')
            after=dump('terrain_'+label+'_after')
            assert word(after,'gShotActive')==expected,label
            assert after[start:start+len(patch)]==patch, 'terrain changed by shot'
            assert long(after,'gScore')==long(before,'gScore'),label
            assert long(after,'gHits')==long(before,'gHits'),label
            write(start,original)
            check('terrain shot: '+label)
        # Force overlapping terrain/object hits in a single sweep. The object
        # is above the blocked tail, so nearest-terrain-first would fail this.
        put('gEnemiesEnabled',1)
        for label,kind,frame in (('boat',7,7),('fuel',15,15),('bridge',14,14)):
            stop_at('RREnemies_Combat')
            before=dump('priority_'+label+'_before')
            back=long(before,'gScreenBackGraphic');scroll=word(before,'gScroll')
            start=back+(scroll+64)*160
            original=before[start:start+32*160]
            write(start,struct.pack('>4H',65535,0,0,0)*20*32)
            for i in range(24):write(a['enemies']+12*i+8,bytes(1))
            world=72-signed(before,'distance')
            actor=struct.pack('>ihhBBBB',world,144,72,kind,frame,0,0)
            assert len(actor)==12
            write(a['enemies'],actor)
            put('gShotX',151);put('gShotY',80);put('gShotActive',1)
            stop_at('RREnemies_Weapons')
            after=dump('priority_'+label+'_after')
            assert after[a['enemies']+9]==2,label
            assert word(after,'gShotActive')==0,label
            assert long(after,'gHits')==long(before,'gHits')+1,label
            assert long(after,'gScore')>long(before,'gScore'),label
            write(start,original)
            check('object hit precedes terrain: '+label)
        restart()
        (OUT/'terrain_report.json').write_text(json.dumps(dict(cases=results,RAM_bytes=524288,
            binary_sha256=hashlib.sha256((DISK/'RR5.PRG').read_bytes()).hexdigest(),
            limits='Controlled clean-terrain/actor injections at real combat frame boundaries; not a full campaign.'),indent=2)+'\n')
        r.s.close()
        sys.exit(0)
    if '--quit-only' in sys.argv:
        baseline=dump('quit_ready_before');panel=long(baseline,'gpPanel')
        escape();assert word(dump('quit_ready_open'),'gQuitConfirm')==1
        runkey('n');cancelled=dump('quit_ready_cancelled')
        assert word(cancelled,'gQuitConfirm')==0
        assert baseline[panel:panel+6400]==cancelled[panel:panel+6400]
        runkey('SPACE');assert word(dump('quit_paused_before'),'gPaused')==1
        escape();runkey('n');paused=dump('quit_paused_cancelled')
        assert word(paused,'gPaused')==1 and word(paused,'gQuitConfirm')==0
        assert baseline[panel:panel+6400]==paused[panel:panel+6400]
        escape();r.cmd('bp all');r.cmd('run');r.s.close()
        h.tap('RETURN',hold=.2,after=1)
        assert h.mem(a['gScreenClass'],4).u32(a['gScreenClass'])==0
        assert not (DISK/'RR5LOG.TXT').exists(), 'exit created a log file'
        check('cancel restores exact HUD bytes and prior pause; ENTER exits to TOS')
        sys.exit(0)
    # Also exercise the real generated first scene without repainting terrain.
    put('gPlayerCollisionEnabled',0);put('gFuelEnabled',0)
    put('gPlayerState',0);put('gEnemiesEnabled',0)
    for ink,label in ((1,'natural land'),(2,'natural brown bank'),(3,'natural water')):
        stop_at('RREnemies_Combat')
        before=dump(label.replace(' ','_')+'_before')
        back=long(before,'gScreenBackGraphic');scroll=word(before,'gScroll')
        def colour(x,y):
            offset=back+(scroll+y)*160+(x//16)*8
            return sum(((struct.unpack_from('>H',before,offset+p*2)[0]>>(15-(x&15)))&1)<<p for p in range(4))
        def matches(x):
            values=[colour(px,y) for y in range(72,88) for px in (x,x+1)]
            return all(v==3 for v in values) if ink==3 else ink in values
        x=next(x for x in range(7,312) if matches(x))
        put('gShotX',x);put('gShotY',80);put('gShotActive',1)
        stop_at('RREnemies_Weapons')
        after=dump(label.replace(' ','_')+'_after')
        assert word(after,'gShotActive')==int(ink==3),label
        assert long(after,'gScore')==long(before,'gScore'),label
        check('projectile on unmodified generated terrain: '+label)
    put('gEnemiesEnabled',1);put('gFuelEnabled',1)
    restart()
    pause_case()
    runkey('LSHIFT');stop()
    assert word(dump('fire_unpause'),'gPaused')==0
    check('SHIFT/FIRE resumes pause')
    first=restart();assert word(first,'gPaused')==0
    check('F1 while paused: fresh READY, lives/fuel/bridge/score reset')
    ram=approach()
    world=signed(ram,'bridgeWorld');number=word(ram,'gEnemyBridge')
    slot=next(i for i in range(24) if ram[a['enemies']+12*i+8]==14 and
              struct.unpack_from('>i',ram,a['enemies']+12*i)[0]==world)
    x=struct.unpack_from('>h',ram,a['enemies']+12*slot+4)[0]
    for i in range(24):
        if i!=slot:write(a['enemies']+12*i+8,bytes(1))
    put('gShotX',x+31);put('gShotY',world+signed(ram,'distance')+20);put('gShotActive',1)
    stop('&& ($%x).b = 2'%(a['enemies']+12*slot+9))
    hit=dump('bridge_shot')
    assert word(hit,'gEnemyBridge')==number+1
    assert long(hit,'gScore')>=500
    cp=a['gCheckpoint'];gen=long(hit,'gpGen')
    shot_snapshot=hit[cp:cp+0x614]
    assert shot_snapshot==hit[gen:gen+0x614]
    assert word(hit,'gCheckpointBridgeAdvance')==1
    assert struct.unpack_from('>h',hit,cp+42516)[0]==word(hit,'gScroll')
    assert signed(hit,'bridgeWorld')+signed(hit,'distance')<141
    check('shot commits generator/scroll checkpoint immediately, before crossing')
    put('gPlayerX',0);put('gPlayerCollisionEnabled',1)
    stop('&& ($%x).w = 1'%a['gPlayerState'])
    before_cross_death=dump('shot_then_death_before_crossing')
    assert signed(before_cross_death,'bridgeWorld')+signed(before_cross_death,'distance')<141
    stop('&& ($%x).w = 4'%a['gPlayerState'])
    shot_respawn=dump('shot_checkpoint_respawn')
    assert word(shot_respawn,'gEnemyBridge')==number+1
    assert word(shot_respawn,'gPlayerLives')==2
    assert signed(shot_respawn,'bridgeWorld')+signed(shot_respawn,'distance')==141
    assert shot_respawn[cp:cp+0x614]==shot_snapshot
    check('death before crossing restores the unlocked section on its bridge, without double count')
    pause_case()
    before_high=long(dump('high_before_restart'),'gHighScore')
    again=restart();assert long(again,'gHighScore')==before_high
    check('F1 during flight preserves HI but clears the current run')
    ram=approach();world=signed(ram,'bridgeWorld');number=word(ram,'gEnemyBridge')
    put('gPlayerX',word(ram,'bridgeX'));put('gPlayerCollisionEnabled',1)
    # Model a live tank on this bridge, plus a survivor waiting on the bank.
    # Target ABI: Tank=24 bytes, state.frame=12, used=16, stationary=18.
    tank_x=word(ram,'bridgeX')+16
    for i,tx in enumerate((tank_x,0)):
        tank=struct.pack('>ihhhhBBBBHHHH',world+1,tx,tx,0,0,5,5,0,1,1,0,0,0)
        assert len(tank)==24
        write(a['tanks']+i*24,tank)
    # Keep the unmodified terrain/generator; only isolate other aircraft.
    for i in range(24):
        if ram[a['enemies']+12*i+8]!=14:write(a['enemies']+12*i+8,bytes(1))
    stop('&& ($%x).w = 1'%a['gPlayerState'])
    dead=dump('bridge_ram')
    assert word(dead,'gEnemyBridge')==number+1
    assert word(dead,'gPlayerLives')==2 and long(dead,'gScore')>=500
    assert dead[a['tanks']+12]&128
    assert 4<=dead[a['tanks']+24+12]<8
    assert word(dead,'gBridgeCrashAdvance')==1
    gen=long(dead,'gpGen');ram_snapshot=dead[cp:cp+0x614]
    assert ram_snapshot==dead[gen:gen+0x614]
    assert word(dead,'gCheckpointBridgeAdvance')==1
    assert struct.unpack_from('>h',dead,cp+42516)[0]==word(dead,'gScroll')
    assert any(dead[a['enemies']+12*i+8]==14 and dead[a['enemies']+12*i+9]==3 for i in range(24))
    pause_case();runkey('SPACE');stop('&& ($%x).w = 4'%a['gPlayerState'])
    respawn=dump('bridge_ram_respawn')
    assert word(respawn,'gEnemyBridge')==number+1 and word(respawn,'gPlayerLives')==2
    assert word(respawn,'gBridgeCrashAdvance')==0
    assert signed(respawn,'bridgeWorld')+signed(respawn,'distance')==141
    assert word(respawn,'gPlayerX')==word(respawn,'gLaunchX')
    assert word(respawn,'gFlight')==64 and word(respawn,'gFuel')==65535
    gen=long(respawn,'gpGen')
    canonical_generator=respawn[gen:gen+0x614]
    assert respawn[cp:cp+0x614]==ram_snapshot
    assert not any(respawn[a['enemies']+12*i+8]==14 and
                   struct.unpack_from('>i',respawn,a['enemies']+12*i)[0]==world for i in range(24))
    assert struct.unpack_from('>H',respawn,a['tanks']+16)[0]==0
    assert struct.unpack_from('>H',respawn,a['tanks']+24+16)[0]==1
    assert respawn[a['tanks']+24+12]==dead[a['tanks']+24+12]
    check('kamikaze commits in death frame; one life lost, new-section READY, no stale bridge explosion')
    check('tank on the destroyed span is absent after respawn; live bank tank is preserved')
    # A second death must keep this checkpoint, not return to stage one.
    put('gPlayerCollisionEnabled',0);runkey('LSHIFT');stop()
    put('gPlayerX',0);put('gPlayerCollisionEnabled',1)
    stop('&& ($%x).w = 4'%a['gPlayerState'])
    again=dump('second_respawn')
    assert word(again,'gEnemyBridge')==number+1 and word(again,'gPlayerLives')==1
    gen=long(again,'gpGen')
    assert again[gen:gen+0x614]==canonical_generator
    assert again[cp:cp+0x614]==ram_snapshot
    assert struct.unpack_from('>H',again,a['tanks']+16)[0]==0
    check('subsequent bank death reuses the new checkpoint')
    runkey('RSHIFT');stop();put('gPlayerX',0)
    stop('&& ($%x).w = 1'%a['gPlayerState'])
    assert word(dump('last_life_explosion'),'gPlayerLives')==0
    h.key_down('LSHIFT')
    stop_at('IKBD_Update','&& ($%x).w = 1'%a['gTitleActive'])
    r.cmd('bp all');r.cmd('run',wait=.01);h.wait(.4);r.cmd('break')
    assert word(dump('held_fire_title'),'gTitleActive')==1
    h.key_up('LSHIFT')
    runkey('LEFT')
    assert word(dump('direction_title'),'gTitleActive')==1
    runkey('RSHIFT');stop()
    fresh=dump('fresh_fire_launch')
    assert word(fresh,'gPlayerState')==0
    assert word(fresh,'gPlayerLives')==3 and word(fresh,'gEnemyBridge')==1
    assert long(fresh,'gScore')==0
    check('last life returns to title; held FIRE/arrows cannot launch; fresh FIRE starts a new run')
    put('gPlayerX',0)
    stop('&& ($%x).w = 1'%a['gPlayerState'])
    restart();check('F1 during explosion returns to a fresh game')
    # restart() stops before this frame's HUD update; compare completed pixels.
    stop_at('Screen_Update')
    quit_ready=dump('quit_ready_reference')
    escape();before=dump('quit_prompt')
    assert word(before,'gQuitConfirm')==1
    r.cmd('run');h.wait(.5);r.cmd('break');after=dump('quit_frozen')
    for n,size in [('gScroll',2),('distance',4),('gFuel',2),('ticks',2),('gPlayerState',2)]:
        assert before[a[n]:a[n]+size]==after[a[n]:a[n]+size],n
    panel=long(before,'gpPanel')
    assert any(before[panel:panel+6400])
    runkey('n');cancelled=dump('quit_cancel_n')
    assert word(cancelled,'gQuitConfirm')==0
    assert quit_ready[panel:panel+6400]==cancelled[panel:panel+6400]
    escape();escape();assert word(dump('quit_cancel_esc'),'gQuitConfirm')==0
    check('ESC confirmation freezes gameplay; N restores exact HUD bytes; ESC cancels')
    escape();r.cmd('bp all');r.cmd('run');r.s.close()
    h.tap('y',hold=.2,after=1)
    assert h.mem(a['gScreenClass'],4).u32(a['gScreenClass'])==0, 'game still running after Y'
    assert not (DISK/'RR5LOG.TXT').exists(), 'exit created a log file'
    check('Y confirms exit to TOS')
    assert not any(s in '\n'.join(h.lines).lower() for s in ('address error','bus error','illegal instruction'))
    (OUT/'report.json').write_text(json.dumps(dict(cases=results,RAM_bytes=524288,
        binary_sha256=hashlib.sha256((DISK/'RR5.PRG').read_bytes()).hexdigest(),
        limits='Natural bridge/generator; isolated aircraft, injected shot and bridge/bank tank states, collision/fuel test toggles. Not a full-campaign run.'),indent=2)+'\n')
