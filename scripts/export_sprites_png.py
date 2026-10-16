"""Lossless PNG export of the A8-derived native sprite assets (requires Pillow).

Reads generated ST4 words without running any asset generators or changing
the game. Transparency comes from masks, not from the sprite colour.
"""
import hashlib
import json
import math
from pathlib import Path
import re
import sys
from PIL import Image, ImageDraw

sys.dont_write_bytecode=True
from build_paths import ROOT, BUILD

source=(BUILD/'generated/native_assets.c').read_text()
output=BUILD/'a8-sprites'
individual=output/'individual'
individual.mkdir(parents=True,exist_ok=True)
manifest=output/'a8-sprites.json'
previous=json.loads(manifest.read_text())['sprites'] if manifest.exists() else []

def numbers(text):
    return [int(n,16) for n in re.findall(r'0x([0-9a-fA-F]+)',text)]

arrays={name:numbers(body) for name,body in re.findall(
    r'(?:static )?const uint16_t (\w+)\[[^;=]+\]\s*=\s*\{(.*?)\n\};',source,re.S)}
palette=arrays['rrPalette']

def rgb(value):
    # STE nibble bits 2:0 are the high DAC bits; bit 3 is its low bit.
    return tuple((((value>>shift)&7)*2+((value>>(shift+3))&1))*17 for shift in (8,4,0))

colours=[rgb(value) for value in palette]

def name(group,pose):
    if group=='player':
        return ('player_neutral','player_right','player_left',
                'player_explosion_1','player_explosion_2','player_explosion_3','player_explosion_4')[pose]
    kinds={7:'jet',8:'helicopter_a',9:'helicopter_b',10:'armed_helicopter_a',
           11:'armed_helicopter_b',12:'balloon',13:'boat'}
    if pose in kinds:return kinds[pose]+'_right'
    if pose-16 in kinds:return kinds[pose-16]+'_left'
    for base,kind in ((32,'helicopter_a'),(42,'helicopter_b'),(52,'armed_helicopter_a'),
                      (62,'armed_helicopter_b'),(72,'boat'),(88,'jet'),(98,'balloon')):
        if base<=pose<base+10:
            phase=pose-base
            return f'{kind}_explosion_{"left" if phase>=5 else "right"}_{phase%5+1}'
    if 82<=pose<=87:return 'fuel' if pose==82 else f'fuel_explosion_{pose-82}'
    if 108<=pose<=113:return 'bridge' if pose==108 else f'bridge_explosion_{pose-108}'
    if 114<=pose<=121:return f'tank_{pose-114}'
    if 122<=pose<=128:return f'projectile_{pose-122}'
    raise ValueError(pose)

sprites=[]
for table,group in (('rrPlayerSprites','player'),('rrEnemySprites','enemy')):
    body=re.search(r'const sSprite '+table+r'\[\d+\] = \{(.*?)\n\};',source,re.S)[1]
    for match in re.finditer(r'\[(\d+)\] = \{\(U16\*\)(\w+), \(U16\*\)(\w+)(?:\[(\d+)\])?, (\d+), (\d+), 4, 4\}',body):
        pose=int(match[1]);gfx=arrays[match[2]];mask=arrays[match[3]]
        width,height=int(match[5]),int(match[6])
        if match[4] is not None:
            start=int(match[4])*height
            mask=mask[start:start+height]
        groups=width//16
        assert len(mask)==groups*height and len(gfx)==groups*height*4
        bitmap=Image.new('RGBA',(width,height))
        for y in range(height):
            for x in range(width):
                word=y*groups+x//16
                bit=0x8000>>(x&15)
                ink=sum((1<<plane) for plane in range(4) if gfx[word*4+plane]&bit)
                if not mask[word]&bit:bitmap.putpixel((x,y),(*colours[ink],255))
        label=name(group,pose)
        filename=f'{group}_{pose:03d}_{label}.png'
        bitmap.save(individual/filename)
        assert Image.open(individual/filename).convert('RGBA').tobytes()==bitmap.tobytes()
        sprites.append((bitmap,dict(group=group,pose=pose,name=label,file='individual/'+filename,
                                    width=width,height=height,empty=bitmap.getbbox() is None)))

columns,cell_w,cell_h=8,80,40
rows=math.ceil(len(sprites)/columns)
atlas=Image.new('RGBA',(columns*cell_w,rows*cell_h))
preview=Image.new('RGB',atlas.size)
draw=ImageDraw.Draw(preview)
for y in range(0,preview.height,8):
    for x in range(0,preview.width,8):
        draw.rectangle((x,y,x+7,y+7),fill='#9baeb7' if (x//8+y//8)&1 else '#c7d2d7')
for index,(bitmap,metadata) in enumerate(sprites):
    cx=(index%columns)*cell_w;cy=(index//columns)*cell_h
    x=cx+(cell_w-bitmap.width)//2;y=cy+2
    atlas.paste(bitmap,(x,y))
    preview.paste(bitmap,(x,y),bitmap)
    draw.rectangle((cx,cy+27,cx+cell_w-1,cy+39),fill='#162b35')
    draw.text((cx+3,cy+28),f'{metadata["group"][0].upper()}{metadata["pose"]:03d}',fill='#edf4e7')
    metadata['atlas_rect']=[x,y,bitmap.width,bitmap.height]
atlas.save(output/'a8-sprites.png')
preview.resize((preview.width*2,preview.height*2),Image.Resampling.NEAREST).save(output/'a8-sprites-preview.png')
reloaded=Image.open(output/'a8-sprites.png').convert('RGBA')
for bitmap,metadata in sprites:
    x,y,w,h=metadata['atlas_rect']
    assert reloaded.crop((x,y,x+w,y+h)).tobytes()==bitmap.tobytes()
current_files={metadata['file'] for _,metadata in sprites}
for old in previous:
    path=output/old['file']
    if old['file'] not in current_files and path.parent==individual:
        path.unlink(missing_ok=True)
manifest.write_text(json.dumps(dict(
    source='Atari 800 ROM sprites as converted by the current STE port',
    palette='Current 16-colour STE palette, not a generic 256-colour A8 palette',
    source_sha256=hashlib.sha256(source.encode()).hexdigest(),
    width=atlas.width,height=atlas.height,palette_rgb=colours,
    sprites=[metadata for _,metadata in sprites]),indent=2)+'\n')
print(f'PASS: {len(sprites)} sprites, transparent {atlas.width}x{atlas.height} atlas, exact PNG round-trip; {output}')
