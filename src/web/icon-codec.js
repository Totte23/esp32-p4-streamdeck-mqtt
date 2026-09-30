export const SIDE = 80;
export const BMP_SIZE = 19254;
export function validName(name) { return /^[A-Za-z0-9_-]{1,32}$/.test(name); }

// Input is ordinary top-left-origin RGBA. Output uses the original Mini's
// native rotation/reflection and bottom-up BMP rows (same as mini_demo_bmp).
export function encodeMiniBmp(rgba) {
  if (rgba.length !== SIDE * SIDE * 4) throw new Error('Erwartet: 80 × 80 Pixel.');
  const bytes = new Uint8Array(BMP_SIZE), view = new DataView(bytes.buffer);
  bytes[0] = 66; bytes[1] = 77;
  for (const [offset, value] of [[2, BMP_SIZE], [10, 54], [14, 40], [18, 80], [22, 80], [34, 19200]]) view.setUint32(offset, value, true);
  view.setUint16(26, 1, true); view.setUint16(28, 24, true);
  for (let y = 0; y < SIDE; y++) for (let x = 0; x < SIDE; x++) {
    const src = (y * SIDE + x) * 4, dst = 54 + ((79 - x) * SIDE + y) * 3;
    bytes[dst] = rgba[src + 2]; bytes[dst + 1] = rgba[src + 1]; bytes[dst + 2] = rgba[src];
  }
  return bytes;
}
export function decodeMiniBmp(buffer) {
  const bytes = new Uint8Array(buffer), view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  if (bytes.length !== BMP_SIZE || bytes[0] !== 66 || bytes[1] !== 77 || view.getUint32(18, true) !== 80 || view.getUint32(22, true) !== 80) throw new Error('Ungültiges gespeichertes Bild.');
  const rgba = new Uint8ClampedArray(SIDE * SIDE * 4);
  for (let y = 0; y < SIDE; y++) for (let x = 0; x < SIDE; x++) {
    const src = 54 + ((79 - x) * SIDE + y) * 3, dst = (y * SIDE + x) * 4;
    rgba[dst] = bytes[src + 2]; rgba[dst + 1] = bytes[src + 1]; rgba[dst + 2] = bytes[src]; rgba[dst + 3] = 255;
  }
  return rgba;
}

export function sanitizeSvg(text) {
  if (/<!DOCTYPE|<!ENTITY|<\?xml-stylesheet/i.test(text)) throw new Error('SVGs mit externen Dokumentdefinitionen werden nicht unterstützt.');
  const doc = new DOMParser().parseFromString(text, 'image/svg+xml');
  const root = doc.documentElement;
  if (doc.querySelector('parsererror') || root.localName !== 'svg' || root.namespaceURI !== 'http://www.w3.org/2000/svg') throw new Error('Keine gültige SVG-Datei.');
  const forbidden = new Set(['script', 'foreignobject', 'image', 'iframe', 'object', 'embed', 'audio', 'video', 'animate', 'animatemotion', 'animatetransform', 'set']);
  for (const element of [root, ...root.querySelectorAll('*')]) {
    if (element.namespaceURI !== root.namespaceURI || forbidden.has(element.localName.toLowerCase())) throw new Error('Bitte ein statisches SVG ohne eingebettete Inhalte verwenden.');
    for (const attr of element.attributes) {
      if (/^on/i.test(attr.name)) throw new Error('SVG-Ereigniscode ist nicht erlaubt.');
      if (attr.localName === 'href' && !attr.value.trim().startsWith('#')) throw new Error('Externe SVG-Verweise sind nicht erlaubt.');
      if (/url\s*\(|\\/i.test(attr.value.replace(/url\(\s*['"]?#[\w:.-]+['"]?\s*\)/gi, ''))) throw new Error('Nur lokale SVG-Füllungen und Masken werden unterstützt.');
      if (attr.localName === 'base') throw new Error('Externe SVG-Basisadressen sind nicht erlaubt.');
    }
    if (element.localName === 'style') {
      // Keep common class-based colors/gradients; reject imports and nonlocal URLs.
      const css = element.textContent.replace(/url\(\s*['"]?#[\w:.-]+['"]?\s*\)/gi, '');
      if (/@import|url\s*\(|\\/i.test(css)) throw new Error('Bitte SVG ohne externe CSS-Ressourcen verwenden.');
    }
  }
  if (!root.hasAttribute('viewBox')) {
    const width = parseFloat(root.getAttribute('width')), height = parseFloat(root.getAttribute('height'));
    if (!(width > 0 && height > 0)) throw new Error('SVG benötigt viewBox oder Breite und Höhe.');
    root.setAttribute('viewBox', `0 0 ${width} ${height}`);
  }
  root.setAttribute('width', '80'); root.setAttribute('height', '80');
  root.setAttribute('preserveAspectRatio', 'xMidYMid meet');
  return new XMLSerializer().serializeToString(root);
}
