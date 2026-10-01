#pragma once
#include "esp_err.h"
esp_err_t network_start(void);

#include <stdbool.h>
// Queues recovery in the network task; never runs driver teardown in the caller.
esp_err_t network_recover(bool reset_c6);
