#pragma once
#define DECK_INITIAL_BRIGHTNESS 30
// The published P4-NANO schematic ties USB-A power-switch EN high via R2.
// No GPIO is needed. Do not copy GPIO46 settings from another board.
#define DECK_USB_TASK_STACK 6144
#define DECK_USB_COMMAND_QUEUE 16
