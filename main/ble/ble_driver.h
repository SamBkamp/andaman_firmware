#ifndef BLE_DRIVER_H
#define BLE_DRIVER_H
#include "prot.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

void ble_init(step_struct *pump_step_data);


#endif
