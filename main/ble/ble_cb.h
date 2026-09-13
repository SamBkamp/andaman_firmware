#ifndef BLE_CB_H
#define BLE_CB_H

#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

int gap_event(struct ble_gap_event *event, void *arg);
void advertising(void);

#endif
