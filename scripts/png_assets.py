"""Strict indexed-STE PNG input for the production asset converter."""
import json
from PIL import Image
from build_paths import ASSETS
from st_palette import ST_PALETTE

def rgb(value):
    return tuple((((value>>shift)&7)*2+((value>>(shift+3))&1))*17 for shift in (8,4,0))

PALETTE=[component for value in ST_PALETTE for component in rgb(value)]

def indexed(path,transparent):
    image=Image.open(path)
    image.load()
    assert image.mode=='P',f'{path}: use indexed colour mode, not RGB'
    assert image.getpalette()[:48]==PALETTE,f'{path}: STE palette indices changed'
    alpha=image.info.get('transparency')
    if isinstance(alpha,bytes):
        assert all(v==(0 if i==transparent else 255) for i,v in enumerate(alpha)),path
        assert len(alpha)>transparent and alpha[transparent]==0,path
    else:
        assert alpha==transparent,f'{path}: wrong transparent index'
    assert all(v<16 for v in image.getdata()),f'{path}: more than 16 colour indices'
    return image

def load_sprites():
    manifest=json.loads((ASSETS/'sprites.json').read_text())
    image=indexed(ASSETS/'sprites.png',0)
    result={'enemy':{},'player':{}}
    for frame in manifest['frames']:
        x,y,width,height=frame['rect']
        assert x>=0 and y>=0 and x+width<=image.width and y+height<=image.height
        assert width in (16,32,64) and height>0
        groups=width//16
        masks=[];gfx=[]
        for row in range(height):
            for group in range(groups):
                bits=0;planes=[0]*4
                for bit in range(16):
                    ink=image.getpixel((x+group*16+bit,y+row))
                    if ink:
                        bits|=0x8000>>bit
                        for plane in range(4):
                            if ink&(1<<plane):planes[plane]|=0x8000>>bit
                masks.append(bits^65535)
                gfx.extend(planes)
        kind=frame['group'];pose=frame['pose']
        assert kind in result and pose not in result[kind]
        result[kind][pose]=(width,height,tuple(masks),tuple(gfx))
    assert set(result['player'])==set(range(7))
    assert len(result['enemy'])==111
    return result['enemy'],result['player']

def load_scenery():
    manifest=json.loads((ASSETS/'scenery.json').read_text())
    image=indexed(ASSETS/'scenery.png',1)
    rows=bytearray(744)
    covered=set()
    for frame in manifest['frames']:
        x,y,width,height=frame['rect']
        address=frame['address']
        assert width%8==0 and x>=0 and y>=0 and x+width<=image.width and y+height<=image.height
        stride=width//8
        offset=address-0xbc06
        assert 0<=offset<offset+stride*height<=len(rows)
        for row in range(height):
            for column in range(stride):
                packed=0
                for pixel in range(4):
                    px=x+column*8+pixel*2
                    ink=image.getpixel((px,y+row))
                    assert ink<4 and ink==image.getpixel((px+1,y+row)), \
                        f'{frame["name"]}: scenery pixels must stay in equal horizontal pairs, indices 0..3'
                    packed|=ink<<(6-pixel*2)
                target=offset+(height-1-row)*stride+column
                assert target not in covered
                covered.add(target);rows[target]=packed
    assert len(covered)==len(rows)
    return bytes(rows)
