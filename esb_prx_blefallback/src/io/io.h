#ifndef IO_H_
#define IO_H_

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/util.h>
#include <inttypes.h>
#include <soc.h>

// 52840dk
#define dk_button1_msk 1 << 11 // button1 is gpio pin 11 in the .dts
#define dk_button2_msk 1 << 12 // button2 is gpio pin 12 in the .dts
#define dk_button3_msk 1 << 24 // button3 is gpio pin 24 in the .dts
#define GPIO_SPEC_AND_COMMA(button_or_led) GPIO_DT_SPEC_GET(button_or_led, gpios),
#define BUTTONS_NODE DT_PATH(buttons)
static const struct gpio_dt_spec buttons[] = {
#if DT_NODE_EXISTS(BUTTONS_NODE)
    DT_FOREACH_CHILD(BUTTONS_NODE, GPIO_SPEC_AND_COMMA)
#endif
};

int leds_init(void);
void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins);
int buttons_init(void);

#endif
