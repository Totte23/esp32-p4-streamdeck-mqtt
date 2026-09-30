import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const code = readFileSync(new URL('../src/web/icon-codec.js', import.meta.url), 'utf8');
const {encodeMiniBmp, decodeMiniBmp, validName} = await import(`data:text/javascript;base64,${Buffer.from(code).toString('base64')}`);
const rgba = new Uint8ClampedArray(80*80*4);
for(let y=0;y<80;y++) for(let x=0;x<80;x++) {
  const p=(y*80+x)*4; rgba[p]=x; rgba[p+1]=y; rgba[p+2]=(x+y)%256; rgba[p+3]=255;
}
const bmp=encodeMiniBmp(rgba), view=new DataView(bmp.buffer);
assert.equal(bmp.length,19254); assert.equal(view.getUint32(10,true),54);
assert.equal(view.getUint16(28,true),24);
// Independent corner checks: logical top-left maps to last BMP row, first column.
assert.deepEqual([...bmp.slice(54+79*240,54+79*240+3)],[0,0,0]);
assert.deepEqual([...bmp.slice(54,57)],[79,0,79]);
assert.deepEqual([...bmp.slice(54+79*3,54+79*3+3)],[158,79,79]);
assert.deepEqual(decodeMiniBmp(bmp.buffer),rgba);
assert.throws(()=>encodeMiniBmp(new Uint8Array(5)));
assert.throws(()=>decodeMiniBmp(new ArrayBuffer(3)));
for(const name of ['lamp_on','A-01','x'.repeat(32)]) assert(validName(name));
for(const name of ['', '../lamp', 'a/b', 'x'.repeat(33), 'a%2fb', 'x.svg', '<script>']) assert(!validName(name));
console.log('PASS: browser BMP format, native orientation, lossless colors, name validation');
