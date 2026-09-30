'use strict';
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const config = JSON.parse(fs.readFileSync('examples/iobroker-aliases.json'));
const objects = new Map(JSON.parse(fs.readFileSync('tests/fixtures/iobroker-objects.json')).map(o => [o._id, o]));
const context = {module: {exports: {}}, Date, setTimeout};
vm.runInNewContext(fs.readFileSync('scripts/iobroker-bridge-core.js', 'utf8'), context);
const {createStreamDeckBridge} = context.module.exports;
const states = new Map(), writes = [], messages = [], subscriptions = [], logs = [];
for (const [id, o] of objects) states.set(id, {val: o.common.type === 'boolean' ? false : 50, ack: true, q: 0});
const api = {
    getStateAsync: async id => states.get(id),
    getObjectAsync: async id => objects.get(id),
    createStateAsync: async (id, value) => {if (!states.has(id)) states.set(id, {val: value, ack: true});},
    setStateAsync: async (id, value, ack) => {
        writes.push({id, value, ack});states.set(id, {val: value, ack, ts: Date.now()});
    },
    sendTo: (adapter, command, data) => {assert.equal(adapter, 'mqtt.0');assert.equal(command, 'sendMessage2Client');messages.push(data);},
    on: (pattern, callback) => subscriptions.push({pattern, callback}),
    setInterval: () => {}, wait: async () => {}, log: s => logs.push(s)
};
const command = (id, entity, action, value) => ({v: 1, id, entity, action, ...(value === undefined ? {} : {value})});
const event = r => ({state: {val: JSON.stringify(r), ts: Date.now(), ack: true}});
const physical = () => writes.filter(w => w.id !== config.dedupState);
(async () => {
    // Configuration references synthetic example metadata; no installation data.
    for (const d of Object.values(config.devices)) {
        for (const id of [...d.read || [], ...d.unreach || [], ...d.position || [], ...d.write ? [d.write] : []]) assert(objects.has(id), id);
        for (const targets of Object.values(d.actions || {})) for (const t of targets) {
            assert(objects.has(t.id), t.id); if(t.valueFrom) assert(objects.has(t.valueFrom));
        }
    }
    const menu = JSON.parse(fs.readFileSync('examples/menu.json'));
    for (const p of Object.values(menu.pages)) for (const b of Object.values(p.buttons)) {
        if(b?.onPress?.entity) assert(Object.hasOwn(config.devices, b.onPress.entity));
        if(b?.state) assert(Object.hasOwn(config.devices, b.state));
    }
    // A stored broker command must never be replayed on startup.
    const old = command('old', 'licht.flur', 'toggle');
    states.set(config.commandState, event(old).state);
    let bridge = createStreamDeckBridge(api, config);await bridge.start();
    await bridge.receive(event(old));assert.equal(physical().length, 0);
    assert(subscriptions[0].pattern.id.test(config.commandState));
    assert(!subscriptions[0].pattern.id.test(config.commandState + '.extra'));
    await bridge.receive(event(command('one', 'licht.flur', 'toggle')));
    assert.deepEqual(physical().at(-1), {id: 'alias.0.example.point_117', value: true, ack: false});
    const before = physical().length;
    await bridge.receive(event(command('one', 'licht.flur', 'toggle')));assert.equal(physical().length, before);
    // Unconfirmed state cannot be toggled a second time or shown as confirmed on.
    await bridge.receive(event(command('pending', 'licht.flur', 'toggle')));assert.equal(physical().length, before);
    assert.equal((await bridge.status('licht.flur')).state, 'unknown');
    states.set('alias.0.example.point_117', {val: true, ack: true});
    assert.equal((await bridge.status('licht.flur')).state, 'on');
    assert(messages.at(-1).retain);
    await bridge.receive(event(command('off', 'licht.flur', 'set', false)));
    assert.equal(physical().at(-1).value, false);
    // The existing scenes intentionally store logical flags with ack=false.
    states.set('alias.0.example.point_116',{val:false,ack:false});
    await bridge.receive(event(command('scene','szene.sofazeit','toggle')));
    assert.equal(physical().at(-1).id,'alias.0.example.point_116');
    assert.equal(physical().at(-1).value,true);
    assert.equal((await bridge.status('szene.sofazeit')).state,'on');
    for (const i of [1,2]) {
        await bridge.receive(event(command('kitchen'+i, 'licht.kueche.licht'+i, 'toggle')));
        assert.equal(physical().at(-1).id, config.devices['licht.kueche.licht'+i].write);
        assert.equal(physical().at(-1).value,true);
    }
    // Strict allowlist, version, type and bounds checks.
    const invalid = [command('x1','alias.0.example.point_117','toggle'),command('x2','licht.flur','set','false'),
        command('x3','rollo.room_a','position',101),command('x4','licht.flur','up'),command('x5','licht.esszimmer.schrank','toggle'),
        {...command('x6','licht.flur','toggle'),datapoint:'x'}, {...command('x7','licht.flur','toggle'),v:2}];
    const n = physical().length;
    for (const r of invalid) await bridge.receive(event(r));
    await bridge.receive({state:{val:JSON.stringify(command('stale','rollo.room_a','up')),ts:Date.now()-11000}});
    assert.equal(physical().length,n);
    await bridge.receive(event(command('room', 'rollo.room_a', 'position', 35)));
    assert.deepEqual(physical().slice(-2).map(w=>[w.id,w.value]),[['alias.0.example.point_112',35],['alias.0.example.point_114',35]]);
    // Alias values must not be inverted twice; differing room positions get a range.
    states.set('alias.0.example.point_075',{val:35,ack:true});
    states.set('alias.0.example.point_073',{val:70,ack:true});
    const mixed = await bridge.status('rollo.room_a');assert.equal(mixed.state,'mixed');assert.equal(mixed.value,'35-70');
    await bridge.receive(event(command('all','rollo.alle','stop')));
    assert.deepEqual(physical().slice(-10).map(w=>w.id),config.devices['rollo.alle'].actions.stop.map(t=>t.id));
    // Preflight catches a missing second group member before operating the first.
    const missing = objects.get('alias.0.example.point_099');objects.delete(missing._id);
    const prev = physical().length;await bridge.receive(event(command('badgroup','rollo.room_a','up')));assert.equal(physical().length,prev);objects.set(missing._id,missing);
    states.set('alias.0.example.point_103',{val:67,ack:false});
    await bridge.receive(event(command('tilt','raffstore.alle','tilt')));
    assert.deepEqual(physical().slice(-4).map(w=>[w.id,w.value]),config.devices['raffstore.alle'].actions.tilt.map(t=>[t.id,67]));
    await bridge.receive(event({id:'legacy',command:'markise.hoch'}));assert.equal(physical().at(-1).id,'alias.0.example.point_104');
    // Duplicate suppression persists across a fresh bridge instance.
    bridge=createStreamDeckBridge(api,config);await bridge.start();
    const count=physical().length;await bridge.receive(event(command('room','rollo.room_a','position',35)));assert.equal(physical().length,count);
    // Retained state, nonretained result; group commands never invent paths.
    assert(messages.filter(m=>m.topic.endsWith('/result')).every(m=>m.retain===false));
    const generated=fs.readFileSync('examples/iobroker-streamdeck.js','utf8');new vm.Script(generated);
    assert(generated.includes(JSON.stringify(config,null,2)));
    console.log('PASS: synthetic aliases, menu coverage, typed actions, groups, tilt, feedback, no replay, persistent dedup, legacy, preflight');
})().catch(e=>{console.error(e);process.exitCode=1;});
