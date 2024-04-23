#ifndef BLE_SERVICE_H_
#define BLE_SERVICE_H_

//incl here for ble library visibility
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <bluetooth/services/lbs.h>

#include <zephyr/settings/settings.h>

#include <dk_buttons_and_leds.h>


int app_bt_init(void);
int app_bt_restart(void);

#endif /* BLE_SERVICE_H_ */
