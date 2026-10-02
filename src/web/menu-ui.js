import {basic,latin} from './font.js';
const $=id=>document.getElementById(id);
const slots=['leftTop','topLeft','topRight','leftBottom','bottomLeft','bottomRight'];
let schema, draft, pageId, assets=new Map();
async function request(path,options={}) {
  const r=await fetch(path,{...options,headers:{'X-Deck-Request':'1',...options.headers}});
  if(!r.ok) throw Error(await r.text()); return r;
}
function status(s,bad=false){$('menu-status').textContent=s;$('menu-status').classList.toggle('error',bad);}
export function validateSchema(value,s,path='$') {
  if(s.oneOf || s.anyOf) {
    const matches=(s.oneOf||s.anyOf).filter(x=>{try{validateSchema(value,x,path);return true;}catch{return false;}}).length;
    if(!matches || (s.oneOf && matches!==1)) throw Error(`${path}: ungültige Auswahl.`);return;
  }
  if('const' in s && value!==s.const) throw Error(`${path}: erwartet ${s.const}.`);
  if(s.enum&&!s.enum.includes(value)) throw Error(`${path}: erlaubt ${s.enum.join(', ')}.`);
  if(s.type==='null' && value!==null) throw Error(`${path}: erwartet null.`);
  if(s.type==='string') {
    if(typeof value!=='string'||value.length>(s.maxLength??Infinity)||value.length<(s.minLength??0)||(s.pattern&&!new RegExp(s.pattern).test(value))) throw Error(`${path}: ungültiger Text.`);
  }
  if(s.type==='boolean'&&typeof value!=='boolean')throw Error(`${path}: erwartet true oder false.`);
  if(s.type==='number'&&(typeof value!=='number'||!Number.isFinite(value)||value<s.minimum||value>s.maximum))throw Error(`${path}: ungültige Zahl.`);
  if(s.type==='integer'&&(!Number.isInteger(value)||value<s.minimum||value>s.maximum)) throw Error(`${path}: ungültige Zahl.`);
  if(s.type==='object') {
    if(!value||typeof value!=='object'||Array.isArray(value)) throw Error(`${path}: erwartet Objekt.`);
    const keys=Object.keys(value);
    if(keys.length<(s.minProperties??0)||keys.length>(s.maxProperties??Infinity)) throw Error(`${path}: falsche Anzahl Einträge.`);
    for(const key of s.required||[]) if(!Object.hasOwn(value,key)) throw Error(`${path}.${key}: fehlt.`);
    for(const key of keys) {
      if(s.propertyNames) validateSchema(key,s.propertyNames,path);
      if(Object.hasOwn(s.properties||{},key)) validateSchema(value[key],s.properties[key],`${path}.${key}`);
      else if(s.additionalProperties===false) throw Error(`${path}.${key}: unbekanntes Feld.`);
      else if(typeof s.additionalProperties==='object') validateSchema(value[key],s.additionalProperties,`${path}.${key}`);
    }
  }
}
export function validateMenu(menu,s) {
  validateSchema(menu,s);
  if(!Object.hasOwn(menu.pages,menu.startPage)) throw Error('startPage existiert nicht.');
  for(const [id,p] of Object.entries(menu.pages)) {
    for(const b of Object.values(p.buttons)) if(b && b.iconY!==undefined && b.iconY+(b.iconSize??48)>80) throw Error(`${id}: Icon liegt außerhalb der Taste.`);
    for(const target of [p.previous,p.next,...Object.values(p.buttons).map(b=>b?.onPress?.page).filter(Boolean)])
      if(!Object.hasOwn(menu.pages,target)) throw Error(`${id}: Zielseite ${target} fehlt.`);
  }
}
function glyph(c) {const code=c.codePointAt(0);return code<128?basic[code]:code>=160&&code<=255?latin[code-160]:basic[63];}
function glyphLeft(c) {let bits=glyph(c).reduce((a,b)=>a|b,0),left=0;if(bits)while(!(bits&1)){left++;bits>>=1;}return left;}
function width(c) {if(c===' ')return 4;let bits=glyph(c).reduce((a,b)=>a|b,0)>>glyphLeft(c),w=0;while(bits){w++;bits>>=1;}return w+1;}
function text(ctx,value,y,size,color) {
  let chars=Array.from(value),scale=size/8,total=chars.reduce((n,c)=>n+width(c),0);
  if(total*scale>80)scale=1;
  if(total>80){while(chars.length&&total+width('~')>80)total-=width(chars.pop());chars.push('~');total+=width('~');}
  let left=Math.floor((80-total*scale)/2);ctx.fillStyle=color;
  chars.forEach(c=>{glyph(c).forEach((row,yy)=>{for(let xx=0;xx<8;xx++) if(row&(1<<xx)) ctx.fillRect(left+(xx-glyphLeft(c))*scale,y+yy*scale,scale,scale);});left+=width(c)*scale;});
}
function draw(canvas,b) {
  const ctx=canvas.getContext('2d'),style=b?.appearance?.[$('menu-state').value]||{};
  ctx.fillStyle=style.background||b?.background||'#18222f';ctx.fillRect(0,0,80,80);
  const name=b?.icon?.replace(/\.(svg|png)$/,'');
  if(assets.has(name)) {
    const s=b.iconSize||48;if(style.iconBackground){ctx.fillStyle=style.iconBackground;ctx.fillRect((80-s)/2-1,(b.iconY??(80-s)/2)-1,s+2,s+2);}ctx.imageSmoothingEnabled=false;ctx.drawImage(assets.get(name),(80-s)/2,b.iconY??(80-s)/2,s,s);
  }
  const size=b?.fontSize||8,ys=[3,40-size/2,75-size];
  ['top','center','bottom'].forEach((p,i)=>{
    const value=(p==='center'?style.center??b?.labels?.[p]:b?.labels?.[p])||'';
    text(ctx,value.replaceAll('{value}',$('menu-value').value),ys[i],size,style.textColor||b?.textColor||'#ffffff');
  });
}
function preview() {
  if(!draft) return;
  const p=draft.pages[pageId];$('menu-preview').replaceChildren();$('menu-page').value=pageId;
  slots.forEach((name,index)=>{
    const slot=(index===0||index===3)&&!Object.hasOwn(p.buttons,name)?null:name;
    const b=slot?p.buttons[slot]:null,el=document.createElement('button'),c=document.createElement('canvas');c.width=c.height=80;
    const destination=slot?b?.onPress?.page:index===0?p.next:p.previous;
    el.title=slot?(b?.labels?.bottom||b?.id||'Leer'):(index===0?`${p.title} – Weiter`:'Zurück · 2 Sekunden halten: Hauptmenü');
    if(slot) draw(c,b);
    else {
      const ctx=c.getContext('2d');ctx.fillStyle='#243447';ctx.fillRect(0,0,80,80);ctx.fillStyle='#55acee';
      for(let y=22;y<54;y++)for(let x=23;x<57;x++)if((y<39&&x>=40-(y-22)&&x<=40+(y-22))||(y>=39&&x>=35&&x<=45))ctx.fillRect(x,index===0?y:75-y,1,1);
      if(index===0) {
        const split=p.title.indexOf(' ');
        text(ctx,split<0?p.title:p.title.slice(0,split),3,8,'#7dd3fc');
        if(split>=0) text(ctx,p.title.slice(split+1),13,8,'#7dd3fc');
      } else text(ctx,'2s: Home',3,8,'#7dd3fc');
      text(ctx,index===0?'Weiter':'Zurück',63,8,'#ffffff');
    }
    el.append(c);el.onclick=()=>{if(destination){pageId=destination;preview();}else if(b?.onPress?.entity) status(`Vorschau: ${b.onPress.entity} → ${b.onPress.action} (wird nicht gesendet)`);else if(b?.onPress?.command) status(`Vorschau: ${b.onPress.command} (wird nicht gesendet)`);};
    $('menu-preview').append(el);
  });
}
async function check() {
  const source=$('menu-json').value;
  if(new TextEncoder().encode(source).length>65536) throw Error('Maximal 64 KiB.');
  const next=JSON.parse(source);validateMenu(next,schema);
  const missing=[];const nextAssets=new Map();
  for(const p of Object.values(next.pages)) for(const b of Object.values(p.buttons)) {
    const id=b?.icon?.replace(/\.(svg|png)$/,'');if(!id||nextAssets.has(id)||missing.includes(id)) continue;
    try {
      const buffer=await(await request(`/api/assets/${id}`)).arrayBuffer();
      if(buffer.byteLength!==25600) throw Error('Bildformat');
      const canvas=document.createElement('canvas');canvas.width=canvas.height=80;
      canvas.getContext('2d').putImageData(new ImageData(new Uint8ClampedArray(buffer),80,80),0,0);nextAssets.set(id,canvas);
    } catch {missing.push(id);}
  }
  assets=nextAssets;draft=next;
  if(!Object.hasOwn(draft.pages,pageId))pageId=draft.startPage;
  $('menu-page').replaceChildren(...Object.entries(draft.pages).map(([id,p])=>new Option(p.title,id)));
  const states=new Set(['unknown','off','on']);
  for(const p of Object.values(draft.pages)) for(const b of Object.values(p.buttons)) Object.keys(b?.appearance||{}).forEach(s=>states.add(s));
  const old=$('menu-state').value;$('menu-state').replaceChildren(...[...states].map(s=>new Option(s,s)));$('menu-state').value=states.has(old)?old:'unknown';
  preview();
  if(missing.length)throw Error(`Zuerst Icons hochladen: ${missing.join(', ')}`);
  status(`${Object.keys(draft.pages).length} Seiten geprüft. Vorschau schaltet keine Geräte.`);
}
async function run(fn){try{await fn();}catch(e){status(e.message,true);}}
export async function initMenu() {
  schema=await(await request('/menu.schema.json')).json();
  $('menu-json').value=await(await request('/api/menu')).text();
  $('menu-file').onchange=()=>run(async()=>{const f=$('menu-file').files[0];if(f){if(f.size>65536)throw Error('Maximal 64 KiB.');$('menu-json').value=await f.text();await check();}});
  $('menu-check').onclick=()=>run(check);
  $('menu-save').onclick=()=>run(async()=>{await check();await request('/api/menu',{method:'PUT',body:$('menu-json').value});status('Menü gespeichert und auf dem ESP aktiviert.');});
  $('menu-download').onclick=()=>run(async()=>{
    await check();const url=URL.createObjectURL(new Blob([$('menu-json').value],{type:'application/json'}));
    const a=document.createElement('a');a.href=url;a.download='menu.json';a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);
  });
  $('menu-page').onchange=()=>{pageId=$('menu-page').value;preview();};
  $('menu-state').onchange=preview;$('menu-value').oninput=preview;
  await run(check);
}
