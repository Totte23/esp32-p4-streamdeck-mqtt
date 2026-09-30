# MQTT protocol v1

Any MQTT backend can implement this contract. Default prefix: `streamdeck/mini`, configurable in secrets. Use a unique prefix and client ID for each panel.

## Commands: panel → backend

Topic: `<prefix>/command`, QoS 0, **not retained**.

```json
{"v":1,"id":"a123b456-7","entity":"licht.flur","action":"toggle"}
```

The entity is a logical identifier from the menu, not necessarily a backend entity ID. The menu supports `toggle`, `set`, `up`, `down`, `stop`, `position`, `slatUp`, `slatDown`, `tilt`, `trigger` and configured legacy `command` strings. Check the JSON schema for value types. Backend adapters must explicitly map and allowlist entities/actions; never execute arbitrary entity IDs or services supplied by MQTT. Deduplicate request IDs when appropriate. Commands made while the panel is disconnected are dropped, not replayed.

## State: backend → panel

Topic: `<prefix>/state/<logical-entity>`. Publish retained confirmed state so reconnects and menu changes can recover the display.

```json
{"v":1,"state":"on","value":true}
```

`state` selects the matching `appearance` entry; `value` fills `{value}` text. Examples: `off`, `unknown`, `position` with numeric percent, `mixed` with a range string. State messages must fit the firmware’s 512-byte receive buffer. Command entity IDs must have at least two dot-separated segments, each starting with a lowercase ASCII letter; remaining characters may be lowercase letters, digits, underscore or hyphen (63 characters maximum). State topics use the same logical IDs.

The button appearance changes from feedback, not optimistically on press. A broker connection alone does not prove the backend/device is online. Backend adapters must publish `unknown` for unavailable devices and refresh stale retained state after restarting.

## Panel availability

`<prefix>/availability`: retained `online` when connected, retained last will `offline` on connection loss. This describes the panel, not the controlled device. Commands remain QoS 0: delivery is not guaranteed.

## Example

```sh
mosquitto_sub -h BROKER -t 'streamdeck/mini/command'
mosquitto_pub -h BROKER -t 'streamdeck/mini/state/licht.flur' -r -m '{"v":1,"state":"on","value":true}'
```

Add your broker authentication options locally. Never retain command messages.
