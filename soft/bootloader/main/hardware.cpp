#include "common.h"
#include "hardware.h"

#include "pins.h"

static const char TAG[] = "HW";

void pins_init(gpio_mode_t mode, gpio_pullup_t pull_up_en, uint64_t bmask)
{
    gpio_config_t gpio_cfg = {
        .pin_bit_mask = bmask,
        .mode = mode,
        .pull_up_en = pull_up_en,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&gpio_cfg));
}

void hw_init()
{
    gpio_set_level(PIN_CRESET, 0);
    pins_init(GPIO_MODE_OUTPUT, GPIO_PULLUP_DISABLE, bit(PIN_CRESET));
    pins_init(GPIO_MODE_INPUT,  GPIO_PULLUP_ENABLE,  bit(PIN_CDONE, PIN_IOB29b, PIN_IOB31b, PIN_SPI_MISO, PIN_SPI_MOSI, PIN_SPI_SCK, PIN_SPI_SS, PIN_ROM_DIS3, PIN_S_UKNC, PIN_ROM_DIS1, PIN_S_BK, PIN_ROM_DIS2));
    pins_init(GPIO_MODE_INPUT,  GPIO_PULLUP_DISABLE, bit(PIN_Hit, PIN_Left, PIN_Right, PIN_Up, PIN_Down));
    gpio_set_level(PIN_CRESET, 0);

    oled.init();
}
