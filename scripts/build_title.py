"""Indexed STE title -> ST-low planar words and a small LZSS stream."""
from collections import defaultdict,deque
import json
import struct
from PIL import Image
from build_paths import ASSETS,BUILD,GENERATED

image=Image.open(ASSETS/'title.png')
assert image.mode=='P' and image.size==(320,200)
assert 'transparency' not in image.info
palette=image.getpalette()[:48]
assert len(palette)==48 and all(v%17==0 for v in palette),'Use exact 4-bit STE DAC colours'
assert all(v<16 for v in image.getdata())
def nibble(v):
    v//=17
    return (v>>1)|((v&1)<<3)
hardware=[(nibble(palette[i])<<8)|(nibble(palette[i+1])<<4)|nibble(palette[i+2]) for i in range(0,48,3)]
raw=bytearray()
for y in range(200):
    for group in range(20):
        words=[0]*4
        for bit in range(16):
            ink=image.getpixel((group*16+bit,y))
            for plane in range(4):
                if ink&(1<<plane):words[plane]|=0x8000>>bit
        raw.extend(struct.pack('>4H',*words))
assert len(raw)==32000

def compress(data):
    chains=defaultdict(deque);packed=bytearray();pos=0
    while pos<len(data):
        flag_at=len(packed);packed.append(0);flags=0
        for bit in range(8):
            if pos==len(data):break
            candidates=chains[bytes(data[pos:pos+3])]
            while candidates and pos-candidates[0]>4096:candidates.popleft()
            length=0;distance=0
            for previous in list(candidates)[-256:][::-1]:
                n=0
                while n<18 and pos+n<len(data) and data[previous+n]==data[pos+n]:n+=1
                if n>length:length=n;distance=pos-previous
                if length==18:break
            if length>=3:
                flags|=1<<bit
                code=((distance-1)<<4)|(length-3)
                packed.extend(struct.pack('>H',code))
            else:
                length=1;packed.append(data[pos])
            for k in range(length):
                chain=chains[bytes(data[pos+k:pos+k+3])]
                chain.append(pos+k)
                while chain and pos+k-chain[0]>4096:chain.popleft()
            pos+=length
        packed[flag_at]=flags
    return bytes(packed)

def decompress(data):
    result=bytearray();pos=0
    while len(result)<32000:
        flags=data[pos];pos+=1
        for bit in range(8):
            if len(result)==32000:break
            if flags&(1<<bit):
                code=int.from_bytes(data[pos:pos+2],'big');pos+=2
                distance=(code>>4)+1;length=(code&15)+3
                assert distance<=len(result)
                for _ in range(length):result.append(result[-distance])
            else:
                result.append(data[pos]);pos+=1
    assert pos==len(data)
    return result

packed=compress(raw)
assert decompress(packed)==raw
(BUILD/'title.st4').write_bytes(raw)
(BUILD/'title.pi1').write_bytes(struct.pack('>H16H',0,*hardware)+raw)
# Container: magic, dimensions, STE palette, unpacked size, packed payload.
(BUILD/'title.stlz').write_bytes(struct.pack('>4sHH16HI',b'STL4',320,200,*hardware,32000)+packed)
header='''#ifndef RR_TITLE_ASSETS_H
#define RR_TITLE_ASSETS_H
#include <stdint.h>
extern const uint16_t rrTitlePalette[16];
extern const uint8_t rrTitlePacked[];
extern const uint32_t rrTitlePackedBytes;
#endif
'''
lines=['#include "title_assets.h"','const uint16_t rrTitlePalette[16] = {',
       '    '+','.join(hex(v) for v in hardware),'};',
       f'const uint32_t rrTitlePackedBytes = {len(packed)}UL;',
       f'const uint8_t rrTitlePacked[{len(packed)}] = {{']
for i in range(0,len(packed),16):lines.append('    '+','.join(str(v) for v in packed[i:i+16])+',')
lines.append('};')
(GENERATED/'title_assets.h').write_text(header)
(GENERATED/'title_assets.c').write_text('\n'.join(lines)+'\n')
(BUILD/'title_report.json').write_text(json.dumps(dict(width=320,height=200,
    colours=len(set(image.getdata())),raw_bytes=len(raw),packed_bytes=len(packed),
    round_trip_identical=True,palette_ste=hardware),indent=2)+'\n')
print(f'Title: ST4 {len(raw)} -> LZSS {len(packed)} bytes, round-trip identical')
