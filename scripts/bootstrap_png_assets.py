"""One-time migration of audited exports to editable indexed source atlases.

Not part of make: never regenerate over an artist's source images.
"""
import json
from PIL import Image,ImageDraw
from build_paths import ASSETS,BUILD
from png_assets import PALETTE,rgb
from st_palette import ST_PALETTE

assert not any((ASSETS/name).exists() for name in ('sprites.png','sprites.json','scenery.png','scenery.json'))
old=json.loads((BUILD/'a8-sprites/a8-sprites.json').read_text())
lookup={(f['group'],f['pose']):f for f in old['sprites']}
rows=[('player',[('player',i) for i in range(7)])]
for kind,label,base in ((13,'boat',72),(7,'jet',88),(8,'helicopter_a',32),
                        (9,'helicopter_b',42),(10,'armed_helicopter_a',52),
                        (11,'armed_helicopter_b',62),(12,'balloon',98)):
    for direction in range(2):
        poses=[kind+direction*16]+list(range(base+direction*5,base+direction*5+5))
        rows.append((label+('_left' if direction else '_right'),[('enemy',p) for p in poses]))
for label,poses in (('fuel',range(82,88)),('bridge',range(108,114)),
                    ('tank',range(114,122)),('projectile',range(122,129))):
    rows.append((label,[('enemy',p) for p in poses]))
sheet=Image.new('P',(640,len(rows)*32),0);sheet.putpalette(PALETTE)
colours={rgb(value):i for i,value in enumerate(ST_PALETTE)}
frames=[];tags=[]
for row,(label,poses) in enumerate(rows):
    tags.append(dict(name=label,row=row,count=len(poses)))
    for column,key in enumerate(poses):
        entry=lookup[key]
        image=Image.open(BUILD/'a8-sprites'/entry['file']).convert('RGBA')
        x=column*80+8;y=row*32+2
        for yy in range(image.height):
            for xx in range(image.width):
                pixel=image.getpixel((xx,yy))
                if pixel[3]:
                    ink=colours[pixel[:3]]
                    assert ink!=0
                    sheet.putpixel((x+xx,y+yy),ink)
        frames.append(dict(group=key[0],pose=key[1],name=entry['name'],
                           rect=[x,y,image.width,image.height]))
assert len(frames)==len(lookup)==118
sheet.save(ASSETS/'sprites.png',bits=4,transparency=0)
(ASSETS/'sprites.json').write_text(json.dumps(dict(cell=[80,32],columns=8,
    origin_in_cell=[8,2],transparent_index=0,rows=tags,frames=frames),indent=2)+'\n')

old=json.loads((BUILD/'a8-scenery/a8-scenery.json').read_text())
sheet=Image.new('P',(264,80),1);sheet.putpalette(PALETTE)
colours={rgb(ST_PALETTE[i]):i for i in range(4)}
frames=[]
for i,entry in enumerate(old['entries']):
    image=Image.open(BUILD/'a8-scenery'/entry['file']).convert('RGBA')
    x=i%3*88+4;y=i//3*40+2
    for yy in range(image.height):
        for xx in range(image.width):
            pixel=image.getpixel((xx,yy))
            if pixel[3]:sheet.putpixel((x+xx,y+yy),colours[pixel[:3]])
    frames.append(dict(name=entry['name'],address=int(entry['address'][1:],16),
                       rect=[x,y,image.width,image.height]))
sheet.save(ASSETS/'scenery.png',bits=4,transparency=1)
(ASSETS/'scenery.json').write_text(json.dumps(dict(cell=[88,40],columns=3,
    origin_in_cell=[4,2],transparent_index=1,frames=frames),indent=2)+'\n')
print('Indexed source atlases created; this script is not a build dependency.')
