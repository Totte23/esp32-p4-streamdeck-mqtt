import {initMenu} from './menu-ui.js';
import {decodeMiniBmp, sanitizeSvg, validName} from './icon-codec.js';
const $ = id => document.getElementById(id);
let state = {icons: [], keys: Array(6).fill(''), ready: false}, sourceImage = null, selectedVersion = 0, busy = false;
const previews = new Map();
function message(text, bad = false) { $('status').textContent = text; $('status').classList.toggle('error', bad); }
async function request(path, options = {}) {
  const response = await fetch(path, {cache: 'no-store', ...options, headers: {'X-Deck-Request': '1', ...(options.headers || {})}});
  if (!response.ok) throw new Error(await response.text() || `HTTP ${response.status}`);
  return response;
}
async function action(task) {
  if (busy) return;
  busy = true; document.querySelectorAll('button').forEach(b => b.disabled = true);
  try { await task(); } catch (error) { message(error.message, true); }
  finally { busy = false; document.querySelectorAll('button').forEach(b => b.disabled = false); $('upload').disabled = !sourceImage || !state.ready; }
}
function paint(canvas, rgba, label = '') {
  const ctx = canvas.getContext('2d');
  if (rgba) ctx.putImageData(new ImageData(rgba, 80, 80), 0, 0);
  else { ctx.fillStyle = '#122a5f'; ctx.fillRect(0,0,80,80); ctx.fillStyle = '#f5f5f5'; ctx.fillRect(8,6,64,3); ctx.font = 'bold 40px system-ui'; ctx.textAlign = 'center'; ctx.fillText(label,40,56); }
}
function canvasFor(name, label = '') {
  const canvas = document.createElement('canvas'); canvas.width = canvas.height = 80;
  paint(canvas, previews.get(name), label); return canvas;
}
function render() {
  $('gallery').replaceChildren();
  for (const name of state.icons) {
    const card = document.createElement('article'); card.className = 'icon';
    const label = document.createElement('strong'); label.textContent = name;
    const remove = document.createElement('button'); remove.textContent = 'Löschen';
    remove.onclick = () => action(async () => {
      if (state.keys.includes(name)) throw new Error('Dieses Icon ist noch zugewiesen. Zuerst bei den entsprechenden Tasten „Demo“ oder ein anderes Icon wählen.');
      if (!confirm(`Piktogramm „${name}“ löschen?`)) return;
      await request(`/api/icons/${name}`, {method:'DELETE'}); await refresh(); message(`„${name}“ gelöscht.`);
    });
    card.append(canvasFor(name), label, remove); $('gallery').append(card);
  }
  if (!state.icons.length) { const p = document.createElement('p'); p.className = 'empty muted'; p.textContent = 'Noch keine Piktogramme gespeichert. Lade links dein erstes Icon hoch.'; $('gallery').append(p); }
  $('count').textContent = `${state.icons.length} / 64`;
  $('space').textContent = state.ready ? `${(state.used / 1024).toFixed(0)} KB von ${(state.total / 1024).toFixed(0)} KB belegt` : 'Bildspeicher nicht verfügbar. Bitte das serielle Log prüfen.';
  $('upload').disabled = busy || !sourceImage || !state.ready;
}
async function refresh() {
  state = await (await request('/api/state')).json(); state.icons.sort((a,b) => a.localeCompare(b));
  previews.clear();
  for (const name of state.icons) {
    const response = await request(`/api/icons/${name}`);
    previews.set(name, decodeMiniBmp(await response.arrayBuffer()));
  }
  render();
  if (!state.ready) throw new Error('LittleFS ist nicht verfügbar. Vorhandene Daten wurden nicht automatisch gelöscht; bitte das serielle Log prüfen.');
}
function drawPreview() {
  const ctx = $('preview').getContext('2d');
  ctx.fillStyle = $('background').value; ctx.fillRect(0,0,80,80);
  if (sourceImage) {
    const scale = Math.min(80 / sourceImage.naturalWidth, 80 / sourceImage.naturalHeight);
    const w = sourceImage.naturalWidth * scale, h = sourceImage.naturalHeight * scale;
    ctx.drawImage(sourceImage, (80-w)/2, (80-h)/2, w, h);
  }
}
$('file').addEventListener('change', async () => {
  const version = ++selectedVersion, file = $('file').files[0]; sourceImage = null; $('upload').disabled = true; drawPreview();
  if (!file) return;
  try {
    if (file.size > 2 * 1024 * 1024) throw new Error('Bitte eine Datei mit höchstens 2 MB wählen.');
    let blob;
    if (/\.svg$/i.test(file.name)) blob = new Blob([sanitizeSvg(await file.text())], {type:'image/svg+xml'});
    else if (/\.png$/i.test(file.name) || file.type === 'image/png') blob = file;
    else throw new Error('Bitte SVG oder PNG auswählen.');
    const url = URL.createObjectURL(blob), image = new Image();
    try { image.src = url; await image.decode(); } finally { URL.revokeObjectURL(url); }
    if (version !== selectedVersion) return;
    if (!image.naturalWidth || !image.naturalHeight || image.naturalWidth > 4096 || image.naturalHeight > 4096) throw new Error('Bitte ein Bild mit maximal 4096 × 4096 Pixeln wählen.');
    sourceImage = image;
    $('name').value = file.name.replace(/\.[^.]+$/, '').normalize('NFKD').replace(/[^A-Za-z0-9_-]/g, '_').slice(0,32) || 'icon';
    drawPreview(); $('upload').disabled = busy || !state.ready; message('Vorschau bereit. Icon-ID prüfen und speichern.');
  } catch(error) { if(version === selectedVersion) message(error.message, true); }
});
$('background').addEventListener('input', drawPreview);
$('upload').onclick = () => action(async () => {
  const name = $('name').value;
  if (!sourceImage || !validName(name)) throw new Error('Bitte eine Datei und eine gültige Icon-ID wählen.');
  if (state.icons.includes(name) && !confirm(`„${name}“ ersetzen? Zugewiesene Tasten verwenden danach das neue Bild.`)) return;
  const canvas=document.createElement('canvas');canvas.width=canvas.height=80;
  const ctx=canvas.getContext('2d');
  if(!$('transparent').checked) {ctx.fillStyle=$('background').value;ctx.fillRect(0,0,80,80);}
  const scale=Math.min(80/sourceImage.naturalWidth,80/sourceImage.naturalHeight);
  const w=sourceImage.naturalWidth*scale,h=sourceImage.naturalHeight*scale;
  ctx.drawImage(sourceImage,(80-w)/2,(80-h)/2,w,h);
  await request(`/api/assets/${name}`, {method:'PUT',headers:{'Content-Type':'application/octet-stream'},body:ctx.getImageData(0,0,80,80).data});
  await refresh(); message(`„${name}“ gespeichert. Du kannst die Icon-ID jetzt in menu.json verwenden.`);
});
$('brightness').oninput = () => $('percent').textContent = `${$('brightness').value} %`;
$('set-brightness').onclick = () => action(async () => {
  await request('/api/brightness', {method:'POST', body:$('brightness').value}); message('Helligkeit an den USB-Treiber übergeben.');
});
$('refresh').onclick = () => action(async () => { await refresh(); message('Piktogramme und Zuordnungen aktualisiert.'); });
drawPreview();
action(async () => { await refresh(); message('Verbunden. Deine Änderungen werden auf dem ESP gespeichert.'); });

initMenu().catch(error=>message(error.message,true));
