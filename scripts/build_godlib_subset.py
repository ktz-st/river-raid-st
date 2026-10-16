"""Build a private STE GodLib subset; never edit the shared library.

The MiNT PRG backend does not discard individual input functions here. Run
section GC in a relocatable m68k ELF link, then convert that object to PRG.
Original assembly sprite/video kernels are extracted verbatim, not rewritten.
"""
import argparse
from pathlib import Path
import re
import shutil
import struct
import subprocess
import json

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--godlib',required=True)
parser.add_argument('--cmini',required=True)
parser.add_argument('objects',nargs='+')
args=parser.parse_args()
root=Path(__file__).resolve().parent.parent
god=Path(args.godlib).resolve()
cmini=Path(args.cmini).resolve()
out=root/'build/godlib'
out.mkdir(parents=True,exist_ok=True)
prefix='m68k-atari-mintelf-'

def run(command):
    return subprocess.check_output([str(x) for x in command],cwd=root,text=True)

def span(text,start,end):
    first=re.search('^'+re.escape(start),text,re.M)
    last=re.search('^'+re.escape(end),text,re.M)
    assert first and last and first.start()<last.start(),(start,end)
    return text[first.start():last.start()]

def asm_subset(relative,exports,body):
    original=(god/relative).read_text()
    header=re.split(r'^\s*TEXT\s*$',original,maxsplit=1,flags=re.M)[0]
    header=re.sub(r'^\s*X(?:DEF|REF)\s+.*$', '',header,flags=re.M|re.I)
    external={'video/video_s.s':['gVideo'], 'audio/audio_s.s':['gAudioSavedYM']}.get(relative,[])
    selected=header+'\n'+'\n'.join('\tXDEF '+n for n in exports)+'\n'
    selected+='\n'.join('\tXREF '+n for n in external)+'\n\tTEXT\n'+body(original)
    dest=out/Path(relative).name
    dest.write_text(selected)
    obj=dest.with_suffix('.o')
    run(['vasmm68k_mot','-nosym','-devpac','-Felf','-I'+str((god/relative).parent)+'/',dest,'-o',obj])
    return obj

sprite=asm_subset('graphic/grf_b4_s.s',
    ['Graphic_4BP_DrawSprite_BLT','Graphic_4BP_DrawSprite_Clip_BLT'],
    lambda s:span(s,'Graphic_4BP_DrawSprite_Clip_BLT:','Atari_BlitInit:')+
    '\n\tDATA\n'+span(s,'gGraphic_4BP_LeftMasks:','gGraphic_4BP_DoubleLeftMasks:')+
    span(s,'gGraphic_4BP_RightMasks:','gGraphic_4BP_DoubleRightMasks:'))
video_names=['Video_SetViewportSTE','Video_SaveRegsSTE','Video_RestoreRegsSTE',
             'Video_GetPalST','Video_SetPalST','Video_UpdateRegsSTE']
video=asm_subset('video/video_s.s',video_names,
    lambda s: ''.join(span(s,a+':',b+':') for a,b in (
        ('Video_SetViewportSTE','Video_SaveRegsST'),
        ('Video_SaveRegsSTE','Video_SaveRegsTT'),
        ('Video_GetPalST','Video_GetPalTT'),
        ('Video_UpdateRegsSTE','Video_UpdateRegsTT'))))
audio=asm_subset('audio/audio_s.s',
    ['Audio_SoundChipOff','Audio_SaveYM','Audio_RestoreYM','Audio_SaveDmaSte',
     'Audio_RestoreDmaSte','Audio_MicroWireWrite','gAudioDmaPlayingFlag'],
    lambda s:span(s,'Audio_SoundChipOff:','Audio_SaveDmaFalcon:')+
    span(s,'Audio_MicroWireWrite:','Audio_MicroWireWriteold:')+
    '\n'+s[s.index('\tDATA'):])

# Discover archive members with the normal linker first. This keeps the subset
# tied to actual game dependencies instead of a brittle hand-maintained list.
link_flags=['-nostdlib','-nostartfiles','-nodefaultlibs','-static','-mshort','-mfastcall','-m68000']
crt=cmini/'lib/crt0.o'
run([prefix+'gcc',*link_flags,'-Wl,--relax','-Wl,-Map='+str(out/'discovery.map'),
     '-L'+str(cmini/'lib/mshort/mfastcall'),'-L'+str(god),'-o',out/'discovery.prg',
     crt,*args.objects,'-Wl,--start-group','-lgod','-lcmini','-lgcc','-Wl,--end-group'])
members=sorted(set(re.findall(r'libgod\.a\(([^)]+)\)',(out/'discovery.map').read_text())))
sources={p.stem.lower()+'.o':p for p in god.rglob('*.c')}
archive=out/'libgod.a'
shutil.copy2(god/'libgod.a',archive)
run([prefix+'ar','d',archive,'grf_b4_s.o','video_s.o','audio_s.o'])
compiled=[]
for member in members:
    if member not in sources:
        continue
    source=sources[member]
    content=source.read_text()
    if member in ('video.o','audio.o'):
        content=re.sub(r'\bSystem_GetVDO\(\s*\)', 'VDO_STE',content)
        content=re.sub(r'\bSystem_GetMCH\(\s*\)', 'MCH_STE',content)
        if member=='video.o':
            content=content.replace('mPalFalcon[256]','mPalFalcon[1]')
    dest=out/source.name
    dest.write_text(content)
    obj=out/member
    run([prefix+'gcc','-c','-mshort','-mfastcall','-m68000','-Os','-Wall',
         '-fno-strict-aliasing','-static','-g','-ffunction-sections','-fdata-sections',
         '-I'+str(source.parent),'-I'+str(god.parent),dest,'-o',obj])
    compiled.append(obj)
run([prefix+'ar','rcs',archive,*compiled])
gcc_library=run([prefix+'gcc','-mshort','-mfastcall','-print-libgcc-file-name']).strip()
linked=out/'linked.o'
run([prefix+'ld','-r','-m','m68kelf','--gc-sections','-u','_start',
     '-Map='+str(out/'gc.map'),'-o',linked,crt,*args.objects,sprite,video,audio,
     '--start-group',archive,cmini/'lib/mshort/mfastcall/libcmini.a',gcc_library,'--end-group'])

# ld -r keeps unused undefined names from discarded functions. Remove only
# those names, not static game symbols used by the debugger/regression tests.
def elf(path):
    data=path.read_bytes()
    shoff=struct.unpack_from('>I',data,32)[0]
    entsize,count,names=struct.unpack_from('>HHH',data,46)
    sections=[struct.unpack_from('>10I',data,shoff+i*entsize) for i in range(count)]
    names_data=data[sections[names][4]:sections[names][4]+sections[names][5]]
    def string(blob,n):return blob[n:blob.index(b'\0',n)].decode()
    section_names=[string(names_data,s[0]) for s in sections]
    symbols={}
    for index,s in enumerate(sections):
        if s[1]!=2:continue
        strings=sections[s[6]]
        blob=data[strings[4]:strings[4]+strings[5]]
        symbols[index]=[(string(blob,n),value,size,info,other,sh)
            for n,value,size,info,other,sh in
            (struct.unpack_from('>IIIBBH',data,p) for p in range(s[4],s[4]+s[5],16))]
    return data,sections,section_names,symbols

data,sections,_,symbols=elf(linked)
referenced=set()
for s in sections:
    if s[1] not in (4,9):continue
    for p in range(s[4],s[4]+s[5],s[9]):
        info=struct.unpack_from('>I',data,p+4)[0]
        referenced.add((s[6],info>>8))
unused=[symbol[0] for table,entries in symbols.items() for i,symbol in enumerate(entries)
        if symbol[0] and symbol[5]==0 and (table,i) not in referenced]
if unused:
    run([prefix+'objcopy',*['--strip-symbol='+n for n in unused],linked])

# Compare the retained assembly kernels with the original object, masking
# relocation fields only. Every instruction and every local branch must match.
def normalized(path,start,end):
    data,sections,names,tables=elf(path)
    syms={entry[0]:entry for entries in tables.values() for entry in entries}
    first=syms[start]; section=first[5]; begin=first[1]
    finish=begin+end if isinstance(end,int) else syms[end][1] if end else sections[section][5]
    blob=bytearray(data[sections[section][4]+begin:sections[section][4]+finish])
    for s in sections:
        if s[1] not in (4,9) or s[7]!=section:continue
        for p in range(s[4],s[4]+s[5],s[9]):
            offset,info=struct.unpack_from('>II',data,p)
            if begin<=offset<finish:
                width={1:4,2:2,3:1,4:4,5:2,6:1}[info&255]
                blob[offset-begin:offset-begin+width]=bytes(width)
    return bytes(blob)

checks=[]
for member,new,start in [
    ('grf_b4_s.o',sprite,'Graphic_4BP_DrawSprite_Clip_BLT'),
    ('video_s.o',video,'Video_UpdateRegsSTE')]:
    old=out/('original_'+member)
    old.write_bytes(subprocess.check_output([prefix+'ar','p',str(god/'libgod.a'),member]))
    extracted=normalized(new,start,None)
    original=normalized(old,start,len(extracted))
    assert original==extracted,(start,len(original),len(extracted))
    checks.append(dict(kernel=start,bytes=len(original),identical=True))
(out/'report.json').write_text(json.dumps(dict(rebuilt_C_members=len(compiled),
    assembly_checks=checks,unused_undefined_removed=len(unused)),indent=2)+'\n')
print('Private STE subset ready; original sprite/VBL instructions verified.')
