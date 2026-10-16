"""Verify original asset hashes and the current release/link/layout invariants."""
import hashlib
import json
import struct
import subprocess
from build_paths import ROOT, ASSETS, BUILD

baseline = json.loads((ASSETS/'migration_baseline.json').read_text())
checks = {}
for name, expected in baseline['files'].items():
    # The original executable hash documents migration, not later gameplay fixes.
    if name == 'release/RR.PRG':
        continue
    actual = hashlib.sha256((ROOT/name).read_bytes()).hexdigest()
    assert actual == expected, (name, actual, expected)
    checks[name] = actual

release = (ROOT/'release/RR.PRG').read_bytes()
assert release[:2] == bytes.fromhex('601a'), 'not a TOS executable'
assert struct.unpack_from('>I', release, 14)[0] == 0, 'release retains symbols'

symbols = subprocess.check_output(
    ['m68k-atari-mintelf-nm', str(BUILD/'RR.PRG')], text=True)
names = {line.split()[-1] for line in symbols.splitlines() if line.split()}
assert {'GodLib_Game_Main', 'RRAudio_Init', 'RRPsg_Vbl', 'rrPsgEffects'} <= names
assert not any(n.startswith(('SlugMixer', 'AudioMixer', 'rrAudioSamples')) for n in names)
assert not {'File_Init', 'File_DeInit', 'File_Open', 'File_Load'} & names
assert not any(n.startswith(('Font_', 'Fade_', 'Video_SetFalcon', 'Video_GetPalTT',
                             'Video_UpdateRegsTT', 'Audio_SaveDmaFalcon',
                             'Sprite_Create', 'Graphic_4BP_DrawPixel')) for n in names)
subset=json.loads((BUILD/'godlib/report.json').read_text())
assert subset['assembly_checks'] and all(c['identical'] for c in subset['assembly_checks'])

legacy_markers = ('../'+'reference-a8/', '../'+'river-raid/')
for folder in ('src', 'assets', 'scripts'):
    for path in (ROOT/folder).rglob('*'):
        assert path.name != '__pycache__', path
        assert path.suffix.lower() not in ('.o', '.prg', '.raw', '.map', '.d', '.pyc'), path
        if path.suffix in ('.c', '.h', '.py', '.s'):
            text = path.read_text()
            assert not any(marker in text for marker in legacy_markers), path
assert {p.name for p in (ROOT/'release').iterdir()} == {'RR.PRG'}

(BUILD/'migration_report.json').write_text(json.dumps({
    'result': 'PASS', 'checked_files': checks,
    'release_bytes': len(release), 'YM_only': True,
    'release_sha256': hashlib.sha256(release).hexdigest(),
    'file_subsystem': False,
    'layout': 'all generated products under build/; release/RR.PRG only',
    'limits': 'Original asset hashes and current link/layout verification, not a gameplay test.'
}, indent=2)+'\n')
print(f'PASS: {len(checks)} original asset hashes, {len(release)}-byte stripped RR.PRG, YM-only link, no File subsystem and clean layout')
