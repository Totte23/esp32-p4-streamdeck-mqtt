#include <assert.h>
#include <stdio.h>
#include "network_watchdog.h"
int main(void) {
 network_watchdog_t w={0};
 assert(network_watchdog_step(&w,0,0,false)==NETWORK_RECOVERY_NONE);
 assert(network_watchdog_step(&w,29,0,false)==NETWORK_RECOVERY_NONE);
 assert(network_watchdog_step(&w,30,0,false)==NETWORK_RECOVERY_RECONNECT);
 assert(network_watchdog_step(&w,60,0,false)==NETWORK_RECOVERY_NONE);
 assert(network_watchdog_step(&w,90,0,false)==NETWORK_RECOVERY_C6);
 assert(network_watchdog_step(&w,179,0,false)==NETWORK_RECOVERY_NONE);
 assert(network_watchdog_step(&w,180,0,false)==NETWORK_RECOVERY_REBOOT);
 // A worker blocked in RPC cannot refresh health; cached IP alone is insufficient.
 assert(network_health_fresh(true,true,125,100));
 assert(!network_health_fresh(true,true,126,100));
 assert(!network_health_fresh(true,false,100,100));
 assert(!network_health_fresh(false,true,100,100));
 assert(network_health_fresh(true,true,5,UINT32_MAX-10));
 // Short recoveries/flapping do not reset the escalation clock.
 w=(network_watchdog_t){0};
 network_watchdog_step(&w,0,0,false);
 assert(network_watchdog_step(&w,30,0,false)==NETWORK_RECOVERY_RECONNECT);
 assert(network_watchdog_step(&w,40,0,true)==NETWORK_RECOVERY_NONE);
 assert(network_watchdog_step(&w,90,0,false)==NETWORK_RECOVERY_C6);
 // Sixty healthy seconds re-arm recovery; MQTT state is intentionally not an input.
 network_watchdog_step(&w,100,0,true);
 network_watchdog_step(&w,159,0,true); assert(w.fault);
 network_watchdog_step(&w,160,0,true); assert(!w.fault);
 assert(network_watchdog_step(&w,200,0,false)==NETWORK_RECOVERY_NONE);
 assert(network_watchdog_step(&w,230,0,false)==NETWORK_RECOVERY_RECONNECT);
 // Cooldown blocks reboot only, not reconnect/reset; no repeated reset requests.
 w=(network_watchdog_t){0};network_watchdog_step(&w,0,600,false);
 assert(network_watchdog_step(&w,30,600,false)==NETWORK_RECOVERY_RECONNECT);
 assert(network_watchdog_step(&w,90,600,false)==NETWORK_RECOVERY_C6);
 assert(network_watchdog_step(&w,180,600,false)==NETWORK_RECOVERY_NONE);
 assert(network_watchdog_step(&w,599,600,false)==NETWORK_RECOVERY_NONE);
 assert(network_watchdog_step(&w,600,600,false)==NETWORK_RECOVERY_REBOOT);
 puts("PASS: recovery stages, stale health, flapping, stable re-arm, reboot guard");
}
