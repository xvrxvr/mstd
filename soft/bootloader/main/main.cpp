#include "common.h"

#include "hardware.h"
#include "setup_data.h"
#include "wifi_module.h"
#include "tftp/include/tftp_ota_server.h"


static void initialise_mdns(void)
{
    mdns_init();
    mdns_hostname_set("mstdp");
    mdns_instance_name_set("MSTD Emulator");

    mdns_txt_item_t serviceTxtData[] = {
        {"board", "esp32"},
        {"path", "/"}
    };

    ESP_ERROR_CHECK(mdns_service_add("ESP32-WebServer", "_http", "_tcp", 80, serviceTxtData,
                                     sizeof(serviceTxtData) / sizeof(serviceTxtData[0])));
}

extern "C" void app_main(void)
{
    hw_init();
    load_config();
    wifi_init();

    initialise_mdns();
    netbiosns_init();
    netbiosns_set_name("mstdp");

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
