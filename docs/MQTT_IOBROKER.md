# Optional ioBroker bridge

The firmware uses the generic [MQTT protocol](MQTT.md). ioBroker is one possible backend.

1. Configure the MQTT adapter on your broker.
2. Edit `examples/iobroker-aliases.json`. All `alias.0.example.point_*` targets are placeholders; replace them with your own typed aliases. The example intentionally does not contain a real installation.
3. Match `mqttAdapter`, `commandState` and `topicPrefix` to your setup. Covers assume a normalized 0 = closed, 100 = open scale; normalize in your aliases if needed.
4. Run `python3 scripts/build_iobroker.py` and paste the generated `examples/iobroker-streamdeck.js` into a new JavaScript adapter script.
5. Review mappings before enabling. Start with one harmless light and verify confirmed feedback.

The bridge validates allowed commands, target types and grouped writes, deduplicates requests and publishes state. `tests/fixtures/iobroker-objects.json` contains synthetic metadata for automated tests only; do not import it into ioBroker. Scene flags indicate the scene flag, not confirmation of every device in a scene. The public example has not been deployed to your installation.
