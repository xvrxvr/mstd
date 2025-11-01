#pragma once

static constexpr size_t MAX_CFG_SIZE = 4096;

struct Config_V0 {
    uint32_t crc;   // crc from next field to end of config record
    uint16_t size;  // = (sizeof(Config_V1)-1)/4. Maximum is 0x3FF. Bits 0xFC00 reserved (should be 0)
    uint8_t version;
    char ssid[33] = {0};
    char passwd[64] = {0};
    uint8_t oled_contrast = {0xCF};
    uint8_t reserved[3] = {0};
};

void load_config();

extern Config_V0* config;
extern size_t cfg_size; // Current config starts from 'config' and span 'cfg_size' bytes
extern uint8_t cfg_full[MAX_CFG_SIZE]; // Image of config partiton
extern bool cfg_full_loaded; // Set to true if partititon config was loaded

// Save config image, return NULL
// On error return error string
// 'image' can be mofified - if CRC field contains -1 it will bne replaced by real crc
const char* save_config_image(void* image, size_t size);

// Write whole partition with config
const char* save_config_full(uint8_t image[MAX_CFG_SIZE]);
