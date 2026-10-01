#pragma once
#include <stdbool.h>
#include <stdint.h>
#define NETWORK_RECONNECT_S UINT64_C(30)
#define NETWORK_C6_RESET_S UINT64_C(90)
#define NETWORK_REBOOT_S UINT64_C(180)
#define NETWORK_REBOOT_GUARD_S UINT64_C(600)
#define NETWORK_HEALTH_MAX_AGE_S 25u
#define NETWORK_STABLE_S UINT64_C(60)
typedef enum { NETWORK_RECOVERY_NONE, NETWORK_RECOVERY_RECONNECT,
               NETWORK_RECOVERY_C6, NETWORK_RECOVERY_REBOOT } network_recovery_t;
typedef struct {
    bool fault, stabilizing;
    uint64_t fault_since, stable_since;
    network_recovery_t stage;
} network_watchdog_t;
static inline bool network_health_fresh(bool online, bool seen, uint32_t now, uint32_t last)
{
    return online && seen && (uint32_t)(now-last) <= NETWORK_HEALTH_MAX_AGE_S;
}
static inline network_recovery_t network_watchdog_step(network_watchdog_t *w, uint64_t now,
                                                      uint64_t not_before, bool healthy)
{
    if (healthy) {
        if (w->fault && !w->stabilizing) { w->stabilizing=true; w->stable_since=now; }
        if (w->stabilizing && now-w->stable_since >= NETWORK_STABLE_S) *w=(network_watchdog_t){0};
        return NETWORK_RECOVERY_NONE;
    }
    w->stabilizing=false;
    if (!w->fault) { w->fault=true; w->fault_since=now; }
    uint64_t elapsed=now-w->fault_since;
    network_recovery_t next=NETWORK_RECOVERY_NONE;
    if (elapsed>=NETWORK_REBOOT_S && now>=not_before) next=NETWORK_RECOVERY_REBOOT;
    else if (elapsed>=NETWORK_C6_RESET_S) next=NETWORK_RECOVERY_C6;
    else if (elapsed>=NETWORK_RECONNECT_S) next=NETWORK_RECOVERY_RECONNECT;
    if (next<=w->stage) return NETWORK_RECOVERY_NONE;
    w->stage=next;
    return next;
}
