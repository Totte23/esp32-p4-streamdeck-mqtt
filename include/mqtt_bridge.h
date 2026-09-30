#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "cJSON.h"
esp_err_t mqtt_bridge_start(void);
uint32_t mqtt_bridge_session(void);
bool mqtt_bridge_command(const char *command, uint32_t session);

void mqtt_bridge_refresh(void);

bool mqtt_bridge_action(const cJSON *action, uint32_t session);
