#ifndef BLE_DRIVER_H
#define BLE_DRIVER_H
#include "prot.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

void ble_init(program_context *ctx);
void ble_on_sync(void);
void ble_on_reset(int reason);
void nimble_host_run_task(void *params);


int gap_event(struct ble_gap_event *event, void *arg);
void advertising(char *device_name);



#endif
