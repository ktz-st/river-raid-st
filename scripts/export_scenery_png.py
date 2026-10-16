"""Export the six ROM playfield decorations, in the same palette as the port.

ANTIC E doubles horizontal pixels; source scanlines are emitted upward by
the original generator. Selector 1 is land padding, not part of the mountain.
"""
import hashlib
import json
import sys
from PIL import Image, ImageDraw

sys.dont_write_bytecode=True
from build_paths import ASSETS, BUILD
from st_palette import ST_PALETTE

rom=(ASSETS/'RIVER_A8.BIN').read_bytes()
assert len(rom)==8192
out=BUILD/'a8-scenery'
individual=out/'individual'
individual.mkdir(parents=True,exist_ok=True)

def read(address,size):
    assert 0xa000<=address<=address+size<=0xc000
    return rom[address-0xa000:address-0xa000+size]

def rgb(value):
    return tuple((((value>>shift)&7)*2+((value>>(shift+3))&1))*17 for shift in (8,4,0))

palette=[rgb(value) for value in ST_PALETTE]
# BBB2 supplies byte widths, which index both BBEB and BBF0.
widths=sorted(set(read(0xbbb2,26)))
assert widths==[3,4,5,6,8,10]
atlas=Image.new('RGBA',(3*88,2*52))
land_atlas=Image.new('RGB',atlas.size,palette[1])
preview=Image.new('RGB',atlas.size)
draw=ImageDraw.Draw(preview)
for y in range(0,preview.height,8):
    for x in range(0,preview.width,8):
        draw.rectangle((x,y,x+7,y+7),fill='#9baeb7' if (x//8+y//8)&1 else '#c7d2d7')
entries=[]
for index,width_bytes in enumerate(widths):
    address=int.from_bytes(read(0xbbf0+2*width_bytes,2),'little')
    height=read(0xbbeb+width_bytes,1)[0]
    raw=read(address,width_bytes*height)
    width=width_bytes*8
    image=Image.new('RGBA',(width,height))
    on_land=Image.new('RGB',(width,height),palette[1])
    for y in range(height):
        source_y=height-1-y
        for x in range(width_bytes*4):
            selector=(raw[source_y*width_bytes+x//4]>>(6-2*(x&3)))&3
            for duplicate in range(2):
                point=(x*2+duplicate,y)
                if selector!=1:image.putpixel(point,(*palette[selector],255))
                on_land.putpixel(point,palette[selector])
    name=f'mountain_{address:04x}'
    image.save(individual/(name+'.png'))
    assert Image.open(individual/(name+'.png')).convert('RGBA').tobytes()==image.tobytes()
    cx=(index%3)*88;cy=(index//3)*52
    x=cx+(88-width)//2;y=cy+3
    atlas.paste(image,(x,y))
    preview.paste(image,(x,y),image)
    land_atlas.paste(on_land,(x,y))
    draw.rectangle((cx,cy+36,cx+87,cy+51),fill='#162b35')
    draw.text((cx+3,cy+38),f'{address:04X} {width}x{height}',fill='#edf4e7')
    entries.append(dict(name=name,address=f'${address:04X}',width_bytes=width_bytes,
        width=width,height=height,transparent_selector=1,atlas_rect=[x,y,width,height],
        file='individual/'+name+'.png',raw_sha256=hashlib.sha256(raw).hexdigest()))
atlas.save(out/'a8-scenery.png')
land_atlas.save(out/'a8-scenery-on-land.png')
preview.resize((preview.width*4,preview.height*4),Image.Resampling.NEAREST).save(out/'a8-scenery-preview.png')
reloaded=Image.open(out/'a8-scenery.png').convert('RGBA')
for entry in entries:
    x,y,w,h=entry['atlas_rect']
    reference=Image.open(out/entry['file']).convert('RGBA')
    assert reloaded.crop((x,y,x+w,y+h)).tobytes()==reference.tobytes()
(out/'a8-scenery.json').write_text(json.dumps(dict(
    source='Original A8 ROM playfield decoration tables BBB2/BBEB/BBF0',
    palette='Current STE port palette, matching the exported sprite atlas',
    scale='Two display pixels per ANTIC E logical pixel; original vertical height',
    houses='No separate house bitmaps found in these ROM decoration tables.',
    entries=entries),indent=2)+'\n')
print(f'PASS: {len(entries)} mountain bitmaps, transparent atlas and exact PNG round-trip; {out}')
