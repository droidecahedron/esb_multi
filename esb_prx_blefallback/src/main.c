/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 * author: johnny nguyen
 */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/drivers/clock_control/nrf_clock_control.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/irq.h>
#include <zephyr/logging/log.h>
#include <nrf.h>
#include <esb.h>
#include <zephyr/kernel.h>
#include <zephyr/types.h>
#include <nrfx_gpiote.h>

LOG_MODULE_REGISTER(esb_prx);

// radio debug pin
#define TEST_PIN 31

static const struct gpio_dt_spec leds[] = {
	GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios),
	GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios),
	GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios),
	GPIO_DT_SPEC_GET(DT_ALIAS(led3), gpios),
};

BUILD_ASSERT(DT_SAME_NODE(DT_GPIO_CTLR(DT_ALIAS(led0), gpios),
						  DT_GPIO_CTLR(DT_ALIAS(led1), gpios)) &&
				 DT_SAME_NODE(DT_GPIO_CTLR(DT_ALIAS(led0), gpios),
							  DT_GPIO_CTLR(DT_ALIAS(led2), gpios)) &&
				 DT_SAME_NODE(DT_GPIO_CTLR(DT_ALIAS(led0), gpios),
							  DT_GPIO_CTLR(DT_ALIAS(led3), gpios)),
			 "All LEDs must be on the same port");

// 52840dk
#define dk_button1_msk 1 << 11 // button1 is gpio pin 11 in the .dts
#define dk_button2_msk 1 << 12 // button2 is gpio pin 12 in the .dts
#define dk_button3_msk 1 << 24 // button3 is gpio pin 24 in the .dts
#define dk_button4_msk 1 << 25 // button4 is gpio pin 25 in the .dts
#define GPIO_SPEC_AND_COMMA(button_or_led) GPIO_DT_SPEC_GET(button_or_led, gpios),
#define BUTTONS_NODE DT_PATH(buttons)
static const struct gpio_dt_spec buttons[] = {
#if DT_NODE_EXISTS(BUTTONS_NODE)
	DT_FOREACH_CHILD(BUTTONS_NODE, GPIO_SPEC_AND_COMMA)
#endif
};

static struct gpio_callback button_callback;

static struct esb_payload rx_payload;
static struct esb_payload tx_payload = ESB_CREATE_PAYLOAD(0,
														  0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17);

/* These are arbitrary default addresses. In end user products
 * different addresses should be used for each set of devices.
 */
#define NUM_PRX_PERIPH 2
volatile int peripheral_number = -1; // used to select addr0 and channel in the inits

static int leds_init(void)
{
	if (!device_is_ready(leds[0].port))
	{
		LOG_ERR("LEDs port not ready");
		return -ENODEV;
	}

	for (size_t i = 0; i < ARRAY_SIZE(leds); i++)
	{
		int err = gpio_pin_configure_dt(&leds[i], GPIO_OUTPUT);

		if (err)
		{
			LOG_ERR("Unable to configure LED%u, err %d.", i, err);
			return err;
		}
	}

	return 0;
}

void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	LOG_INF("Button pressed at %" PRIu32, k_cycle_get_32());
	LOG_INF("pins var: %d", pins);

	switch (pins)
	{
	case dk_button1_msk:
		LOG_INF("BUTTON1");
		peripheral_number = 0; // first peripheral
		break;

	case dk_button2_msk:
		LOG_INF("BUTTON2");
		peripheral_number = 1;
		break;

	case dk_button3_msk:
		LOG_INF("BUTTON3");
		break;

	case dk_button4_msk:
		LOG_INF("BUTTON4");
		break;

	default:
		LOG_INF("unknown pin in button callback");
	}
}

static int buttons_init(void)
{
	int err = NRFX_SUCCESS;
	if (!device_is_ready(buttons[0].port))
	{
		LOG_ERR("LEDs port not ready");
		return -ENODEV;
	}

	//--- Buttons
	for (size_t i = 0; i < ARRAY_SIZE(buttons); i++)
	{
		/* Enable pull resistor towards the inactive voltage. */
		gpio_flags_t flags =
			buttons[i].dt_flags & GPIO_ACTIVE_LOW ? GPIO_PULL_UP : GPIO_PULL_DOWN;
		err = gpio_pin_configure_dt(&buttons[i], GPIO_INPUT | flags);

		if (err)
		{
			LOG_ERR("Cannot configure button gpio");
			return err;
		}
	}

	uint32_t pin_mask = 0;

	for (size_t i = 0; i < ARRAY_SIZE(buttons); i++)
	{
		err = gpio_pin_interrupt_configure_dt(&buttons[i],
											  GPIO_INT_EDGE_TO_ACTIVE);
		if (err)
		{
			LOG_ERR("Cannot disable callbacks()");
			return err;
		}

		pin_mask |= BIT(buttons[i].pin);
	}

	gpio_init_callback(&button_callback, button_pressed, pin_mask);

	for (size_t i = 0; i < ARRAY_SIZE(buttons); i++)
	{
		err = gpio_add_callback(buttons[i].port, &button_callback);
		if (err)
		{
			LOG_ERR("Cannot add callback");
			return err;
		}
	}

	return err;
}

static void leds_update(uint8_t value)
{
	bool led0_status = !(value % 8 > 0 && value % 8 <= 4);
	bool led1_status = !(value % 8 > 1 && value % 8 <= 5);
	bool led2_status = !(value % 8 > 2 && value % 8 <= 6);
	bool led3_status = !(value % 8 > 3);

	gpio_port_pins_t mask = BIT(leds[0].pin) | BIT(leds[1].pin) |
							BIT(leds[2].pin) | BIT(leds[3].pin);

	gpio_port_value_t val = led0_status << leds[0].pin |
							led1_status << leds[1].pin |
							led2_status << leds[2].pin |
							led3_status << leds[3].pin;

	gpio_port_set_masked_raw(leds[0].port, mask, val);
}

void event_handler(struct esb_evt const *event)
{
	switch (event->evt_id)
	{
	case ESB_EVENT_TX_SUCCESS:
		LOG_DBG("TX SUCCESS EVENT");
		break;
	case ESB_EVENT_TX_FAILED:
		LOG_DBG("TX FAILED EVENT");
		break;
	case ESB_EVENT_RX_RECEIVED:
		if (esb_read_rx_payload(&rx_payload) == 0)
		{
			LOG_DBG("Packet received, len %d : "
					"0x%02x, 0x%02x, 0x%02x, 0x%02x, "
					"0x%02x, 0x%02x, 0x%02x, 0x%02x",
					rx_payload.length, rx_payload.data[0],
					rx_payload.data[1], rx_payload.data[2],
					rx_payload.data[3], rx_payload.data[4],
					rx_payload.data[5], rx_payload.data[6],
					rx_payload.data[7]);

			leds_update(rx_payload.data[1]); // this might be slow.
		}
		else
		{
			LOG_ERR("Error while reading rx packet");
		}
		LOG_INF("RX RECEIVED");
		nrf_gpio_pin_toggle(TEST_PIN); // faster
		break;
	}
}

int clocks_start(void)
{
	int err;
	int res;
	struct onoff_manager *clk_mgr;
	struct onoff_client clk_cli;

	clk_mgr = z_nrf_clock_control_get_onoff(CLOCK_CONTROL_NRF_SUBSYS_HF);
	if (!clk_mgr)
	{
		LOG_ERR("Unable to get the Clock manager");
		return -ENXIO;
	}

	sys_notify_init_spinwait(&clk_cli.notify);

	err = onoff_request(clk_mgr, &clk_cli);
	if (err < 0)
	{
		LOG_ERR("Clock request failed: %d", err);
		return err;
	}

	do
	{
		err = sys_notify_fetch_result(&clk_cli.notify, &res);
		if (!err && res)
		{
			LOG_ERR("Clock could not be started: %d", res);
			return res;
		}
	} while (err);

	LOG_DBG("HF clock started");
	return 0;
}

int esb_initialize(void)
{
	int err;

	uint8_t g_base_addr_0[NUM_PRX_PERIPH][4] = {{0xE7, 0xE7, 0xE7, 0xE7}, {0xEE, 0xEE, 0xEE, 0xEE}};
	uint8_t base_addr_1[4] = {0xC2, 0xC2, 0xC2, 0xC2};
	uint8_t addr_prefix[8] = {0xE7, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8};
	uint32_t g_channels[NUM_PRX_PERIPH] = {2, 4}; // channel selection per periph

	struct esb_config config = ESB_DEFAULT_CONFIG;

	config.protocol = ESB_PROTOCOL_ESB_DPL;
	config.bitrate = ESB_BITRATE_2MBPS;
	config.mode = ESB_MODE_PRX;
	config.event_handler = event_handler;
	config.selective_auto_ack = true;
	config.retransmit_count = 0; // dont retransmit.
	config.use_fast_ramp_up = true;

	err = esb_init(&config);
	if (err)
	{
		return err;
	}

	err = esb_set_base_address_0(g_base_addr_0[peripheral_number]);
	if (err)
	{
		return err;
	}

	err = esb_set_base_address_1(base_addr_1);
	if (err)
	{
		return err;
	}

	err = esb_set_prefixes(addr_prefix, ARRAY_SIZE(addr_prefix));
	if (err)
	{
		return err;
	}

	err = esb_set_rf_channel(g_channels[peripheral_number]);
	if (err)
	{
		return err;
	}

	return 0;
}

int main(void)
{
	int err;

	LOG_INF("Enhanced ShockBurst prx sample");

	// init test pin
	nrf_gpio_cfg_output(TEST_PIN);
	nrf_gpio_pin_clear(TEST_PIN); // start clear

	err = clocks_start();
	if (err)
	{
		return 0;
	}

	err = leds_init();
	if (err)
	{
		return 0;
	}

	err = buttons_init();
	if (err)
	{
		return 0;
	}

	// wait until peripheral number selection
	while (peripheral_number < 0)
	{
		// press button 1 or 2 to set up device and leave
	}

	err = esb_initialize();
	if (err)
	{
		LOG_ERR("ESB initialization failed, err %d", err);
		return 0;
	}

	LOG_INF("Initialization complete");

	err = esb_write_payload(&tx_payload);
	if (err)
	{
		LOG_ERR("Write payload, err %d", err);
		return 0;
	}

	LOG_INF("Setting up for packet receiption");

	err = esb_start_rx();
	if (err)
	{
		LOG_ERR("RX setup failed, err %d", err);
		return 0;
	}

	/* return to idle thread */
	return 0;
}
