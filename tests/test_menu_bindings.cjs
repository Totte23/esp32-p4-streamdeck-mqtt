const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const {pathToFileURL}=require('node:url');
const {chromium}=require(process.env.PLAYWRIGHT_MODULE||'playwright');
const root=path.resolve(__dirname,'..');
const menu=JSON.parse(fs.readFileSync(path.join(root,'examples/menu.json')));
const {devices}=JSON.parse(fs.readFileSync(path.join(root,'examples/iobroker-aliases.json')));
(async()=>{
 const browser=await chromium.launch({headless:true});const page=await browser.newPage();const errors=[];page.on('pageerror',e=>errors.push(e.message));
 const counts={pages:0,buttons:0,empty:0,mqtt:0,navigation:0,status:0,lights:0};
 try{
 await page.goto(pathToFileURL(path.join(root,'examples/menu-editor.html')).href);
 await page.selectOption('#mode','edit');
 for(const [id,p] of Object.entries(menu.pages)){
  counts.pages++;
  assert(Object.hasOwn(menu.pages,p.next)&&Object.hasOwn(menu.pages,p.previous),id);
  await page.locator('#pages').getByRole('button',{name:p.title,exact:true}).click();
  for(const [slot,b] of Object.entries(p.buttons)){
   await page.locator('#deck').getByRole('button',{name:slot,exact:true}).click();
   if(!b){counts.empty++;assert(await page.locator('#button-form').isHidden());continue;}
   counts.buttons++;const a=b.onPress||{};
   assert.equal(await page.locator('#entity').inputValue(),a.entity||'',`${id}/${slot}: editor entity`);
   assert.equal(await page.locator('#state').inputValue(),b.state||'',`${id}/${slot}: editor state`);
   if(a.entity){
    counts.mqtt++;assert(Object.hasOwn(devices,a.entity),`${id}/${slot}: entity unknown`);
    const d=devices[a.entity];assert(['switch','scene'].includes(d.kind)&&['toggle','set'].includes(a.action)||d.kind==='cover'&&a.action==='position'||Object.hasOwn(d.actions||{},a.action),`${id}/${slot}: action unsupported`);
    if(b.state)assert.equal(b.state,a.entity,`${id}/${slot}: wrong status device`);
   }
   if(a.page){counts.navigation++;assert(Object.hasOwn(menu.pages,a.page));assert.equal(await page.locator('#target').inputValue(),a.page);
    if(b.state)assert(Object.values(menu.pages[a.page].buttons).some(x=>x?.onPress?.entity===b.state),`${id}/${slot}: destination/status mismatch`);
   }
   if(b.state){counts.status++;assert(devices[b.state]?.read?.length,`${id}/${slot}: no status source`);}
   if(a.entity?.startsWith('licht.')){counts.lights++;assert.equal(b.state,a.entity);assert.equal(b.appearance.on.background,'#FFF3B0');}
   if(Object.keys(b.appearance||{}).length)assert(b.state,`${id}/${slot}: styles without status`);
  }
 }
 assert.deepEqual(JSON.parse(await page.locator('#raw').inputValue()),menu,'Inspection must not add placeholder values to JSON');
 assert.deepEqual(errors,[]);console.log('PASS: every button binding, allowed actions, status/destination match, empty slots and real editor values:',JSON.stringify(counts));
 }finally{await browser.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
