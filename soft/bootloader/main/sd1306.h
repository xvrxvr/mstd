#pragma once

#include <initializer_list>
#include "driver/i2c_master.h"

class SSD1306 {
    i2c_master_dev_handle_t dev = 0;
    uint8_t pi_row;
    uint8_t pi_data;
    uint8_t pi_count;

    void send_cmd(const std::initializer_list<i2c_master_transmit_multi_buffer_info_t>& args);
    void setup_pi_row(int row);

    const char* draw_text_imp(int y, const char* text);
public:
    void init();

    void clr_screen(int row_start=0, int row_end=8);
    void set_contrast(uint8_t);
    void swap_display(bool);

    void draw_text(int y, const char* text);
    void start_pi(int row) {clr_screen(row); setup_pi_row(row);}
    void inc_pi();
};
