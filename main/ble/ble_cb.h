#ifndef BLE_CB_H
#define BLE_CB_H

#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

int gap_event(struct ble_gap_event *event, void *arg);
void ble_on_sync(void);
void ble_on_reset(int reason);
void nimble_host_run_task(void *params);

#endif
