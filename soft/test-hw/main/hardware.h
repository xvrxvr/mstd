#pragma once

#include "sd1306.h"

static constexpr int OLED_AP_ROW  = 0;
static constexpr int OLED_STA_ROW = 3;
static constexpr int OLED_MSG_ROW = 5;
static constexpr int OLED_MSG_PI  = 6;

/*  8 x 21               V
 0: SSID: MSTDP          <
 1: PSWD: MSTD-BK-UKNC   <
 2: IP  : 192.168.4.1    <
 3: STA : ....           <
 4: AKA: mstdp[.local]   <
 5: Waiting for update   <
    Update firmware
    Update config
    Download config
    Update failed:
    Update done
 6: .. Progress indicator ..  (16*128 = 2048)
    Reboot in 5 seconds  <
    Wrong file extension <
    Prepare partition
    Partition writing
    Flash verification
    Config CRC
    Config start
    Config size
 7: .. Progress indicator ..

*/
static constexpr int TOTAL_PI = 2048;
 
extern SSD1306 oled;

void hw_init();

inline void msg(const char* msg) {oled.draw_text(OLED_MSG_ROW, msg);}

void load_fpga();
