/* Shared, locally testable implementation. The generated example includes this file. */
function createStreamDeckBridge(api, config) {
    'use strict';
    const own = (o, k) => Object.prototype.hasOwnProperty.call(o, k);
    const devices = config.devices;
    const seen = new Set();
    let queue = Promise.resolve(), pending = 0, ready = false;
    const good = s => s && s.ack === true && (!s.q || s.q === 0);
    const read = async id => { try { return await api.getStateAsync(id); } catch { return null; } };
    const send = (suffix, payload, retain) => api.sendTo(config.mqttAdapter, 'sendMessage2Client', {
        topic: `${config.topicPrefix}/${suffix}`, message: JSON.stringify(payload), retain
    });
    function parse(raw) {
        if (typeof raw !== 'string' || raw.length > 1024) throw Error('Ungültige Nachricht');
        const r = JSON.parse(raw);
        if (!r || Array.isArray(r) || typeof r !== 'object' || typeof r.id !== 'string' ||
            !/^[A-Za-z0-9_-]{1,80}$/.test(r.id)) throw Error('Request-ID fehlt/ungültig');
        if (own(r, 'command')) {
            if (Object.keys(r).some(k => !['id', 'command'].includes(k)) ||
                !own(config.legacyCommands, r.command)) throw Error('Unbekannter Altbefehl');
            return {v: 1, id: r.id, ...config.legacyCommands[r.command]};
        }
        if (r.v !== 1 || typeof r.entity !== 'string' || r.entity.length > 63 ||
            !/^[a-z][a-z0-9_-]*(\.[a-z][a-z0-9_-]*)+$/.test(r.entity) ||
            Object.keys(r).some(k => !['v', 'id', 'entity', 'action', 'value'].includes(k))) throw Error('Ungültiger Vertrag');
        if (r.action === 'set') {
            if (typeof r.value !== 'boolean') throw Error('set erwartet Boolean');
        } else if (r.action === 'position') {
            if (typeof r.value !== 'number' || !Number.isFinite(r.value) || r.value < 0 || r.value > 100) throw Error('Position 0–100 erwartet');
        } else if (!['toggle', 'up', 'down', 'stop', 'slatUp', 'slatDown', 'tilt', 'trigger'].includes(r.action) || own(r, 'value')) {
            throw Error('Ungültige Aktion');
        }
        return r;
    }
    async function status(entity) {
        const d = devices[entity];
        /** @type {{v: number, state: string, value: string | number | boolean}} */
        let payload = {v: 1, state: 'unknown', value: '?'};
        if (d.kind !== 'disabled' && d.read?.length) {
            const states = await Promise.all(d.read.map(read));
            const unreachable = await Promise.all((d.unreach || []).map(read));
            const healthy = unreachable.every(s => good(s) && s.val === false);
            const stateGood = s => d.kind === 'scene' ? s && (!s.q || s.q === 0) : good(s);
            if (healthy && states.every(stateGood)) {
                if (['switch', 'scene'].includes(d.kind) && typeof states[0].val === 'boolean') {
                    payload = {v: 1, state: states[0].val ? 'on' : 'off', value: states[0].val};
                } else if (d.kind === 'cover' && states.every(s => typeof s.val === 'number' && Number.isFinite(s.val) && s.val >= 0 && s.val <= 100)) {
                    const values = states.map(s => Math.round(s.val));
                    const low = Math.min(...values), high = Math.max(...values);
                    payload = {v: 1, state: low === high ? 'position' : 'mixed', value: low === high ? low : `${low}-${high}`};
                }
            }
        }
        send(`state/${entity}`, payload, true);
        // Old menus keep their state IDs; values now use the normalized alias scale.
        for (const id of d.legacyStates || []) send(`state/${id}`, payload, true);
        return payload;
    }
    async function plan(r) {
        if (!own(devices, r.entity)) throw Error('Unbekannte entity');
        const d = devices[r.entity];
        if (d.kind === 'disabled') throw Error(d.note || 'Zuordnung fehlt');
        let writes;
        if (['switch', 'scene'].includes(d.kind) && ['toggle', 'set'].includes(r.action)) {
            let value = r.value;
            if (r.action === 'toggle') {
                const s = await read(d.read[0]);
                if (!(d.kind === 'scene' ? s && (!s.q || s.q === 0) : good(s)) || typeof s.val !== 'boolean') throw Error('Kein bestätigter Schaltzustand');
                value = !s.val;
            }
            writes = [{id: d.write, value}];
        } else if (d.kind === 'cover' && r.action === 'position') {
            writes = d.position.map(id => ({id, value: r.value}));
        } else if (own(d.actions || {}, r.action)) {
            writes = d.actions[r.action].map(t => ({...t}));
        } else throw Error('Aktion für diese entity nicht freigegeben');
        // Preflight the complete group before the first write; never resolve IDs from MQTT.
        for (const w of writes) {
            if (w.valueFrom) {
                const s = await read(w.valueFrom);
                if (!s || (s.q && s.q !== 0) || typeof s.val !== 'number' || !Number.isFinite(s.val) || s.val < 0 || s.val > 100)
                    throw Error('Ungültiger KIPPWERT');
                w.value = s.val; // User setting, not a device acknowledgement.
            }
            const obj = await api.getObjectAsync(w.id);
            if (!obj || obj.type !== 'state' || obj.common.write !== true || typeof w.value !== obj.common.type)
                throw Error(`Ziel fehlt oder Typ/Schreibrecht ungültig: ${w.id}`);
            if (typeof w.value === 'number' && (w.value < (obj.common.min ?? -Infinity) || w.value > (obj.common.max ?? Infinity)))
                throw Error('Wert außerhalb Zielbereich');
        }
        return writes;
    }
    async function execute(r, received) {
        if (Date.now() - received > 10000) throw Error('Befehl zu alt');
        if (seen.has(r.id)) return;
        const writes = await plan(r);
        // Persist before actuation: after a crash a command may be lost, never replayed.
        seen.add(r.id);
        while (seen.size > 512) seen.delete(seen.values().next().value);
        await api.setStateAsync(config.dedupState, JSON.stringify([...seen]), true);
        for (let i = 0; i < writes.length; i++) {
            if (Date.now() - received > 10000) throw Error('Befehl während Gruppenfahrt abgelaufen');
            if (i && r.action !== 'stop') await api.wait(100);
            await api.setStateAsync(writes[i].id, writes[i].value, false);
        }
        // This reports submission, never a confirmed physical state.
        send('result', {v: 1, id: r.id, accepted: true}, false);
    }
    function receive(event) {
        if (!ready) return Promise.resolve();
        let r;
        try {
            r = parse(event?.state?.val);
            const ts = event.state.ts;
            if (!Number.isFinite(ts) || Date.now() - ts > 10000 || ts - Date.now() > 5000) throw Error('Veraltetes Ereignis');
            if (seen.has(r.id)) return Promise.resolve();
            if (pending >= 32) throw Error('Warteschlange voll');
        } catch (e) { api.log(`Stream Deck: ${e.message}`, 'warn'); return Promise.resolve(); }
        pending++;
        queue = queue.then(() => execute(r, event.state.ts)).catch(e => {
            api.log(`Stream Deck ${r.entity}: ${e.message}`, 'warn');
            send('result', {v: 1, id: r.id, accepted: false, error: e.message}, false);
        }).finally(() => { pending--; });
        return queue;
    }
    async function start() {
        await api.createStateAsync(config.dedupState, '[]', false, {type: 'string', role: 'json', read: true, write: false});
        const saved = await read(config.dedupState);
        let ids;
        try { ids = JSON.parse(saved?.val || '[]'); } catch { throw Error('Defekter Duplikatspeicher'); }
        if (!Array.isArray(ids) || ids.some(x => typeof x !== 'string')) throw Error('Defekter Duplikatspeicher');
        ids.slice(-512).forEach(id => seen.add(id));
        // A command cached by the MQTT adapter must not run when the script starts.
        const previous = await read(config.commandState);
        try { seen.add(parse(previous?.val).id); } catch { /* no previous command */ }
        const watch = new Map();
        for (const [entity, d] of Object.entries(devices)) {
            for (const id of [...(d.read || []), ...(d.unreach || [])]) {
                if (!watch.has(id)) watch.set(id, new Set());
                watch.get(id).add(entity);
            }
        }
        const escape = s => s.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
        api.on({id: new RegExp(`^${escape(config.commandState)}$`), change: 'any'}, receive);
        for (const [id, entities] of watch) api.on({id, change: 'any'}, () => {
            for (const entity of entities) refresh(entity);
        });
        // Refresh on reconnect, including when an MQTT server restarted and lost retained data.
        api.on({id: `${config.mqttAdapter}.${config.topicPrefix.replaceAll('/', '.')}.availability`, change: 'any'}, e => {
            if (e.state?.val === 'online') refreshAll();
        });
        api.setInterval(refreshAll, 30000);
        ready = true;
        await Promise.all(Object.keys(devices).map(refresh));
    }
    // Serialize each entity's status reads so an older read cannot overwrite a newer result.
    const refreshes = new Map();
    function refresh(entity) {
        const next = (refreshes.get(entity) || Promise.resolve()).then(() => status(entity)).catch(e => api.log(`Stream Deck Status: ${e.message}`, 'warn'));
        refreshes.set(entity, next);
        return next;
    }
    function refreshAll() { for (const entity of Object.keys(devices)) refresh(entity); }
    return {start, receive, status, parse};
}
if (typeof module !== 'undefined') module.exports = {createStreamDeckBridge};
