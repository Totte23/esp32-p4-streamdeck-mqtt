"""Execute pinned original Python methods against a fake USB transport, compare C bytes.
Requires Pillow. No USB, network or smart-home calls.
"""
import ast, ctypes as C, io, pathlib, subprocess, typing
from PIL import Image
ROOT=pathlib.Path(__file__).resolve().parents[1]
subprocess.run(['cc','-shared','-fPIC','-Iinclude','src/mini_protocol.c','src/tile_render.c','-o','build/tests/libprotocol.dylib'],cwd=ROOT,check=True)
lib=C.CDLL(str(ROOT/'build/tests/libprotocol.dylib'))
U=C.c_uint8; BMP=19254
lib.mini_image_page.argtypes=[U,C.c_size_t,C.POINTER(U),C.c_size_t,C.POINTER(U)];lib.mini_image_page.restype=C.c_bool
lib.mini_brightness.argtypes=[U,C.POINTER(U)];lib.mini_brightness.restype=C.c_bool
lib.mini_parse_keys.argtypes=[C.POINTER(U),C.c_size_t,C.POINTER(U)];lib.mini_parse_keys.restype=C.c_bool
class Transport:
 def __init__(self):self.writes=[];self.features=[];self.input=None
 def write(self,b):self.writes.append(bytes(b))
 def write_feature(self,b):self.features.append(bytes(b))
 def read(self,n):return self.input
class ControlType:KEY=0
ns={'ClassVar':typing.ClassVar,'StreamDeck':object,'ControlType':ControlType}
tree=ast.parse((ROOT/'tests/reference/python-streamdeck/StreamDeckMini.py').read_text())
tree.body=[n for n in tree.body if not isinstance(n,(ast.Import,ast.ImportFrom))]
exec(compile(tree,'reference-mini','exec'),ns)
deck=ns['StreamDeckMini']();deck.device=Transport()
for percent in range(101):
 out=(U*17)();assert lib.mini_brightness(percent,out);deck.set_brightness(percent);assert bytes(out)==deck.device.features[-1]
image=bytes((i*37+i//256)%256 for i in range(BMP));raw=(U*BMP).from_buffer_copy(image)
for key in range(6):
 deck.device.writes=[];deck.set_key_image(key,image);assert len(deck.device.writes)==20
 for page,want in enumerate(deck.device.writes):
  out=(U*1024)();assert lib.mini_image_page(key,page,raw,BMP,out);assert bytes(out)==want,(key,page)
for mask in range(64):
 data=bytes([1]+[(mask>>k)&1 for k in range(6)]);deck.device.input=data
 expected=deck._read_control_states()[ControlType.KEY];out=U();report=(U*7).from_buffer_copy(data)
 assert lib.mini_parse_keys(report,7,C.byref(out));assert out.value==sum(int(b)<<i for i,b in enumerate(expected))
# Real original helper: Pillow rotate(90), flip vertical, save BMP.
ns={'Image':Image,'io':io,'StreamDeck':object};tree=ast.parse((ROOT/'tests/reference/python-streamdeck/PILHelper.py').read_text());tree.body=[n for n in tree.body if not isinstance(n,(ast.Import,ast.ImportFrom))];exec(compile(tree,'reference-helper','exec'),ns)
source=Image.new('RGBA',(80,80));source.putdata([(x*3,y*3,(x+y)%256,(x*7+y*11)%256) for y in range(80) for x in range(80)])
rgba=(U*25600).from_buffer_copy(source.tobytes());out=(U*BMP)();lib.tile_background.argtypes=[C.POINTER(U),C.c_uint32];lib.tile_rgba.argtypes=[C.POINTER(U),C.POINTER(U),C.c_uint]
lib.tile_background(out,0x18222f);lib.tile_rgba(out,rgba,80)
composite=Image.new('RGB',(80,80),(24,34,47));composite.paste(source,(0,0),source)
expected=ns['_to_native_format'](composite,{'size':(80,80),'rotation':90,'flip':(False,True),'format':'BMP'})
assert len(expected)==BMP
# DPI metadata may differ; dimensions, pixel layout and all 19,200 pixel bytes must agree.
assert bytes(out)[18:34]==expected[18:34];assert bytes(out)[54:]==expected[54:]
print('PASS: original Python library vs C: 101 brightness values, 120 image reports, 64 key masks, full Pillow orientation/alpha comparison')
