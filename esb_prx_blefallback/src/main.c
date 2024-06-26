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

#include "ble/ble_service.h"
#include "io/io.h"

// radio debugs
#include <debug/ppi_trace.h>
#include <hal/nrf_radio.h>
#include <hal/nrf_uarte.h>

LOG_MODULE_REGISTER(esb_prx);

// radio debugging
#define TEST_PIN 29 // this test pin is in the esb cb
#define RADIO_TEST_PIN 31
static void radio_ppi_trace_setup(void)
{
	uint32_t start_evt;
	uint32_t stop_evt;
	void *handle;

	start_evt = nrf_radio_event_address_get(NRF_RADIO,
											NRF_RADIO_EVENT_READY);
	stop_evt = nrf_radio_event_address_get(NRF_RADIO,
										   NRF_RADIO_EVENT_DISABLED);

	handle = ppi_trace_pair_config(RADIO_TEST_PIN,
								   start_evt, stop_evt); // pin is high when radio is active
	__ASSERT(handle != NULL, "Failed to configure PPI trace pair.\n");

	ppi_trace_enable(handle);
}

static struct esb_payload rx_payload;
static struct esb_payload tx_payload = ESB_CREATE_PAYLOAD(0,
														  0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17);

/* These are arbitrary default addresses. In end user products
 * different addresses should be used for each set of devices.
 */
#define NUM_PRX_PERIPH 2
extern volatile int peripheral_number; // used to select addr0 and channel in the inits
volatile bool esb_running = true;

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

// RF Swap workQ
// So it runs from a cooperative thread. Work thread is cooperative, so calling fxn as work item works. Invoked in button callback in io.c
struct k_work rf_swap_work;
static void rf_swap_work_fxn(struct k_work *work)
{
	if (esb_running)
	{
		LOG_INF("Disable ESB, Enable BLE");
		esb_running = false;
		// esb_stop_rx();
		esb_disable();
		app_bt_restart();
	}
	else
	{
		LOG_INF("Disable BLE, Enable ESB");
		esb_running = true;
		bt_disable();
		esb_initialize();
		esb_start_rx();
	}
}

int main(void)
{
	int err;

	LOG_INF("Enhanced ShockBurst prx sample");

	radio_ppi_trace_setup();

	k_work_init(&rf_swap_work, rf_swap_work_fxn);

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

	err = app_bt_init();
	if (err)
	{
		return 0;
	}

	// wait until peripheral number selection
	while (peripheral_number < 0)
	{
		// press button 1 or 2 to set up device and leave
		k_msleep(100);
	}

	bt_disable(); // esb app first
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
