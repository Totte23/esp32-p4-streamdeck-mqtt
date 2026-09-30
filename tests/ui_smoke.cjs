// Real browser, actual web assets, mock API: no ESP or smart-home commands.
const {chromium}=require(process.env.PLAYWRIGHT_MODULE||'playwright');
const assert=require('node:assert/strict'),http=require('node:http'),fs=require('node:fs'),path=require('node:path');
const root=path.resolve(__dirname,'../src/web'),icons=new Map();
const bundle=fs.readFileSync(path.resolve(root,'../starter_icons.bin'));
for(let i=0;i<bundle[8];i++){const p=9+i*(33+25600),end=bundle.indexOf(0,p);icons.set(bundle.toString('ascii',p,end),bundle.subarray(p+33,p+33+25600));}
let menu=fs.readFileSync(path.resolve(root,'../../examples/menu.json'),'utf8'),brightness=30,uploads=0;
function bmp(rgba){const b=Buffer.alloc(19254);b.write('BM');b.writeUInt32LE(19254,2);b.writeUInt32LE(54,10);b.writeUInt32LE(40,14);b.writeUInt32LE(80,18);b.writeUInt32LE(80,22);b.writeUInt16LE(1,26);b.writeUInt16LE(24,28);b.writeUInt32LE(19200,34);for(let y=0;y<80;y++)for(let x=0;x<80;x++){const s=(y*80+x)*4,d=54+((79-x)*80+y)*3;for(let c=0;c<3;c++)b[d+c]=rgba[s+2-c];}return b;}
const server=http.createServer(async(req,res)=>{
 const url=req.url;res.setHeader('Cache-Control','no-store');res.setHeader('Content-Security-Policy',"default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; img-src 'self' blob: data:; object-src 'none'; base-uri 'none'; frame-ancestors 'none'");
 if(req.method==='GET') {
  if(['/','/app.js','/menu-ui.js','/font.js','/icon-codec.js','/menu.schema.json'].includes(url)){const f=url==='/'?'index.html':url.slice(1);res.setHeader('Content-Type',f.endsWith('.html')?'text/html':f.endsWith('.json')?'application/json':'text/javascript');return res.end(fs.readFileSync(f==='menu.schema.json'?path.resolve(root,'../../examples',f):path.join(root,f)));}
  if(url==='/api/menu')return res.end(menu);
  if(url==='/api/state')return res.end(JSON.stringify({ready:true,total:2097152,used:icons.size*25600,icons:[...icons.keys()],keys:Array(6).fill('')}));
  if(url.startsWith('/api/assets/')){const a=icons.get(url.slice(12));if(a)return res.end(a);}
  if(url.startsWith('/api/icons/')){const a=icons.get(url.slice(11));if(a)return res.end(bmp(a));}
  res.statusCode=404;return res.end('Not found');
 }
 if(req.headers['x-deck-request']!=='1'){res.statusCode=403;return res.end('Forbidden');}
 const chunks=[];for await(const c of req)chunks.push(c);const body=Buffer.concat(chunks);
 if(req.method==='PUT'&&url.startsWith('/api/assets/')){assert.equal(body.length,25600);icons.set(url.slice(12),body);}
 else if(req.method==='PUT'&&url==='/api/menu'){menu=body.toString();uploads++;}
 else if(req.method==='POST'&&url==='/api/brightness')brightness=Number(body.toString());
 else if(req.method==='DELETE'&&url.startsWith('/api/icons/')){const name=url.slice(11);if(menu.includes(`"${name}.svg"`)){res.statusCode=409;return res.end('Dieses Icon wird im Menü verwendet.');}icons.delete(name);}
 else {res.statusCode=404;return res.end('Not found');}res.end('OK');
});
(async()=>{
 await new Promise(r=>server.listen(0,'127.0.0.1',r));const browser=await chromium.launch({headless:true});const page=await browser.newPage({viewport:{width:1200,height:1050}}),errors=[];
 page.on('pageerror',e=>errors.push(e.message));page.on('dialog',d=>d.accept());
 try {
  await page.goto(`http://127.0.0.1:${server.address().port}/`);
  await page.waitForFunction(()=>document.querySelector('#menu-status').textContent.includes('29 Seiten'));
  assert.equal(await page.locator('#menu-preview button').count(),6);
  await page.locator('#menu-preview').screenshot({path:path.resolve(__dirname,'../build/tests/starter-menu.png')});
  await page.locator('#menu-preview button').nth(1).click();assert.equal(await page.locator('#menu-page').inputValue(),'licht');
  await page.locator('#menu-preview button').nth(1).click();assert.equal(await page.locator('#menu-page').inputValue(),'wohnzimmer');
  await page.locator('#menu-preview button').nth(1).click();await page.waitForFunction(()=>document.querySelector('#menu-status').textContent.includes('wird nicht gesendet'));assert.equal(uploads,0);
  const unknown=await page.locator('#menu-preview canvas').nth(1).evaluate(c=>c.toDataURL());
  await page.selectOption('#menu-state','on');const on=await page.locator('#menu-preview canvas').nth(1).evaluate(c=>c.toDataURL());assert.notEqual(unknown,on);
  const actionChecks=await page.evaluate(async()=>{
    const {validateMenu}=await import('/menu-ui.js');
    const schema=await (await fetch('/menu.schema.json')).json();
    const base=JSON.parse(document.querySelector('#menu-json').value);
    return [
      [{entity:'licht.flur',action:'set',value:false},true],
      [{entity:'rollo.room_a',action:'position',value:45.5},true],
      [{entity:'licht.flur',action:'set',value:'false'},false],
      [{entity:'rollo.room_a',action:'position',value:101},false],
      [{entity:'licht.flur',action:'toggle',value:true},false],
      [{entity:'licht..flur',action:'toggle'},false]
    ].map(([action,expected])=>{const m=structuredClone(base);m.pages.wohnzimmer.buttons.topLeft.onPress=action;let valid=true;try{validateMenu(m,schema);}catch{valid=false;}return valid===expected;});
  });assert(actionChecks.every(Boolean));
  const original=JSON.parse(menu);const invalid=structuredClone(original);invalid.pages.licht.next='missing';
  await page.fill('#menu-json',JSON.stringify(invalid));await page.click('#menu-save');await page.waitForFunction(()=>document.querySelector('#menu-status').textContent.includes('fehlt'));assert.equal(uploads,0);
  const bad='<svg xmlns="http://www.w3.org/2000/svg"><script>alert(1)</script></svg>';
  await page.setInputFiles('#file',{name:'bad.svg',mimeType:'image/svg+xml',buffer:Buffer.from(bad)});await page.waitForFunction(()=>document.querySelector('#status').classList.contains('error'));assert(await page.locator('#upload').isDisabled());
  const svg='<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 80 80"><rect width="40" height="40" fill="#ff0000"/><rect x="40" width="40" height="40" fill="#00ff00"/><rect y="40" width="40" height="40" fill="#0000ff"/></svg>';
  await page.setInputFiles('#file',{name:'farben.svg',mimeType:'image/svg+xml',buffer:Buffer.from(svg)});await page.waitForFunction(()=>!document.querySelector('#upload').disabled);await page.click('#upload');await page.waitForFunction(()=>document.querySelector('#count').textContent==='43 / 64');
  const rgba=icons.get('farben');assert(rgba);const pixel=(x,y)=>[...rgba.subarray((y*80+x)*4,(y*80+x)*4+4)];assert.deepEqual(pixel(10,10),[255,0,0,255]);assert.deepEqual(pixel(60,10),[0,255,0,255]);assert.deepEqual(pixel(10,60),[0,0,255,255]);assert.equal(pixel(60,60)[3],0);
  original.pages.hauptmenue.buttons.topLeft.icon='farben.svg';await page.fill('#menu-json',JSON.stringify(original,null,2));await page.click('#menu-save');await page.waitForFunction(()=>document.querySelector('#menu-status').textContent.includes('aktiviert'));assert.equal(uploads,1);
  await page.reload();await page.waitForFunction(()=>document.querySelector('#menu-status').textContent.includes('29 Seiten'));assert(JSON.parse(await page.locator('#menu-json').inputValue()).pages.hauptmenue.buttons.topLeft.icon==='farben.svg');
  await page.locator('#gallery article').filter({has:page.locator('strong',{hasText:'farben'})}).getByRole('button').click();await page.waitForFunction(()=>document.querySelector('#status').textContent.includes('im Menü verwendet'));assert(icons.has('farben'));
  await page.fill('#brightness','65');await page.click('#set-brightness');await page.waitForFunction(()=>document.querySelector('#status').textContent.includes('Helligkeit'));assert.equal(brightness,65);
  fs.mkdirSync(path.resolve(__dirname,'../build/tests'),{recursive:true});await page.screenshot({path:path.resolve(__dirname,'../build/tests/web-desktop.png'),fullPage:true});
  await page.setViewportSize({width:390,height:844});assert(await page.evaluate(()=>document.documentElement.scrollWidth<=window.innerWidth));await page.screenshot({path:path.resolve(__dirname,'../build/tests/web-mobile.png'),fullPage:true});assert.deepEqual(errors,[]);
  console.log('PASS: menu preview/navigation, states, invalid reference, SVG rejection, RGBA colors/transparency, upload/reload, deletion guard, mobile layout');
 }finally{await browser.close();server.close();}
})().catch(e=>{console.error(e);server.close();process.exitCode=1;});
