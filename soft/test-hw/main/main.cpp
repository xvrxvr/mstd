#include "common.h"

#include "hardware.h"

extern "C" void app_main(void)
{
    hw_init();
    oled.draw_text(OLED_AP_ROW, "HW Test run");

    load_fpga();
    for(;;);
}

/*
void reboot()
{
    esp_task_wdt_config_t twdt_config = {
        .timeout_ms = 5000,
        .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,    // Bitmask of all cores
        .trigger_panic = true
    };
    ESP_ERROR_CHECK(esp_task_wdt_init(&twdt_config));
    esp_task_wdt_add(NULL);
}
*/
