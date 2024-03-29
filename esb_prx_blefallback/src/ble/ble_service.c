#include <zephyr/logging/log.h>
#include "ble_service.h"

#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

#define RUN_STATUS_LED DK_LED1
#define CON_STATUS_LED DK_LED2
#define USER_LED DK_LED3
#define RUN_LED_BLINK_INTERVAL 1000

#define USER_BUTTON 30 // gpio pin 30

LOG_MODULE_REGISTER(ble_service);

static bool app_button_state;

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static const struct bt_data sd[] = {
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_LBS_VAL),
};

static void connected(struct bt_conn *conn, uint8_t err)
{
    if (err)
    {
        LOG_INF("Connection failed (err %u)\n", err);
        return;
    }

    LOG_INF("Connected\n");

    dk_set_led_on(CON_STATUS_LED);
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    LOG_INF("Disconnected (reason %u)\n", reason);

    dk_set_led_off(CON_STATUS_LED);
}

#ifdef CONFIG_BT_LBS_SECURITY_ENABLED
static void security_changed(struct bt_conn *conn, bt_security_t level,
                             enum bt_security_err err)
{
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    if (!err)
    {
        LOG_INF("Security changed: %s level %u\n", addr, level);
    }
    else
    {
        LOG_INF("Security failed: %s level %u err %d\n", addr, level,
               err);
    }
}
#endif

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
#ifdef CONFIG_BT_LBS_SECURITY_ENABLED
    .security_changed = security_changed,
#endif
};

#if defined(CONFIG_BT_LBS_SECURITY_ENABLED)
static void auth_passkey_display(struct bt_conn *conn, unsigned int passkey)
{
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    LOG_INF("Passkey for %s: %06u\n", addr, passkey);
}

static void auth_cancel(struct bt_conn *conn)
{
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    LOG_INF("Pairing cancelled: %s\n", addr);
}

static void pairing_complete(struct bt_conn *conn, bool bonded)
{
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    LOG_INF("Pairing completed: %s, bonded: %d\n", addr, bonded);
}

static void pairing_failed(struct bt_conn *conn, enum bt_security_err reason)
{
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    LOG_INF("Pairing failed conn: %s, reason %d\n", addr, reason);
}

static struct bt_conn_auth_cb conn_auth_callbacks = {
    .passkey_display = auth_passkey_display,
    .cancel = auth_cancel,
};

static struct bt_conn_auth_info_cb conn_auth_info_callbacks = {
    .pairing_complete = pairing_complete,
    .pairing_failed = pairing_failed};
#else
static struct bt_conn_auth_cb conn_auth_callbacks;
static struct bt_conn_auth_info_cb conn_auth_info_callbacks;
#endif

static void app_led_cb(bool led_state)
{
    dk_set_led(USER_LED, led_state);
}

static bool app_button_cb(void)
{
    return app_button_state;
}

static struct bt_lbs_cb lbs_callbacks = {
    .led_cb = app_led_cb,
    .button_cb = app_button_cb,
};

static void button_changed(uint32_t button_state, uint32_t has_changed)
{
    if (has_changed & USER_BUTTON)
    {
        uint32_t user_button_state = button_state & USER_BUTTON;

        bt_lbs_send_button_state(user_button_state);
        app_button_state = user_button_state ? true : false;
    }
}

static int init_button(void)
{
    int err;

    err = dk_buttons_init(button_changed);

    if (err)
    {
        LOG_INF("Cannot init input (err: %d)\n", err);
    }

    return err;
}

int app_bt_init(void)
{
    int err = 0;

    err = init_button();
    if (err)
    {
        LOG_INF("Button init failed (err %d)\n", err);
        return err;
    }

    if (IS_ENABLED(CONFIG_BT_LBS_SECURITY_ENABLED))
    {
        err = bt_conn_auth_cb_register(&conn_auth_callbacks);
        if (err)
        {
            LOG_INF("Failed to register authorization callbacks.\n");
            return err;
        }

        err = bt_conn_auth_info_cb_register(&conn_auth_info_callbacks);
        if (err)
        {
            LOG_INF("Failed to register authorization info callbacks.\n");
            return err;
        }
    }

    err = bt_enable(NULL);
    if (err)
    {
        LOG_INF("Bluetooth init failed (err %d)\n", err);
        return err;
    }

    LOG_INF("Bluetooth initialized\n");

    if (IS_ENABLED(CONFIG_SETTINGS))
    {
        settings_load();
    }

    err = bt_lbs_init(&lbs_callbacks);
    if (err)
    {
        LOG_INF("Failed to init LBS (err:%d)\n", err);
        return err;
    }

    err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
    if (err)
    {
        LOG_INF("Advertising failed to start (err %d)\n", err);
        return err;
    }

    return 0;
}
