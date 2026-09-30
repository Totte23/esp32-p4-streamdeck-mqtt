# Home Assistant via MQTT

The panel is a generic MQTT client. It does not implement Home Assistant discovery or act as a Home Assistant device integration.

1. Configure the [MQTT integration](https://www.home-assistant.io/integrations/mqtt/) and connect the panel to the same broker using its own credentials.
2. Copy entries from `examples/home-assistant/automations.yaml` into your automation configuration, without adding a second `automation:` root. Replace `light.example_hall` everywhere with your actual light entity ID.
3. Adapt the topic prefix if changed on the panel. The sample maps the demo menu's `licht.flur` to that light.
4. Validate Home Assistant configuration and reload automations. Open the Flur page. A press sends `toggle`; feedback from the real light selects AN/AUS/unknown styling.

The example allows only that exact logical entity and toggle action. Add explicit mappings and allowed actions for other devices; do not pass incoming MQTT values directly as arbitrary service names. Never retain commands. The minimal example does not persistently deduplicate request IDs; use a backend with deduplication where duplicate commands are unacceptable. QoS 0 commands can be lost.

State is published retained on change, startup, panel connection and once per minute. Unavailable/unknown lights show unknown. If Home Assistant itself stops unexpectedly while the broker stays available, its last retained state can remain visible; this example does not implement a backend heartbeat timeout. Do not interpret displayed status as a safety guarantee.

This YAML was prepared against the official [MQTT trigger documentation](https://www.home-assistant.io/docs/automation/trigger/#mqtt-trigger) and syntax-checked locally. It has not been tested against a running Home Assistant installation. ioBroker is not required.
