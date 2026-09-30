#!/usr/bin/env python3
"""Build a self-contained, offline menu editor from the firmware's schema and renderer."""
import base64,json
from pathlib import Path
root=Path(__file__).resolve().parents[1]
template=(root/'scripts/menu-editor.template.html').read_text()
def js(data): return json.dumps(data,ensure_ascii=False).replace('<','\\u003c')
bundle=(root/'src/starter_icons.bin').read_bytes();icons={}
for i in range(bundle[8]):
 p=9+i*(33+25600);name=bundle[p:p+33].split(b'\0')[0].decode()
 icons[name]=base64.b64encode(bundle[p+33:p+33+25600]).decode()
data='const INITIAL='+js(json.loads((root/'examples/menu.json').read_text()))+';\n'
data+='const SCHEMA='+js(json.loads((root/'examples/menu.schema.json').read_text()))+';\n'
menu=json.loads((root/'examples/menu.json').read_text())
entities={b['onPress']['entity']:{} for p in menu['pages'].values() for b in p['buttons'].values() if b and b.get('onPress',{}).get('entity')}
data+='const DEVICES='+js(entities)+';\n'
data+='const ICONS='+js(icons)+';\n'
ui=(root/'src/web/menu-ui.js').read_text()
shared=(root/'src/web/font.js').read_text().replace('export const','const')
shared+=ui[ui.index('export function validateSchema'):ui.index('function preview()')].replace('export function','function')
out=template.replace('/*__DATA__*/',data).replace('/*__SHARED__*/',shared)
(root/'examples/menu-editor.html').write_text(out)
print('Generated examples/menu-editor.html')
