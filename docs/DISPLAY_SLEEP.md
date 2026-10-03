# Display idle mode

After 180 seconds without key activity the host sets the Mini LCD backlight to
0%, pauses image transfers, and selects Home internally. MQTT and WLAN stay up.
The first key press wakes at Home without publishing an action or navigating.
The lower-left release/hold of that wake gesture is consumed too. A held key
prevents sleep. On wake all six images are refreshed before restoring the stored
brightness. Brightness adjustments while asleep take effect upon waking.

This does not remove USB power: the Mini must remain powered to report keys.
Actual backlight darkness and power savings require checking the physical unit.
The documented hardware sleep-duration command exists, but is not used here:
its interaction with incoming image updates and wake-key reporting is not yet
verified on our Mini firmware. The host timer provides deterministic behavior.

Source: https://docs.elgato.com/streamdeck/hid/mini/#set-backlight-brightness
