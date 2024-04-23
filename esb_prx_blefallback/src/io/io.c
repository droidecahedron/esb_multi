#include <zephyr/logging/log.h>
#include "io.h"

LOG_MODULE_REGISTER(IO_C);

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

volatile int peripheral_number = -1; // used to select addr0 and channel in the inits
extern struct k_work rf_swap_work;   // kernel work item to perform RF Swap.

static struct gpio_callback button_callback;

int leds_init(void)
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
    LOG_DBG("Button pressed at %" PRIu32, k_cycle_get_32());
    LOG_DBG("pins var: %d", pins);

    switch (pins)
    {
    case dk_button1_msk:
        LOG_DBG("BUTTON1");
        peripheral_number = 0; // first peripheral
        break;

    case dk_button2_msk:
        LOG_DBG("BUTTON2");
        peripheral_number = 1;
        break;

    case dk_button3_msk:
        LOG_DBG("BUTTON3");
        if (peripheral_number >= 0)
        {
            k_work_submit(&rf_swap_work);
        }
        break;

    default:
        LOG_DBG("unknown pin in button callback");
    }
}

int buttons_init(void)
{
    int err = NRFX_SUCCESS;
    static int num_buttons = ARRAY_SIZE(buttons) - 1; // Button 4 will be for BLE service.
    if (!device_is_ready(buttons[0].port))
    {
        LOG_ERR("LEDs port not ready");
        return -ENODEV;
    }

    //--- Buttons
    for (size_t i = 0; i < num_buttons; i++)
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
    for (size_t i = 0; i < num_buttons; i++)
    {
        err = gpio_pin_interrupt_configure_dt(&buttons[i], GPIO_INT_EDGE_TO_ACTIVE);
        if (err)
        {
            LOG_ERR("Cannot disable callbacks()");
            return err;
        }
        pin_mask |= BIT(buttons[i].pin);
    }

    gpio_init_callback(&button_callback, button_pressed, pin_mask);
    for (size_t i = 0; i < num_buttons; i++)
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
