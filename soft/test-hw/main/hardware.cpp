#include "common.h"
#include "hardware.h"

#include "pins.h"

static const char TAG[] = "HW";

spi_device_handle_t spi_h;

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
    pins_init(GPIO_MODE_OUTPUT, GPIO_PULLUP_DISABLE, bit(PIN_CRESET, PIN_SPI_MOSI, PIN_SPI_SCK, PIN_SPI_SS, PIN_ROM_DIS3, PIN_ROM_DIS2, PIN_ROM_DIS1));
    pins_init(GPIO_MODE_INPUT,  GPIO_PULLUP_ENABLE,  bit(PIN_CDONE, PIN_IOB29b, PIN_IOB31b, PIN_S_UKNC, PIN_S_BK));
    pins_init(GPIO_MODE_INPUT,  GPIO_PULLUP_DISABLE, bit(PIN_Hit, PIN_Left, PIN_Right, PIN_Up, PIN_Down, PIN_SPI_MISO));
    gpio_set_level(PIN_CRESET, 0);
    gpio_set_level(PIN_SPI_SS, 0);


    spi_bus_config_t buscfg = {
            .mosi_io_num = PIN_SPI_MOSI,
            .miso_io_num = PIN_SPI_MISO,
            .sclk_io_num = PIN_SPI_SCK,
            .quadwp_io_num = -1,
            .quadhd_io_num = -1,
    };

    auto err = spi_bus_initialize(HSPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
    ESP_ERROR_CHECK(err);

    spi_device_interface_config_t spi_cfg = {
        .mode = 3,
        .clock_speed_hz = SPI_MASTER_FREQ_20M,
        .spics_io_num = -1,
        .flags = SPI_DEVICE_NO_DUMMY,
        .queue_size=1
    };

    err = spi_bus_add_device(HSPI_HOST, &spi_cfg, &spi_h);
    ESP_ERROR_CHECK(err);

    oled.init();
}

#define ERROR_CHECK(m, val) if (!(val)) {msg("ERR: " m); return;}

void load_fpga()
{
    spi_transaction_t t{0};
#define SETUP_ZERO_BYTE \
    t.length=8; \
    t.flags = SPI_TRANS_USE_TXDATA; \
    t.tx_buffer = NULL
#define SEND(name) ERROR_CHECK(name, spi_device_polling_transmit(spi_h, &t))
#define SEND_ZERO_BYTE(name) SETUP_ZERO_BYTE; SEND(name)
#define SEND_BUF(name, buf, size) \
    t.length = (size);            \
    t.tx_buffer = buf;            \
    t.flags = 0;                  \
    SEND(name)

    gpio_set_level(PIN_CRESET, 0);
    gpio_set_level(PIN_SPI_SS, 0);
    vTaskDelay(ms2ticks(1));
    gpio_set_level(PIN_CRESET, 1);
    vTaskDelay(ms2ticks(2));
    gpio_set_level(PIN_SPI_SS, 1);
    SEND_ZERO_BYTE("Tr6");
    gpio_set_level(PIN_SPI_SS, 0);
    SEND_BUF("Tr8", fpga_image, fpga_image_size*8);
    gpio_set_level(PIN_SPI_SS, 1);
    int cnt = 0;
    SETUP_ZERO_BYTE;
    while(!gpio_get_level(PIN_CDONE))
    {
        ERROR_CHECK("CDONE", ++cnt < 13);
        SEND("Tr10");
    }
    uint64_t buf = 0;
    SEND_BUF("Tr11", &buf, 49);
    msg("FPGA Load done");
}
