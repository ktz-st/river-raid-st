"""One-time GIF -> indexed 16-colour STE PNG; title.png is the build source."""
from PIL import Image
from build_paths import ASSETS

target=ASSETS/'title.png'
assert not target.exists(),'Do not overwrite an edited title.png'
image=Image.open(ASSETS/'title.gif')
assert image.size==(320,200) and image.mode=='P'
assert len(set(image.getdata()))<=16
palette=image.getpalette()[:48]
palette=[round(value/17)*17 for value in palette]
image.putpalette(palette)
image.info.clear()
image.save(target,bits=4)
print('title.png: 320x200 indexed, 16 STE colours; original GIF retained as reference')
