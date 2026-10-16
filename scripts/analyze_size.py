"""Account for the linked TOS image; optional isolated GodLib section-GC probe."""
import argparse
from collections import defaultdict
import hashlib
import json
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
sys.dont_write_bytecode = True
from build_paths import ROOT, BUILD

ALLOC = {'.text', 'text', 'CODE', '.rodata', '.data', 'DATA', '.bss', 'BSS'}
CODE = {'.text', 'text', 'CODE'}

def allocated(section):
    return section in ALLOC or section.startswith(('.text.','.rodata.','.data.','.bss.'))

def section_kind(section):
    return 'bss' if section in ('.bss','BSS') or section.startswith('.bss.') else \
        'code' if section in CODE or section.startswith('.text.') else 'data'

def run(args):
    return subprocess.check_output([str(x) for x in args], cwd=ROOT, text=True)

def read_map(path):
    body = path.read_text().split('Linker script and memory map\n', 1)[1]
    sections, records, loads = {}, [], []
    current = pending = None
    for line in body.splitlines():
        if line.startswith('LOAD '):
            loads.append(line[5:])
        output = re.match(r'^(\S+)\s+0x([\da-f]+)\s+0x([\da-f]+)', line)
        if output:
            current = output[1]
            if allocated(current):
                sections[current] = dict(address=int(output[2],16), bytes=int(output[3],16))
        elif line and not line[0].isspace():
            current = line.strip() if allocated(line.strip()) else None
            pending = None
        if not current or not allocated(current):
            continue
        continuation_output=re.match(r'^\s+0x([\da-f]+)\s+0x([\da-f]+)\s*$',line)
        if continuation_output:
            sections[current]=dict(address=int(continuation_output[1],16),bytes=int(continuation_output[2],16))
            continue
        entry = re.match(r'^ (\S+)\s+0x([\da-f]+)\s+0x([\da-f]+)\s+(\S+\.o\)?)$', line)
        if entry:
            records.append(dict(output=current, section=entry[1], address=int(entry[2],16),
                                bytes=int(entry[3],16), object=entry[4]))
            pending = None
        elif re.match(r'^ \S+$', line):
            pending = line.strip()
        elif pending:
            continuation = re.match(r'^\s+0x([\da-f]+)\s+0x([\da-f]+)\s+(\S+\.o\)?)$', line)
            if continuation:
                records.append(dict(output=current, section=pending, address=int(continuation[1],16),
                                    bytes=int(continuation[2],16), object=continuation[3]))
            pending = None
    return sections, records, loads

def owner(name):
    if '/build/godlib/' in name or name in ('build/ste_graphic.o','build/ste_platform.o'):
        return 'GodLib'
    if name.startswith('build/'):
        return 'game'
    if 'libgod.a(' in name:
        return 'GodLib'
    if 'libcmini.a(' in name:
        return 'libcmini'
    if 'libgcc.a(' in name:
        return 'libgcc'
    return 'crt0'

def image(path, map_path):
    sections, records, loads = read_map(map_path)
    if 'build/godlib/linked.o' in loads:
        # The relocatable GC link retains attribution to the original inputs.
        # Final PRG layout merges these sections into a single linked object.
        _, records, _ = read_map(BUILD/'godlib/gc.map')
    groups = defaultdict(lambda: dict(code=0, data=0, bss=0))
    objects = defaultdict(lambda: dict(code=0, data=0, bss=0))
    for r in records:
        kind = section_kind(r['output'])
        groups[owner(r['object'])][kind] += r['bytes']
        objects[r['object']][kind] += r['bytes']
    raw = path.read_bytes()
    magic, text, data, bss, symbols, reserved, flags, absolute = struct.unpack_from('>H6IH',raw)
    assert magic == 0x601a
    payload = text+data
    assert sum(s['bytes'] for k,s in sections.items() if section_kind(k)!='bss') <= payload
    assert sum(v['code']+v['data'] for v in objects.values()) <= payload
    return dict(file_bytes=len(raw), text=text, data=data, bss=bss, payload=payload,
                section_table_bytes=symbols, header_bytes=28,
                relocation_bytes=len(raw)-28-text-data-symbols,
                groups=dict(groups), objects=dict(objects), sections=sections,
                sha256=hashlib.sha256(raw).hexdigest()), loads

def nm_sizes(path):
    result = {}
    for line in run(['m68k-atari-mintelf-nm','-S',str(path)]).splitlines():
        p = line.split()
        if len(p)==4:
            result[p[3]] = int(p[1],16)
    return result

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--probe-sections',action='store_true',help='Recompile only a private GodLib archive; never replace release or shared library')
args = parser.parse_args()
out = BUILD/'size_audit'
out.mkdir(exist_ok=True)
base, loads = image(BUILD/'RR.RAW',BUILD/'RR.map')
sizes = nm_sizes(BUILD/'native_assets.o')
native_bytes = base['objects']['build/native_assets.o']['data']
native_tables = sum(sizes[n] for n in ('rrGeneratorTables','rrSceneDefaults','rrGameDefaults','rrRoadInks','rrPalette'))
hud_bytes = base['objects']['build/hud_masks.o']['data']
audio_bytes = base['objects']['build/audio_psg_data.o']['data']
categories = {
    'game_code_C_and_68000': base['groups']['game']['code'],
    'GodLib_code_and_data': sum(base['groups']['GodLib'][k] for k in ('code','data')),
    'libcmini_and_libgcc_code_data': sum(base['groups'][g][k] for g in ('libcmini','libgcc') for k in ('code','data')),
    'crt0_code_data': sum(base['groups']['crt0'][k] for k in ('code','data')),
    'sprites_masks_preshifts_descriptors': native_bytes-native_tables,
    'HUD_masks_and_lookup_tables': hud_bytes,
    'generator_defaults_palette_road_tables': native_tables,
    'YM_register_sequences': audio_bytes,
    'other_game_data_and_shared_string_pool': base['groups']['game']['data']-native_bytes-hud_bytes-audio_bytes,
}
categories['headers_padding_ELF_metadata_relocations'] = base['file_bytes']-sum(categories.values())
assert sum(categories.values()) == base['file_bytes']
report = dict(baseline=base, categories=categories,
              note='BSS is not on disk. Screens and reserved stack are additional RAM. Merged strings are attributed as in the linker map; nm function sizes are NOT used for assembly boundaries.')

if args.probe_sections:
    if 'build/godlib/linked.o' in loads:
        parser.error('The private subset already uses ELF section GC; the legacy probe is not applicable.')
    probe = out/'sections_probe'
    probe.mkdir(exist_ok=True)
    archive = probe/'libgod.a'
    shutil.copy2(ROOT/'../godlib/libgod.a',archive)
    source_files = run(['rg','--files','../godlib','-g','*.[cC]']).splitlines()
    source_by_name = defaultdict(list)
    for filename in source_files:
        source_by_name[Path(filename).stem.lower()+'.o'].append(filename)
    members = sorted({re.search(r'libgod\.a\(([^)]+)\)',n)[1]
                      for n in base['objects'] if 'libgod.a(' in n})
    compiled = []
    for member in members:
        sources = source_by_name[member]
        if not sources:
            continue  # Original assembly members remain byte-identical.
        assert len(sources)==1,(member,sources)
        obj = probe/member
        command = ['m68k-atari-mintelf-gcc','-c','-mshort','-mfastcall','-Os','-Wall',
                   '-fno-strict-aliasing','-static','-g','-DdGODLIB_FADE','-I..',
                   '-ffunction-sections','-fdata-sections',sources[0],'-o',obj]
        subprocess.run([str(x) for x in command],cwd=ROOT,check=True)
        compiled.append(obj)
    run(['m68k-atari-mintelf-ar','rcs',archive,*compiled])
    program = probe/'RR_SECTIONS.RAW'
    probe_map = probe/'RR_SECTIONS.map'
    inputs = [n for n in loads if n.startswith('build/') or n.endswith('/crt0.o')]
    run(['m68k-atari-mintelf-gcc','-s','-nostdlib','-nostartfiles','-nodefaultlibs','-static',
         '-mshort','-mfastcall','-m68000','-Wl,--gc-sections','-Wl,--relax',
         '-Wl,-Map='+str(probe_map),'-L'+str(probe),'-L../libcmini/lib/mshort/mfastcall','-L../godlib',
         '-o',program,*inputs,'-Wl,--start-group','-lgod','-lcmini','-lgcc','-Wl,--end-group'])
    run(['m68k-atari-mintelf-strip','--strip-unneeded',program])
    trial,_ = image(program,probe_map)
    packed = probe/'RR_SECTIONS.PRG'
    run(['upx','-9','--force-overwrite','-o',packed,program])
    report['sections_probe'] = dict(image=trial, rebuilt_C_members=len(compiled),
        raw_saving=base['file_bytes']-trial['file_bytes'],
        payload_saving=base['payload']-trial['payload'], bss_saving=base['bss']-trial['bss'],
        packed_bytes=packed.stat().st_size,
        limits='Private diagnostic link only; no source or shared-library changes. Not yet run/profiled in Hatari; no FPS guarantee.')

no_file = out/'RR_NOFILE.RAW'
if no_file.exists() and (out/'RR_NOFILE.map').exists() and no_file.stat().st_mtime>=(BUILD/'RR.RAW').stat().st_mtime:
    trial, _ = image(no_file, out/'RR_NOFILE.map')
    report['no_file_probe'] = dict(
        raw_bytes=trial['file_bytes'], raw_saving=base['file_bytes']-trial['file_bytes'],
        packed_bytes=(out/'RR_NOFILE.PRG').stat().st_size,
        limits='File_Init/DeInit wrapped to no-ops in a diagnostic link only. TOS lifecycle and runtime not verified.')

(out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
rows = ['# RR Size Audit','', '| Component | Bytes | KiB |','|---|---:|---:|']
for name,value in categories.items():
    rows.append(f'| {name} | {value} | {value/1024:.2f} |')
rows += [f'| TOTAL raw file | {base["file_bytes"]} | {base["file_bytes"]/1024:.2f} |', '',
         f'BSS: {base["bss"]} bytes (RAM only, not part of file).', '', '## Linked objects', '',
         '| Object | Code | Data | BSS |','|---|---:|---:|---:|']
for name,values in sorted(base['objects'].items(),key=lambda x:x[1]['code']+x[1]['data'],reverse=True):
    if sum(values.values()):
        rows.append(f'| {name} | {values["code"]} | {values["data"]} | {values["bss"]} |')
if 'sections_probe' in report:
    p=report['sections_probe']
    rows += ['', '## Isolated section-GC probe', '',
             f'Raw saving: {p["raw_saving"]} bytes; payload saving: {p["payload_saving"]}; BSS saving: {p["bss_saving"]}.',
             f'Packed probe: {p["packed_bytes"]} bytes. {p["limits"]}']
if 'no_file_probe' in report:
    p = report['no_file_probe']
    rows += ['', '## Isolated file-initialization probe', '',
             f'Raw saving: {p["raw_saving"]} bytes; packed size: {p["packed_bytes"]} bytes. {p["limits"]}']
rows += ['', '## Recommendations', '',
         '- Keep unrolled sprite/road kernels (5826 bytes), preshifted masks and native HUD glyphs: they buy frame time.',
         '- File initialization has been removed. The current private STE subset also removes generic graphics dispatch, fonts/converters, fades and unused video/audio machine branches.',
         '- Generic machine detection and hardware save/restore remain deliberately conservative; do not remove interrupt or TOS lifecycle support blindly.',
         '- Do not remove all planar sprite data: clipped enemies and the wide bridge still use generic rendering; collision masks are also required.',
         '- Generator tables (1056 bytes) and YM sequences (2004 bytes) are small; unpacking them during play is a poor first tradeoff.',
         '- BSS is RAM, not disk size. Reducing checkpoint/screen memory does not directly shrink this executable.',
         '- Probe links are size measurements only, not evidence of unchanged pixels or performance. Production release is untouched.']
(out/'report.md').write_text('\n'.join(rows)+'\n')
print(json.dumps(dict(file_bytes=base['file_bytes'], categories=categories,
                     bss=base['bss'],
                     sections_probe={k:v for k,v in report.get('sections_probe',{}).items() if k!='image'},
                     no_file_probe=report.get('no_file_probe',{})),indent=2))
