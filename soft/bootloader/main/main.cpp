#include "common.h"

#include "hardware.h"
#include "setup_data.h"
#include "wifi_module.h"
#include "tftp/include/tftp_ota_server.h"

extern "C" void app_main(void)
{
    hw_init();
    load_config();
    wifi_init();

    oled.draw_text(OLED_AP_ROW, "SSID: " MASTER_WIFI_SSID "\nPSWD: " MASTER_WIFI_PASSWD "\nIP  : 192.168.4.1");

    TftpOtaServer srv;
    if (srv.start())
    {
        msg("Can't start TFTP srv\nRestart please.");
        for(;;);
    }
    msg("Waiting for update");
    for(;;) srv.run();
}

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
