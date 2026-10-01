# Validation scope

The original implementation was physically tested on Waveshare ESP32-P4-NANO, P4 silicon v1.3, with Stream Deck Mini 0fd9:0063: image output, navigation, long-press Home, Wi-Fi and an MQTT light toggle with confirmed feedback.

This public edition replaces private mappings with example targets and redistributes only icons with documented provenance. No physical-device automation is run by the host test suite. Home Assistant integration requires validation on an actual installation. Revision 3 builds are compile checks, not hardware validation.

USB FIFO settings reserve 131 RX words for 512-byte input and 256 periodic TX words for 1024-byte output. Background: https://github.com/espressif/esp-idf/issues/19143 . The C6 transport is initialized before Wi-Fi initialization.

Public-edition checks: host C tests, synthetic ioBroker bridge tests, exhaustive menu bindings, offline editor and web UI smoke tests passed. Both P4 environments built with empty example credentials; image checks confirmed flash/revision metadata and hashes. Home Assistant YAML parsed successfully; no live Home Assistant validation was performed.


## Network recovery (2026-10-01)

The shared network implementation was tested on a Waveshare P4-NANO with P4 v1.3 and C6 Hosted 2.12.13. Hardware checks confirmed gateway ping, WLAN reconnect, C6/SDIO reinitialization and subsequent MQTT reconnection without a P4 reboot. The public demo menu/backend configuration was not flashed in that test. Unit tests cover 30/90/180-second escalation, stale health, flapping, stable recovery and reboot cooldown. The final reboot stage was policy-tested, not validated by deliberately deadlocking the hardware driver. Long-term stability remains unproven.

Public-repository verification: all host C tests pass; both PlatformIO environments build successfully with the reduced, public-demo icon bundle and Hosted 2.12.13. Private credentials and personal menu data were not copied.
